#! /bin/sh
# Copyright (C) 2013 Red Hat, Inc.
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

testfiles testfile-dwfl-report-elf-align-shlib.so

# /proc/PID/maps when the process was running:
# 7f3560c92000-7f3560c93000 r-xp 00000000 fd:02 25037063 testfile-dwfl-report-elf-align-shlib.so
# 7f3560c93000-7f3560e92000 ---p 00001000 fd:02 25037063 testfile-dwfl-report-elf-align-shlib.so
# 7f3560e92000-7f3560e93000 rw-p 00000000 fd:02 25037063 testfile-dwfl-report-elf-align-shlib.so
# testfile-dwfl-report-elf-align-shlib.so:
# Program Headers:
#   Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
#   LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x00065c 0x00065c R E 0x200000
#   LOAD           0x000660 0x0000000000200660 0x0000000000200660 0x0001f0 0x000200 RW  0x200000
# Symbol table '.dynsym' contains 12 entries:
#    Num:    Value          Size Type    Bind   Vis      Ndx Name
#      8: 000000000000057c    11 FUNC    GLOBAL DEFAULT   11 shlib
# GDB output showing proper relocation:
# #1  0x00007f3560c92585 in shlib () from ./testfile-dwfl-report-elf-align-shlib.so
#
# 0x7f3560c92000 is VMA address of first byte of testfile-dwfl-report-elf-align-shlib.so.
# 0x7f3560c92585 = 0x7f3560c92000 + 0x585
# where 0x585 is any address inside the shlib function: 0x57c .. 0x57c + 11 -1

testrun ${abs_builddir}/dwfl-report-elf-align ./testfile-dwfl-report-elf-align-shlib.so \
				0x7f3560c92000 0x7f3560c92585 shlib

exit 0
