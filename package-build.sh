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

. libpas/common.sh

set -e
set -x

build_name=filc-0.668.7-$OS-$ARCH

rm -rf $build_name

mkdir $build_name
cp README.md $build_name/
cp LLVM-LICENSE.txt $build_name/
cp libpas/LICENSE.txt $build_name/PAS-LICENSE.txt
cp usermusl/COPYRIGHT $build_name/MUSL-LICENSE.txt

mkdir -p $build_name/build/bin
cp build/bin/clang-17 $build_name/build/bin/
strip $build_name/build/bin/clang-17
(cd $build_name/build/bin/ &&
     ln -s clang-17 clang &&
     ln -s clang-17 clang++)

mkdir -p $build_name/build/include/
cp -R build/include/c++ $build_name/build/include/
mkdir -p $build_name/build/include/x86_64-unknown-linux-gnu/
cp -R build/include/x86_64-unknown-linux-gnu/c++ $build_name/build/include/x86_64-unknown-linux-gnu/
mkdir -p $build_name/build/lib/clang/17/
cp -R build/lib/clang/17/include $build_name/build/lib/clang/17/

cp -R pizfix $build_name/
rm -f $build_name/pizfix/etc/moduli
rm -f $build_name/pizfix/etc/ssh_host*
rm -rf $build_name/pizfix/yolo/lib-old
rm -rf $build_name/pizfix/os-include

sourcedir=$PWD

cd $build_name

echo '#!/bin/sh' > setup.sh
echo 'set -e' >> setup.sh
echo 'set -x' >> setup.sh

for binary in pizfix/lib/*.so pizfix/lib/*.so.* pizfix/lib64/*.so pizfix/lib64/*.so.* pizfix/bin/* pizfix/sbin/* pizfix/libexec/*
do
    if test ! -L $binary
    then
        if patchelf --set-rpath pizfix/yolo/lib:pizfix/lib64:pizfix/lib $binary
        then
            echo "patchelf --set-rpath \$PWD/pizfix/yolo/lib:\$PWD/pizfix/lib64:\$PWD/pizfix/lib $binary" >> setup.sh
        fi
        if patchelf --set-interpreter pizfix/yolo/lib/ld-yolo-x86_64.so $binary
        then
            echo "patchelf --set-interpreter \$PWD/pizfix/yolo/lib/ld-yolo-x86_64.so $binary" >> setup.sh
        fi
    fi
done

rm pizfix/yolo/lib/ld-yolo-x86_64.so
(cd pizfix/yolo/lib/ && ln -s libyoloc.so ld-yolo-x86_64.so)

echo "cd pizfix" >> setup.sh
echo "mkdir os-include" >> setup.sh
echo "cd os-include" >> setup.sh
echo "ln -s /usr/include/linux ." >> setup.sh
echo "ln -s /usr/include/x86_64-linux-gnu/asm ." >> setup.sh
echo "ln -s /usr/include/asm-generic ." >> setup.sh
echo "cd ../.." >> setup.sh

echo 'set +x' >> setup.sh
echo 'echo' >> setup.sh
echo 'echo "You are all set. Try compiling something with:"' >> setup.sh
echo 'echo' >> setup.sh
echo "echo \"    build/bin/clang -o whatever whatever.c -O2 -g\"" >> setup.sh
echo 'echo' >> setup.sh
echo 'echo "or:"' >> setup.sh
echo 'echo' >> setup.sh
echo "echo \"    build/bin/clang++ -o whatever whatever.cpp -O2 -g\"" >> setup.sh
echo 'echo' >> setup.sh
echo "echo \"Take a look at pizfix/stdfil-include/stdfil.h for Fil-C-specific APIs. You can\"" >> setup.sh
echo "echo \"optionally #include <stdfil.h> if you find those APIs useful.\"" >> setup.sh
echo 'echo' >> setup.sh
echo "echo \"New releases are at: https://github.com/pizlonator/llvm-project-deluge/releases\"" >> setup.sh
echo "echo \"The Fil-C Manifesto: https://github.com/pizlonator/llvm-project-deluge/blob/deluge/Manifesto.md\"" >> setup.sh
echo 'echo' >> setup.sh
echo "echo \"Have fun and thank you for trying $build_name.\"" >> setup.sh

chmod 755 setup.sh

cd ..

tar -cJvf $build_name.tar.xz $build_name

