#!/usr/bin/env bash
# C++ driver for the macOS Homebrew Clang launches.
#
# CMake adds the macOS SDK's usr/include as a system include. Homebrew's
# libc++ must see its own C wrapper headers before those, so this driver
# puts that include directory first. C and Objective-C stay on Homebrew clang.

set -euo pipefail

clangxx="/opt/homebrew/opt/llvm/bin/clang++"
libcxx="/opt/homebrew/opt/llvm/include/c++/v1"

if [[ ! -x "${clangxx}" || ! -d "${libcxx}" ]]; then
  echo "error: Homebrew Clang was not found. Install it with: brew install llvm" >&2
  exit 1
fi

for arg in "$@"; do
  case "${arg}" in
    *.m | *.mm)
      exec "${clangxx}" -isystem "${libcxx}" -fno-objc-msgsend-class-selector-stubs "$@"
      ;;
  esac
done

exec "${clangxx}" -isystem "${libcxx}" "$@"
