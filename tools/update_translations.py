#!/usr/bin/env python3
"""Extract user-visible strings into locales/coredeck.pot and merge that template
into every locales/*.po catalog. Translations are not filled in here.

Usage:
  python3 tools/update_translations.py
"""

import os
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCALES = ROOT / "locales"
POT = LOCALES / "coredeck.pot"
PACKAGE_NAME = "CoreDeck"
COPYRIGHT_HOLDER = "CoreDeck contributors"
BUGS_ADDRESS = "https://github.com/devmuaz/CoreDeck/issues/new?template=translation.md"
POT_AUTHOR = "AbdulMuaz Aqeel <info@devmuaz.com>"
KEYWORDS = (
    "--keyword=Tr:1",
    "--keyword=TrNoop:1",
    "--keyword=TrC:1c,2",
    "--keyword=TrN:1,2",
    "--keyword=TrFormat:1",
    "--keyword=TrFormatN:1,2",
    "--keyword=TrWindow:1",
)


def tool_candidates(name: str) -> list[Path]:
    names = [name, f"{name}.exe"] if os.name == "nt" else [name]
    directories: list[Path] = []
    env_dir = os.environ.get("COREDECK_GETTEXT_TOOLS")
    if env_dir:
        directories.append(Path(env_dir))
    directories.extend(
        [
            Path("/opt/homebrew/bin"),
            Path("/opt/homebrew/opt/gettext/bin"),
            Path("/usr/local/bin"),
            Path("/usr/local/opt/gettext/bin"),
        ]
    )
    vcpkg = os.environ.get("VCPKG_INSTALLATION_ROOT")
    if vcpkg:
        installed = Path(vcpkg) / "installed"
        for triplet in ("x64-windows", "arm64-windows", "x86-windows"):
            directories.append(installed / triplet / "tools" / "gettext" / "bin")
    return [directory / tool_name for directory in directories for tool_name in names]


def find_tool(name: str) -> Path:
    found = shutil.which(name)
    if found:
        return Path(found)
    for candidate in tool_candidates(name):
        if candidate.is_file():
            return candidate
    print(
        f"error: {name} not found. Install gettext "
        "(brew install gettext, apt install gettext, or the gettext tools on Windows).",
        file=sys.stderr,
    )
    raise SystemExit(1)


def fix_template(text: str, year: str) -> str:
    text = re.sub(
        r"^# Copyright \(C\) YEAR ",
        f"# Copyright (C) {year} ",
        text,
        count=1,
        flags=re.MULTILINE,
    )
    text = re.sub(
        r"^# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR\.",
        f"# {POT_AUTHOR}, {year}.",
        text,
        count=1,
        flags=re.MULTILINE,
    )
    text = text.replace(
        "Last-Translator: FULL NAME <EMAIL@ADDRESS>",
        f"Last-Translator: {POT_AUTHOR}",
        1,
    )
    return re.sub(
        r"(Content-Type: text/plain; charset=)CHARSET",
        r"\1UTF-8",
        text,
        count=1,
    )


def main() -> int:
    xgettext = find_tool("xgettext")
    msgmerge = find_tool("msgmerge")
    sources = sorted(
        path for path in (ROOT / "src").rglob("*") if path.suffix in {".cpp", ".h"}
    )
    source_list = POT.with_suffix(".sources")
    source_list.write_text("".join(f"{path}\n" for path in sources), encoding="utf-8")
    try:
        subprocess.run(
            [
                os.fspath(xgettext),
                "--from-code=UTF-8",
                "--language=C++",
                *KEYWORDS,
                f"--package-name={PACKAGE_NAME}",
                f"--msgid-bugs-address={BUGS_ADDRESS}",
                f"--copyright-holder={COPYRIGHT_HOLDER}",
                "-o",
                os.fspath(POT),
                "-f",
                os.fspath(source_list),
            ],
            check=True,
        )
    finally:
        source_list.unlink(missing_ok=True)
    year = str(datetime.now().year)
    POT.write_text(
        fix_template(POT.read_text(encoding="utf-8"), year),
        encoding="utf-8",
        newline="\n",
    )
    catalogs = sorted(LOCALES.glob("*.po"))
    if not catalogs:
        print(f"updated {POT}; no .po catalogs to merge")
        return 0
    for catalog in catalogs:
        subprocess.run(
            [
                os.fspath(msgmerge),
                "--update",
                "--backup=none",
                os.fspath(catalog),
                os.fspath(POT),
            ],
            check=True,
        )
        print(f"merged {catalog.name}")
    print(f"updated {POT}")
    print(
        "Fill in empty msgstr entries, then rebuild so msgfmt compiles the .mo files."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
