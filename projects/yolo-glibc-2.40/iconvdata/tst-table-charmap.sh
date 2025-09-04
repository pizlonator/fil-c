#!/bin/sh
# Copyright (C) 2000-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#

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

# Converts a glibc format charmap to a simple format .table file.

LC_ALL=C
export LC_ALL

sed -e 's,^%IRREVERSIBLE%,,' | grep '^<U[0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]*>[ 	]*/x' | grep -v 'not a real character' | sed -e 's,<U\(....\)>[ 	]*\([/x0-9A-Fa-f]*\).*$,\2	0x\1,' -e 's,<U0*\([1-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]*\)>[ 	]*\([/x0-9A-Fa-f]*\).*$,\2	0x\1,' | tr abcdef ABCDEF | sed -e 's,/x\([0-9A-F][0-9A-F]\),\1,g' | sed -e 's,^,0x,' | sort | uniq | grep -v '^0x00	0x\([1-9A-F]...\|.[1-9A-F]..\|..[1-9A-F].\|...[1-9A-F]\)'
