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

# see tests/testfile-dwarf-45.source
testfiles testfile-splitdwarf-4 testfile-hello4.dwo testfile-world4.dwo
testfiles testfile-splitdwarf-5 testfile-hello5.dwo testfile-world5.dwo

testrun_compare ${abs_builddir}/attr-integrate-skel testfile-splitdwarf-4 << EOF
file: testfile-splitdwarf-4
Split DIE: hello.c
addr_base secoff: 0x0
low_pc addr: 0x401160
Skel has high_pc.

Split DIE: world.c
addr_base secoff: 0x98
low_pc addr: 0x0
Skel has ranges.

EOF

testrun_compare ${abs_builddir}/attr-integrate-skel testfile-splitdwarf-5 << EOF
file: testfile-splitdwarf-5
Split DIE: hello.c
addr_base secoff: 0x8
low_pc addr: 0x401160
Skel has high_pc.

Split DIE: world.c
addr_base secoff: 0xa8
low_pc addr: 0x0
Skel has ranges.

EOF

exit 0
