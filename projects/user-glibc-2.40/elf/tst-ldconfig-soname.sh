#!/bin/sh
# Test that ldconfig creates symlinks according to the library's soname
# (and in particular, does not create symlinks for libraries without a soname)
# Copyright (C) 2000-2024 Free Software Foundation, Inc.
# Copyright The GNU Toolchain Authors.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

set -ex

common_objpfx=$1
test_wrapper_env=$2
run_program_env=$3

testroot="${common_objpfx}elf/bug30125-test-directory"
cleanup () {
    rm -rf "$testroot"
}
trap cleanup 0

rm -rf "$testroot"
mkdir -p $testroot/lib
cp "${common_objpfx}elf/tst-ldconfig-soname-lib-with-soname.so" \
    $testroot/lib/libtst-ldconfig-soname-lib-with-soname.so.1.2.3
cp "${common_objpfx}elf/tst-ldconfig-soname-lib-without-soname.so" \
    $testroot/lib/libtst-ldconfig-soname-lib-without-soname.so.1.2.3

${test_wrapper_env} \
${run_program_env} \
${common_objpfx}elf/ldconfig -vn $testroot/lib

LINKS=$(cd $testroot/lib && find . -type l)
if [ "$LINKS" != "./libtst-ldconfig-soname-lib-with-soname.so.1" ]; then
    echo "error: $0 - extra symlinks found"
    exit 1
fi
