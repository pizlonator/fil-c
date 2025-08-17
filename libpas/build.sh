#!/bin/sh
#
# Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
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

. ./common.sh

set -e
set -x

mkdir -p build
mkdir -p ../pizfix/include
mkdir -p ../pizfix/stdfil-include
mkdir -p ../pizfix/musl-include
mkdir -p ../pizfix/bin
mkdir -p ../pizfix/sbin
mkdir -p ../pizfix/libexec
mkdir -p ../pizfix/share
mkdir -p ../pizfix/lib
mkdir -p ../pizfix/lib_gcverify
mkdir -p ../pizfix/lib_test
mkdir -p ../pizfix/lib_test_gcverify
mkdir -p ../pizfix/man
mkdir -p ../pizfix/man/man1
$MAKE -f Makefile-setup
$MAKE -f Makefile -j $NCPU

set +x

echo Pizlonator Approves.
