<div align="center">
  <img src="media/branding/header-logo.png" alt="CoreDeck" width="220" />
  <br /><br />
  <a href="https://www.producthunt.com/products/coredeck?embed=true&amp;utm_source=badge-featured&amp;utm_medium=badge&amp;utm_campaign=badge-coredeck" target="_blank" rel="noopener noreferrer"><img alt="CoreDeck - Manage Android emulators without opening Android Studio | Product Hunt" width="250" height="54" src="https://api.producthunt.com/widgets/embed-image/v1/featured.svg?post_id=1138545&amp;theme=light&amp;t=1777884973182" /></a>
</div>

<br />

[![Build](https://github.com/devmuaz/CoreDeck/actions/workflows/build.yml/badge.svg)](https://github.com/devmuaz/CoreDeck/actions/workflows/build.yml)
[![Release](https://github.com/devmuaz/CoreDeck/actions/workflows/release.yml/badge.svg)](https://github.com/devmuaz/CoreDeck/actions/workflows/release.yml)
[![Latest release](https://img.shields.io/github/v/release/devmuaz/CoreDeck?label=release)](https://github.com/devmuaz/CoreDeck/releases)
[![Latest pre-release](https://img.shields.io/github/v/release/devmuaz/CoreDeck?include_prereleases&label=pre-release)](https://github.com/devmuaz/CoreDeck/releases)
[![Downloads](https://img.shields.io/github/downloads/devmuaz/CoreDeck/total)](https://github.com/devmuaz/CoreDeck/releases)
[![Stars](https://img.shields.io/github/stars/devmuaz/CoreDeck)](https://github.com/devmuaz/CoreDeck/stargazers)
[![Issues](https://img.shields.io/github/issues/devmuaz/CoreDeck)](https://github.com/devmuaz/CoreDeck/issues)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
[![License](https://img.shields.io/github/license/devmuaz/CoreDeck)](LICENSE)

[CoreDeck](https://coredeck.dev) is an open source native desktop application around your Android SDK’s official
emulator, avdmanager, and sdkmanager binaries — running them for you in one place, through a friendly GUI, so you get
the same results without hand-writing commands. Use it for everyday work without opening Android Studio. Built with
C++20 and Dear ImGui.

> [!IMPORTANT]
> You still need the Android SDK and its tooling on your machine. Installing Android Studio is the usual way to get
> them.

<div align="center">
  <video src="https://github.com/user-attachments/assets/cade70a8-c7b4-47ac-98c1-6b8986893dcc" controls width="720"></video>
</div>

## Features

- **AVD Management** — Create, delete, and browse your Android Virtual Devices
- **System Image Management** — List, install, and uninstall Android system images with ease
- **Emulator Control** — Launch, stop, or wipe & run AVDs with one click
- **Per-AVD Options** — Configure GPU, RAM, CPU cores, camera, network, boot mode, and more
- **Live Log Viewer** — Stream emulator output in real time with search and auto-scroll
- **Storage Overview** — Inspect per-AVD disk usage and clear heavy or unused data
- **SDK Auto-Detection** — Picks up your Android SDK from environment variables or standard paths
- **Guided Setup** — Onboarding wizard to configure the SDK on first run
- **Cross-Platform** — Runs natively on Windows, macOS, and Linux

## Preview

|                                AVD List & Options                                 |                                    Running Emulator & Logs                                    |
| :-------------------------------------------------------------------------------: | :-------------------------------------------------------------------------------------------: |
| <img src="media/screenshots/list-avds.png" alt="AVD List & Options" width="400"/> | <img src="media/screenshots/run-selected-avd.png" alt="Running Emulator & Logs" width="400"/> |
|                 _Browse AVDs with per-device options and details_                 |                      _Live emulator output with search and auto-scroll_                       |

|                                 Create New AVD                                 |                                  Device Profile Selection                                   |
| :----------------------------------------------------------------------------: | :-----------------------------------------------------------------------------------------: |
| <img src="media/screenshots/create-avd.png" alt="Create New AVD" width="400"/> | <img src="media/screenshots/select-device.png" alt="Device Profile Selection" width="400"/> |
|              _Configure system image, device, RAM, and GPU mode_               |                    _Pick from a rich catalog of Android device profiles_                    |

|                                         System Image Browser                                          |                                    Storage Overview                                    |
| :---------------------------------------------------------------------------------------------------: | :------------------------------------------------------------------------------------: |
| <img src="media/screenshots/install-select-system-image.png" alt="System Image Browser" width="400"/> | <img src="media/screenshots/storage-overview.png" alt="Storage Overview" width="400"/> |
|                           _List, install, and remove Android system images_                           |                     _Inspect AVD disk usage and clear heavy data_                      |

## Downloads

Grab the latest prebuilt binaries from the official [CoreDeck](https://coredeck.dev) website or the [Releases](https://github.com/devmuaz/CoreDeck/releases) page:

| Platform | Architecture          | File            |
| -------- | --------------------- | --------------- |
| Windows  | x86-64                | `.msi` / `.zip` |
| macOS    | arm64 (Apple Silicon) | `.dmg`          |
| Linux    | x86-64, arm64         | `.tar.gz`       |

Each release artifact ships with a matching `.sha256` checksum for download verification.

## Requirements

- **Android SDK** with `emulator`, `avdmanager`, and `sdkmanager` available (typically installed via Android Studio).
- **OS:** Windows 10/11, macOS 12+ (Apple Silicon), or a recent Linux distribution.

## Build from source

CoreDeck builds with **CMake + Ninja** on all platforms. Output is single-config: `build-debug/` and `build-release/` contain the binary directly, with a `compile_commands.json` for editor tooling.

### Prerequisites

All platforms need **CMake 3.23+**, **Ninja**, and **Git** (with submodule support). Per-platform compiler:

| Platform | Compiler              | Install                                                                                                                                                                     |
| -------- | --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| macOS    | Apple Clang           | `xcode-select --install`, then `brew install cmake ninja`                                                                                                                   |
| Windows  | MSVC 19.30+ (VS 2022) | [Build Tools for VS 2022](https://visualstudio.microsoft.com/downloads/) → _Desktop development with C++_ workload. Then `winget install Kitware.CMake Ninja-build.Ninja`   |
| Linux    | GCC 11+ / Clang 14+   | `sudo apt-get install build-essential cmake ninja-build libcurl4-openssl-dev libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev` |

### Clone and build

```bash
git clone --recursive https://github.com/devmuaz/CoreDeck.git
cd CoreDeck
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

> **Windows note:** Ninja + MSVC needs `cl.exe` on PATH. Run the commands from the **Developer Command Prompt for VS 2022** (Start Menu), or launch VS Code / Cursor from it so the bundled tasks pick up the environment. The provided VS Code tasks handle this automatically via `vswhere` + `Enter-VsDevShell`.

### Run

```bash
# macOS
open build-release/CoreDeck.app

# Linux
./build-release/CoreDeck

# Windows
.\build-release\CoreDeck.exe
```

### Develop in VS Code or Cursor

The repo ships pre-configured `.vscode/` settings (build tasks, launch configs, IntelliSense). Open the project, accept the recommended extensions when prompted, then press **F5** to build and debug.

**Per-editor extensions** — the two editors use different language servers and debuggers:

| Editor                      | Install                                                        | Purpose                                                                           |
| --------------------------- | -------------------------------------------------------------- | --------------------------------------------------------------------------------- |
| **VS Code**                 | `ms-vscode.cpptools`, `ms-vscode.cmake-tools`                  | IntelliSense + CMake integration + `cppvsdbg` (Microsoft MSVC debugger)           |
| **VS Code** _(optional)_    | `vadimcn.vscode-lldb`                                          | Adds LLDB debugger as an alternative to MSVC                                      |
| **Cursor**                  | `llvm-vs-code-extensions.vscode-clangd`, `vadimcn.vscode-lldb` | clangd IntelliSense + LLDB debugger (Microsoft's `cppvsdbg` is locked to VS Code) |
| **Cursor** _(Windows only)_ | LLVM toolchain — `winget install LLVM.LLVM`                    | Provides the `clangd.exe` binary for the extension                                |

**Launch configurations** in the Run & Debug panel:

| Name                                | Editors           | Debugger        |
| ----------------------------------- | ----------------- | --------------- |
| `CoreDeck (Debug) [macOS]`          | VS Code or Cursor | `cppdbg` + LLDB |
| `CoreDeck (Debug) [Windows · MSVC]` | VS Code only      | `cppvsdbg`      |
| `CoreDeck (Debug) [Windows · LLDB]` | VS Code or Cursor | CodeLLDB        |

Each has a matching Release entry and `CoreDeck Tests (...)` variant for the Catch2 test suite. Cursor on Windows should pick the `LLDB` entries; VS Code on Windows can pick either (MSVC is the better PDB-aware option when available).

The first build is a full from-scratch compile of all bundled dependencies (sentry-native, Dear ImGui, GLFW, reflect-cpp, etc.). Subsequent builds are incremental and fast.

## FAQ

**The app starts but says my Android SDK isn't detected.**
CoreDeck looks at `ANDROID_HOME` / `ANDROID_SDK_ROOT` and standard install paths. If your SDK lives elsewhere, point it
at the right location through the onboarding wizard or set the environment variable before launching.

**An emulator won't launch / boots forever.**
Make sure the matching system image is installed and that hardware acceleration is enabled (HAXM/Hyper-V on Windows,
Hypervisor.framework on macOS, KVM on Linux). The live log viewer usually shows the underlying error from `emulator`.

**Does CoreDeck replace Android Studio?**
No — it wraps the same official command-line tools that Android Studio uses, so you still need the Android SDK
installed. CoreDeck just gives you a focused GUI for AVD and emulator workflows.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the branching model, PR guidelines, and how to get started.

## Acknowledgements

CoreDeck is built on top of these excellent open source projects:

- [Dear ImGui](https://github.com/ocornut/imgui) — immediate-mode GUI
- [GLFW](https://github.com/glfw/glfw) — windowing and input
- [reflect-cpp](https://github.com/getml/reflect-cpp) — reflection and serialization
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) — native file dialogs
- [Catch2](https://github.com/catchorg/Catch2) — testing framework
- [sentry-native](https://github.com/getsentry/sentry-native) — crash reporting

## License

See [LICENSE](LICENSE) for details.
