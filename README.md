## Setup
```bash
git clone --recursive https://github.com/devmuaz/emu-launcher.git
cd emu-launcher
mkdir build && cd build
cmake ..
make
```

If you already cloned without `--recursive`:
```bash
git submodule update --init
```