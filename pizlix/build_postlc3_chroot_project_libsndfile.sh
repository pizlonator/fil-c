#!/bin/bash

set -e
set -x


# libsndfile 1.2.2 — required by pulseaudio even for the client-only build
# (libpulsecommon links pulsecore/sound-file.c, and pacat/pactl link sndfile).
#
# Flags (./configure --help):
# --disable-external-libs: no FLAC/Ogg/Vorbis in the chroot (WAV/AIFF/AU
#   support is all pacat/pactl use).
# --disable-mpeg: no LAME/MPG123.
# --disable-alsa: no ALSA headers (and daemon-side anyway).
# --disable-sqlite: not needed.
# --disable-full-suite? (not offered in this release) — tests are not run,
#   we just don't invoke them.
# autotools build (the release tarball has no meson.build; CMakeLists exists
# but autotools matches the rest of the pizlix chain and lets us disable
# external libs cleanly).
#
# Fil-C notes: plain C, no asm, no version script — built and installed
# unpatched so far.
cd /sources
rm -rf pizlonated-libsndfile
tar -xf pizlonated-libsndfile.tar.gz
cd pizlonated-libsndfile
./configure             \
    --prefix=/usr       \
    --disable-static    \
    --disable-external-libs \
    --disable-mpeg      \
    --disable-alsa      \
    --disable-sqlite
make
make install
cd ../..
rm -rf pizlonated-libsndfile
hash -r
