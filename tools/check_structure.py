#!/usr/bin/env python3
"""Check the source tree against the layout rules in docs/building.md.

Exits with status 1 and one line per violation when a rule is broken.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

SOURCE_SUFFIXES = (".h", ".cpp", ".mm", ".cpp.in")
LIBRARIES = ("platform", "core", "app", "extensions", "gui")
ALLOWED_DEPENDENCIES = {
    "platform": {"platform"},
    "core": {"platform", "core"},
    "app": {"platform", "app"},
    "extensions": {"platform", "core", "app", "extensions"},
    "gui": set(LIBRARIES),
}
BANNED_NAMES = {"utilities", "helpers", "common", "misc"}
FEATURE_FOLDERS = {"sdk", "home", "adb", "apk", "preferences", "extensions"}
KIND_SUFFIXES = (
    "_workspace",
    "_panel",
    "_tab",
    "_dialog",
    "_section",
    "_banner",
    "_screen",
    "_bar",
    "_controller",
    "_widgets",
    "_state",
)
QUOTED_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
GUARD = re.compile(r"^#ifndef (\w+)\n#define \1\n", re.MULTILINE)


def stem_of(path: Path) -> str:
    for suffix in SOURCE_SUFFIXES:
        if path.name.endswith(suffix):
            return path.name[: -len(suffix)]
    return path.stem


def source_files() -> list[Path]:
    return sorted(p for p in SRC.rglob("*") if p.is_file() and p.name.endswith(SOURCE_SUFFIXES))


def library_of(relative: Path) -> str | None:
    return relative.parts[0] if len(relative.parts) > 1 and relative.parts[0] in LIBRARIES else None


def expected_guard(relative: Path) -> str:
    name = relative.with_name(stem_of(relative)).as_posix()
    return "COREDECK_" + re.sub(r"[^A-Za-z0-9]", "_", name).upper() + "_H"


def check() -> list[str]:
    problems: list[str] = []
    folders_by_stem: dict[str, set[Path]] = {}

    for file in source_files():
        relative = file.relative_to(SRC)
        stem = stem_of(relative)
        folders_by_stem.setdefault(stem, set()).add(relative.parent)
        text = file.read_text(encoding="utf-8")
        library = library_of(relative)

        if stem in BANNED_NAMES:
            problems.append(f"{relative}: '{stem}' says nothing about what the file holds; name it after its contents")

        parts = relative.parts
        if len(parts) == 3 and parts[0] == "gui" and parts[1] in FEATURE_FOLDERS and not stem.endswith(KIND_SUFFIXES):
            problems.append(f"{relative}: GUI feature files end with a kind suffix ({', '.join(KIND_SUFFIXES)})")

        for target in QUOTED_INCLUDE.findall(text):
            if target.startswith("."):
                problems.append(f"{relative}: include \"{target}\" is relative; root it at src/")
                continue
            included_library = target.split("/", 1)[0]
            if included_library not in LIBRARIES:
                if (file.parent / target).exists():
                    problems.append(f"{relative}: include \"{target}\" is relative; root it at src/")
                continue
            if not (SRC / target).exists():
                problems.append(f"{relative}: include \"{target}\" does not exist under src/")
            if library and included_library not in ALLOWED_DEPENDENCIES[library]:
                problems.append(f"{relative}: {library} must not include {included_library} (\"{target}\")")
            if stem.endswith("_controller") and target.startswith("gui/widgets/"):
                problems.append(f"{relative}: controllers hold logic only and must not include widgets")

        if stem.endswith("_controller") and re.search(r'#\s*include\s+"imgui', text):
            problems.append(f"{relative}: controllers hold logic only and must not include ImGui")

        if relative.suffix == ".h":
            found = GUARD.search(text)
            guard = expected_guard(relative)
            if not found:
                problems.append(f"{relative}: missing include guard {guard}")
            elif found.group(1) != guard:
                problems.append(f"{relative}: include guard {found.group(1)} should be {guard}")
            elif not text.rstrip().endswith(f"#endif // {guard}"):
                problems.append(f"{relative}: file should end with '#endif // {guard}'")

    for stem, folders in sorted(folders_by_stem.items()):
        if len(folders) > 1:
            listed = ", ".join(sorted(f"{folder.as_posix()}/{stem}" for folder in folders))
            problems.append(f"'{stem}' is used by more than one file: {listed}")

    return problems


def main() -> int:
    problems = check()
    for problem in problems:
        print(problem)
    if problems:
        print(f"\n{len(problems)} structure problem(s) found.")
        return 1
    print("Source structure is clean.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
