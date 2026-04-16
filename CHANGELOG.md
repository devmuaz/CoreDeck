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
