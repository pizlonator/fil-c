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

testrun ${abs_builddir}/unit-info testfile-debug-types

# see run-readelf-dwz-multi.sh
testfiles testfile_multi_main testfile_multi.dwz

testrun ${abs_builddir}/unit-info testfile_multi_main

# see tests/run-dwflsyms.sh
testfiles testfilebazdbgppc64.debug

testrun ${abs_builddir}/unit-info testfilebazdbgppc64.debug

# see tests/testfile-dwarf-45.source
testfiles testfile-dwarf-4 testfile-dwarf-5
testfiles testfile-splitdwarf-4 testfile-splitdwarf-5
testfiles testfile-hello4.dwo testfile-hello5.dwo
testfiles testfile-world4.dwo testfile-world5.dwo

testrun ${abs_builddir}/unit-info testfile-dwarf-4
testrun ${abs_builddir}/unit-info testfile-dwarf-5

# The consistency checks should find most issue, but make sure the
# output is also what we expect in case we break dwarf_get_units and
# dwarf_cu_info at the same time.
testrun_compare ${abs_builddir}/unit-info \
		testfile-splitdwarf-4 testfile-splitdwarf-5 <<\EOF
file: testfile-splitdwarf-4
Iterate getting all info, compare with dwarf_cu_info.
0 cu dietag: 11, subtag: 11, version 4, unit_type 4
0 subdietag: 11, subtag: 0, version 4, unit_type 5
1 cu dietag: 11, subtag: 11, version 4, unit_type 4
1 subdietag: 11, subtag: 0, version 4, unit_type 5
rechecking: testfile-splitdwarf-4
Iterate no info, compare recorded info with dwarf_cu_info.
0 re dietag: 11, subtag: 11, version 4, unit_type 4
0 subdietag: 11, subtag: 0, version 4, unit_type 5
1 re dietag: 11, subtag: 11, version 4, unit_type 4
1 subdietag: 11, subtag: 0, version 4, unit_type 5

file: testfile-splitdwarf-5
Iterate getting all info, compare with dwarf_cu_info.
0 cu dietag: 4a, subtag: 11, version 5, unit_type 4
0 subdietag: 11, subtag: 0, version 5, unit_type 5
1 cu dietag: 4a, subtag: 11, version 5, unit_type 4
1 subdietag: 11, subtag: 0, version 5, unit_type 5
rechecking: testfile-splitdwarf-5
Iterate no info, compare recorded info with dwarf_cu_info.
0 re dietag: 4a, subtag: 11, version 5, unit_type 4
0 subdietag: 11, subtag: 0, version 5, unit_type 5
1 re dietag: 4a, subtag: 11, version 5, unit_type 4
1 subdietag: 11, subtag: 0, version 5, unit_type 5

EOF

# Self test (not on obj files, since those need relocation first).
testrun_on_self_exe ${abs_builddir}/unit-info
testrun_on_self_lib ${abs_builddir}/unit-info

exit 0
