#!/usr/bin/env bash
# Sign CoreDeck.app, package a DMG, then notarize and staple it.

set -euo pipefail

: "${CERTIFICATE_NAME:?}"
: "${EXPECTED_ARCH:?}"
: "${ASSET_NAME:?}"
: "${APPLE_ID:?}"
: "${APPLE_PASSWORD:?}"
: "${APPLE_TEAM_ID:?}"

APP_PATH="build/CoreDeck.app"
BIN="$APP_PATH/Contents/MacOS/CoreDeck"
ACTUAL_ARCH=$(lipo -archs "$BIN")
if [[ "$ACTUAL_ARCH" != "$EXPECTED_ARCH" ]]; then
  echo "::error::CoreDeck.app is $ACTUAL_ARCH, expected $EXPECTED_ARCH"
  exit 1
fi

# Sign nested Mach-O files before the bundle (replaces deprecated --deep).
# Detect Mach-O by content, not extension, so helpers such as
# sentry-native's crashpad_handler are included.
# Skip the main executable. Signing Contents/MacOS/CoreDeck on its
# own clears the signature on sibling helpers, and verify --strict
# then reports crashpad_handler as unsigned.
while IFS= read -r -d '' f; do
  [[ "$f" == "$BIN" ]] && continue
  if file -b "$f" 2>/dev/null | grep -q "Mach-O"; then
    codesign --force --options runtime --sign "$CERTIFICATE_NAME" "$f"
  fi
done < <(find "$APP_PATH" -type f -print0)

# Sign nested .framework bundles, if any.
if [ -d "$APP_PATH/Contents/Frameworks" ]; then
  while IFS= read -r f; do
    [ -z "$f" ] && continue
    codesign --force --options runtime --sign "$CERTIFICATE_NAME" "$f"
  done < <(find "$APP_PATH/Contents/Frameworks" -maxdepth 1 -type d -name "*.framework")
fi

# Sign the helper last among nested code, then seal the bundle.
if [[ -f "$APP_PATH/Contents/MacOS/crashpad_handler" ]]; then
  codesign --force --options runtime --sign "$CERTIFICATE_NAME" "$APP_PATH/Contents/MacOS/crashpad_handler"
fi

# Then sign the outer bundle, which signs the main executable.
codesign --force --options runtime --sign "$CERTIFICATE_NAME" "$APP_PATH"

# Sanity-check the signature before packaging.
codesign --verify --strict --verbose=2 "$APP_PATH"

# Build the .dmg containing the signed .app via CPack (DragNDrop).
# hdiutil detach can fail transiently on GitHub runners when Spotlight,
# Finder helpers, or a leftover mount from a previous attempt keep the
# volume busy. Force-eject any stale /Volumes/CoreDeck mount first,
# then retry the cpack invocation a couple of times if it fails.
mkdir -p release
cd build

# Volume name comes from CPACK_DMG_VOLUME_NAME = ${PROJECT_NAME}.
detach_stale_volume() {
  for mount in /Volumes/CoreDeck /Volumes/CoreDeck-*; do
    if [ -d "$mount" ]; then
      echo "Detaching stale mount: $mount"
      hdiutil detach "$mount" -force || true
    fi
  done
}

# Clean up before the first attempt, not just on retry, in case a
# previous job on this runner left a stale /Volumes mount behind.
detach_stale_volume

attempts=0
max_attempts=3
until cpack -G DragNDrop; do
  attempts=$((attempts + 1))
  if [ "$attempts" -ge "$max_attempts" ]; then
    echo "::error::cpack -G DragNDrop failed after $attempts attempts"
    exit 1
  fi
  echo "::warning::cpack DragNDrop attempt $attempts failed; cleaning up and retrying in 5s"
  detach_stale_volume
  rm -f ./*.dmg
  sleep 5
done
cd ..
# CPack produces a versioned filename (e.g. CoreDeck-1.2.3-Darwin.dmg);
# grab whichever .dmg it created and rename to our stable asset name.
DMG_BUILT=$(ls build/*.dmg | head -n 1)
mv "$DMG_BUILT" "release/${ASSET_NAME}"

# Sign the .dmg itself, then notarize and staple it.
codesign --force --sign "$CERTIFICATE_NAME" "release/${ASSET_NAME}"

# Submit and capture the JSON output so we can fetch the log on failure.
set +e
NOTARY_OUTPUT=$(xcrun notarytool submit "release/${ASSET_NAME}" \
  --apple-id "${APPLE_ID}" \
  --password "${APPLE_PASSWORD}" \
  --team-id "${APPLE_TEAM_ID}" \
  --wait \
  --output-format json)
NOTARY_EXIT=$?
set -e
echo "$NOTARY_OUTPUT"

NOTARY_STATUS=$(echo "$NOTARY_OUTPUT" | python3 -c "import json,sys; print(json.load(sys.stdin).get('status',''))" 2>/dev/null || echo "")
NOTARY_ID=$(echo "$NOTARY_OUTPUT" | python3 -c "import json,sys; print(json.load(sys.stdin).get('id',''))" 2>/dev/null || echo "")

if [ "$NOTARY_STATUS" != "Accepted" ]; then
  echo "::error::Notarization failed (status=$NOTARY_STATUS). Fetching log:"
  if [ -n "$NOTARY_ID" ]; then
    xcrun notarytool log "$NOTARY_ID" \
      --apple-id "${APPLE_ID}" \
      --password "${APPLE_PASSWORD}" \
      --team-id "${APPLE_TEAM_ID}" || true
  fi
  exit ${NOTARY_EXIT:-1}
fi

xcrun stapler staple "release/${ASSET_NAME}"
