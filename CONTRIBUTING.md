# Contributing to CoreDeck

Thanks for your interest in contributing! This guide covers the workflow, branching model, and conventions used in this
project.

## Branching Model

| Branch    | Purpose                                                   |
|-----------|-----------------------------------------------------------|
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

## Release Process

Releases are handled by maintainers:

1. `develop` is merged into `main` via PR
2. A version tag (`vX.Y.Z`) is pushed to `main`
3. The release workflow builds and publishes artifacts for all platforms

## Questions?

Open an [issue](https://github.com/devmuaz/CoreDeck/issues) or start
a [discussion](https://github.com/devmuaz/CoreDeck/discussions) if you have questions before contributing.
