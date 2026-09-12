#!/bin/sh
# Test wrapper for AArch64 tests for GCS that are expected to abort.
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

objpfx=$1; shift
tstname=$1; shift
tstrun=$1; shift

logfile=$objpfx/$tstname.out

rm -vf $logfile
touch $logfile

${tstrun} 2>> $logfile >> $logfile; status=$?

if test $status -eq 127 \
  && grep -q -w "not GCS compatible" "$logfile" ; then
  exit 0
elif test $status -eq 77; then
  exit 77
else
  echo "unexpected test output or exit status $status"
  exit 1
fi
