#!/bin/bash

set -e
set -x

# libsass 3.6.6 + sassc 3.6.2 as build tools for libadwaita.
# They are installed under /yolo, NOT /usr.
#
# NOTE: the yolo gcc (/yolo/bin/gcc) is a C-only toolchain - it has no
# libstdc++ headers or library at all (kernel builds only ever need C), so
# C++ targets cannot be linked with it.  libsass/sassc are C++, so they are
# built with the chroot's Fil-C g++ (which uses the LLVM libc++ runtime that
# LibreOffice et al. already use).  The binaries are Fil-C binaries, which is
# fine: /yolo lives inside the Pizlix system where the Fil-C runtime is the
# system runtime.  libsass is linked statically into sassc so only one
# binary ends up in /yolo/bin.

rm -rf pizlonated-libsass pizlonated-sassc
tar -xf pizlonated-libsass.tar.gz
tar -xf pizlonated-sassc.tar.gz

cd pizlonated-libsass
make BUILD=static CC=gcc CXX=g++ -j$(nproc)
make install BUILD=static PREFIX=/yolo CC=gcc CXX=g++
cd ..

cd pizlonated-sassc
SASS_LIBSASS_PATH=/sources/pizlonated-libsass make BUILD=static CC=gcc CXX=g++ -j$(nproc)
make install BUILD=static PREFIX=/yolo SASS_LIBSASS_PATH=/sources/pizlonated-libsass CC=gcc CXX=g++
cd ..

rm -rf pizlonated-libsass pizlonated-sassc
hash -r

# Verify the tool works.
echo 'a{b:c}' | /yolo/bin/sassc
