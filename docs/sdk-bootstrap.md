# Android SDK Bootstrap

How CoreDeck installs the Android SDK itself, and the machinery that keeps it working.
Feature background: [issue #28](https://github.com/devmuaz/CoreDeck/issues/28).

## The pipeline

`BootstrapAndroidSdk` (`src/core/sdk_bootstrap.cpp`) downloads Google's official
command-line tools, verifies the SHA-256, extracts with miniz, accepts the SDK
licenses, then installs `platform-tools` and `emulator` via `sdkmanager`.

Work happens in `<installRoot>/.coredeck-bootstrap/`, and only moves to
`<installRoot>/cmdline-tools/latest/` after a verified extract. Same filesystem, so the
final step is an atomic rename. Staging is wiped on success, failure, and cancel, so a
retry always starts clean.

No global state is touched: no `PATH`, no `ANDROID_HOME`, no `JAVA_HOME`. The location is
remembered via `Paths::Onboarding::SaveSdkPathOverride`.

## The pin, and why it goes stale rather than breaking

`GetBundledCmdlineToolsRelease()` returns a hardcoded URL, SHA-256, and size per platform
(currently build `15859902`). It's pinned so a tampered or truncated download is rejected —
you can't verify a download whose hash you learned from the same server.

The cost: bumping it needs a code change and a release.

That cost is low, for two reasons:

1. **Google keeps old archives.** Verified back to build `6858069` (2020) — still HTTP 200
   today. A stale pin means users install slightly older tools, not a broken installer.
2. **The pin only seeds the first install.** `cmdline-tools;latest` is itself an SDK
   package, so once bootstrapped a user's SDK updates its own tools through `sdkmanager`
   with no involvement from CoreDeck.

## Two test files, two questions

| File                                   | Cases | Asks                    | Runs             |
| -------------------------------------- | ----- | ----------------------- | ---------------- |
| `tests/test_sdk_bootstrap.cpp`         | 17    | Is our logic correct?   | Every PR         |
| `tests/test_sdk_bootstrap_network.cpp` | 1     | Is our pin still valid? | Monthly / manual |

The offline tests get their power from lying to the code through the `BootstrapDeps` seam.
They can force conditions Google would never produce: a corrupt archive, a refused license,
a package install that silently omits the emulator. They're deterministic and gate merges.
They will also keep passing forever if the URL dies, because they never touch the network.

The network test is the opposite: it really downloads, checks size and SHA-256, extracts,
and asserts `bin/sdkmanager` exists and is executable. It stops short of installing
packages — that would test Google's package repo, not our pin.

They're separate files because a red build means different things. Offline red means you
broke your code and fix it here. Network red means the outside world moved and you re-pin.
Mixing a hermetic suite with a flaky-by-nature one is how you learn to ignore red builds.

Opt-in mechanics: the leading dot in the `[.network]` tag hides it from the default Catch2
run; it calls `SKIP()` unless `COREDECK_TEST_NETWORK=1`; and `tests/CMakeLists.txt`
registers it as its own CTest entry with `LABELS network` and `SKIP_RETURN_CODE 4` (Catch2
exits 4 when every selected test skipped itself).

## The monthly check

`.github/workflows/sdk-bootstrap-check.yml` runs on GitHub's runners. It exists purely to tell the maintainer when the pin stops resolving.

- **Triggers:** `cron: '0 6 1 * *'` (06:00 UTC, 1st of the month) and a manual
  **Run workflow** button via `workflow_dispatch`.
- **Matrix:** four runners with `fail-fast: false`, because each platform has its own
  archive and hash — one passing tells you nothing about the others.
- **Gotchas:** scheduled workflows only run from the **default branch**, so this does
  nothing until merged to `main`. GitHub also disables cron schedules after 60 days of
  repository inactivity.

Linux arm64 note: Google publishes no aarch64 Linux archive, so that runner verifies the
same x86-64 archive. The tools are Java and work fine, but the `emulator` package is
x86-64-only on Linux, so a real bootstrap there fails at the verify step with
`EmulatorMissingAfterInstall`.

## Running it yourself

```bash
# offline suite (what CI runs on every PR)
ctest --test-dir build --output-on-failure

# real download against Google
COREDECK_TEST_NETWORK=1 ctest --test-dir build -L network --output-on-failure
```

## When the monthly check goes red

Most often a transient CDN or DNS blip — re-run it first. If the archive is genuinely gone
or its hash changed, follow
[Bumping the bundled command-line tools](../CONTRIBUTING.md#bumping-the-bundled-command-line-tools).
