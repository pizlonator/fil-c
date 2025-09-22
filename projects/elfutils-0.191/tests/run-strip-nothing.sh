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

# If there is nothing to strip then -o output should be identical to input.
# And there should not be an (empty) -f debug file.

tempfiles a.out strip.out debug.out

# Create no-debug a.out.
echo "int main() { return 1; }" | ${CC} -s -xc -

# strip to file
testrun ${abs_top_builddir}/src/strip -g -o strip.out ||
  { echo "*** failed to strip -g -o strip.out a.out"; exit -1; }

testrun ${abs_top_builddir}/src/elfcmp a.out strip.out ||
  { echo "*** failed strip.out different from a.out"; exit -1; }

# strip original
testrun ${abs_top_builddir}/src/strip -g ||
  { echo "*** failed to strip -g a.out"; exit -1; }

testrun ${abs_top_builddir}/src/elfcmp strip.out a.out ||
  { echo "*** failed a.out different from strip.out"; exit -1; }

# strip to file with debug file
testrun ${abs_top_builddir}/src/strip -g -o strip.out -f debug.out ||
  { echo "*** failed to strip -g -o strip.out -f debug.out a.out"; exit -1; }

testrun ${abs_top_builddir}/src/elfcmp a.out strip.out ||
  { echo "*** failed strip.out different from a.out (with debug)"; exit -1; }

test ! -f debug.out ||
  { echo "*** failed strip.out and debug.out exist"; exit -1; }

# strip original with debug file
testrun ${abs_top_builddir}/src/strip -g -f debug.out ||
  { echo "*** failed to strip -g -f debug.out a.out"; exit -1; }

testrun ${abs_top_builddir}/src/elfcmp strip.out a.out ||
  { echo "*** failed a.out different from strip.out (with debug)"; exit -1; }

test ! -f debug.out ||
  { echo "*** failed a.out and debug.out exist"; exit -1; }

exit 0
