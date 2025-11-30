#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

tar -xf fribidi-1.0.15.tar.xz
cd fribidi-1.0.15
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf fribidi-1.0.15
hash -r

tar -xf shared-mime-info-2.4.tar.gz
cd shared-mime-info-2.4
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized -D update-mimedb=true ..
ninja
ninja install
cd ../..
rm -rf shared-mime-info-2.4
hash -r

tar -xf pycairo-1.26.1.tar.gz
cd pycairo-1.26.1
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pycairo-1.26.1
hash -r

./build_postlc4_chroot_project_pygobject.sh
./build_postlc4_chroot_project_graphene.sh
hash -r

tar -xf iso-codes_4.16.0.orig.tar.xz
cd iso-codes-4.16.0
./configure --prefix=/usr
make
make install
cd ..
rm -rf iso-codes-4.16.0
hash -r

tar -xf Mako-1.3.5.tar.gz
cd Mako-1.3.5
pip3 wheel -w dist --no-build-isolation --no-deps --no-cache-dir $PWD
pip3 install --no-index --find-links=dist --no-cache-dir --no-user Mako
cd ..
rm -rf Mako-1.3.5
hash -r

./build_postlc4_chroot_project_mesa.sh
hash -r

tar -xf libepoxy-1.5.10.tar.xz
cd libepoxy-1.5.10
mkdir -v build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized .. -Dx11=false
ninja
ninja install
cd ../..
rm -rf libepoxy-1.5.10
hash -r

./build_postlc4_chroot_project_pango.sh
./build_postlc4_chroot_project_gdk-pixbuf.sh
./build_postlc4_chroot_project_gtk4.sh
hash -r

