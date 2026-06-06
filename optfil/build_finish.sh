#!/bin/bash
#
# Copyright (c) 2025-2026 Epic Games, Inc. All Rights Reserved.
# Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

set -e
set -x

ulimit -c unlimited

test "x$FILCSRC" != "x"
test -d $FILCSRC
test -d $FILCSRC/libpas
test -d $FILCSRC/llvm
test -d $FILCSRC/clang
test -d $FILCSRC/filc
test -d $FILCSRC/optfil

FILCOWNER=`stat -c %U $FILCSRC`
test `id -u` -eq `id -u $FILCOWNER`

cd $FILCSRC/optfil

. ../libpas/common.sh

package_name=optfil-0.679-$OS-$ARCH

rm -rf $package_name
mkdir -v $package_name

cp -v fil.tar.xz $package_name/
cp -v ../README.md $package_name/
cp -v ../LLVM-LICENSE.txt $package_name/
cp -v ../libpas/LICENSE.txt $package_name/PAS-LICENSE.txt
cp -v setup.sh $package_name/
cp -v sshd_setup.md $package_name/

# Copy all project license files
licenses_dir=$package_name/additional_licenses
mkdir -p $licenses_dir
cp -v *-LICENSE.txt $licenses_dir/
cp -v *-LICENSE.md $licenses_dir/

tar -cJf $package_name.tar.xz $package_name
