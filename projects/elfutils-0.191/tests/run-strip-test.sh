#! /bin/sh
# Copyright (C) 1999, 2000, 2002, 2003, 2005, 2007, 2008 Red Hat, Inc.
# This file is part of elfutils.
# Written by Ulrich Drepper <drepper@redhat.com>, 1999.
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

original=${original:-testfile11}
stripped=${stripped:-testfile7}
debugout=${debugfile:+-f testfile.debug.temp -F $debugfile}

testfiles $original
test x$stripped = xtestfile.temp || testfiles $stripped $debugfile

tempfiles testfile.temp testfile.debug.temp testfile.unstrip

testrun ${abs_top_builddir}/src/strip -o testfile.temp $debugout $original

status=0

cmp $stripped testfile.temp || status=$?

# Check elflint and the expected result.
testrun ${abs_top_builddir}/src/elflint --gnu -q testfile.temp || status=$?

test -z "$debugfile" || {
cmp $debugfile testfile.debug.temp || status=$?

# Check elflint and the expected result.
testrun ${abs_top_builddir}/src/elflint --gnu -q -d testfile.debug.temp || status=$?

# Now test unstrip recombining those files.
testrun ${abs_top_builddir}/src/unstrip -o testfile.unstrip testfile.temp testfile.debug.temp

# Check that it came back whole.
testrun ${abs_top_builddir}/src/elfcmp --hash-inexact $original testfile.unstrip
}

# test strip -g
testrun ${abs_top_builddir}/src/strip -g -o testfile.temp $original

# Buggy eu-strip created multiple .shstrtab sections
shstrtab_SECS=$(testrun ${abs_top_builddir}/src/readelf -S testfile.temp | grep '.shstrtab' | wc --lines)
test $shstrtab_SECS -eq 1 ||
  { echo "*** failure not just one '.shstrtab' testfile.temp ($shstrtab_SECS)"; status=1; }

# Now strip in-place and make sure it is smaller.
SIZE_original=$(stat -c%s $original)
testrun ${abs_top_builddir}/src/strip $original
SIZE_stripped=$(stat -c%s $original)
test $SIZE_stripped -lt $SIZE_original ||
  { echo "*** failure in-place strip file not smaller $original"; status=1; }

tempfiles testfile.sections
testrun ${abs_top_builddir}/src/readelf -S testfile.temp > testfile.sections || status=$?
grep -F ' .debug_' testfile.sections && status=1

exit $status
