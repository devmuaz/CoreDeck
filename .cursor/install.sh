#!/usr/bin/env bash
#
# Idempotent repository bootstrap for CoreDeck Cloud Agents.
# Runs from /workspace after the repository is checked out.
set -euo pipefail

# Only the submodules the default (Sentry-disabled) build needs. This skips the
# heavy sentry-native tree and reflect-cpp's bundled vcpkg, which are not used
# unless COREDECK_SENTRY_DSN is set. Non-recursive on purpose.
git submodule update --init \
    extern/imgui \
    extern/glfw \
    extern/reflect-cpp \
    extern/tinyfiledialogs \
    extern/catch2 \
    extern/miniz

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCOREDECK_BUILD_TESTS=ON
cmake --build build --parallel
