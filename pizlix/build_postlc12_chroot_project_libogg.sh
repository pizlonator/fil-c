#!/bin/bash

set -e
set -x

# libogg 1.3.5 -- the Ogg container bitstream library.  First of the
# postlc12 "login manager" stage's audio trio: libvorbis (next in this
# stage) needs it, libcanberra needs libvorbis (its configure hard-requires
# vorbisfile -- there is no --disable-vorbis switch, and src/read-sound-
# file.c calls the ca_vorbis_* reader unconditionally), and libcanberra-
# gtk3 is in turn an UNCONDITIONAL dependency of gdm 46.2 (meson.build:66,
# consumed only by utils/gdm-screenshot, but the dependency() call has no
# required: argument -- without the library GDM's meson setup dies).
#
# Fil-C notes: plain C, no asm, no version script.  Built unpatched.

cd /sources
rm -rf pizlonated-libogg
tar -xf pizlonated-libogg.tar.gz
cd pizlonated-libogg
./configure     \
    --prefix=/usr   \
    --disable-static
make
make install
cd ../..
rm -rf pizlonated-libogg
hash -r

# --- gates ---------------------------------------------------------------------

echo "=== gate: the library + its .pc file ==="
test -e /usr/lib/libogg.so
test -e /usr/lib/pkgconfig/ogg.pc
pkg-config --modversion ogg

echo "=== gate: headers ==="
test -e /usr/include/ogg/ogg.h
test -e /usr/include/ogg/os_types.h

echo "=== gate: ldd clean ==="
if ldd /usr/lib/libogg.so 2>&1 | grep -q 'not found'; then
    echo "FATAL: libogg has unresolved shared libraries:" >&2
    ldd /usr/lib/libogg.so 2>&1 | grep 'not found' >&2
    exit 1
fi
echo "ldd: clean"

echo "postlc12 libogg: ALL GATES GREEN"
