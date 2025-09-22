#! /bin/sh
# Copyright (C) 2014 Red Hat, Inc.
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

# Just some random testfiles, four with, one without .gnu_debugaltlink
testfiles testfile42 testfile_multi_main testfile-dwzstr \
    test-offset-loop libtestfile_multi_shared.so

testrun_compare  ${abs_builddir}/debugaltlink testfile42 \
    testfile_multi_main testfile-dwzstr \
    test-offset-loop libtestfile_multi_shared.so <<\EOF
testfile42: <no .gnu_debugaltlink section>
testfile_multi_main: testfile_multi.dwz, build ID: a0d6c06e0d912d74033b6fe2808753cae8f6f594
testfile-dwzstr: testfile-dwzstr.multi, build ID: 6da22627dae55c1d62cf9122827c665e240a056b
test-offset-loop: test-offset-loop.alt, build ID: 066bbf1a7bc5676f5015ee1966a088f23bdb83ae
libtestfile_multi_shared.so: testfile_multi.dwz, build ID: a0d6c06e0d912d74033b6fe2808753cae8f6f594
EOF

exit 0
