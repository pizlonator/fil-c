#!/bin/sh
# Test basic functionality of sprof
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

set -e

common_objpfx=$1
test_wrapper_env=$2
run_program_env=$3

sprof_mod=tst-sprof-mod.so

${test_wrapper_env} \
${run_program_env} \
LD_PROFILE=$sprof_mod \
LD_PROFILE_OUTPUT=${common_objpfx}elf \
LD_LIBRARY_PATH=${common_objpfx}.:${common_objpfx}elf \
  ${common_objpfx}elf/ld.so ${common_objpfx}elf/tst-sprof-basic

${test_wrapper_env} \
${run_program_env} \
LD_LIBRARY_PATH=${common_objpfx}.:${common_objpfx}elf \
  ${common_objpfx}elf/ld.so ${common_objpfx}elf/sprof -p $sprof_mod \
  ${common_objpfx}elf/${sprof_mod}.profile

exit $?
