#!/bin/bash

set -e
set -x

# lcms2 2.16, built from the projeny-managed Fil-C package
# (projects/lcms2.projeny).  This is the ONE lcms2 build in the chain: no
# later stage re-packages or rebuilds it.  utils/samples default to false
# and jpeg/tiff are auto-detected from the installed libraries.

rm -rf pizlonated-lcms2
tar -xf pizlonated-lcms2.tar.gz
cd pizlonated-lcms2
mkdir build
cd build
meson setup \
      --prefix=/usr \
      --buildtype=debugoptimized \
      ..
ninja
ninja install
cd ../..
rm -rf pizlonated-lcms2
hash -r
