#!/bin/bash

set -e
set -x

rm -rf pizlonated-graphene
tar -xf pizlonated-graphene.tar.gz
cd pizlonated-graphene
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pizlonated-graphene
