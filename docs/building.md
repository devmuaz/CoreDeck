# Building

CoreDeck builds with CMake and Ninja. A launch name names four separate choices: the platform, the configuration, the compiler, and the debugger. Ninja is the build system for every launch. The debugger attaches to the binary Ninja produced. It does not choose the compiler, and the compiler does not choose the debugger.

```
[platform · Debug|Release] CoreDeck [compiler · Ninja · debugger]
```

The Catch2 suite uses `CoreDeck Tests` in that same pattern. The Run and Debug menu groups entries under macOS, Windows, and Linux. Debug and Release stay in the name itself.

## Prerequisites

Every platform needs **CMake 3.23+**, **Ninja**, and **Git** with submodule support.

| Platform | Compiler              | Install                                                                                                                                                                           |
| -------- | --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| macOS    | Apple Clang           | `xcode-select --install`, then `brew install cmake ninja`                                                                                                                         |
| Windows  | MSVC 19.30+ (VS 2022) | [Build Tools for VS 2022](https://visualstudio.microsoft.com/downloads/), then the _Desktop development with C++_ workload. Then `winget install Kitware.CMake Ninja-build.Ninja` |
| Linux    | GCC 11+ / Clang 14+   | `sudo apt-get install build-essential cmake ninja-build pkg-config libgtk-3-dev libcurl4-openssl-dev libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev` |

On macOS, two extra compilers are optional. They exist so this machine can run the same app through more than Apple Clang:

```bash
brew install gcc llvm
```

`brew install gcc` provides `g++-16`. `brew install llvm` provides Homebrew Clang. Apple Clang, from Xcode, remains the default.

## Clone and build

```bash
git clone --recursive https://github.com/devmuaz/CoreDeck.git
cd CoreDeck
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
```

If the clone omitted `--recursive`:

```bash
git submodule update --init --recursive
```

On Windows, Ninja still invokes MSVC, so `cl.exe` has to be on `PATH`. Run the commands from the **Developer Command Prompt for VS 2022**, or start VS Code or Cursor from that prompt. The bundled tasks do this themselves with `vswhere` and `Enter-VsDevShell`.

Run the binary from the directory you just built:

```bash
# macOS
open build-release/CoreDeck.app

# Linux
./build-release/CoreDeck

# Windows
.\build-release\CoreDeck.exe
```

The first build compiles the bundled dependencies, including sentry-native, Dear ImGui, GLFW, and reflect-cpp. Later builds are incremental.

## Source layout

`src/` is four static libraries. A library includes only itself and the libraries it is allowed to use. `tools/check_structure.py` enforces the table, the file names, and the include rules. Run it locally before a pull request.

| Library | Folder | May include | Holds |
| --- | --- | --- | --- |
| `coredeck_platform` | `src/platform` | platform | Processes, HTTP, archives, hashing, logging, translation, paths, and generic helpers split by what they hold: `strings`, `files`, `secrets`, `desktop` |
| `coredeck_core` | `src/core` | platform, core | Android logic, grouped by domain: `sdk/`, `avd/`, `adb/`, `apk/` |
| `coredeck_app` | `src/app` | platform, app | Configuration, app paths, crash reporting, update checks, and file dialogs |
| `coredeck_gui` | `src/gui` | all of the above | Everything that draws. Dear ImGui is its own `imgui` target |

Core and App do not include each other. The executable is `src/main.cpp` and links only `coredeck_gui`. A call names the layer: `Platform::JoinPaths`, `Core::SignApk`, `App::LoadAppConfig`.

Folders name the area a person sees: `gui/shell/`, `gui/home/`, `gui/adb/`, `gui/frida/`, `gui/apk/`, `gui/sdk/`, `gui/preferences/`, and `gui/widgets/` for controls with no feature knowledge. No two files under `src/` share a name. These file names are banned because they say nothing: `utilities`, `helpers`, `common`, `misc`.

A GUI feature file ends with the kind of thing it is:

| Suffix | What it is | Entry point |
| --- | --- | --- |
| `_workspace` | A full-window page, one per `WorkspacePage` value | `Build…Workspace`, `Close…Workspace` |
| `_panel` | A dockable window inside a workspace | `Build…Panel` |
| `_tab` | One tab of a workspace | `Build…Tab` |
| `_dialog` | A modal, including a confirmation | `Open…Dialog`, `Build…Dialog` |
| `_section` | Part of a dialog or panel, drawn by its parent | `Build…Section` |
| `_banner` | A strip above a workspace | `Build…Banner` |
| `_screen` | Replaces the whole window, one per `Screen` value | `Build…Screen` |
| `_bar` | The menu bar | `Build…Bar` |
| `_controller` | A feature's tasks, polling, and selection. No ImGui include | Free functions |
| `_widgets` | Drawing helpers used only inside one feature | Free functions |
| `_state` | A feature's UI state, held by `Context` | `struct …State` |

Includes are rooted at `src/` (`#include "gui/widgets/widgets.h"`), never `../`. A header guard follows the path, such as `COREDECK_GUI_ADB_ADB_SHELL_TAB_H`, and the file ends with `#endif // COREDECK_GUI_ADB_ADB_SHELL_TAB_H`.

Namespaces follow the libraries: `CoreDeck::Platform`, `Core`, `App`, and `Gui`. A feature folder may use `Gui::<Folder>` (such as `Gui::Adb`) for helpers shared by its own files. Entry points stay in `Gui`. Helpers used by one file stay in an anonymous namespace.

`Context` is grouped by kind. `Host` holds services, `Prefs` holds preferences, and `Shell` holds the screen, the active workspace, the native window, and panel visibility. `Workspaces` holds each feature's state (`Home`, `Adb`, `ApkAnalyzer`, `ApkSigner`, `JarSigner`, `Keytool`). `Dialogs` holds each dialog, and a dialog opens only through its `Open…Dialog` function: `context.Dialogs.About.Open` is set by `OpenAboutDialog`. Window titles, `###` widget ids, and `config.json` keys stay stable when a file moves.

## Why Ninja

Ninja is the build system on macOS, Windows, and Linux. Both local scripts pass `-G Ninja`:

- macOS and Linux use `.vscode/scripts/cmake-build.sh`.
- Windows uses `.vscode/scripts/cmake-build.ps1`. That script opens the Visual Studio developer shell, then configures Ninja.

Ninja runs the platform compiler. On Windows that compiler is MSVC, and the binary still carries the PDB the Visual Studio debugger reads. On Linux the compiler is GCC, and both GDB and LLDB attach to that binary. The launches use one directory per configuration, `build-debug` and `build-release`, which matches a single-config Ninja tree. Each tree also contains `compile_commands.json` for the editor.

GitHub Actions configures a directory named `build` and does not pass a generator. A local launch and a CI job are separate trees.

## Build directories

A CMake directory remembers one compiler. Windows and Linux each have one compiler in the Run menu, so one directory per configuration is enough. The two debuggers on those platforms share that directory.

macOS can build with Apple Clang, Homebrew GCC 16, and Homebrew Clang. Those three cannot share `build-debug`, because configuring a second compiler would replace the cache of the first. Each compiler has its own tree. `build-debug` and `build-release` stay the default names because clangd, the C/C++ extension, and the open-app and test scripts read that tree on every platform.

| Platform | Compiler        | Debug directory              | Release directory              |
| -------- | --------------- | ---------------------------- | ------------------------------ |
| macOS    | Apple Clang     | `build-debug`                | `build-release`                |
| macOS    | Homebrew GCC 16 | `build-homebrew-gcc16-debug` | `build-homebrew-gcc16-release` |
| macOS    | Homebrew Clang  | `build-homebrew-clang-debug` | `build-homebrew-clang-release` |
| Windows  | MSVC            | `build-debug`                | `build-release`                |
| Linux    | GCC             | `build-debug`                | `build-release`                |

The app and the tests live in the same directory. On macOS the app is `CoreDeck.app/Contents/MacOS/CoreDeck` and the tests are `tests/coredeck_tests`. On Linux those paths are `CoreDeck` and `tests/coredeck_tests`. On Windows they are `CoreDeck.exe` and `tests/coredeck_tests.exe`.

The editor hides these directories and does not watch them. They are generated output, and a build creates thousands of files. clangd still reads `build-debug/compile_commands.json`, so hiding the folder leaves IntelliSense on the default compiler.

## Compilers on macOS

The command name and the compiler are different things on a Mac with both Xcode and Homebrew installed:

| Command                                                  | What it actually is    |
| -------------------------------------------------------- | ---------------------- |
| `/usr/bin/cc`, `/usr/bin/c++`, `gcc`, `g++`              | Apple Clang from Xcode |
| `/usr/bin/clang`, `/usr/bin/clang++`                     | Apple Clang from Xcode |
| `clang`, `clang++` when Homebrew LLVM is first on `PATH` | Homebrew Clang         |
| `/opt/homebrew/bin/g++-16`                               | Homebrew GCC 16        |
| `/opt/homebrew/opt/llvm/bin/clang++`                     | Homebrew Clang         |

`/usr/bin/g++` is Apple Clang. The real GCC binary on this machine is `g++-16`.

Homebrew GCC 16 cannot parse Apple's block syntax in the macOS SDK, so a pure GCC build of the Mac app fails in the Cocoa headers. The GCC launch is a hybrid. C++ is compiled and linked with `g++-16` through `.vscode/scripts/homebrew-g++-16.sh`. C and Objective-C stay on Apple Clang. Two Cocoa translation units, `imgui_impl_glfw.cpp` and `theme_mac.mm`, are compiled with Apple `clang++` against GCC's libstdc++ so they can link into that binary. The resulting app links Homebrew `libstdc++`.

Homebrew Clang is a separate launch. `.vscode/scripts/homebrew-clang++.sh` runs `/opt/homebrew/opt/llvm/bin/clang++` and puts Homebrew's libc++ headers ahead of the macOS SDK includes. Objective-C class stubs are disabled so the binary still deploys to macOS 12. This build links the system libc++.

Xcode LLDB debugs all three macOS binaries. GDB is not part of these launches. Homebrew LLVM does not ship LLDB, and the LLDB used here is `/usr/bin/lldb` from Xcode.

## Editors

Open the repository, accept the recommended extensions, and press **F5**. The default launch is the Apple Clang debug app on macOS, MSVC on Windows, and GCC on Linux.

| Editor                      | Install                                                        | Purpose                                                                                |
| --------------------------- | -------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| **VS Code**                 | `ms-vscode.cpptools`, `ms-vscode.cmake-tools`                  | IntelliSense, CMake integration, and `cppvsdbg`, the Microsoft MSVC debugger           |
| **VS Code** _(optional)_    | `vadimcn.vscode-lldb`                                          | LLDB alongside the Visual Studio debugger                                              |
| **Cursor**                  | `llvm-vs-code-extensions.vscode-clangd`, `vadimcn.vscode-lldb` | clangd IntelliSense and the LLDB debugger. Microsoft's `cppvsdbg` is locked to VS Code |
| **Cursor** _(Windows only)_ | LLVM toolchain — `winget install LLVM.LLVM`                    | The `clangd.exe` binary for the extension                                              |

clangd and the C/C++ extension both use `build-debug/compile_commands.json`. On macOS that is the Apple Clang build. The Homebrew trees keep their own compilation databases and are not the editor default.

## Launches

Every launch builds with Ninja before it starts the debugger. Tests use the same compiler, build directory, and debugger as the app next to them.

### macOS

All three compilers are debugged with Xcode LLDB.

| Launch                                                      | Directory                    | Compiler        |
| ----------------------------------------------------------- | ---------------------------- | --------------- |
| `[macOS · Debug] CoreDeck [Apple Clang · Ninja · LLDB]`     | `build-debug`                | Apple Clang     |
| `[macOS · Debug] CoreDeck [Homebrew GCC 16 · Ninja · LLDB]` | `build-homebrew-gcc16-debug` | Homebrew GCC 16 |
| `[macOS · Debug] CoreDeck [Homebrew Clang · Ninja · LLDB]`  | `build-homebrew-clang-debug` | Homebrew Clang  |

Release uses `build-release`, `build-homebrew-gcc16-release`, and `build-homebrew-clang-release`. Each app launch has a matching `CoreDeck Tests` launch.

### Windows

Both launches compile with MSVC and Ninja into `build-debug`. Release uses `build-release`. The two entries differ only by debugger. VS Code can use either. Cursor uses the LLDB entries, because `cppvsdbg` belongs to VS Code. The Visual Studio debugger reads the MSVC PDB directly.

| Launch                                                      | Debugger                   |
| ----------------------------------------------------------- | -------------------------- |
| `[Windows · Debug] CoreDeck [MSVC · Ninja · Visual Studio]` | Visual Studio (`cppvsdbg`) |
| `[Windows · Debug] CoreDeck [MSVC · Ninja · LLDB]`          | LLDB                       |

Each one has a matching `CoreDeck Tests` launch, and the same set exists for Release.

### Linux

Both launches compile with GCC and Ninja into `build-debug`. Release uses `build-release`. The two entries differ only by debugger.

| Launch                                          | Debugger |
| ----------------------------------------------- | -------- |
| `[Linux · Debug] CoreDeck [GCC · Ninja · GDB]`  | GDB      |
| `[Linux · Debug] CoreDeck [GCC · Ninja · LLDB]` | LLDB     |

Each one has a matching `CoreDeck Tests` launch, and the same set exists for Release.

## Frida

CoreDeck statically links the official `frida-core` devkit. CMake downloads the
tarball for the host you are compiling, checks its SHA-256, and links it. The
archive is cached under `~/.cache/coredeck/frida` (or `%LOCALAPPDATA%\coredeck\frida`
on Windows). Set `COREDECK_FRIDA_CACHE` to put that cache somewhere else, or set
`COREDECK_FRIDA_DEVKIT_DIR` to a directory that already contains `frida-core.h` and
the static library. The cache path cannot contain spaces.

`pins/frida.json` pins the client version, each devkit and `frida-server`
download URL, and their SHA-256 checksums. CMake reads that file. To move to another Frida
release, run:

```bash
python3 tools/update_frida_pins.py 17.9.2
```

The script writes the version, the nine download URLs, and their SHA-256 values into `pins/frida.json`.
Configure again after that. The Java bridge version lives only in
`assets/frida/project/node_modules/frida-java-bridge/package.json`. The agent
source is `assets/frida/project/agent/coredeck_agent.js`. There is no separate
stamp file. Confirm `frida-java-bridge` still compiles with the new devkit, and
rebuild `coredeck_frida_smoke` on every platform. The smoke test calls
`frida_init` and lists local devices. It is the check that the static library
actually linked. An agent edit ships with the bundle. Bump the bridge package
only when that library has to change.

MinGW cannot link the devkit. On Windows the process uses the release static CRT
(`/MT`) in every configuration, including Debug, because the devkit is `/MT`.
Debug builds still emit PDBs. Iterator debugging and the debug heap are off.

macOS loads the whole archive with `-Wl,-load_hidden` so Frida's constructors run
and its GLib symbols stay hidden. Linux uses `--whole-archive` and `--exclude-libs`.
Windows uses `/WHOLEARCHIVE`. Sync Frida calls run on a worker thread. Frida's own
loop thread only delivers signals. Calling a sync API on that loop re-enters it.

The Frida window is its own workspace under Analysis, drawn from `src/gui/frida/`.
Start pushes `coredeck-frida-server` to the device, binds `127.0.0.1:27099`, and
forwards it with the SDK `adb`. The device needs root. If that process is already
running, Start and Use existing both resume it on port 27099, and Stop shuts it
down. Use existing attaches to a separate `frida-server` on the Settings port
(27042 by default) and leaves that process running.
Snippet mode compiles with `frida-java-bridge` imported. Module mode compiles the
buffer unchanged. The compiler reads `assets/frida/project` from the app bundle
unless the Settings path points somewhere else. Reset Agent Project copies that
bundled project onto the override, and does nothing when Settings still points
at the bundle. Paths and the script runtime live on the Settings tab and are
stored in `config.json`. Leaving the workspace detaches sessions and stops a
server CoreDeck started.
