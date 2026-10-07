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

# Figure out which libc flavor the current build uses.  This probes the same
# marker files that the compiler driver and filc/run-tests use: the cosmo
# flavor leaves pizfix/lib/libyolocosmo.a, the glibc flavor leaves
# pizfix/lib/libc.so.6666, and the musl flavor is everything else.
if test ! -d pizfix
then
    echo "pizfix does not exist; run a build first (see build_all_fast.sh)."
    exit 1
fi

if test -f pizfix/lib/libyolocosmo.a
then
    flavor=cosmo
    build_name_base="cosmo-filc"
elif test -f pizfix/lib/libc.so.6666
then
    echo "This is a glibc build.  package-build.sh does not package the glibc" \
         "flavor; glibc Fil-C is distributed as the /opt/fil distribution" \
         "instead (see optfil/build_package.sh).  To package Fil-C, build the" \
         "musl flavor (./build_all_fast.sh) or the cosmo flavor" \
         "(./build_all_fast_cosmo.sh) and run this script again."
    exit 1
else
    flavor=musl
    build_name_base="filc"
fi

# The cosmo APE tooling (pizfix/libexec/apelink and pizfix/libexec/pecheck)
# is only ever built as x86_64 Linux binaries, since it runs on the build
# host, so that is the only host this script knows how to package the cosmo
# flavor on for now.
if test "x$flavor" = "xcosmo" && test "x$ARCH" != "xx86_64"
then
    echo "Packaging the cosmo flavor requires x86_64 for now."
    exit 1
fi

build_name=$build_name_base-0.686-$OS-$ARCH

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
if test "x$flavor" = "xcosmo" && test -d build/include/aarch64-unknown-linux-gnu/c++
then
    # The aarch64 C++ headers, which the driver's nested aarch64 clang
    # invocation needs to build the aarch64 half of a fat APE out of C++
    # sources (it recompiles everything for aarch64, and that compile reads
    # build/include/aarch64-unknown-linux-gnu/c++/v1/__config_site).  Fat APEs
    # are the default in cosmo mode now, so this is needed for every C++ fat
    # APE build, not just for explicit --filc-fat-ape links.
    mkdir -p $build_name/build/include/aarch64-unknown-linux-gnu/
    cp -R build/include/aarch64-unknown-linux-gnu/c++ $build_name/build/include/aarch64-unknown-linux-gnu/
fi
mkdir -p $build_name/build/lib/clang/20/
cp -R build/lib/clang/20/include $build_name/build/lib/clang/20/

cp -R pizfix $build_name/
rm -f $build_name/pizfix/etc/moduli
rm -f $build_name/pizfix/etc/ssh_host*
# pizfix/os-include only contains symlinks into the build host's /usr/include,
# so it never ships; setup.sh recreates it for whoever installs the package.
rm -rf $build_name/pizfix/os-include
if test "x$flavor" = "xmusl"
then
    # The musl flavor does not ship the yolo headers.
    rm -rf $build_name/pizfix/yolo-include
else
    # The cosmo flavor ships pizfix/yolo-include (the cosmo headers that
    # libpas is compiled against, needed by anyone rebuilding the Fil-C
    # runtime from the package).  pizfix/os-include-aarch64 is dropped just
    # like os-include, since it is also just symlinks into the build host's
    # /usr/aarch64-linux-gnu/include; setup.sh recreates it.
    rm -rf $build_name/pizfix/os-include-aarch64
fi

sourcedir=$PWD

cd $build_name

echo '#!/bin/sh' > setup.sh
echo 'set -e' >> setup.sh
echo 'set -x' >> setup.sh

# The musl flavor links dynamically, so every dynamic binary in the package
# needs its rpath and interpreter pointed at the package's own pizfix.  The
# cosmo flavor is static-only (no shared libraries and no dynamic linker), so
# there is nothing to patch there.
if test "x$flavor" = "xmusl"
then
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
fi

echo "if test -f pizfix/bin/sarcasm" >> setup.sh
echo "then" >> setup.sh
echo "    sed -i \"1s|.*|#!\$PWD/pizfix/bin/minilute|\" pizfix/bin/sarcasm" >> setup.sh
echo "fi" >> setup.sh

if test "x$flavor" = "xmusl"
then
    # The musl flavor's dynamic loader lives at this path, but the real file
    # is libyoloc.so (patchelf above already pointed the interpreter at
    # ld-fil1-$ARCH.so); keep a symlink there so that the layout looks normal.
    rm pizfix/lib/ld-fil1-$ARCH.so
    (cd pizfix/lib/ && ln -s libyoloc.so ld-fil1-$ARCH.so)
fi

echo "cd pizfix" >> setup.sh
echo "mkdir os-include" >> setup.sh
echo "cd os-include" >> setup.sh
echo "ln -s /usr/include/linux ." >> setup.sh
echo "if test -d /usr/include/x86_64-linux-gnu/asm" >> setup.sh
echo "then" >> setup.sh
echo "    ln -s /usr/include/x86_64-linux-gnu/asm ." >> setup.sh
echo "else" >> setup.sh
echo "    ln -s /usr/include/asm ." >> setup.sh
echo "fi" >> setup.sh
echo "ln -s /usr/include/asm-generic ." >> setup.sh
echo "cd ../.." >> setup.sh

if test "x$flavor" = "xcosmo"
then
    # The aarch64 kernel headers, exactly like build_yolocosmo.sh creates
    # them in-tree.  They are only recreated when the package actually has
    # the aarch64 cosmo tree (needed for every default fat-APE link, whose
    # nested aarch64 half compiles and links with --target=aarch64-linux-gnu,
    # and for explicit --target=aarch64-linux-gnu links) and when the
    # machine installing the package has the aarch64 kernel headers;
    # otherwise the driver falls back to /usr/aarch64-linux-gnu/include on
    # its own.
    echo "if test -d pizfix/lib-aarch64 && test -d /usr/aarch64-linux-gnu/include/linux" >> setup.sh
    echo "then" >> setup.sh
    echo "    cd pizfix" >> setup.sh
    echo "    mkdir os-include-aarch64" >> setup.sh
    echo "    cd os-include-aarch64" >> setup.sh
    echo "    ln -s /usr/aarch64-linux-gnu/include/linux ." >> setup.sh
    echo "    ln -s /usr/aarch64-linux-gnu/include/asm ." >> setup.sh
    echo "    ln -s /usr/aarch64-linux-gnu/include/asm-generic ." >> setup.sh
    echo "    cd ../.." >> setup.sh
    echo "fi" >> setup.sh
fi

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

if test "x$flavor" = "xcosmo"
then
    echo 'echo "The commands above produce fat APEs (actually portable executables):"' >> setup.sh
    echo 'echo "whatever is one file that runs on both x86_64 and ARM64, on Linux,"' >> setup.sh
    echo 'echo "Windows, macOS, and the BSDs.  Every link also writes two side files"' >> setup.sh
    echo 'echo "next to it: whatever.com.dbg (the x86_64 ELF) and whatever.aarch64.elf"' >> setup.sh
    echo 'echo "(the ARM64 ELF, handy for running the ARM64 half under"' >> setup.sh
    echo 'echo "qemu-aarch64 or in a debugger)."' >> setup.sh
    echo 'echo' >> setup.sh
    echo 'echo "For plain single-architecture output instead (plain ELF executables,"' >> setup.sh
    echo 'echo "objects only where you asked for them, no side files), use"' >> setup.sh
    echo 'echo "--filc-no-ape:"' >> setup.sh
    echo 'echo' >> setup.sh
    echo 'echo "    build/bin/clang --filc-no-ape -o whatever whatever.c"' >> setup.sh
    echo 'echo' >> setup.sh
    echo 'echo "For a single-architecture APE (the APE at whatever, the ELF at"' >> setup.sh
    echo 'echo "whatever.dbg), use --filc-ape:"' >> setup.sh
    echo 'echo' >> setup.sh
    echo 'echo "    build/bin/clang --filc-ape -o whatever whatever.c"' >> setup.sh
    echo 'echo' >> setup.sh
fi

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

