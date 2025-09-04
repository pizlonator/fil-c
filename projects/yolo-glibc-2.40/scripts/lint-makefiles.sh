#!/bin/bash
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

# This script checks to see that all Makefiles in the source tree
# conform to the sorted variable rules as defined by:
# scripts/sort-makefile-lines.py.
# Any difference is an error and should be corrected e.g. the lines
# reordered to sort correctly.
# The intent with this check is to ensure that changes made by
# developers match the expected format for the project.

export LC_ALL=C

tmpfile="$(mktemp)"

cleanup () {
  rm -f -- "$tmpfile"
}

trap cleanup 0

PYTHON=$1
# Absolute or relative path to the source directory.
srcdir=$2

# Must specify $PYTHON.
if [ -z "$PYTHON" ]; then
  echo 'Please specify $PYTHON interpreter'
  exit 1
fi
# Absolute or relative $srcdir must exist and be a directory.
if [ ! -d "$srcdir" ]; then
  echo 'Please specify $srcdir in which to look for Makefiles'
  exit 1
fi

linted=0
failed=0
for mfile in `find "$srcdir" -name Makefile`; do
    $PYTHON "${srcdir}/scripts/sort-makefile-lines.py" < "$mfile" > "$tmpfile"
    # Printed the expected -> actual difference on error.
    if ! diff -u --label "$mfile.expected" "$tmpfile" "$mfile"; then
        failed=$((failed+1))
    fi
    linted=$((linted+1))
done
# Must have linted at least the top-level Makefile.
if [ $linted -lt 1 ]; then
  echo "Did not lint any Makefiles!"
  exit 1
fi
if [ $failed -gt 0 ]; then
  echo "---"
  echo "Tested $linted Makefiles and $failed were incorrectly sorted"
  echo 'Please use `patch -R -pN` and the output above to correct the sorting'
  exit 1
fi
# All Makefiles linted clean.
exit 0
