#!/bin/bash
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
# build_zegarmistrz.sh: build the zegarmistrz emulator (projects/zegarmistrz)
# and install the binary at filc/zegarmistrz.
#
# Usage: ./build_zegarmistrz.sh [-j N]
set -u

JOBS="$(nproc 2>/dev/null || echo 8)"
while [ $# -gt 0 ]; do
    case "$1" in
        -j) JOBS="$2"; shift 2 ;;
        -j*) JOBS="${1#-j}"; shift ;;
        *) echo "usage: $0 [-j N]" >&2; exit 1 ;;
    esac
done

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/projects/zegarmistrz"
DST="$ROOT/filc/zegarmistrz"

# Zydis decoder dependency (system package provides it).
if ! echo '#include <Zydis/Zydis.h>
int main(){return 0;}' | gcc -x c -o /dev/null - -lZydis -lZycore 2>/dev/null; then
    echo "build_zegarmistrz: Zydis not found." >&2
    echo "Install it first, e.g.:" >&2
    echo "  sudo apt-get install -y libzydis-dev libzycore-dev" >&2
    if command -v apt-get >/dev/null 2>&1 && [ "$(id -u)" = 0 ]; then
        echo "Trying apt-get install..." >&2
        apt-get update && apt-get install -y libzydis-dev libzycore-dev || exit 1
    else
        exit 1
    fi
fi

echo "build_zegarmistrz: building in $SRC (-j$JOBS)..."
make -C "$SRC" -j"$JOBS" || { echo "build_zegarmistrz: build FAILED" >&2; exit 1; }

mkdir -p "$(dirname "$DST")"
cp -f "$SRC/zegarmistrz" "$DST" || { echo "build_zegarmistrz: install FAILED" >&2; exit 1; }
chmod +x "$DST"
echo "build_zegarmistrz: installed $DST"
"$DST" --help >/dev/null 2>&1 || true
