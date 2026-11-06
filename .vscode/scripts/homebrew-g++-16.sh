#!/usr/bin/env bash
# C++ driver for the macOS Homebrew GCC 16 launches.
#
# FSF GCC cannot parse Apple block syntax in the macOS SDK. The Cocoa
# translation units are compiled with Apple clang++ against this GCC's
# libstdc++, so they link with the rest of the GCC build. Everything else,
# including the link, is g++-16.

set -euo pipefail

gcc_bin="/opt/homebrew/bin/g++-16"
clang_bin="/usr/bin/clang++"

if [[ ! -x "${gcc_bin}" ]]; then
  echo "error: ${gcc_bin} was not found. Install it with: brew install gcc" >&2
  exit 1
fi

source_file=""
for arg in "$@"; do
  case "${arg}" in
    *.cpp | *.cc | *.cxx | *.mm)
      source_file="${arg}"
      ;;
  esac
done

use_clang=0
case "${source_file}" in
  */backends/imgui_impl_glfw.cpp | */src/gui/theme_mac.mm)
    use_clang=1
    ;;
esac

if [[ "${use_clang}" -eq 0 ]]; then
  exec "${gcc_bin}" "$@"
fi

libstd_includes=()
started=0
while IFS= read -r line; do
  if [[ "${line}" == "#include <...> search starts here:" ]]; then
    started=1
    continue
  fi
  if [[ "${started}" -eq 0 ]]; then
    continue
  fi
  if [[ "${line}" == "End of search list." ]]; then
    break
  fi
  line="${line#"${line%%[![:space:]]*}"}"
  if [[ "${line}" == *"/include/c++/"* ]]; then
    libstd_includes+=("${line}")
  fi
done < <("${gcc_bin}" -std=c++20 -E -x c++ - -v </dev/null 2>&1)

if [[ ${#libstd_includes[@]} -eq 0 ]]; then
  echo "error: could not find GCC libstdc++ headers via ${gcc_bin}" >&2
  exit 1
fi

clang_args=(-stdlib=libstdc++ -nostdinc++)
for include_dir in "${libstd_includes[@]}"; do
  clang_args+=(-isystem "${include_dir}")
done

exec "${clang_bin}" "${clang_args[@]}" "$@"
