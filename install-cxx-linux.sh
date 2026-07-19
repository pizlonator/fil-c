#!/bin/sh
#
# Copyright (c) 2024 Epic Games, Inc. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS'' AND ANY
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

set -e
set -x

arch=$(uname -m)

cp build/lib/$arch-unknown-linux-gnu/libc++.so pizfix/lib
cp build/lib/$arch-unknown-linux-gnu/libc++.so.1.0 pizfix/lib
cp build/lib/$arch-unknown-linux-gnu/libc++abi.so.1.0 pizfix/lib
cp build/lib/$arch-unknown-linux-gnu/libc++.a pizfix/lib
cp build/lib/$arch-unknown-linux-gnu/libc++abi.a pizfix/lib
(cd pizfix/lib &&
     rm -f libc++.so.1 &&
     rm -f libc++abi.so.1 &&
     rm -f libc++abi.so &&
     ln -s libc++.so.1.0 libc++.so.1 &&
     ln -s libc++abi.so.1.0 libc++abi.so.1 &&
     ln -s libc++abi.so.1 libc++abi.so)
