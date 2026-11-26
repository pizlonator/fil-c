#!/bin/bash

set -e
set -x

ulimit -c unlimited


cd /sources
rm -rf pizlonated-desktop-file-utils
tar -xf pizlonated-desktop-file-utils.tar.gz
cd pizlonated-desktop-file-utils
mkdir -v build
cd build
meson setup ..                   \
      --prefix=/usr              \
      --buildtype=debugoptimized
ninja
ninja install
cd ../..

# --- gates ---------------------------------------------------------------------

echo "=== gate: binaries installed ==="
test -x /usr/bin/desktop-file-validate
test -x /usr/bin/desktop-file-install
test -x /usr/bin/update-desktop-database
test -L /usr/bin/desktop-file-edit
ls -la /usr/bin/desktop-file-validate /usr/bin/desktop-file-install /usr/bin/update-desktop-database /usr/bin/desktop-file-edit

echo "=== gate: update-desktop-database --version runs ==="
V=`update-desktop-database --version 2>&1`
echo "$V"
echo "$V" | grep -q "0.28"

echo "=== gate: desktop-file-validate runs ==="
V2=`desktop-file-validate --version 2>&1 || true`
echo "$V2"

echo "=== gate: ldd of every binary (no 'not found') ==="
for f in desktop-file-validate desktop-file-install update-desktop-database; do
    if ldd /usr/bin/$f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd /usr/bin/$f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

rm -rf pizlonated-desktop-file-utils
hash -r

echo "postlc4 desktop-file-utils: ALL GATES GREEN"
