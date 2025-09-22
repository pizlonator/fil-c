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

# See run-typeiter.sh
testfiles testfile-debug-types

testrun ${abs_builddir}/get-units-invalid testfile-debug-types

# see run-readelf-dwz-multi.sh
testfiles testfile_multi_main testfile_multi.dwz

testrun ${abs_builddir}/get-units-invalid testfile_multi_main

# see tests/run-dwflsyms.sh
testfiles testfilebazdbgppc64.debug

testrun ${abs_builddir}/get-units-invalid testfilebazdbgppc64.debug

# see tests/testfile-dwarf-45.source
testfiles testfile-dwarf-4 testfile-dwarf-5

testrun ${abs_builddir}/get-units-invalid testfile-dwarf-4
testrun ${abs_builddir}/get-units-invalid testfile-dwarf-5

# Self test
testrun_on_self ${abs_builddir}/get-units-invalid

exit 0
