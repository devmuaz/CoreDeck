# Version Check

How CoreDeck decides that a newer release exists, and which tag shapes that decision understands.
The comparison lives in `CompareSemanticVersion` (`src/app/version_check.cpp`). Tests are in `tests/test_version_check.cpp`.

## What the running app does

On startup, when **Check for updates on launch** is enabled, and again from **Check for Updates...**, the app calls `QueryRemoteNewerVersion`.

**Include beta releases** in **Preferences → General** is off until the user turns it on. While it is off, the check requests GitHub's latest stable release:

`https://api.github.com/repos/devmuaz/CoreDeck/releases/latest`

GitHub returns the newest release that is not a draft and not a pre-release. The release workflow sets `prerelease: true` for any tag that contains `-`, so `v0.14.0-beta.1` stays out of that response. The prompt appears when a stable tag such as `v0.14.0` is published and becomes the latest release.

While **Include beta releases** is on, the check requests the published releases list instead:

`https://api.github.com/repos/devmuaz/CoreDeck/releases?per_page=100`

That list includes pre-releases. Drafts are skipped. `SelectNewestRelease` keeps the greatest tag under the comparison below, stable or beta. Someone on `v0.14.0` is offered `v0.15.0-beta.1` when that tag is the greatest published release. A stable tag of the same core still ranks above its betas, so `v0.14.0` is chosen over `v0.14.0-beta.2`. The first 100 releases in GitHub's list are considered. If that list cannot be read, the check uses the latest stable release.

The installed version is `COREDECK_VERSION`. CMake sets it from the release tag, strips one leading `v`, and keeps the pre-release suffix. A build tagged `v0.14.0-beta.1` embeds `0.14.0-beta.1`.

The dialog opens when the chosen tag compares greater than that embedded version. A manual check that finds nothing shows "You're running the latest CoreDeck release." The startup check stays quiet in that case. **Download** opens `https://coredeck.dev` for a stable release, and the GitHub page for that tag when the release is a beta.

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

The comparison above is what the function does with two tags. Which tag reaches the dialog depends on **Include beta releases**.

With the preference off, the dialog receives the tag from `/releases/latest`.

| Installed | Newest published tag | What GitHub returns | Prompt |
| --- | --- | --- | --- |
| `v0.13.0` | `v0.14.0-beta.1` | previous stable | no |
| `v0.13.0` | `v0.14.0` | `v0.14.0` | yes |
| `v0.14.0-beta.1` | `v0.14.0-beta.2` | previous stable | no |
| `v0.14.0-beta.1` | `v0.14.0` | `v0.14.0` | yes |
| `v0.14.0` | `v0.14.0-beta.1` | `v0.14.0` | no |
| `v0.14.1` | `v0.14.0` | `v0.14.0` | no |

With the preference on, the dialog receives the greatest published tag in the releases list. A beta is offered when it compares greater than the installed version and greater than every other published tag in that list.

| Installed | Greatest published tag | Prompt |
| --- | --- | --- |
| `v0.13.0` | `v0.14.0-beta.1` | yes |
| `v0.14.0-beta.1` | `v0.14.0-beta.2` | yes |
| `v0.14.0` | `v0.15.0-beta.1` | yes |
| `v0.14.0` | `v0.14.0`, while `v0.14.0-beta.2` is also published | no |
| `v0.14.0-beta.2` | `v0.14.0` | yes |
| `v0.15.0-beta.2` | `v0.15.0-beta.1` | no |
| `v0.14.9` | `v0.15.0-beta.1`, while stable `v0.14.5` is also published | yes |

The stable tag of a version still reaches people who leave the preference off, and people who turn it on.

## Tags that parse and still compare wrong

The parser does not reject unknown text. It reads digits until the first other character in a core component, and it splits the pre-release on `.` only. These are not release tags:

| Tag | Read as | Why it fails |
| --- | --- | --- |
| `v0.14.0b1` | `0.14.0` | no hyphen, so `b1` is dropped from the patch |
| `v0.14.0-beta2` | pre-release identifier `beta2` | the number is not its own identifier, so `beta10` sorts before `beta2` |
| `beta-0.14.0` | `0.0.0-0.14.0` | the core is the word `beta`, which becomes `0` |

Publish `vX.Y.Z` and `vX.Y.Z-beta.N` (or `alpha`, `rc`, and further dot-separated identifiers). Installed copies already understand those shapes, so a later tag in the same shape does not need a new parser.
