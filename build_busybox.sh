#!/bin/sh
#
# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
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

cd projects/busybox-1.37.0
extract_source

# Make a default config and then modify it.
CC=$PWD/../../../build/bin/clang make defconfig

# Fix the error when running `make menuconfig`, although we don't need it in
# this automated build.
sed -i 's/^main() {}/int main() { return 0; }/' ./scripts/kconfig/lxdialog/check-lxdialog.sh

# Modify the config to build a static busybox without tc (traffic control).
# - We want a static busybox so that it can run in minimal environments.
# - We don't want tc because it relies on specific kernel modules,
#   which lead to build failures if the host kernel version >= 6.8.
#   https://lists.busybox.net/pipermail/busybox-cvs/2024-January/041752.html
sed -i \
  -e 's/^# CONFIG_STATIC is not set$/CONFIG_STATIC=y/' \
  -e 's/^CONFIG_TC=y$/# CONFIG_TC is not set/' \
  -e 's/^CONFIG_FEATURE_TC_INGRESS=y$/# CONFIG_FEATURE_TC_INGRESS is not set/' \
  .config

CC=$PWD/../../../build/bin/clang make -j $NCPU
cp busybox $PWD/../../../pizfix/bin/busybox
