#!/bin/sh
#
# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
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
# THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS AND ANY
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

cd projects/perl-5.40.0
extract_source
sh ./Configure -der \
    -Dcc="$PWD/../../../build/bin/clang" \
    -Doptimize="-O3 -g -fno-strict-aliasing -D_GNU_SOURCE" \
    -Dprefix="$PWD/../../../pizfix" \
    -Dinstallprefix="$PWD/../../../pizfix" \
    -Dbin="$PWD/../../../pizfix/bin" \
    -Dprivlib="$PWD/../../../pizfix/lib/perl5/5.40.0" \
    -Darchlib="$PWD/../../../pizfix/lib/perl5/5.40.0/x86_64-linux" \
    -Dsiteprefix="$PWD/../../../pizfix" \
    -Dsitelib="$PWD/../../../pizfix/perl5/site_perl/5.40.0" \
    -Dsitearch="$PWD/../../../pizfix/perl5/site_perl/5.40.0/x86_64-linux" \
    -Dman1dir="$PWD/../../../pizfix/share/man/man1" \
    -Dman3dir="$PWD/../../../pizfix/share/man/man1" \
    -Dscriptdir="$PWD/../../../pizfix/bin" \
    -Dsitehtml1dir="$PWD/../../../pizfix/share/man/man1" \
    -Dsitehtml3dir="$PWD/../../../pizfix/share/man/man3" \
    -Dsitescript="$PWD/../../../pizfix/bin" \
    -Dsitebin="$PWD/../../../pizfix/bin" \
    -Dsiteman1dir="$PWD/../../../pizfix/share/man/man1" \
    -Dsiteman3dir="$PWD/../../../pizfix/share/man/man3" \
    -Dlibpth=' ' \
    -Dglibpth=' ' \
    -Dloclibpth=' ' \
    -Dplibpth=' ' \
    -Dlocincpth=' ' \
    -D usethreads
make -j $NCPU
make -j $NCPU install

# Do my own tests
cd fil-tests
../../../../pizfix/bin/perl threads.pl
../../../../pizfix/bin/perl storable_demo.pl

cd ../../../openssl-3.5.7/extracted-source
../../../pizfix/bin/perl "-I." "-Iproviders/common/der" "-Mconfigdata" "-Mconfigdata" "-Mconfigdata" "-Moids_to_c" "util/dofile.pl" "-oMakefile" providers/common/include/prov/der_digests.h.in

cd ../../..
pizfix/bin/perl projects/curl-8.9.1/src/mkhelp.pl -c < Manifesto.md > /dev/null
