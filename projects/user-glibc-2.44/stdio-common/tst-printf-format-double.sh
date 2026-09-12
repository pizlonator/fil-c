#!/bin/bash
# Testing of double printf conversions.
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
format=$1; shift
common_objpfx=$1; shift
test_program_prefix=$1; shift

# For floating-point formats we need to use the bignum mode even if the
# regular mode would do, because GAWK in the latter mode uses sprintf(3)
# internally to process the conversion requested, so any bug in our code
# would then be verified against itself, defeating the objective of doing
# the verification against an independent implementation.
AWK="${AWK:-awk} -M"

status=77

# Verify that AWK can handle the range required.  It also catches:
# "gawk: warning: -M ignored: MPFR/GMP support not compiled in"
# message produced where bignum support is not there, which is the
# only indication as the use of '-M' does not affect the exit status
# in this case.
ref="-1.79769313486231570814527423731704357e+308"
val=$(echo "$ref" | $AWK '{ printf "%.35e\n", $1 }' 2>&1) &&
  test "$val" = "$ref" && status=0

test $status -eq 0 || { echo "No working AWK found" && exit $status; }

# Check for any additional conversions that AWK handles conditionally
# according to its version and/or the environment it has been built in.
# The 'A' and 'a' conversions are not suitable to use at this point, as
# output produced by AWK is different apparently due to a subtlety in
# rounding, so do not try them.
declare -A conversion_disabled
ref="-inf"
for f in f F; do
  conversion_disabled[$f]=true
  val=$(echo "$ref" | $AWK '{ printf "%'$f'\n", $1 }' 2>&1) &&
    test "${val^^}" = "${ref^^}" && unset conversion_disabled[$f]
done

if test "${conversion_disabled[$format]+set}" = set; then
  echo Unsupported $format
  status=77
else
  echo Verifying $format
  (set -o pipefail
   ${test_program_prefix} \
    ${common_objpfx}stdio-common/tst-printf-format-${xprintf}-double $format |
     $AWK -f tst-printf-format.awk 2>&1 |
     head -n 1 |
     sed "s/^/Conversion $format output error, first line:\n/") 2>&1 ||
    status=1
fi

exit $status
