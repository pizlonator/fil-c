#!/bin/bash

set -e
set -x

rm -rf pizlonated-graphite2
tar -xf pizlonated-graphite2.tar.gz
cd pizlonated-graphite2

mkdir -v build
cd build
cmake -D CMAKE_INSTALL_PREFIX=/usr ..
make
make install
cd ../..
rm -rf pizlonated-graphite2
