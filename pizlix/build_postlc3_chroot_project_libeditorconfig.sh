#!/bin/bash

set -e
set -x

# libeditorconfig 0.12.11 (editorconfig-core-c; the EditorConfig parser
# library that gnome-text-editor links against).
#
# cmake with the Fil-C chroot gcc.  Notes:
# - -DCMAKE_INSTALL_LIBDIR=lib: GNUInstallDirs would otherwise pick lib64
#   on some multiarch layouts; this system keeps everything in /usr/lib.
# - There is no BUILD_DOCUMENTATION option in 0.12.11 (no doxygen/sphinx
#   target), so none is passed.
# - The empty /usr/lib/libgcc.a stub that cmake-driven builds want was
#   already created by the postlc2 stage; it persists in the chroot.
# - PCRE2 comes from the pcre2-10.49 autotools build (postlc2 stage);
#   FindPCRE2.cmake locates /usr/include/pcre2.h and libpcre2-8.

rm -rf pizlonated-libeditorconfig
tar -xf pizlonated-libeditorconfig.tar.gz
cd pizlonated-libeditorconfig
cmake -B build                       \
      -DCMAKE_INSTALL_PREFIX=/usr    \
      -DCMAKE_BUILD_TYPE=Release     \
      -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build -j$(nproc)
cmake --install build
cd ..
rm -rf pizlonated-libeditorconfig
hash -r