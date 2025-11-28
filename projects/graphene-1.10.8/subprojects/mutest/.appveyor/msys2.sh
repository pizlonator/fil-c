#!/bin/bash

set -e

if [[ "$MSYSTEM" == "MINGW32" ]]; then
    export MSYS2_ARCH="i686"
else
    export MSYS2_ARCH="x86_64"
fi

# Install the required packages
pacman --noconfirm -Syyu --needed \
    mingw-w64-$MSYS2_ARCH-gcc \
    mingw-w64-$MSYS2_ARCH-meson \
    mingw-w64-$MSYS2_ARCH-ninja \
    mingw-w64-$MSYS2_ARCH-pkg-config \

# Build
meson _build -Dstatic=true
cd _build
ninja

# Test
export MUTEST_OUTPUT=tap
meson test || {
  cat meson-logs/testlog.txt
  exit 1
}
