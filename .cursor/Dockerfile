# Cloud Agent base image for CoreDeck.
#
# CoreDeck is a C++20 desktop app (CMake + Ninja, Dear ImGui + GLFW + OpenGL).
# This image installs the stable system toolchain and the GL/X11 development
# libraries the app links against, plus a headless software-rendering stack
# (Xvfb + Mesa llvmpipe) so the GUI can be launched and exercised without a
# physical display. Repository-specific work (submodules + build) happens in
# the install step, not here.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        git \
        curl \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        libcurl4-openssl-dev \
        libgl1-mesa-dev \
        libx11-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libxext-dev \
        libglu1-mesa \
        libgl1-mesa-dri \
        xvfb \
        x11-utils \
        mesa-utils \
    && rm -rf /var/lib/apt/lists/*

USER ubuntu
