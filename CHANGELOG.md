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
