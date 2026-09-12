#!/bin/sh
# Check that running ld.so with a non-existing file prints an error.
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

export LC_ALL=C

# --inhibit-cache to suppress "No such file or directory" message when
# /etc/ld.so.cache does not exist.
rtld="$1 --inhibit-cache"
tmp_out="$(mktemp)"

$rtld program-does-not-exist 2>"$tmp_out"
status=$?
echo "$rtld exit status: $status"
echo "output (with expected error):"
cat "$tmp_out"

if test $status -eq 127 \
	&& grep -q "^program-does-not-exist: error while loading shared libraries: program-does-not-exist: cannot open shared object file$" "$tmp_out" \
	&& test "$(wc -l < "$tmp_out")" -eq 1 ; then
    status=0
else
    status=1
fi
rm "$tmp_out"
exit $status
