#!/usr/bin/env bash
# Run CTest, then fail if the build directory registered no tests.
# Usage: run-ctest.sh <Debug|Release>

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: run-ctest.sh <Debug|Release>" >&2
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
build_path="${root}/${build_dir}"

ctest --test-dir "${build_path}" --output-on-failure

if ! ctest --test-dir "${build_path}" -N | grep -qE 'Total Tests: [1-9]'; then
  echo "error: CTest registered no tests in ${build_path}. Configure with -DCOREDECK_BUILD_TESTS=ON." >&2
  exit 1
fi
