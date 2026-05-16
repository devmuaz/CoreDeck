## [v0.7.0-beta.5](https://github.com/devmuaz/CoreDeck/releases/tag/v0.7.0-beta.5) — 2026-05-14

- Fix HiDPI text overflowing buttons and sub-windows by removing duplicate font scaling (thanks [@sjoerdev](https://github.com/sjoerdev), [#23](https://github.com/devmuaz/CoreDeck/issues/23))
- Switch hardcoded pixel widths and heights throughout the UI to text-derived units so dialogs, sub-panels, buttons, and table columns now scale with the font on high-DPI displays

## [v0.7.0-beta.3](https://github.com/devmuaz/CoreDeck/releases/tag/v0.7.0-beta.3) — 2026-05-14

- Add HiDPI monitor support so the UI scales correctly on high-DPI Windows displays at 100%, 125%, 150%, and 200% (thanks [@sjoerdev](https://github.com/sjoerdev), [#23](https://github.com/devmuaz/CoreDeck/issues/23))
- Add live resource usage section in the AVD Details window for running emulators, including CPU, memory, and disk I/O history
- Show full release notes inside the in-app update modal
- Fix emulator processes not being killed cleanly on app exit
- Fix Windows build failure caused by `psapi.h` being included without `windows.h` and an undefined `socket_t` alias in the emulator console
- Add project-wide `.clang-tidy` configuration and conform existing code to its rules
- Add initial VSCode and CMake integration configuration files

## [v0.6.0](https://github.com/devmuaz/CoreDeck/releases/tag/v0.6.0) — 2026-05-07

- Add skin management with a dedicated picker window for browsing, previewing, and assigning device skins from the SDK or custom paths
- Add log filtering to the AVD logs window with case-sensitive and regex search modes plus result navigation
- Expand preferences with theme controls and broader app settings
- Add unit tests covering the log filter and skin modules
- Add `.clang-tidy` with project-wide naming conventions and a curated check set
- Other minor bug fixes and improvements

## [v0.5.0](https://github.com/devmuaz/CoreDeck/releases/tag/v0.5.0) — 2026-05-04

- Ship macOS as a notarized `.dmg` with a drag-to-Applications window layout and a Retina-ready background image
- Add SHA256 checksums alongside every release artifact for download verification
- Sign nested Mach-O components (including `crashpad_handler`) explicitly during macOS packaging, replacing the deprecated `--deep` flag
- Surface Apple's full notarization log in CI when notarization fails
- Cache compilation across releases via `ccache` on Linux and macOS to shorten release times
- Wire release tags through to the embedded version string via a new `COREDECK_VERSION_OVERRIDE` CMake option

## [v0.4.0](https://github.com/devmuaz/CoreDeck/releases/tag/v0.4.0) — 2026-05-03

- Redesign AVD creation around dedicated device profile and system image picker dialogs with search, category filters, and table-based selection
- Redesign storage overview into a statistics-focused view with summary cards, a visual usage breakdown, and async size calculation
- Show more readable AVD and emulator details, including friendly GPU, screen, network, acceleration, and SELinux option labels
- Parse and display richer system image metadata for AVDs, including Google APIs, Google Play, 16 KB page size support, variants, and tags
- Improve system image management by moving image removal into the image picker and refreshing image state after install or uninstall
- Improve Windows emulator stop detection by tracking the emulator console port instead of relying only on the launcher process

## [v0.3.0](https://github.com/devmuaz/CoreDeck/releases/tag/v0.3.0) — 2026-04-29

- Add opt-in Sentry crash reporting via `sentry-native` with a `CrashReporter` facade so the SDK is fully isolated from the rest of the codebase
- Refactor `main.cpp` to delegate the full GLFW/ImGui lifecycle to `Application`, with RAII teardown and platform error handling
- Fix Windows title bar icon, Task Manager description, and em dash rendering in version info and the About dialog (thanks [@maramadany](https://github.com/maramadany))
- Fix Windows `.bat` handling, console flashes, and SDK license acceptance (thanks [@maramadany](https://github.com/maramadany))
- Fix emulator stop hang and orphaned `qemu` processes on shutdown (thanks [@maramadany](https://github.com/maramadany))
- Add `tools/audit_pch.sh` script to audit `pch.h` against actual header usage across the codebase

## [v0.2.0](https://github.com/devmuaz/CoreDeck/releases/tag/v0.2.0) — 2026-04-25

- Add device-type icons to the AVD list
- Auto-fill Name and Display Name in the Create AVD dialog from the selected device profile and system image
- Replace the runtime `curl` binary dependency with libcurl on macOS/Linux and WinHTTP on Windows (thanks [@maramadany](https://github.com/maramadany))
- Reduce idle CPU usage by replacing `glfwPollEvents` with focus-aware `glfwWaitEventsTimeout` (thanks [@maramadany](https://github.com/maramadany))
- Fix Create AVD button when no AVDs exist (thanks [@maramadany](https://github.com/maramadany))
- Fix blank AVD names by falling back to the internal AVD name when `avd.ini.displayname` is missing (thanks [@maramadany](https://github.com/maramadany))
- Fix Create AVD dialog layout and disabled-button states
- Fix font path resolution to be relative to the executable rather than the working directory (thanks [@maramadany](https://github.com/maramadany))
- Update README

## [v0.1.0](https://github.com/devmuaz/CoreDeck/releases/tag/v0.1.0) — 2026-04-22

- Fix crash when launching an AVD after a previous run of the same AVD

## [v0.1.0-beta.1](https://github.com/devmuaz/CoreDeck/releases/tag/v0.1.0-beta.1) — 2026-04-21

- Add Catch2 unit test suite with 35 tests covering utilities, paths, log buffer, and version check
- Add CI test step that runs the suite on Windows, macOS, Linux x86-64, and Linux ARM64 on every PR
- Fix log buffer FIFO eviction that was dropping newest entries instead of oldest
- Fix update check to correctly notify pre-release users when the matching stable release ships
- Auto-detect pre-release tags in the release workflow so betas are not marked as the latest stable

## [v0.0.8](https://github.com/devmuaz/CoreDeck/releases/tag/v0.0.8) — 2026-04-19

- Add system image install and uninstall via sdkmanager
- Add storage overview with per-AVD disk usage and async wipe user data
- Add auto and manual update check via GitHub API
- Add app settings persistence and preferences UI
- Add CONTRIBUTING.md with branching model and PR guidelines
- Refactor system image handling into its own core module
- Update README with features, preview screenshots, build dependencies, and contributing section

## [v0.0.7](https://github.com/devmuaz/CoreDeck/releases/tag/v0.0.7) — 2026-04-16

- Add Linux ARM64 build support
- Update artifact naming to platform-arch convention (`darwin-arm64`, `linux-x86-64`, `windows-x86-64`)

## [v0.0.6](https://github.com/devmuaz/CoreDeck/releases/tag/v0.0.6) — 2026-04-16

- Add Create AVD feature with system image, device profile, RAM, and GPU mode selection
- Add onboarding wizard with SDK setup, file picker, and validation
- Add menu bar, About dialog, and AVD delete with confirmation
- Refactor source into core/gui layers with Context-based window architecture
- Refactor create, delete, and about windows to modal window type with input validation
- Compile tinyfiledialogs as a separate static library to fix Windows build
- Fix GCC 13 build errors on Linux (missing `<algorithm>`, `constexpr std::string`)
- Enable cross-platform CI for build and release workflows
- Fix Windows title bar icon and install platform-specific icons per OS

## [v0.0.5](https://github.com/devmuaz/CoreDeck/releases/tag/v0.0.5) — 2026-04-12

- Fix Windows app icon in resources
- Fix Windows app details metadata

## [v0.0.4](https://github.com/devmuaz/CoreDeck/releases/tag/v0.0.4) — 2026-04-11

- Add error message box when GLFW or ImGui fail to initialize on Windows

## [v0.0.3](https://github.com/devmuaz/CoreDeck/releases/tag/v0.0.3) — 2026-04-11

- Fix Windows icon display
- Add installer shortcuts
- Improve CI workflows

## [v0.0.2](https://github.com/devmuaz/CoreDeck/releases/tag/v0.0.2) — 2026-04-11

- Add MIT license
- Fix MSI install directory and empty package issue

## [v0.0.1](https://github.com/devmuaz/CoreDeck/releases/tag/v0.0.1) — 2026-04-11

- Initial release
- AVD listing, launching, stopping, and wipe & run
- Per-AVD emulator options (GPU, RAM, CPU, boot, audio, network, camera)
- Live log viewer with search and auto-scroll
- Emulator manager with threading and proper cleanup
- Platform support for Windows, macOS (Apple Silicon), and Linux
- macOS app bundle with code signing and notarization
- Windows MSI installer and ZIP portable packaging
- GitHub Actions CI/CD for build and release
