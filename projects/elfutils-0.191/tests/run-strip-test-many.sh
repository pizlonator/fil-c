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

status=0

# Use the original file from run-strip-test.sh but with many sections
testfiles testfile
tempfiles testfile1.strip testfile2.strip testfile1.debug testfile2.debug testfile.unstrip

echo "Adding sections to testfile"
testrun ${abs_builddir}/addsections 65535 testfile ||
{ echo "*** failure addsections testfile"; status=1; }

echo "Testing strip -o"
testrun ${abs_top_builddir}/src/strip -o testfile1.strip -f testfile1.debug testfile ||
{ echo "*** failure strip -o"; status=1; }

# Do the parts check out?
echo "elflint testfile1.strip"
testrun ${abs_top_builddir}/src/elflint --gnu -q testfile1.strip ||
{ echo "*** failure elflint testfile1.strip"; status=1; }

echo "elflint testfile1.debug"
testrun ${abs_top_builddir}/src/elflint --gnu -q -d testfile1.debug ||
{ echo "*** failure elflint testfile1.debug"; status=1; }

# Now test unstrip recombining those files.
echo "unstrip"
testrun ${abs_top_builddir}/src/unstrip -o testfile.unstrip testfile1.strip testfile1.debug ||
{ echo "*** failure unstrip"; status=1; }

echo "elfcmp"
testrun ${abs_top_builddir}/src/elfcmp testfile testfile.unstrip ||
{ echo "*** failure elfcmp"; status=1; }

# test strip -g
echo "Testing strip -g"
testrun ${abs_top_builddir}/src/strip -g -o testfile2.strip -f testfile2.debug testfile ||
{ echo "*** failure strip -g"; status=1; }

# Do the parts check out?
echo "elflint testfile2.strip"
testrun ${abs_top_builddir}/src/elflint --gnu -q testfile2.strip ||
{ echo "*** failure elflint testfile2.strip"; status=1; }

echo "elflint testfile2.debug"
testrun ${abs_top_builddir}/src/elflint --gnu -q -d testfile2.debug ||
{ echo "*** failure elflint testfile2.debug"; status=1; }

# Now strip "in-place" and make sure it is smaller.
echo "Testing strip in-place"
SIZE_original=$(stat -c%s testfile)
echo "original size $SIZE_original"

testrun ${abs_top_builddir}/src/strip testfile ||
{ echo "*** failure strip in-place"; status=1; }

SIZE_stripped=$(stat -c%s testfile)
echo "stripped size $SIZE_stripped"
test $SIZE_stripped -lt $SIZE_original ||
  { echo "*** failure in-place strip file not smaller $original"; exit 1; }

echo "elflint in-place"
testrun ${abs_top_builddir}/src/elflint --gnu -q testfile ||
{ echo "*** failure elflint in-place"; status=1; }

exit $status
