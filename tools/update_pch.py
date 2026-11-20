#!/usr/bin/env python3
"""Rewrite src/pch.h from actual include usage.

A header is added when it is used in at least 5 translation units, is
cross-platform, and is a standard-library header. A header already in pch.h
is removed when it is used fewer than 3 times.

Usage:
  python3 tools/update_pch.py
"""

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
PCH = SRC / "pch.h"
ADD_THRESHOLD = 5
REMOVE_THRESHOLD = 3
INCLUDE_RE = re.compile(r"^#include <([^>]+)>")
PLATFORM_RE = re.compile(
    r"^(windows\.h|unistd\.h|io\.h|fcntl\.h|sys/.*|pwd\.h|dirent\.h|dlfcn\.h|pthread\.h|signal\.h|spawn\.h|CoreFoundation/.*|mach/.*|libgen\.h)$"
)
NONSTD_RE = re.compile(r"\.(h|hpp|hxx)$|/")


def usage_counts() -> Counter[str]:
    counts: Counter[str] = Counter()
    for path in SRC.rglob("*"):
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError):
            continue
        for line in text.splitlines():
            match = INCLUDE_RE.match(line)
            if match:
                counts[match.group(1)] += 1
    return counts


def pch_headers(text: str) -> list[str]:
    headers = []
    for line in text.splitlines():
        match = INCLUDE_RE.match(line)
        if match:
            headers.append(match.group(1))
    return headers


def eligible(counts: Counter[str]) -> list[tuple[int, str]]:
    chosen = [
        (count, header)
        for header, count in counts.items()
        if count >= ADD_THRESHOLD
        and PLATFORM_RE.match(header) is None
        and NONSTD_RE.search(header) is None
    ]
    chosen.sort(key=lambda item: (-item[0], item[1]))
    return chosen


def rewrite(text: str, remove: set[str], add: list[str]) -> str:
    lines = text.splitlines(keepends=True)
    if text and not text.endswith(("\n", "\r")):
        lines[-1] = lines[-1] + "\n"
    output: list[str] = []
    in_includes = False
    emitted = False

    def emit_additions() -> None:
        nonlocal emitted
        if emitted:
            return
        emitted = True
        for header in add:
            output.append(f"#include <{header}>\n")

    for line in lines:
        match = INCLUDE_RE.match(line)
        if match:
            in_includes = True
            if match.group(1) in remove:
                continue
            output.append(line if line.endswith("\n") else line + "\n")
            continue
        if in_includes:
            emit_additions()
        output.append(line if line.endswith("\n") else line + "\n")
    if in_includes:
        emit_additions()
    return "".join(output)


def main() -> int:
    if not PCH.is_file():
        print(f"error: {PCH} not found", file=sys.stderr)
        return 2
    counts = usage_counts()
    current = pch_headers(PCH.read_text(encoding="utf-8"))
    current_set = set(current)
    additions = [
        (count, header)
        for count, header in eligible(counts)
        if header not in current_set
    ]
    removals = sorted(
        (
            (counts[header], header)
            for header in current
            if counts[header] < REMOVE_THRESHOLD
        ),
        key=lambda item: (item[0], item[1]),
    )
    if not additions and not removals:
        print("src/pch.h is already up to date.")
        return 0
    updated = rewrite(
        PCH.read_text(encoding="utf-8"),
        {header for _, header in removals},
        [header for _, header in additions],
    )
    PCH.write_text(updated, encoding="utf-8", newline="\n")
    print("Updated src/pch.h")
    if additions:
        print("Added:")
        for _, header in additions:
            print(f"  <{header}>")
    if removals:
        print("Removed:")
        for _, header in removals:
            print(f"  <{header}>")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
