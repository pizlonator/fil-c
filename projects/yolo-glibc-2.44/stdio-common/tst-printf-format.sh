#!/bin/bash
# Formatted printf output test script dispatcher.
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

output=${1##*/}; shift

tmp=${output#tst-printf-format-}
tmp=${tmp%.out}

# We are given the name of the make target in $1.  With the common prefix
# and suffix both removed we are left with the inner part, which encodes
# the function under test, the conversion type, and optionally the format
# specifier, all separated with hyphens, i.e. F-T-S or F-T.  Extract them
# and call the script corresponding to the conversion type, passing the
# function under test and any format specifier as arguments.

xprintf=${tmp%%-*}; tmp=${tmp#*-}
conv=${tmp%%-*}; tmp=${tmp#${conv}}
fmt=${tmp#*-}
script=tst-printf-format-$conv.sh

exec ${BASH:-bash} $script $xprintf $fmt "$@"
