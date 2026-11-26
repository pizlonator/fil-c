#!/bin/bash
set -e
set -x
rm -rf pizlonated-abseil
tar -xf pizlonated-abseil.tar.gz
cd pizlonated-abseil
mkdir -v build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DABSL_BUILD_TESTING=OFF -DABSL_PROPAGATE_CXX_STD=ON
make -j$(nproc)
make install
cd ../..
rm -rf pizlonated-abseil
