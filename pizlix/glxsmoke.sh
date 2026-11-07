#!/bin/bash
# Fil-C llvmpipe smoke: build + run inside the chroot (Gate 4 of sprint 2).
set -e
set -x
cd /sources
export XDG_RUNTIME_DIR=/tmp/xdg
mkdir -p $XDG_RUNTIME_DIR
cc -enable-filc-jit -o glxsmoke glxsmoke.c -lEGL -lgbm -lGLESv2
./glxsmoke llvmpipe
# softpipe baseline for the timing comparison (same binary, other driver)
./glxsmoke softpipe
