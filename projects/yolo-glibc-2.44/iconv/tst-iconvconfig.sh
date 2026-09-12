#!/bin/bash
# Check if iconvconfig correctly handle config links (BZ 32339)
# Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

set -e

build_dir=$1
test_wrapper_env="$2"
run_program_env="$3"
# Remove the last space to allow concatenate extra paths.
library_path="$(echo $4)"

# We have to have some directories in the library path.
LIBPATH=$build_dir:$build_dir/iconvdata

ICONVCONFIG="
$build_dir/elf/ld.so --library-path $library_path:$LIBPATH $build_dir/iconv/iconvconfig
"

ICONVCONFIG="$test_wrapper_env $run_program_env $ICONVCONFIG"

TIMEOUTFACTOR=${TIMEOUTFACTOR:-1}

tmpdir=$(mktemp -d $build_dir/iconv/tst-iconvconfig.XXXXXX)
#trap 'rm -fr $tmpdir' 0 1 2 3 15 EXIT

touch $tmpdir/gconv-modules-extra.conf
mkdir $tmpdir/gconv-modules.d
cd $tmpdir/gconv-modules.d && ln -s ../gconv-modules-extra.conf . && cd -

$ICONVCONFIG --nostdlib $tmpdir -o $tmpdir/gconv-modules.cache

[ ! -e $tmpdir/tmpdir/gconv-modules.cache ] || exit 1

exit 0
