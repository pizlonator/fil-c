#! /bin/sh
# Test for --debug-dump=info+ and finding split unit (in wrong file).
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

# see tests/testfile-dwarf-45.source
testfiles testfile-splitdwarf-5
testfiles testfile-world5.dwo

# note, wrong file, renamed as if this contains the correct dwo...
tempfiles testfile-hello5.dwo
cp testfile-world5.dwo testfile-hello5.dwo

testrun ${abs_top_builddir}/src/readelf --debug-dump=info+ testfile-splitdwarf-5

exit 0
