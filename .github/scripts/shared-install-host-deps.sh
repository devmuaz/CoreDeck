#!/usr/bin/env bash
# Host packages required to configure and build CoreDeck.

set -euo pipefail

case "${RUNNER_OS}" in
  Linux)
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends \
      build-essential \
      gettext \
      libcurl4-openssl-dev \
      libgl1-mesa-dev \
      libx11-dev \
      libxrandr-dev \
      libxinerama-dev \
      libxcursor-dev \
      libxi-dev \
      libxext-dev
    ;;
  macOS)
    brew install gettext
    ;;
  Windows)
    pwsh -NoProfile -File ./.github/scripts/windows-install-gettext.ps1
    ;;
  *)
    echo "No host dependency step for RUNNER_OS=${RUNNER_OS}" >&2
    exit 1
    ;;
esac
