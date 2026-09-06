#!/bin/sh

. libpas/common.sh

set -e
set -x

cd projects/Vulkan-Loader-1.4.346
rm -rf build
CC=$PWD/../../build/bin/clang CXX=$PWD/../../build/bin/clang++ \
    cmake -DCMAKE_INSTALL_PREFIX=$PWD/../../pizfix \
          -D CMAKE_BUILD_TYPE=Release \
          -D BUILD_WERROR=ON -D UPDATE_DEPS=On \
          -B build
cmake --build build -j $NCPU
