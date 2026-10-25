#!/usr/bin/env bash
# Write the release version and GitHub release body to GITHUB_OUTPUT.

set -euo pipefail

: "${GITHUB_OUTPUT:?}"
: "${GITHUB_REF:?}"

version="${GITHUB_REF#refs/tags/}"
script_dir=$(cd "$(dirname "$0")" && pwd)
notes=$("$script_dir/release-extract-changelog.sh" "$version" CHANGELOG.md)
if [[ -z "$notes" ]]; then
  notes="_See [CHANGELOG.md](https://github.com/devmuaz/CoreDeck/blob/main/CHANGELOG.md) for details._"
fi

export version notes
body=$(python3 - <<'PY'
import os
template = """### CoreDeck __VERSION__

A new CoreDeck release is available. Here's what's changed in this version.

### What's New
__NOTES__

---

Download the appropriate version for your platform:

**Windows (x86-64):**
- `coredeck-windows-x86-64.msi` - Professional installer (recommended)
- `coredeck-windows-x86-64.zip` - Portable version

**macOS (Apple Silicon):** `coredeck-darwin-arm64.dmg`

**macOS (Intel):** `coredeck-darwin-x86-64.dmg`

**Linux (x86-64):** `coredeck-linux-x86-64.tar.gz`

**Linux (ARM64):** `coredeck-linux-arm64.tar.gz`

Each artifact has an accompanying `.sha256` file for download verification.

### Installation
**Windows:** Double-click the MSI to install, or extract the ZIP and run CoreDeck.exe directly.

**macOS:** Open the DMG and drag `CoreDeck.app` into your Applications folder.

**Linux:** Extract the archive and run the `CoreDeck` executable.

### Troubleshooting (macOS)
If the app won't open after copying it out of the DMG, remove the quarantine attribute:
```bash
xattr -cr /Applications/CoreDeck.app
```
"""
text = template.replace("__VERSION__", os.environ["version"])
text = text.replace("__NOTES__", os.environ["notes"])
print(text, end="")
PY
)

{
  echo "version=${version}"
  echo "body<<__EOF__"
  printf '%s\n' "$body"
  echo "__EOF__"
} >> "$GITHUB_OUTPUT"
