#!/bin/bash

set -e
set -x

# libusb 1.0.28 (autotools).  udev support is auto-detected via libudev.

rm -rf pizlonated-libusb
tar -xf pizlonated-libusb.tar.gz
cd pizlonated-libusb
./configure --prefix=/usr --disable-static
make -j$(nproc)
make install
cd ..
rm -rf pizlonated-libusb
hash -r
