#!/bin/sh
# Test expected messages generated when mcount overflows
# Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

LC_ALL=C
export LC_ALL
set -e
exec 2>&1

program="$1"

check_msg() {
    if ! grep -q "$1" "$program.out"; then
       echo "FAIL: expected message not in output: $1"
       exit 1
    fi
}

check_msg 'monstartup: maxarcs < minarcs, setting maxarcs = minarcs'
check_msg 'mcount: call graph buffer size limit exceeded, gmon.out will not be generated'

for data_file in $1.data.*; do
  if [ -f "$data_file" ]; then
    echo "FAIL: expected no data files, but found $data_file"
    exit 1
  fi
done

echo PASS
