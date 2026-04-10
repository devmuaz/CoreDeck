# CoreDeck

A fast, modern desktop app for managing and launching Android emulators — built with C++20 and Dear ImGui.

## Setup

```bash
git clone --recursive https://github.com/devmuaz/CoreDeck.git
cd CoreDeck
mkdir build && cd build
cmake ..
make
```

If you already cloned without `--recursive`:

```bash
git submodule update --init
```