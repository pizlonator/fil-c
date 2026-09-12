#!/bin/sh
# Test if $ORIGIN works correctly with symlinks (BZ 25263)
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

objpfx=$1
test_wrapper_env=$2
run_program_env=$3
library_path=$4
test_program=$5
test_library=$6

cleanup()
{
  # Move the binary and library back to build directory
  mv $tmpdir/sub/$test_program ${objpfx}elf
  mv $tmpdir/sub/$test_library ${objpfx}elf

  rm -rf $tmpdir
}

tmpdir=$(mktemp -d "${objpfx}elf/tst-origin.XXXXXXXXXX")
trap cleanup 0

mkdir ${tmpdir}/sub

# Remove the dependency from $library_path
mv ${objpfx}elf/$test_program  $tmpdir/sub
mv ${objpfx}elf/$test_library  $tmpdir/sub

cd ${tmpdir}
ln -s sub/$test_program $test_program

${test_wrapper_env} \
${run_program_env} \
${objpfx}elf/ld.so --library-path "$library_path" \
  ./$test_program 2>&1 && rc=0 || rc=$?

# Also check if ldd resolves the dependency
LD_TRACE_LOADED_OBJECTS=1 \
${objpfx}elf/ld.so --library-path "$library_path" \
  ./$test_program 2>&1 | grep 'not found' && rc=1 || rc=0

exit $rc
