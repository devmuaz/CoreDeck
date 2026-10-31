#!/usr/bin/env bash
# Launch the CoreDeck binary for this host.
# macOS opens the .app bundle. Linux detaches the ELF, like `open` and Start-Process.
# Usage: open-coredeck.sh <Debug|Release>

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: open-coredeck.sh <Debug|Release>" >&2
  exit 2
fi

case "$1" in
  Debug) build_dir="build-debug" ;;
  Release) build_dir="build-release" ;;
  *)
    echo "error: build type must be Debug or Release (got $1)" >&2
    exit 2
    ;;
esac

root="$(cd "$(dirname "$0")/../.." && pwd)"

case "$(uname -s)" in
  Darwin)
    open "${root}/${build_dir}/CoreDeck.app"
    ;;
  Linux)
    bin="${root}/${build_dir}/CoreDeck"
    if [[ ! -x "${bin}" ]]; then
      echo "error: Linux executable not found or not executable: ${bin}" >&2
      exit 1
    fi
    if command -v setsid >/dev/null 2>&1; then
      setsid -f "${bin}"
    else
      "${bin}"
    fi
    ;;
  *)
    echo "error: open-coredeck.sh does not support $(uname -s). On Windows, use open-coredeck.ps1." >&2
    exit 2
    ;;
esac
