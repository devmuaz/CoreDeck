#!/usr/bin/env bash
# Print the CHANGELOG.md section for a version tag, without the heading.
# Leading and trailing blank lines are removed.
# Usage: release-extract-changelog.sh VERSION [CHANGELOG_PATH]

set -euo pipefail

version="${1:?version is required}"
changelog="${2:-CHANGELOG.md}"

awk -v ver="$version" '
  $0 ~ "^## \\["ver"\\]" { found=1; next }
  found && /^## \[/      { exit }
  found                  { print }
' "$changelog" | awk '
  !started && $0 ~ /./ { started=1 }
  started { lines[++n]=$0 }
  $0 ~ /./ { last=n }
  END { for (i=1; i<=last; i++) print lines[i] }
'
