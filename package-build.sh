#!/bin/sh
#
# Copyright (c) 2024-2026 Epic Games, Inc. All Rights Reserved.
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

. libpas/common.sh

set -e
set -x

build_name=filc-0.685-$OS-$ARCH

rm -rf $build_name

mkdir $build_name
cp README.md $build_name/
cp LLVM-LICENSE.txt $build_name/
cp libpas/LICENSE.txt $build_name/PAS-LICENSE.txt
cp projects/usermusl/COPYRIGHT $build_name/MUSL-LICENSE.txt

mkdir -p $build_name/build/bin
cp build/bin/clang-20 $build_name/build/bin/
strip $build_name/build/bin/clang-20
(cd $build_name/build/bin/ &&
     ln -s clang-20 clang &&
     ln -s clang-20 clang++ &&
     ln -s clang-20 filcc &&
     ln -s clang-20 fil++ &&
     ln -s clang-20 filcpp)

mkdir -p $build_name/build/include/
cp -R build/include/c++ $build_name/build/include/
mkdir -p $build_name/build/include/$ARCH-unknown-linux-gnu/
cp -R build/include/$ARCH-unknown-linux-gnu/c++ $build_name/build/include/$ARCH-unknown-linux-gnu/
# The compiler can also target $CROSSARCH. The per-target libc++ include directory only
# holds __config_site, which is architecture-independent, so install it under the other
# triple too so that clang++ --target=$CROSSARCH-linux-gnu can find it.
cp -R $build_name/build/include/$ARCH-unknown-linux-gnu $build_name/build/include/$CROSSARCH-unknown-linux-gnu
mkdir -p $build_name/build/lib/clang/20/
cp -R build/lib/clang/20/include $build_name/build/lib/clang/20/

cp -R pizfix $build_name/
rm -f $build_name/pizfix/etc/moduli
rm -f $build_name/pizfix/etc/ssh_host*
rm -rf $build_name/pizfix/yolo-include
rm -rf $build_name/pizfix/os-include

sourcedir=$PWD

cd $build_name

echo '#!/bin/sh' > setup.sh
echo 'set -e' >> setup.sh
echo 'set -x' >> setup.sh

for binary in pizfix/lib/*.so pizfix/lib/*.so.* pizfix/lib64/*.so pizfix/lib64/*.so.* pizfix/bin/* pizfix/sbin/* pizfix/libexec/* pizfix/lib_test/*.so pizfix/lib_test_gcverify/*.so pizfix/lib_gcverify/*.so
do
    if test ! -L $binary && test $binary != pizfix/lib/libyoloc.so
    then
        if patchelf --set-rpath pizfix/lib64:pizfix/lib $binary
        then
            echo "patchelf --set-rpath \$PWD/pizfix/lib64:\$PWD/pizfix/lib $binary" >> setup.sh
        fi
        if patchelf --set-interpreter pizfix/lib/ld-fil1-$ARCH.so $binary
        then
            echo "patchelf --set-interpreter \$PWD/pizfix/lib/ld-fil1-$ARCH.so $binary" >> setup.sh
        fi
    fi
done

echo "if test -f pizfix/bin/sarcasm" >> setup.sh
echo "then" >> setup.sh
echo "    sed -i \"1s|.*|#!\$PWD/pizfix/bin/minilute|\" pizfix/bin/sarcasm" >> setup.sh
echo "fi" >> setup.sh

rm pizfix/lib/ld-fil1-$ARCH.so
(cd pizfix/lib/ && ln -s libyoloc.so ld-fil1-$ARCH.so)

echo "target_arch=$ARCH" >> setup.sh
cat >> setup.sh <<'EOF'
cd pizfix
mkdir os-include
cd os-include
kernel_include=/usr/include
if test -d /usr/include/$target_arch-linux-gnu/asm
then
    ln -s /usr/include/$target_arch-linux-gnu/asm .
elif test -d /usr/$target_arch-linux-gnu/include/asm
then
    # Use the matching generic headers from the cross header package too.
    kernel_include=/usr/$target_arch-linux-gnu/include
    ln -s $kernel_include/asm .
elif test "`uname -m`" = "$target_arch" && test -d /usr/include/asm
then
    ln -s /usr/include/asm .
else
    echo "warning: no $target_arch kernel headers found; install them (for example linux-libc-dev-<arch>-cross on Debian/Ubuntu) and link their asm directory into pizfix/os-include, or compile with --filc-os-include=<dir>" >&2
fi
ln -s $kernel_include/linux .
ln -s $kernel_include/asm-generic .
cd ../..
EOF

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
echo "echo \"New releases are at: https://github.com/pizlonator/fil-c/releases\"" >> setup.sh
echo "echo \"More information on the website: https://fil-c.org/\"" >> setup.sh
echo 'echo' >> setup.sh
echo "echo \"Have fun and thank you for trying $build_name.\"" >> setup.sh

chmod 755 setup.sh

cd ..

tar -cJvf $build_name.tar.xz $build_name

