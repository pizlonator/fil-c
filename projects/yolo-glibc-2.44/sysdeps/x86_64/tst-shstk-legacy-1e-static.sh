#!/bin/sh
# Check that legacy shadow stack code in init_array will trigger
# segfault.
# Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

common_objpfx=$1; shift

GLIBC_TUNABLES=glibc.cpu.hwcaps=SHSTK \
${common_objpfx}elf/tst-shstk-legacy-1e-static
# The exit status should only be unsupported (77) or segfault (139).
status=$?
if test $status -eq 77; then
  exit 77
elif test $status -eq 139; then
  exit 0
else
  exit 1
fi
