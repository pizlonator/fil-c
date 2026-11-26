#!/bin/bash

set -e
set -x

rm -rf pizlonated-freetype
tar -xf pizlonated-freetype.tar.gz
cd pizlonated-freetype
./configure --prefix=/usr --enable-freetype-config --disable-static
make
make install
cd ..
rm -rf pizlonated-freetype
