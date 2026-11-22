#!/bin/bash

set -e
set -x

# libvorbis 1.3.7 -- the Vorbis codec (vorbis/vorbisenc/vorbisfile).
# Second of the postlc12 audio trio: libcanberra's configure hard-requires
# vorbisfile (PKG_CHECK_MODULES(VORBIS, [vorbisfile]) with no failure
# action, configure.ac:586) and its ogg sound-file reader is compiled in
# unconditionally, and libcanberra-gtk3 >= 0.4 is in turn an unconditional
# gdm dependency (meson.build:66).  Built right after libogg (same stage).
#
# Fil-C notes: plain C float DSP (no asm on x86-64 -- the x86 SSE paths are
# runtime-dispatched C built from source), no version script.  Built
# unpatched.

cd /sources
rm -rf pizlonated-libvorbis
tar -xf pizlonated-libvorbis.tar.gz
cd pizlonated-libvorbis
./configure     \
    --prefix=/usr   \
    --disable-static
make
make install
cd ../..
rm -rf pizlonated-libvorbis
hash -r

# --- gates ---------------------------------------------------------------------

echo "=== gate: the three libraries + their .pc files ==="
test -e /usr/lib/libvorbis.so
test -e /usr/lib/libvorbisfile.so
test -e /usr/lib/libvorbisenc.so
test -e /usr/lib/pkgconfig/vorbis.pc
test -e /usr/lib/pkgconfig/vorbisfile.pc
test -e /usr/lib/pkgconfig/vorbisenc.pc
pkg-config --modversion vorbis vorbisfile vorbisenc

echo "=== gate: headers ==="
test -e /usr/include/vorbis/vorbisfile.h
test -e /usr/include/vorbis/codec.h

echo "=== gate: ldd clean (the ogg linkage resolves) ==="
for f in /usr/lib/libvorbis.so /usr/lib/libvorbisfile.so /usr/lib/libvorbisenc.so; do
    if ldd $f 2>&1 | grep -q 'not found'; then
        echo "FATAL: $f has unresolved shared libraries:" >&2
        ldd $f 2>&1 | grep 'not found' >&2
        exit 1
    fi
done
echo "ldd: clean"

echo "postlc12 libvorbis: ALL GATES GREEN"
