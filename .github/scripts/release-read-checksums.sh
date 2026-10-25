#!/usr/bin/env bash
# Read the Homebrew checksums into GITHUB_OUTPUT.

set -euo pipefail

: "${GITHUB_OUTPUT:?}"

DARWIN_ARM64_SHA_FILE=$(find artifacts -name 'coredeck-darwin-arm64.dmg.sha256' | head -n 1)
DARWIN_X86_64_SHA_FILE=$(find artifacts -name 'coredeck-darwin-x86-64.dmg.sha256' | head -n 1)
LINUX_ARM64_SHA_FILE=$(find artifacts -name 'coredeck-linux-arm64.tar.gz.sha256' | head -n 1)
LINUX_X86_64_SHA_FILE=$(find artifacts -name 'coredeck-linux-x86-64.tar.gz.sha256' | head -n 1)
if [[ -z "$DARWIN_ARM64_SHA_FILE" || -z "$DARWIN_X86_64_SHA_FILE" || -z "$LINUX_ARM64_SHA_FILE" || -z "$LINUX_X86_64_SHA_FILE" ]]; then
  echo "::error::Missing a release checksum (darwin arm64='${DARWIN_ARM64_SHA_FILE}', darwin x86-64='${DARWIN_X86_64_SHA_FILE}', linux arm64='${LINUX_ARM64_SHA_FILE}', linux x86-64='${LINUX_X86_64_SHA_FILE}')"
  exit 1
fi
echo "sha256_darwin_arm64=$(awk '{print $1}' "$DARWIN_ARM64_SHA_FILE")" >> "$GITHUB_OUTPUT"
echo "sha256_darwin_x86_64=$(awk '{print $1}' "$DARWIN_X86_64_SHA_FILE")" >> "$GITHUB_OUTPUT"
echo "sha256_linux_arm64=$(awk '{print $1}' "$LINUX_ARM64_SHA_FILE")" >> "$GITHUB_OUTPUT"
echo "sha256_linux_x86_64=$(awk '{print $1}' "$LINUX_X86_64_SHA_FILE")" >> "$GITHUB_OUTPUT"
