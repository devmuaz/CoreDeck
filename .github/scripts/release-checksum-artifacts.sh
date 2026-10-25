#!/usr/bin/env bash
# Write a .sha256 file next to each release archive under artifacts/.

set -euo pipefail

cd artifacts
while IFS= read -r f; do
  [ -z "$f" ] && continue
  sha256sum "$f" > "${f}.sha256"
done < <(find . -type f \( -name '*.tar.gz' -o -name '*.msi' -o -name '*.zip' -o -name '*.dmg' \))
