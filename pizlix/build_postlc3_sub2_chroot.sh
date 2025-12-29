#!/bin/bash

set -e
set -x

ulimit -c unlimited

# Some things in this build require a libgcc.a, which looks like an idiotic byproduct of CMake being
# retarded. Give them what they want instead of trying to understand CMake.
cd /usr/lib
rm -f libgcc.a
ar cr libgcc.a

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
mkdir -v build
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

tar -xf pizlonated-libjpeg-turbo.tar.gz
cd pizlonated-libjpeg-turbo
mkdir -v build
cd build
cmake -G"Unix Makefiles" .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_ASM_NASM_COMPILER="" \
      -D CMAKE_BUILD_TYPE=RELEASE         \
      -D ENABLE_STATIC=FALSE              \
      -D CMAKE_INSTALL_DEFAULT_LIBDIR=lib \
      -D CMAKE_SKIP_INSTALL_RPATH=ON      \
      -D CMAKE_INSTALL_DOCDIR=/usr/share/doc/libjpeg-turbo-3.0.1
make
make install
cd ../..
rm -rf pizlonated-libjpeg-turbo
hash -r

tar -xf pizlonated-tiff.tar.gz
cd pizlonated-tiff
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-tiff
hash -r

tar -xf pizlonated-libwebp.tar.gz
cd pizlonated-libwebp
./configure \
    --prefix=/usr \
    --enable-libwebpmux     \
    --enable-libwebpdemux   \
    --enable-libwebpdecoder \
    --enable-libwebpextras  \
    --enable-swap-16bit-csp    
make
make install
cd ..
rm -rf pizlonated-libwebp
hash -r

# Rebuild tiff because of circular dep with libwebp
tar -xf pizlonated-tiff.tar.gz
cd pizlonated-tiff
./configure --prefix=/usr
make
make install
cd ..
rm -rf pizlonated-tiff
hash -r

tar -xf pizlonated-openjpeg.tar.gz
cd pizlonated-openjpeg
mkdir -v build
cd build
cmake -G"Unix Makefiles" .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
make
make install
cd ../..
rm -rf pizlonated-openjpeg
hash -r

tar -xf pixman-0.43.4.tar.gz
cd pixman-0.43.4
mkdir -v build
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
./build_postlc3_chroot_project_freetype.sh
./build_postlc3_chroot_project_graphite.sh
./build_postlc3_chroot_project_fontconfig.sh
./build_postlc3_chroot_project_cairo.sh
./build_postlc3_chroot_project_harfbuzz.sh
./build_postlc3_chroot_project_freetype.sh
./build_postlc3_chroot_project_graphite.sh
./build_postlc3_chroot_project_libdrm.sh
./build_postlc3_chroot_project_seatd.sh
hash -r

tar -xf lcms2-2.16.tar.gz
cd lcms2-2.16
./configure --prefix=/usr --disable-static
make
make install
cd ..
rm -rf lcms2-2.16
hash -r

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

