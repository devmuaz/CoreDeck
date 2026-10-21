#!/usr/bin/env bash
# Rewrites src/pch.h from actual include usage.
# Inclusion criteria for PCH:
#   - Used in ≥5 translation units
#   - Cross-platform (no <windows.h>, <unistd.h>, etc.)
#   - Standard library or stable third-party (no project headers)
# A header already in pch.h is removed when it is used fewer than 3 times.

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

awk '{ print $2 }' "${TMPDIR}/add_candidates.txt" > "${TMPDIR}/add_names.txt"
awk '{ print $2 }' "${TMPDIR}/remove_candidates.txt" > "${TMPDIR}/remove_names.txt"

if [ ! -s "${TMPDIR}/add_names.txt" ] && [ ! -s "${TMPDIR}/remove_names.txt" ]; then
    echo "src/pch.h is already up to date."
    exit 0
fi

awk -v add_file="${TMPDIR}/add_names.txt" -v remove_file="${TMPDIR}/remove_names.txt" '
    function emit_additions() {
        if (emitted) {
            return
        }
        emitted = 1
        for (i = 1; i <= nadd; i++) {
            printf "#include <%s>\n", add[i]
        }
    }
    BEGIN {
        while ((getline line < remove_file) > 0) {
            remove[line] = 1
        }
        close(remove_file)
        while ((getline line < add_file) > 0) {
            nadd++
            add[nadd] = line
        }
        close(add_file)
    }
    /^#include </ {
        in_includes = 1
        header = $0
        sub(/^#include </, "", header)
        sub(/>.*$/, "", header)
        if (header in remove) {
            next
        }
        print
        next
    }
    {
        if (in_includes) {
            emit_additions()
        }
        print
    }
    END {
        if (in_includes) {
            emit_additions()
        }
    }
' "${PCH_FILE}" > "${TMPDIR}/pch.h"

mv "${TMPDIR}/pch.h" "${PCH_FILE}"

echo "Updated src/pch.h"
if [ -s "${TMPDIR}/add_candidates.txt" ]; then
    echo "Added:"
    awk '{ printf "  <%s>\n", $2 }' "${TMPDIR}/add_candidates.txt"
fi
if [ -s "${TMPDIR}/remove_candidates.txt" ]; then
    echo "Removed:"
    awk '{ printf "  <%s>\n", $2 }' "${TMPDIR}/remove_candidates.txt"
fi