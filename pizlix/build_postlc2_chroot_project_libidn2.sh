#!/bin/bash

set -e
set -x

rm -rf pizlonated-libidn2
tar -xf pizlonated-libidn2.tar.gz
cd pizlonated-libidn2
./configure --prefix=/usr --disable-static
make
make install
cd ..
rm -rf pizlonated-libidn2
