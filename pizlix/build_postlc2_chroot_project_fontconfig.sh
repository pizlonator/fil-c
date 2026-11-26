#!/bin/bash

set -e
set -x

rm -rf pizlonated-fontconfig
tar -xf pizlonated-fontconfig.tar.gz
cd pizlonated-fontconfig
./configure --prefix=/usr        \
            --sysconfdir=/etc    \
            --localstatedir=/var \
            --disable-docs       \
            --docdir=/usr/share/doc/fontconfig-2.15.0
make
make install
cd ..
rm -rf pizlonated-fontconfig
