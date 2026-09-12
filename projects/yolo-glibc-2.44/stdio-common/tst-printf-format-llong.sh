#!/bin/bash
# Testing of long long int printf conversions.
# Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

xprintf=$1; shift
common_objpfx=$1; shift
test_program_prefix=$1; shift

AWK=${AWK:-awk}

status=77

# Verify that AWK can handle the range required.  It also catches:
# "gawk: warning: -M ignored: MPFR/GMP support not compiled in"
# message produced where bignum support is not there, which is the
# only indication as the use of '-M' does not affect the exit status
# in this case.
ref="9223372036854775807"
for AWK in "$AWK -M" "$AWK"; do
  val=$(echo "$ref" | $AWK '{ printf "%d\n", $1 }' 2>&1) || continue
  test "$val" = "$ref" && status=0 && break
done

test $status -eq 0 || { echo "No working AWK found" && exit $status; }

for f in d i; do
  echo Verifying $f
  (set -o pipefail
   ${test_program_prefix} \
    ${common_objpfx}stdio-common/tst-printf-format-${xprintf}-llong $f |
     $AWK -f tst-printf-format.awk 2>&1 |
     head -n 1 | sed "s/^/Conversion $f output error, first line:\n/") 2>&1 ||
    status=1
done

exit $status
