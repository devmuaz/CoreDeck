#!/usr/bin/env bash
# Pack the Linux CoreDeck binary and assets into a tar.gz.

set -euo pipefail

: "${ARTIFACT_NAME:?}"
: "${ASSET_NAME:?}"

desktop="build/com.devmuaz.coredeck.desktop"
if [[ ! -f "${desktop}" ]]; then
  echo "Missing ${desktop}. Configure CMake before packaging." >&2
  exit 1
fi

mkdir -p "release/${ARTIFACT_NAME}/share/applications" "release/${ARTIFACT_NAME}/share/icons"
cp build/CoreDeck "release/${ARTIFACT_NAME}/"
cp -r build/assets "release/${ARTIFACT_NAME}/"
cp "${desktop}" "release/${ARTIFACT_NAME}/share/applications/com.devmuaz.coredeck.desktop"
cp -R packaging/linux/hicolor "release/${ARTIFACT_NAME}/share/icons/hicolor"
tar -czf "release/${ASSET_NAME}" -C release "${ARTIFACT_NAME}"
