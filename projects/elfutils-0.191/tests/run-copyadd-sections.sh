#! /bin/sh
# Copyright (C) 2018 Red Hat, Inc.
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

test_copy_and_add ()
{
  in_file="$1"
  out_file="${in_file}.copy"
  out_file_mmap="${out_file}.mmap"

  testfiles ${in_file}
  tempfiles ${out_file} ${out_file_mmap} readelf.out

  # Can we copy the file?
  testrun ${abs_builddir}/elfcopy ${in_file} ${out_file}
  testrun ${abs_top_builddir}/src/elfcmp ${in_file} ${out_file}

  # Can we add a section (in-place)?
  testrun ${abs_builddir}/addsections 3 ${out_file}
  testrun ${abs_top_builddir}/src/readelf -S ${out_file} > readelf.out
  nr=$(grep '.extra' readelf.out | wc -l)
  if test ${nr} != 3; then
    # Show what went wrong
    testrun ${abs_top_builddir}/src/readelf -S ${out_file}
    exit 1
  fi

  # Can we add a section (in-place) using ELF_C_WRITE_MMAP?
  testrun ${abs_builddir}/elfcopy --mmap ${in_file} ${out_file_mmap}
  testrun ${abs_top_builddir}/src/elfcmp ${in_file} ${out_file_mmap}

  # Can we add a section (in-place) using ELF_C_RDWR_MMAP?
  # Note we are only adding one sections, adding more might fail
  # because mremap cannot extend too much.
  testrun ${abs_builddir}/addsections --mmap 1 ${out_file_mmap}
  testrun ${abs_top_builddir}/src/readelf -S ${out_file_mmap} > readelf.out
  nr=$(grep '.extra' readelf.out | wc -l)
  if test ${nr} != 1; then
    # Show what went wrong
    testrun ${abs_top_builddir}/src/readelf -S ${out_file_mmap}
    exit 1
  fi
}

# A collection of random testfiles to test 32/64bit, little/big endian
# and non-ET_REL (with phdrs)/ET_REL (without phdrs).

# 32bit, big endian, rel
test_copy_and_add testfile29

# 64bit, big endian, rel
test_copy_and_add testfile23

# 32bit, little endian, rel
test_copy_and_add testfile9

# 64bit, little endian, rel
test_copy_and_add testfile38

# 32bit, big endian, non-rel
test_copy_and_add testfile26

# 64bit, big endian, non-rel
test_copy_and_add testfile27

# 32bit, little endian, non-rel
test_copy_and_add testfile

# 64bit, little endian, non-rel
test_copy_and_add testfile10

exit 0
