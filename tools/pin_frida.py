#!/usr/bin/env python3
"""Print the SHA-256 pins for a Frida release.

Usage: python3 tools/pin_frida.py 17.9.2
"""

import json
import sys
import urllib.request

ASSETS = (
    "frida-core-devkit-{version}-macos-arm64.tar.xz",
    "frida-core-devkit-{version}-macos-x86_64.tar.xz",
    "frida-core-devkit-{version}-linux-x86_64.tar.xz",
    "frida-core-devkit-{version}-linux-arm64.tar.xz",
    "frida-core-devkit-{version}-windows-x86_64.tar.xz",
    "frida-server-{version}-android-arm.xz",
    "frida-server-{version}-android-arm64.xz",
    "frida-server-{version}-android-x86.xz",
    "frida-server-{version}-android-x86_64.xz",
)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: pin_frida.py <version>", file=sys.stderr)
        return 2
    version = sys.argv[1]
    request = urllib.request.Request(
        f"https://api.github.com/repos/frida/frida/releases/tags/{version}",
        headers={"Accept": "application/vnd.github+json", "User-Agent": "CoreDeck"},
    )
    with urllib.request.urlopen(request) as response:
        release = json.load(response)
    digests = {asset["name"]: asset.get("digest", "") for asset in release.get("assets", [])}
    missing = False
    for pattern in ASSETS:
        name = pattern.format(version=version)
        digest = digests.get(name, "")
        if not digest.startswith("sha256:"):
            print(f"MISSING {name}", file=sys.stderr)
            missing = True
            continue
        print(f"{name} {digest.removeprefix('sha256:')}")
    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
