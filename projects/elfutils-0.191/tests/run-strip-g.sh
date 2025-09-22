#! /bin/sh
# Copyright (C) 2017 Red Hat, Inc.
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

# When stripping just the debug sections/symbols we keep the symtab
# in the main ELF file. There should be no symbols pointing into the
# debug sections and so there should not be a copy in the debug file
# except for a NOBITS one.

tempfiles a.out strip.out debug.out readelf.out

echo Create debug a.out.
echo "int main() { return 1; }" | ${CC} -g -xc -

echo strip -g to file with debug file
testrun ${abs_top_builddir}/src/strip -g -o strip.out -f debug.out ||
  { echo "*** failed to strip -g -o strip.out -f debug.out a.out"; exit -1; }

status=0
testrun ${abs_top_builddir}/src/readelf -S strip.out > readelf.out
grep SYMTAB readelf.out || status=$?
echo $status
if test $status -ne 0; then
  echo no symtab found in strip.out
  exit 1
fi

status=0
testrun ${abs_top_builddir}/src/readelf -S debug.out > readelf.out
grep SYMTAB readelf.out || status=$?
echo $status
if test $status -ne 1; then
  echo symtab found in debug.out
  exit 1
fi

# arm (with data marker in .debug_frame). See tests/run-addrcfi.sh
testfiles testfilearm

echo arm strip -g to file with debug file
testrun ${abs_top_builddir}/src/strip -g -o strip.out -f debug.out testfilearm ||
  { echo "*** failed to strip -g -o strip.out -f debug.out a.out"; exit -1; }

status=0
testrun ${abs_top_builddir}/src/readelf -S strip.out > readelf.out
grep SYMTAB readelf.out || status=$?
echo $status
if test $status -ne 0; then
  echo no symtab found in strip.out
  exit 1
fi

status=0
testrun ${abs_top_builddir}/src/readelf -S debug.out > readelf.out
grep SYMTAB readelf.out || status=$?
echo $status
if test $status -ne 1; then
  echo symtab found in debug.out
  exit 1
fi

# aarch64 (with data marker in .debug_frame). See tests/run-addrcfi.sh
testfiles testfileaarch64

echo aarch64 strip -g to file with debug file
testrun ${abs_top_builddir}/src/strip -g -o strip.out -f debug.out testfileaarch64 ||
  { echo "*** failed to strip -g -o strip.out -f debug.out a.out"; exit -1; }

status=0
testrun ${abs_top_builddir}/src/readelf -S strip.out > readelf.out
grep SYMTAB readelf.out || status=$?
echo $status
if test $status -ne 0; then
  echo no symtab found in strip.out
  exit 1
fi

status=0
testrun ${abs_top_builddir}/src/readelf -S debug.out > readelf.out
grep SYMTAB readelf.out || status=$?
echo $status
if test $status -ne 1; then
  echo symtab found in debug.out
  exit 1
fi

exit 0
