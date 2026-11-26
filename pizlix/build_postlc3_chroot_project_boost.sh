#!/bin/bash
set -e
set -x
rm -rf boost_1_86_0
tar -xf boost_1_86_0.tar.bz2
cd boost_1_86_0
patch -p1 < ../boost-filc.patch
./bootstrap.sh --prefix=/usr --with-toolset=gcc
./b2 -j$(nproc) --without-coroutine context-impl=ucontext cxxflags=-g --prefix=/usr install
cd ..
rm -rf boost_1_86_0
