#!/usr/bin/env bash
# Inclusion criteria for PCH:
#   - Used in ≥5 translation units
#   - Cross-platform (no <windows.h>, <unistd.h>, etc.)
#   - Standard library or stable third-party (no project headers)

set -eu

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="${REPO_ROOT}/src"
PCH_FILE="${SRC_DIR}/pch.h"

ADD_THRESHOLD=5
REMOVE_THRESHOLD=3

PLATFORM_PATTERN='^(windows\.h|unistd\.h|io\.h|fcntl\.h|sys/.*|pwd\.h|dirent\.h|dlfcn\.h|pthread\.h|signal\.h|spawn\.h|CoreFoundation/.*|mach/.*|libgen\.h)$'
NONSTD_PATTERN='\.(h|hpp|hxx)$|/'

if [ ! -f "${PCH_FILE}" ]; then
    echo "error: ${PCH_FILE} not found" >&2
    exit 2
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "${TMPDIR}"' EXIT

grep -rh "^#include <" "${SRC_DIR}" 2>/dev/null \
    | sed -E 's/^#include <([^>]+)>.*$/\1/' \
    | sort | uniq -c | awk '{$1=$1; print}' \
    | sort -rn > "${TMPDIR}/usage.txt"

# Headers currently in pch.h.
grep -E '^#include <' "${PCH_FILE}" \
    | sed -E 's/^#include <([^>]+)>.*$/\1/' \
    | sort -u > "${TMPDIR}/in_pch.txt"

awk -v threshold="${ADD_THRESHOLD}" \
    -v platform_re="${PLATFORM_PATTERN}" \
    -v nonstd_re="${NONSTD_PATTERN}" '
    {
        count = $1
        header = $2
        if (count < threshold) next
        if (header ~ platform_re) next
        if (header ~ nonstd_re) next
        print count " " header
    }
' "${TMPDIR}/usage.txt" > "${TMPDIR}/eligible.txt"

awk 'NR==FNR { in_pch[$0]=1; next } !($2 in in_pch)' \
    "${TMPDIR}/in_pch.txt" "${TMPDIR}/eligible.txt" > "${TMPDIR}/add_candidates.txt"

awk -v threshold="${REMOVE_THRESHOLD}" '
    NR==FNR { count[$2] = $1; next }
    {
        c = ($0 in count) ? count[$0] : 0
        if (c < threshold) print c " " $0
    }
' "${TMPDIR}/usage.txt" "${TMPDIR}/in_pch.txt" \
    | sort -n > "${TMPDIR}/remove_candidates.txt"

EXIT_CODE=0

if [ -s "${TMPDIR}/add_candidates.txt" ]; then
    echo "Headers to consider ADDING to pch.h (used >= ${ADD_THRESHOLD} times, currently missing):"
    awk '{ printf "  %3d uses  <%s>\n", $1, $2 }' "${TMPDIR}/add_candidates.txt"
    echo
    EXIT_CODE=1
fi

if [ -s "${TMPDIR}/remove_candidates.txt" ]; then
    echo "Headers to consider REMOVING from pch.h (used < ${REMOVE_THRESHOLD} times):"
    awk '{ printf "  %3d uses  <%s>\n", $1, $2 }' "${TMPDIR}/remove_candidates.txt"
    echo
    EXIT_CODE=1
fi

if [ "${EXIT_CODE}" -eq 0 ]; then
    echo "pch.h looks healthy — no suggestions."
    echo
fi

echo "Top 25 standard headers by usage:"
awk -v platform_re="${PLATFORM_PATTERN}" -v nonstd_re="${NONSTD_PATTERN}" '
    $2 ~ platform_re { next }
    $2 ~ nonstd_re { next }
    { print }
' "${TMPDIR}/usage.txt" | head -25 \
    | awk -v in_pch_file="${TMPDIR}/in_pch.txt" '
        BEGIN {
            while ((getline line < in_pch_file) > 0) in_pch[line] = 1
            close(in_pch_file)
        }
        {
            marker = ($2 in in_pch) ? "[in pch]" : "        "
            printf "  %s  %3d uses  <%s>\n", marker, $1, $2
        }
    '

exit "${EXIT_CODE}"