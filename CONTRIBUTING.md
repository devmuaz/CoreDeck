# Contributing to CoreDeck

Thanks for your interest in contributing! This guide covers the workflow, branching model, and conventions used in this
project.

## Branching Model

| Branch    | Purpose                                                                    |
| --------- | -------------------------------------------------------------------------- |
| `main`    | Stable, release-ready code. Protected — no direct pushes.                  |
| `develop` | Active development. Feature work merges here. Test tags are cut from here. |

```
feature/* ──> develop ──> tag vX.Y.Z-label ──> pre-release
                 │
                 └──> main (via PR) ──> tag vX.Y.Z ──> stable release
```

## How to Contribute

1. **Fork** the repository and clone it:

   ```bash
   git clone --recursive https://github.com/<your-username>/CoreDeck.git
   ```

2. **Create a branch** off `develop`:

   ```bash
   git checkout develop
   git checkout -b feature/your-feature-name
   ```

3. **Make your changes**, build, and test locally:

   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release --parallel
   ```

4. **Commit** with a clear message:

   ```
   Add AVD snapshot support
   ```

   Keep commits focused — one logical change per commit.

5. **Push** your branch and open a **Pull Request** targeting `develop` (not `main`).

## PR Guidelines

- A pull request into `develop` runs the build on Windows, macOS, and Linux. Merge the feature there. A pull request into `main` runs that same build and is how `develop` is promoted for a stable release, not how each feature lands
- Make sure the build passes on all platforms before requesting review
- Keep PRs focused — avoid mixing unrelated changes
- Update the [CHANGELOG.md](CHANGELOG.md) if your change is user-facing

### AI-assisted changes

An assistant may draft a change. You open the pull request from your own account, read it, trim it, and stand behind it. A model, bot, or agent account is not an author. Unreviewed generated output — AI slop — is not a contribution.

- You are the author of the pull request and of its commits
- Read every changed line and be ready to explain it. If you cannot, remove it
- Keep only the change you meant to make. Drop restated comments, unused helpers, speculative error handling, and edits the task did not need
- Do not reformat or refactor neighboring code
- Build and test the change yourself. A suggestion that claims to pass is not a passing build
- Write the commit message and the PR description from what the diff actually does, not from the model's summary

## Commit Message Style

Use short, imperative messages that describe what the commit does:

- `Add create AVD dialog with validation`
- `Fix GCC 13 build errors on Linux`
- `Refactor emulator options into per-AVD config`

Prefix with `fix:`, `refactor:`, or `chore:` when it helps clarify intent, but it's not required.

## Code Style

- C++20 standard
- No comments unless the "why" is non-obvious
- Keep changes minimal — don't refactor unrelated code in the same PR
- File names, layers, and entry points follow [the source layout](docs/building.md#source-layout). Run `tools/structure_format_tidy.sh` before opening a pull request. It runs the structure check, `clang-format`, and `clang-tidy` that CI runs. The Windows, macOS, and Linux builds start only after that job passes

## Bumping the bundled command-line tools

The in-app SDK installer downloads a specific Android command-line tools archive per platform. The URL, SHA-256, and size are pinned in `GetBundledCmdlineToolsRelease` (`src/core/sdk/sdk_bootstrap.cpp`) so a tampered or truncated download is rejected. The scheduled `SDK Bootstrap Check` workflow re-downloads the pinned archives monthly.

This is routine maintenance, not a hotfix: Google keeps old archives available for years, so a stale pin means users install slightly older tools rather than hitting a broken installer. A red monthly check is more often a transient CDN blip than a real removal — re-run it before assuming the pin is dead. See [docs/sdk-bootstrap.md](docs/sdk-bootstrap.md) for how the whole thing fits together.

1. Find the current release in Google's manifest and note the four archive names:

   ```bash
   curl -sSL https://dl.google.com/android/repository/repository2-3.xml | grep -A40 'cmdline-tools;latest'
   ```

2. Download all four archives and compute their SHA-256 (Google only publishes SHA-1, so we verify SHA-1 against the
   manifest and pin the stronger hash):

   ```bash
   for f in commandlinetools-linux-<build>_latest.zip \
            commandlinetools-mac_x86_64-<build>_latest.zip \
            commandlinetools-mac_arm64-<build>_latest.zip \
            commandlinetools-win-<build>_latest.zip; do
     curl -sS -O "https://dl.google.com/android/repository/$f"
   done
   shasum -a 1 *.zip    # must match the <checksum type="sha1"> values in the manifest
   shasum -a 256 *.zip  # these are what we pin
   ```

3. Update `CMDLINE_TOOLS_VERSION` plus the per-platform `PlatformArchiveSha256` and `PlatformArchiveSize` values in
   `src/core/sdk/sdk_bootstrap.cpp`.

4. Verify the new pins actually resolve:

   ```bash
   COREDECK_TEST_NETWORK=1 ctest --test-dir build -L network --output-on-failure
   ```

## Release Process

Maintainers publish two kinds of builds. Pushing a tag that starts with `v` starts the release workflow. The tag's commit is what gets built. `main` does not have to move for a test build.

A tag with a hyphen, such as `v0.17.0-phase1`, is a pre-release. A tag with no hyphen, such as `v0.17.0`, is a stable release. A stable tag also updates the Homebrew tap. A pre-release does not.

### Pre-release

A pre-release is an unfinished test build. It is not battle-tested, it may crash or misbehave, and it is not a build to rely on. Install a stable release for everyday use.

1. The approved work is on `develop`. Leave `main` where it is.
2. Tag that commit with a hyphen in the name and push the tag:

   ```bash
   git checkout develop
   git tag v0.17.0-phase1
   git push origin v0.17.0-phase1
   ```

3. The workflow publishes a GitHub pre-release with the signed installers. The Homebrew tap stays on the last stable version.

### Stable release

1. Open a pull request from `develop` into `main` and wait for the build.
2. Merge it, then push a tag with no hyphen to that `main` commit:

   ```bash
   git checkout main
   git tag v0.17.0
   git push origin v0.17.0
   ```

3. The workflow publishes the stable release and updates the Homebrew tap.

## Questions?

Open an [issue](https://github.com/devmuaz/CoreDeck/issues) or start
a [discussion](https://github.com/devmuaz/CoreDeck/discussions) if you have questions before contributing.
