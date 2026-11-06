#!/usr/bin/env bash
# Configure a Ninja tree with tests enabled, then build one target.
# Usage: cmake-build.sh <Debug|Release> <CoreDeck|coredeck_tests> [gcc|llvm]
#
# With no toolchain, CMake uses Apple Clang in build-debug / build-release.
# gcc writes build-homebrew-gcc16-debug / build-homebrew-gcc16-release.
# llvm writes build-homebrew-clang-debug / build-homebrew-clang-release.
# gcc compiles C++ with Homebrew g++-16. C and Objective-C stay on Apple
# Clang, because the Cocoa sources use Apple blocks, which GCC cannot parse.
# llvm compiles C, C++, and Objective-C with Homebrew Clang. Objective-C
# class stubs are turned off so the binary still deploys to macOS 12.

set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "usage: cmake-build.sh <Debug|Release> <CoreDeck|coredeck_tests> [gcc|llvm]" >&2
  exit 2
fi

build_type="$1"
target="$2"
toolchain="${3:-}"

case "${build_type}" in
  Debug) build_dir="build-debug" ;;
  Release) build_dir="build-release" ;;
  *)
    echo "error: build type must be Debug or Release (got ${build_type})" >&2
    exit 2
    ;;
esac

case "${toolchain}" in
  "" | gcc | llvm) ;;
  *)
    echo "error: toolchain must be gcc or llvm (got ${toolchain})" >&2
    exit 2
    ;;
esac

if [[ -n "${toolchain}" ]]; then
  if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: the ${toolchain} toolchain builds the macOS app on this machine. Linux and Windows use their own compiler." >&2
    exit 2
  fi
  case "${toolchain}-${build_type}" in
    gcc-Debug) build_dir="build-homebrew-gcc16-debug" ;;
    gcc-Release) build_dir="build-homebrew-gcc16-release" ;;
    llvm-Debug) build_dir="build-homebrew-clang-debug" ;;
    llvm-Release) build_dir="build-homebrew-clang-release" ;;
  esac
fi

case "${target}" in
  CoreDeck | coredeck_tests) ;;
  *)
    echo "error: target must be CoreDeck or coredeck_tests (got ${target})" >&2
    exit 2
    ;;
esac

root="$(cd "$(dirname "$0")/../.." && pwd)"
build_path="${root}/${build_dir}"

cmake_args=(
  -S "${root}"
  -B "${build_path}"
  -G Ninja
  -DCMAKE_BUILD_TYPE="${build_type}"
  -DCOREDECK_BUILD_TESTS=ON
)

if [[ "${toolchain}" == "gcc" ]]; then
  cxx_wrapper="${root}/.vscode/scripts/homebrew-g++-16.sh"
  if [[ ! -x "${cxx_wrapper}" ]]; then
    echo "error: ${cxx_wrapper} is missing or not executable" >&2
    exit 1
  fi
  if [[ ! -x /opt/homebrew/bin/g++-16 ]]; then
    echo "error: /opt/homebrew/bin/g++-16 was not found. Install it with: brew install gcc" >&2
    exit 1
  fi
  cmake_args+=(
    -DCMAKE_C_COMPILER=/usr/bin/clang
    -DCMAKE_CXX_COMPILER="${cxx_wrapper}"
    -DCMAKE_OBJC_COMPILER=/usr/bin/clang
    -DCMAKE_OBJCXX_COMPILER=/usr/bin/clang++
    -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
  )
elif [[ "${toolchain}" == "llvm" ]]; then
  clang_bin="/opt/homebrew/opt/llvm/bin/clang"
  clangxx_wrapper="${root}/.vscode/scripts/homebrew-clang++.sh"
  if [[ ! -x "${clang_bin}" || ! -x "${clangxx_wrapper}" ]]; then
    echo "error: Homebrew Clang was not found. Install it with: brew install llvm" >&2
    exit 1
  fi
  cmake_args+=(
    -DCMAKE_C_COMPILER="${clang_bin}"
    -DCMAKE_CXX_COMPILER="${clangxx_wrapper}"
    -DCMAKE_OBJC_COMPILER="${clang_bin}"
    -DCMAKE_OBJCXX_COMPILER="${clangxx_wrapper}"
    -DCMAKE_OBJC_FLAGS=-fno-objc-msgsend-class-selector-stubs
    -DCMAKE_CXX_FLAGS=
  )
fi

cmake "${cmake_args[@]}"
cmake --build "${build_path}" --parallel --target "${target}"
