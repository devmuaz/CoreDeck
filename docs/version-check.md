# Version Check

How CoreDeck decides that a newer release exists, and which tag shapes that decision understands.
The comparison lives in `CompareSemanticVersion` (`src/core/version_check.cpp`). Tests are in `tests/test_version_check.cpp`.

## What the running app does

On startup, and again from **Check for Updates...**, the app calls `QueryRemoteNewerVersion`. That requests GitHub's latest release:

`https://api.github.com/repos/devmuaz/CoreDeck/releases/latest`

GitHub returns the newest release that is not a draft and not a pre-release. The release workflow sets `prerelease: true` for any tag that contains `-`, so `v0.14.0-beta.1` is never that response. A user on `v0.13.0` is not offered `v0.14.0-beta.1`. The prompt appears when a stable tag such as `v0.14.0` is published and becomes the latest release.

The installed version is `COREDECK_VERSION`. CMake sets it from the release tag, strips one leading `v`, and keeps the pre-release suffix. A build tagged `v0.14.0-beta.1` embeds `0.14.0-beta.1`.

The dialog opens when the tag GitHub returned compares greater than that embedded version. A manual check that finds nothing shows "You're running the latest CoreDeck release." The startup check stays quiet in that case. **Download** opens `https://coredeck.dev`.

The notes in the dialog are the release body up to the last line that is exactly `---`. The workflow puts the changelog section above that line and the download troubleshooting below it.

## Tag shape

A supported tag is:

```
[v|V] MAJOR [ . MINOR [ . PATCH [ . MORE ... ]]] [ - IDENTIFIER [ . IDENTIFIER ... ]] [ + BUILD ]
```

`MAJOR`, `MINOR`, `PATCH`, and each `MORE` are numbers. An `IDENTIFIER` is the text between dots in the pre-release. `BUILD` is everything after the first `+`.

These are the shapes CoreDeck publishes, and the shapes the comparison orders correctly:

| Tag | Read as |
| --- | --- |
| `v0.14.0` | stable `0.14.0` |
| `0.14.0` | same as `v0.14.0` |
| `V0.14.0` | same as `v0.14.0` |
| `v0.14` | `0.14.0` (missing patch is `0`) |
| `v0.14.0-beta.1` | pre-release of `0.14.0`, identifiers `beta` and `1` |
| `v0.14.0-beta` | pre-release of `0.14.0`, identifier `beta` |
| `v0.14.0-rc.1` | pre-release of `0.14.0`, identifiers `rc` and `1` |
| `v0.14.0-alpha.1` | pre-release of `0.14.0`, identifiers `alpha` and `1` |
| `v0.14.0+macos` | `0.14.0`; the build suffix is ignored |
| `v0.14.0-beta.1+macos` | `0.14.0-beta.1`; the build suffix is ignored |

A number that should order numerically has to be its own identifier. `beta.10` is newer than `beta.2`. `beta10` is one word, so it is compared letter by letter and sorts before `beta2`.

## Comparison order

`CompareSemanticVersion(remote, installed)` returns `1` when `remote` is newer, `0` when they are the same release, and `-1` when `remote` is older. The update dialog requires `1`.

Core numbers are compared first, left to right. A missing component is `0`. `10` is greater than `9`. The first difference decides.

| Remote | Installed | Result |
| --- | --- | --- |
| `v0.14.0` | `v0.13.0` | newer (minor `14` > `13`) |
| `v0.14.1` | `v0.14.0` | newer (patch) |
| `v1.0.0` | `v0.14.9` | newer (major) |
| `v0.14.0` | `v0.14.0` | same |
| `v0.14` | `v0.14.0` | same |
| `v0.14.10` | `v0.14.9` | newer (`10` > `9`) |
| `v0.13.0` | `v0.14.0` | older |

When the core numbers match, a tag with no pre-release is newer than a tag with one. `v0.14.0` is newer than `v0.14.0-beta.1`. A higher core still wins: `v0.14.0-beta.1` is newer than `v0.13.0`, and `v0.14.1` is newer than `v0.14.0-beta.1`.

| Remote | Installed | Result |
| --- | --- | --- |
| `v0.14.0` | `v0.14.0-beta.1` | newer |
| `v0.14.0-beta.1` | `v0.14.0` | older |
| `v0.14.0-beta.1` | `v0.13.0` | newer |
| `v0.14.1` | `v0.14.0-beta.1` | newer |
| `v0.15.0-beta.1` | `v0.14.9` | newer |

Pre-release identifiers are then compared left to right:

- Two numbers compare by value. Leading zeros do not count, so `beta.01` is the same identifier as `beta.1`, and `beta.10` is newer than `beta.2`.
- A number is lower than a word. `1` is lower than `alpha`.
- Two words compare by ASCII order. `alpha` is lower than `beta`, and `beta` is lower than `rc1`.
- When one pre-release is the other plus more identifiers, the longer one is newer. `beta` is lower than `beta.1`.

| Remote | Installed | Result |
| --- | --- | --- |
| `v0.14.0-beta.2` | `v0.14.0-beta.1` | newer |
| `v0.14.0-beta.10` | `v0.14.0-beta.2` | newer |
| `v0.14.0-beta.1` | `v0.14.0-beta` | newer |
| `v0.14.0-rc.1` | `v0.14.0-beta.10` | newer |
| `v0.14.0-alpha.1` | `v0.14.0-beta.1` | older |
| `v0.14.0-beta.1` | `v0.14.0-beta.1` | same |
| `v0.14.0-beta.1+macos` | `v0.14.0-beta.1` | same |

Text after the first `+` is removed before any of this runs, so build metadata never changes the result.

## What the prompt can see

The comparison above is what the function does with two tags. The dialog only receives the tag from `/releases/latest`, which skips pre-releases.

| Installed | Newest published tag | What GitHub returns | Prompt |
| --- | --- | --- | --- |
| `v0.13.0` | `v0.14.0-beta.1` | `v0.13.0` | no |
| `v0.13.0` | `v0.14.0` | `v0.14.0` | yes |
| `v0.14.0-beta.1` | `v0.14.0-beta.2` | previous stable | no |
| `v0.14.0-beta.1` | `v0.14.0` | `v0.14.0` | yes |
| `v0.14.0` | `v0.14.0-beta.1` | `v0.14.0` | no |
| `v0.14.1` | `v0.14.0` | `v0.14.0` | no |

A later beta is visible to people who install it themselves. It is not offered through this check. The stable tag of that version is the release that reaches both stable users and beta users.

## Tags that parse and still compare wrong

The parser does not reject unknown text. It reads digits until the first other character in a core component, and it splits the pre-release on `.` only. These are not release tags:

| Tag | Read as | Why it fails |
| --- | --- | --- |
| `v0.14.0b1` | `0.14.0` | no hyphen, so `b1` is dropped from the patch |
| `v0.14.0-beta2` | pre-release identifier `beta2` | the number is not its own identifier, so `beta10` sorts before `beta2` |
| `beta-0.14.0` | `0.0.0-0.14.0` | the core is the word `beta`, which becomes `0` |

Publish `vX.Y.Z` and `vX.Y.Z-beta.N` (or `alpha`, `rc`, and further dot-separated identifiers). Installed copies already understand those shapes, so a later tag in the same shape does not need a new parser.
