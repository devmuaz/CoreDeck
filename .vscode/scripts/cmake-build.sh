#!/usr/bin/env bash
# Configure a Ninja tree with tests enabled, then build one target.
# Usage: cmake-build.sh <Debug|Release> <CoreDeck|coredeck_tests>

set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: cmake-build.sh <Debug|Release> <CoreDeck|coredeck_tests>" >&2
  exit 2
fi

build_type="$1"
target="$2"

case "${build_type}" in
  Debug) build_dir="build-debug" ;;
  Release) build_dir="build-release" ;;
  *)
    echo "error: build type must be Debug or Release (got ${build_type})" >&2
    exit 2
    ;;
esac

case "${target}" in
  CoreDeck | coredeck_tests) ;;
  *)
    echo "error: target must be CoreDeck or coredeck_tests (got ${target})" >&2
    exit 2
    ;;
esac

root="$(cd "$(dirname "$0")/../.." && pwd)"
build_path="${root}/${build_dir}"

cmake -S "${root}" -B "${build_path}" -G Ninja \
  -DCMAKE_BUILD_TYPE="${build_type}" \
  -DCOREDECK_BUILD_TESTS=ON
cmake --build "${build_path}" --parallel --target "${target}"
