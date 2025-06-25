#!/bin/sh
#
# Copyright (c) 2023-2025 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

. libpas/common.sh

set -e
set -x

cd yolomusl
./configure --prefix=$PWD/../pizfix/yolo --syslibdir=$PWD/../pizfix/yolo/lib
$MAKE clean
$MAKE -j $NCPU
$MAKE install

cd ../pizfix
mkdir -p lib
cd lib
mv ../yolo/lib/libyoloc.so .
mv ../yolo/lib/libyoloc.a .
mv ../yolo/lib/crt1.o .
mv ../yolo/lib/crti.o .
mv ../yolo/lib/crtn.o .
mv ../yolo/lib/rcrt1.o .
mv ../yolo/lib/Scrt1.o .
rm -f libyolom.a
ar cr libyolom.a
ln -fs libyoloc.so ld-yolo-x86_64.so

cd ..
rm -rf yolo-include
mv yolo/include yolo-include

rm -rf yolo

