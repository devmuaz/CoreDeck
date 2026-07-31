# Contributing to CoreDeck

Thanks for your interest in contributing! This guide covers the workflow, branching model, and conventions used in this
project.

## Branching Model

| Branch    | Purpose                                                   |
| --------- | --------------------------------------------------------- |
| `main`    | Stable, release-ready code. Protected — no direct pushes. |
| `develop` | Active development. All feature work merges here first.   |

```
feature/* ──> develop ──> main (via PR) ──> tag vX.Y.Z ──> release
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

- PRs to `develop` trigger the CI build workflow across Windows, macOS, and Linux
- Make sure the build passes on all platforms before requesting review
- Keep PRs focused — avoid mixing unrelated changes
- Update the [CHANGELOG.md](CHANGELOG.md) if your change is user-facing

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

## Bumping the bundled command-line tools

The in-app SDK installer downloads a specific Android command-line tools archive per platform. The URL, SHA-256, and size are pinned in `GetBundledCmdlineToolsRelease` (`src/core/sdk_bootstrap.cpp`) so a tampered or truncated download is rejected. The scheduled `SDK Bootstrap Check` workflow re-downloads the pinned archives monthly.

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
   `src/core/sdk_bootstrap.cpp`.

4. Verify the new pins actually resolve:

   ```bash
   COREDECK_TEST_NETWORK=1 ctest --test-dir build -L network --output-on-failure
   ```

## Release Process

Releases are handled by maintainers:

1. `develop` is merged into `main` via PR
2. A version tag (`vX.Y.Z`) is pushed to `main`
3. The release workflow builds and publishes artifacts for all platforms

## Questions?

Open an [issue](https://github.com/devmuaz/CoreDeck/issues) or start
a [discussion](https://github.com/devmuaz/CoreDeck/discussions) if you have questions before contributing.
