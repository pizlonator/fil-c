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

case `uname -s` in
    Linux)
        MAKE=make
        CCPREFIX=""
        OS=linux
        ;;
    *)
        echo "Unsupported OS"
        exit 1
        ;;
esac

case `uname -m` in
    amd64|x86_64)
        ARCH=x86_64
        LLVMARCH=X86
        ;;
    arm64|aarch64)
        ARCH=aarch64
        LLVMARCH=AArch64
        ;;
    *)
        echo "Unsupported arch"
        exit 1
        ;;
esac

case $OS in
    linux)
        NCPU=`nproc`
        ;;
    *)
        NCPU=`sysctl -n hw.ncpu`
        ;;
esac

extract_source()
{
    rm -rf extracted-source
    git archive --format=tar HEAD --prefix=extracted-source/ | tar -xf -
    git diff HEAD | (cd extracted-source && patch -p1)
    cd extracted-source
}
