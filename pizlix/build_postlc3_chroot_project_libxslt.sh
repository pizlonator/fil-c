#!/bin/bash
set -e
set -x
rm -rf pizlonated-libxslt
tar -xf pizlonated-libxslt.tar.gz
cd pizlonated-libxslt
CFLAGS="-g -O2" CXXFLAGS="-g -O2" ./configure --prefix=/usr
make -j$(nproc)
make install
cd ..
rm -rf pizlonated-libxslt
