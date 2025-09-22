#! /bin/sh
# Copyright (C) 2011, 2013 Red Hat, Inc.
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

if test -n "$ELFUTILS_MEMORY_SANITIZER"; then
  echo "binaries linked with memory sanitizer are too big"
  exit 77
fi

tempfiles readelf.out1 readelf.out2
tempfiles out.stripped1 out.debug1 out.stripped2 out.debug2

runtest_status=0
runtest() {
  infile=$1
  is_ET_REL=$2
  outfile1=out.stripped1
  debugfile1=out.debug1
  outfile2=out.stripped2
  debugfile2=out.debug2

  echo "runtest $infile"

  rm -f $outfile1 $debugfile1 $outfile2 $debugfile2

  testrun ${abs_top_builddir}/src/strip -o $outfile1 -f $debugfile1 $infile ||
  { echo "*** failure strip $infile"; runtest_status=1; }

  testrun ${abs_top_builddir}/src/strip --reloc-debug-sections -o $outfile2 \
	-f $debugfile2 $infile ||
  { echo "*** failure strip --reloc-debug-sections $infile"; runtest_status=1; }

  testrun ${abs_top_builddir}/src/elfcmp $outfile1 $outfile2 ||
  { echo "*** failure compare stripped files $infile"; runtest_status=1; }

  # debug files however should be smaller, when ET_REL.
  SIZE1=$(stat -c%s $debugfile1)
  SIZE2=$(stat -c%s $debugfile2)
  test \( \( $is_ET_REL -eq 1 \) -a \( $SIZE1 -gt $SIZE2 \) \) \
	-o \( \( $is_ET_REL -eq 0 \) -a \( $SIZE1 -eq $SIZE2 \) \) ||
  { echo "*** failure --reloc-debug-sections not smaller $infile"; runtest_status=1; }

  # Strip of DWARF section lines, offset will not match.
  # Everything else should match.
  testrun ${abs_top_builddir}/src/readelf -N -w $debugfile1 \
	| grep -v ^DWARF\ section > readelf.out1 ||
  { echo "*** failure readelf -N -w debugfile1 $infile"; runtest_status=1; }

  testrun ${abs_top_builddir}/src/readelf -N -w $debugfile2 \
	| grep -v ^DWARF\ section > readelf.out2 ||
  { echo "*** failure readelf -N -w debugfile2 $infile"; runtest_status=1; }

  testrun_compare cat readelf.out1 < readelf.out2 ||
  { echo "*** failure readelf -N -w compare $infile"; runtest_status=1; }

  testrun ${abs_top_builddir}/src/strip --reloc-debug-sections-only \
	  $debugfile1 ||
  { echo "*** failure strip --reloc-debug-sections-only $debugfile1"; \
    runtest_status=1; }

  cmp $debugfile1 $debugfile2 ||
  { echo "*** failure --reloc-debug-sections[-only] $debugfile1 $debugfile2"; \
    runtest_status=1; }
}
