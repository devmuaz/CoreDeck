#!/usr/bin/env bash
# Pack the Linux CoreDeck binary and assets into a tar.gz.

set -euo pipefail

: "${ARTIFACT_NAME:?}"
: "${ASSET_NAME:?}"

mkdir -p "release/${ARTIFACT_NAME}"
cp build/CoreDeck "release/${ARTIFACT_NAME}/"
cp -r build/assets "release/${ARTIFACT_NAME}/"
tar -czf "release/${ASSET_NAME}" -C release "${ARTIFACT_NAME}"
