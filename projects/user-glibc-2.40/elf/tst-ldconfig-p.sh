#!/bin/sh
# Test that ldconfig -p prints something useful.
# Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

# Check that the newly built ldconfig -p can dump the system
# /etc/ld.so.cache file.  This should always work even if the ABIs are
# not compatible, except in a cross-endian build (that presumably
# involves emulation when running ldconfig).

common_objpfx=$1
sysconfdir=$2
test_wrapper_env=$3
run_program_env=$4

if ! test -r "${sysconfdir}/ld.so.cache"; then
    echo "warning: ${sysconfdir}/ld.so.cache does not exist, test skipped"
    exit 77
fi

testout="${common_objpfx}elf/tst-ldconfig-p.out"
# Truncate file.
: > "$testout"

${test_wrapper_env} \
${run_program_env} \
${common_objpfx}elf/ldconfig -p \
  $testroot/lib >>"$testout" 2>>"$testout"
status=$?
echo "info: ldconfig exit status: $status" >>"$testout"

errors=0
case $status in
    (0)
	if head -n 1 "$testout" | \
		grep -q "libs found in cache \`${sysconfdir}/ld.so.cache'\$" ; then
	    echo "info: initial string found" >>"$testout"
	else
	    echo "error: initial string not found" >>"$testout"
	    errors=1
	fi
	if grep -q "^	libc\.so\..* => " "$testout"; then
	    echo "info: libc.so.* string found" >>"$testout"
	else
	    echo "error: libc.so.* string not found" >>"$testout"
	    errors=1
	fi
	;;
    (1)
	if head -n 1 "$testout" | \
		grep -q ": Cache file has wrong endianness\.$" ; then
	    echo "info: cache file has wrong endianness" >> "$testout"
	else
	    echo "error: unexpected ldconfig error message" >> "$testout"
	    errors=1
	fi
	;;
    (*)
	echo "error: unexpected exit status" >> "$testout"
	errors=1
	;;
esac

exit $errors
