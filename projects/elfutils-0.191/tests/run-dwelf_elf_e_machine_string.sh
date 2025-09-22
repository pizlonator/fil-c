#! /usr/bin/env bash
# Test to make sure all EM values in elf.h are recognized
# Copyright (C) 2019 Red Hat, Inc.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. $srcdir/test-subr.sh

# Get all known EM values from elf.h and pass them through the
# preprocessor to get the numbers. Call dwelf_elf_e_machine_string on
# all of them.
EM_VALUES=$(grep ^\#define\ EM_ ${abs_srcdir}/../libelf/elf.h \
            | cut -f2 -d\  | cut -f1 | grep -v ^EM_NUM$ | xargs echo)
# echo "EM_VALUES: $EM_VALUES"
EM_NUMBERS=$((cat ${abs_srcdir}/../libelf/elf.h; echo "$EM_VALUES") \
             | gcc -E - | tail -1)
# echo "EM_NUMBERS: $EM_NUMBERS"

testrun ${abs_top_builddir}/tests/dwelf_elf_e_machine_string $EM_NUMBERS
