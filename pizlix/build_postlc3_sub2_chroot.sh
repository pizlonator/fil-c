#!/bin/bash

set -e
set -x

ulimit -c unlimited

cd /sources

tar -xf pizlonated-wayland.tar.gz
cd pizlonated-wayland
mkdir build
cd build
meson setup ..            \
      --prefix=/usr       \
      --buildtype=debugoptimized \
      -D documentation=false
ninja
ninja install
cd ../..
rm -rf pizlonated-wayland
hash -r

tar -xf wayland-protocols-1.45.tar.xz
cd wayland-protocols-1.45
mkdir build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf wayland-protocols-1.45
hash -r

tar -xf pizlonated-libevdev.tar.gz
cd pizlonated-libevdev
mkdir build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized -D documentation=disabled ..
ninja
ninja install
cd ../..
rm -rf pizlonated-libevdev
hash -r

tar -xf pizlonated-libinput.tar.gz
cd pizlonated-libinput
mkdir build
cd build
meson setup ..            \
      --prefix=/usr       \
      --buildtype=debugoptimized \
      -D debug-gui=false  \
      -D libwacom=false \
      -D mtdev=false \
      -D documentation=false
ninja
ninja install
cd ../..
rm -rf pizlonated-libinput
hash -r

tar -xf xkeyboard-config-2.42.tar.xz
cd xkeyboard-config-2.42
mkdir build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf xkeyboard-config-2.42
hash -r

tar -xf pizlonated-xkbcommon.tar.gz
cd pizlonated-xkbcommon
mkdir build
cd build
meson setup ..             \
      --prefix=/usr        \
      --buildtype=debugoptimized \
      -D enable-x11=false  \
      -D enable-docs=false
ninja
ninja install
cd ../..
rm -rf pizlonated-xkbcommon
hash -r

tar -xf pizlonated-libpng.tar.gz
cd pizlonated-libpng
./configure --prefix=/usr --disable-static
make
make install
cd ..
rm -rf pizlonated-libpng
hash -r

tar -xf pixman-0.43.4.tar.gz
cd pixman-0.43.4
mkdir build
cd build
meson setup --prefix=/usr --buildtype=debugoptimized ..
ninja
ninja install
cd ../..
rm -rf pixman-0.43.4
hash -r

tar -xf packaging-24.1.tar.gz
cd packaging-24.1
pip3 wheel -w dist --no-build-isolation --no-deps --no-cache-dir $PWD
pip3 install --no-index --find-links=dist --no-cache-dir --no-user packaging
cd ..
rm -rf packaging-24.1
hash -r

./build_postlc3_chroot_project_glib.sh
./build_postlc3_chroot_project_cairo.sh
./build_postlc3_chroot_project_libdrm.sh
./build_postlc3_chroot_project_seatd.sh
./build_postlc3_chroot_project_weston.sh
hash -r

tar -xf daemon-0.6.4.tar.gz
cd daemon-0.6.4
./configure
make -j 1
make -j 1 PREFIX=/usr install
cd ..
rm -rf daemon-0.6.4
hash -r

./build_postlc3_chroot_setup_scripts_for_weston.sh

