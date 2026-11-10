#!/usr/bin/env bash
# The same structure, format, and clang-tidy check as .github/workflows/structure-format-tidy.yml.
# Usage: tools/structure_format_tidy.sh [build-directory]

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "${root}"

export PATH="/opt/homebrew/opt/llvm/bin:/usr/local/opt/llvm/bin:${PATH}"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "error: clang-format was not found. CI uses clang-format 23.1.5." >&2
  exit 1
fi
if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "error: clang-tidy was not found. CI uses clang-tidy 23.1.4." >&2
  exit 1
fi

echo "Checking source structure..."
python3 tools/check_structure.py

echo "Checking format..."
git ls-files 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h' | xargs clang-format --dry-run --Werror
echo "Format is clean."

build_dir="${1:-build-tidy}"
echo "Configuring ${build_dir} for clang-tidy..."
cmake -S "${root}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCOREDECK_BUILD_TESTS=ON \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON

if [[ "$(uname -s)" == "Darwin" ]]; then
  jobs="$(sysctl -n hw.ncpu)"
else
  jobs="$(nproc)"
fi

file_list="$(mktemp)"
trap 'rm -f "${file_list}"' EXIT
python3 - "${root}" "${build_dir}/compile_commands.json" "${file_list}" <<'PY'
import json
import sys

root, commands_path, out_path = sys.argv[1:]
commands = json.load(open(commands_path, encoding="utf-8"))
prefix = root + "/src/"
with open(out_path, "w", encoding="utf-8") as out:
    for entry in commands:
        path = entry["file"]
        if path.startswith(prefix) and path.endswith(".cpp"):
            print(path, file=out)
PY
file_count="$(wc -l < "${file_list}" | tr -d ' ')"
echo "Running clang-tidy on ${file_count} files..."
xargs -P "${jobs}" -n 1 clang-tidy -p "${build_dir}" --quiet --checks=-misc-include-cleaner --warnings-as-errors='*' < "${file_list}"
echo "clang-tidy is clean."
