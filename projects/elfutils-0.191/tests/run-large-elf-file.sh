#! /usr/bin/env bash
# Copyright (C) 2019 Red Hat, Inc.
# Copyright (C) 2022 Mark J. Wielaard <mark@klomp.org>
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

# Only run on 64bit systems, 32bit systems don't support > 4GB
# ELF files.
long_bit=$(getconf LONG_BIT)
echo "long_bit: $long_bit"
if test $long_bit -ne 64; then
  echo "Only 64bit systems can create > 4GB ELF files"
  exit 77
fi

# The test binary also needs to be 64bits itself
elfclass=64
testrun ${abs_top_builddir}/src/readelf -h ${abs_builddir}/addsections | grep ELF32 \
	&& elfclass=32
echo elfclass: $elfclass
if test $elfclass -ne 64; then
  echo "Only 64bit binaries can create > 4GB ELF files"
  exit 77
fi

# These tests need lots of disk space since they test files > 4GB.
# Skip if there just isn't enough (2.5 * 4 = 10GB).
space_available=$[$(stat -f --format="%a*%S" .)/(1024 * 1024 * 1024)]
echo "space_available: $space_available"
if test $space_available -lt 10; then
  echo "Not enough disk space, need at least 10GB available"
  exit 77
fi

# Make sure the files fit into memory, assume 6GB needed (2.5 * 2 + 1 extra).
# Running under valgrind might need even more.
mem_needed=6
if [ "x$VALGRIND_CMD" != "x" ]; then
  mem_needed=$[${mem_needed} + 2]
fi
echo "mem_needed: $mem_needed"
mem_available=$(free -g 2>/dev/null | grep ^Mem: | awk -F ' +' '{print $7}')
echo "mem_available: $mem_available"
if test -z "$mem_available" || test $mem_available -lt $mem_needed; then
  echo "Need at least ${mem_needed}GB free available memory"
  exit 77
fi

# Make sure the disk is reasonably fast, should be able to write 100MB/s
fast_disk=1
timeout -s9 10s dd conv=fsync if=/dev/zero of=tempfile bs=1M count=1K \
  || fast_disk=0; rm tempfile
if test $fast_disk -eq 0; then
  echo "File system not fast enough, need at least 100MB/s"
  exit 77
fi

# NOTE: test file will be mangled and removed!
test_file ()
{
  in_file="$1"
  readelf_out="${in_file}.readelf.out"
  out_file_strip="${in_file}.strip"
  out_file_debug="${in_file}.debug"

  testfiles ${in_file}
  tempfiles ${readelf_out} ${out_file_mmap} ${out_file_strip} ${out_file_debug}

  # Add two 2GB sections to the file.
  echo "addsections 2 ${in_file} 2147483648"
  testrun ${abs_builddir}/addsections 2 ${in_file} 2147483648
  testrun ${abs_top_builddir}/src/readelf -S ${in_file} > ${readelf_out}
  nr=$(grep '.extra' ${readelf_out} | wc -l)
  if test ${nr} != 2; then
    # Show what went wrong
    cat ${readelf_out}
    exit 1
  fi

  echo "strip -o ${out_file_strip} -f ${out_file_debug} ${in_file}"
  testrun ${abs_top_builddir}/src/strip -o ${out_file_strip} \
                                        -f ${out_file_debug} ${in_file}

  echo "elflint --gnu ${out_file_strip}"
  testrun ${abs_top_builddir}/src/elflint --gnu ${out_file_strip}

  echo "elflint --gnu -d ${out_file_debug}"
  testrun ${abs_top_builddir}/src/elflint --gnu -d ${out_file_debug}

  # Now test unstrip recombining those files.
  echo "unstrip ${out_file_strip} ${out_file_debug}"
  testrun ${abs_top_builddir}/src/unstrip ${out_file_strip} ${out_file_debug}

  echo "elfcmp ${out_file} ${out_file_strip}"
  testrun ${abs_top_builddir}/src/elfcmp ${in_file} ${out_file_debug}

  # Remove the temp files immediately, they are big...
  rm -f ${in_file} ${out_file_strip} ${out_file_debug}
}

# A collection of random testfiles to test 64bit, little/big endian
# and non-ET_REL (with phdrs)/ET_REL (without phdrs).
# Don't test 32bit, they cannot go beyond 4GB.

# 64bit, little endian, rel
test_file testfile38

# 64bit, big endian, non-rel
test_file testfile27

# See testfile-dwp-cu-index-overflow.source
testfiles testfile-dwp-5-cu-index-overflow testfile-dwp-5-cu-index-overflow.dwp

testrun_compare ${abs_builddir}/cu-dwp-section-info testfile-dwp-5-cu-index-overflow.dwp << EOF
file: testfile-dwp-5-cu-index-overflow.dwp
INFO: 0x0 0x8000004c
TYPES: 0x0 0x0
ABBREV: 0x0 0x50
LINE: 0x0 0x61
LOCLISTS: 0x0 0x0
STR_OFFSETS: 0x0 0x1c
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x8000004c 0x6f
TYPES: 0x0 0x0
ABBREV: 0x50 0x15e
LINE: 0x61 0x63
LOCLISTS: 0x0 0xd4
STR_OFFSETS: 0x1c 0x24
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x22

INFO: 0x800000bb 0xff
TYPES: 0x0 0x0
ABBREV: 0x50 0x15e
LINE: 0x61 0x63
LOCLISTS: 0x0 0xd4
STR_OFFSETS: 0x1c 0x24
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x22

INFO: 0x800001ba 0x8000004c
TYPES: 0x0 0x0
ABBREV: 0x1ae 0x50
LINE: 0xc4 0x61
LOCLISTS: 0x0 0x0
STR_OFFSETS: 0x40 0x1c
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x100000206 0x6c
TYPES: 0x0 0x0
ABBREV: 0x1fe 0xc8
LINE: 0x125 0x63
LOCLISTS: 0x0 0x0
STR_OFFSETS: 0x5c 0x20
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x100000272 0x6f
TYPES: 0x0 0x0
ABBREV: 0x1fe 0xc8
LINE: 0x125 0x63
LOCLISTS: 0x0 0x0
STR_OFFSETS: 0x5c 0x20
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x1000002e1 0x182
TYPES: 0x0 0x0
ABBREV: 0x2c6 0x188
LINE: 0x188 0x65
LOCLISTS: 0xd4 0xee
STR_OFFSETS: 0x7c 0x44
MACRO: 0x0 0x0
RNGLISTS: 0x22 0x43

EOF

testrun_compare ${abs_builddir}/get-units-split testfile-dwp-5-cu-index-overflow << EOF
file: testfile-dwp-5-cu-index-overflow
Got cudie unit_type: 4
Found a skeleton unit, with split die: filler1.cc
Got cudie unit_type: 4
Found a skeleton unit, with split die: foo.cc
Got cudie unit_type: 4
Found a skeleton unit, with split die: filler2.cc
Got cudie unit_type: 4
Found a skeleton unit, with split die: bar.cc
Got cudie unit_type: 4
Found a skeleton unit, with split die: main.cc

EOF

rm -f testfile-dwp-5-cu-index-overflow testfile-dwp-5-cu-index-overflow.dwp

# See testfile-dwp-cu-index-overflow.source
testfiles testfile-dwp-4-cu-index-overflow testfile-dwp-4-cu-index-overflow.dwp

testrun_compare ${abs_builddir}/cu-dwp-section-info testfile-dwp-4-cu-index-overflow.dwp << EOF
file: testfile-dwp-4-cu-index-overflow.dwp
INFO: 0x0 0x8000004b
TYPES: 0x0 0x0
ABBREV: 0x0 0x58
LINE: 0x0 0x2c
LOCLISTS: 0x0 0x0
STR_OFFSETS: 0x0 0x14
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x8000004b 0x116
TYPES: 0x0 0x0
ABBREV: 0x58 0x16f
LINE: 0x2c 0x34
LOCLISTS: 0x0 0x110
STR_OFFSETS: 0x14 0x1c
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x80000161 0x8000004b
TYPES: 0x0 0x0
ABBREV: 0x1c7 0x58
LINE: 0x60 0x2c
LOCLISTS: 0x0 0x0
STR_OFFSETS: 0x30 0x14
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x1000001ac 0x6e
TYPES: 0x0 0x0
ABBREV: 0x21f 0xd4
LINE: 0x8c 0x34
LOCLISTS: 0x0 0x0
STR_OFFSETS: 0x44 0x18
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x10000021a 0x1b5
TYPES: 0x0 0x0
ABBREV: 0x2f3 0x19b
LINE: 0xc0 0x35
LOCLISTS: 0x110 0x12a
STR_OFFSETS: 0x5c 0x3c
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x0 0x0
TYPES: 0x0 0x6e
ABBREV: 0x58 0x16f
LINE: 0x2c 0x34
LOCLISTS: 0x0 0x110
STR_OFFSETS: 0x14 0x1c
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

INFO: 0x0 0x0
TYPES: 0x6e 0x6b
ABBREV: 0x21f 0xd4
LINE: 0x8c 0x34
LOCLISTS: 0x0 0x0
STR_OFFSETS: 0x44 0x18
MACRO: 0x0 0x0
RNGLISTS: 0x0 0x0

EOF

testrun_compare ${abs_builddir}/get-units-split testfile-dwp-4-cu-index-overflow << EOF
file: testfile-dwp-4-cu-index-overflow
Got cudie unit_type: 4
Found a skeleton unit, with split die: filler1.cc
Got cudie unit_type: 4
Found a skeleton unit, with split die: foo.cc
Got cudie unit_type: 4
Found a skeleton unit, with split die: filler2.cc
Got cudie unit_type: 4
Found a skeleton unit, with split die: bar.cc
Got cudie unit_type: 4
Found a skeleton unit, with split die: main.cc

EOF

rm -f testfile-dwp-4-cu-index-overflow testfile-dwp-4-cu-index-overflow.dwp

exit 0
