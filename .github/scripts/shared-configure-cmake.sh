#!/usr/bin/env bash
# Configure the CoreDeck CMake build.
# Callers pass options through the environment.

set -euo pipefail

build_type="${BUILD_TYPE:-Release}"
build_tests="${BUILD_TESTS:-false}"
osx_arch="${OSX_ARCH:-}"
detect_osx_arch="${DETECT_OSX_ARCH:-false}"
use_ccache="${USE_CCACHE:-false}"
version_ref="${COREDECK_VERSION_REF:-}"

if [[ "${RUNNER_OS}" != "Windows" && "${use_ccache}" == "true" ]]; then
  export CMAKE_C_COMPILER_LAUNCHER=ccache
  export CMAKE_CXX_COMPILER_LAUNCHER=ccache
fi

cmake_args=(-B build -DCMAKE_BUILD_TYPE="${build_type}")

if [[ -n "${version_ref}" ]]; then
  cmake_args+=(-DCOREDECK_VERSION_OVERRIDE="${version_ref}")
fi

if [[ "${build_tests}" == "true" ]]; then
  cmake_args+=(-DCOREDECK_BUILD_TESTS=ON)
fi

if [[ -n "${osx_arch}" ]]; then
  cmake_args+=(-DCMAKE_OSX_ARCHITECTURES="${osx_arch}")
elif [[ "${detect_osx_arch}" == "true" && "${RUNNER_OS}" == "macOS" ]]; then
  case "$(uname -m)" in
    x86_64) cmake_args+=(-DCMAKE_OSX_ARCHITECTURES=x86_64) ;;
    arm64) cmake_args+=(-DCMAKE_OSX_ARCHITECTURES=arm64) ;;
  esac
fi

if [[ "${RUNNER_OS}" == "Windows" ]]; then
  cmake_args+=(-DCMAKE_PREFIX_PATH="${COREDECK_GETTEXT_ROOT}")
fi

cmake "${cmake_args[@]}"
