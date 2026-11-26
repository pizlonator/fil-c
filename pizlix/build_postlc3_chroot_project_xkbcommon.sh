#!/bin/bash

set -e
set -x

# xkbcommon X11 rebuild (postlc3).
#
# The base install (postlc2) was built with -D enable-x11=false because
# nothing needed xkbcommon-x11 back then.  Mutter (later in this stage)
# requires xkbcommon-x11, so rebuild the same tarball with X11 enabled and
# install over the existing files.  Needs libxcb's xcb-xkb (present since
# this stage's X11 library block) and xkeyboard-config (postlc2).

rm -rf pizlonated-xkbcommon
tar -xf pizlonated-xkbcommon.tar.gz
cd pizlonated-xkbcommon
mkdir -v build
cd build
meson setup ..              \
      --prefix=/usr         \
      --buildtype=debugoptimized \
      -D enable-x11=true    \
      -D enable-docs=false
ninja
ninja install
cd ../..
rm -rf pizlonated-xkbcommon
hash -r
