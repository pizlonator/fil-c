#!/bin/bash

set -e
set -x

# dconf 0.40.0.  No bash-completion.pc in this system and no vala, so both
# get turned off; man needs xsltproc which is absent too.

rm -rf pizlonated-dconf
tar -xf pizlonated-dconf.tar.gz
cd pizlonated-dconf
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      -D bash_completion=false \
      -D man=false \
      -D vapi=false \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-dconf
hash -r
