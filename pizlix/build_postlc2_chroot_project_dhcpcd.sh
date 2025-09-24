#!/bin/bash

set -e
set -x

rm -rf pizlonated-dhcpcd
tar -xf pizlonated-dhcpcd.tar.gz
cd pizlonated-dhcpcd
./configure --prefix=/usr                \
            --sysconfdir=/etc            \
            --libexecdir=/usr/lib/dhcpcd \
            --dbdir=/var/lib/dhcpcd      \
            --runstatedir=/run           \
            --disable-privsep
make
make install
cd ..
rm -rf pizlonated-dhcpcd
