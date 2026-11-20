#!/usr/bin/env python3
"""Pin Android command-line tools in pins/cmdline-tools.json.

Usage:
  python3 tools/update_cmdline_tools_pins.py
  python3 tools/update_cmdline_tools_pins.py 15859902

With no build number, the script pins cmdline-tools;latest from Google's repository.
"""

import hashlib
import json
import os
import ssl
import sys
import urllib.request
import xml.etree.ElementTree as ET
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PINS = ROOT / "pins" / "cmdline-tools.json"
REPOSITORY = "https://dl.google.com/android/repository/repository2-3.xml"
ARCHIVE_BASE = "https://dl.google.com/android/repository/"
HOSTS = (
    ("commandlinetools-win-", "windows"),
    ("commandlinetools-mac_arm64-", "mac-arm64"),
    ("commandlinetools-mac_x86_64-", "mac-x86_64"),
    ("commandlinetools-linux-", "linux"),
)


def open_url(request: urllib.request.Request):
    paths = ssl.get_default_verify_paths()
    if paths.cafile and os.path.isfile(paths.cafile):
        return urllib.request.urlopen(request)
    system_bundle = "/etc/ssl/cert.pem"
    if os.path.isfile(system_bundle):
        context = ssl.create_default_context(cafile=system_bundle)
        return urllib.request.urlopen(request, context=context)
    return urllib.request.urlopen(request)


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def fetch_text(url: str) -> str:
    request = urllib.request.Request(url, headers={"User-Agent": "CoreDeck"})
    with open_url(request) as response:
        return response.read().decode("utf-8")


def archive_records(package: ET.Element) -> list[dict[str, str | int]]:
    records = []
    for archive in package.iter():
        if local_name(archive.tag) != "archive":
            continue
        complete = next((child for child in list(archive) if local_name(child.tag) == "complete"), None)
        if complete is None:
            continue
        url = ""
        size = 0
        sha1 = ""
        for child in list(complete):
            name = local_name(child.tag)
            text = (child.text or "").strip()
            if name == "url":
                url = text
            elif name == "size" and text.isdigit():
                size = int(text)
            elif name == "checksum" and child.attrib.get("type") == "sha1":
                sha1 = text.lower()
        if url and size and sha1:
            records.append({"url": url, "size": size, "sha1": sha1})
    return records


def host_key(filename: str) -> str | None:
    for prefix, key in HOSTS:
        if filename.startswith(prefix) and filename.endswith("_latest.zip"):
            return key
    return None


def build_number(filename: str, key: str) -> str:
    prefix = next(item[0] for item in HOSTS if item[1] == key)
    return filename[len(prefix) : -len("_latest.zip")]


def select_archives(root: ET.Element, version: str) -> dict[str, dict[str, str | int]]:
    chosen: dict[str, dict[str, str | int]] = {}
    packages = [element for element in root.iter() if local_name(element.tag) == "remotePackage"]
    if not version:
        packages = [element for element in packages if element.attrib.get("path") == "cmdline-tools;latest"]
    for package in packages:
        for record in archive_records(package):
            filename = str(record["url"])
            key = host_key(filename)
            if key is None or key in chosen:
                continue
            if version and build_number(filename, key) != version:
                continue
            chosen[key] = record
    return chosen


def hash_archive(filename: str) -> tuple[str, str, int]:
    request = urllib.request.Request(ARCHIVE_BASE + filename, headers={"User-Agent": "CoreDeck"})
    sha1 = hashlib.sha1()
    sha256 = hashlib.sha256()
    size = 0
    print(f"Downloading {filename}", flush=True)
    with open_url(request) as response:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            sha1.update(chunk)
            sha256.update(chunk)
            size += len(chunk)
    return sha1.hexdigest(), sha256.hexdigest(), size


def main() -> int:
    if len(sys.argv) > 2:
        print("usage: update_cmdline_tools_pins.py [build]", file=sys.stderr)
        return 2
    version = sys.argv[1] if len(sys.argv) == 2 else ""
    root = ET.fromstring(fetch_text(REPOSITORY))
    selected = select_archives(root, version)
    missing = [key for _, key in HOSTS if key not in selected]
    if missing:
        label = version or "cmdline-tools;latest"
        print(f"MISSING {label}: {', '.join(missing)}", file=sys.stderr)
        return 1

    archives: dict[str, dict[str, str | int]] = {}
    pinned_version = ""
    for _, key in HOSTS:
        record = selected[key]
        filename = str(record["url"])
        pinned_version = build_number(filename, key)
        actual_sha1, sha256, size = hash_archive(filename)
        if actual_sha1 != record["sha1"]:
            print(f"SHA-1 mismatch for {filename}", file=sys.stderr)
            print(f"  manifest {record['sha1']}", file=sys.stderr)
            print(f"  download {actual_sha1}", file=sys.stderr)
            return 1
        if size != record["size"]:
            print(f"Size mismatch for {filename}: manifest {record['size']}, download {size}", file=sys.stderr)
            return 1
        url = ARCHIVE_BASE + filename
        archives[key] = {"url": url, "sha256": sha256, "size": size}
        print(f"{url} {sha256} {size}")

    stamp = datetime.now().astimezone().strftime("%Y-%m-%d %H:%M %z")
    text = json.dumps(
        {
            "_comment": f"Generated by tools/update_cmdline_tools_pins.py at {stamp}. Do not edit by hand.",
            "version": pinned_version,
            "archives": archives,
        },
        indent=2,
    ) + "\n"
    temporary = PINS.with_suffix(".json.tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(PINS)
    print(f"Updated {PINS.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
