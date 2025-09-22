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

# Just some random testfiles, four with, one without build-id,
# and one without shdrs forcing reading the notes through phdrs.
# eu-strip --strip-sections -g --output=testfile42_noshdrs testfile42
# See also run-debugaltlink.sh.
testfiles testfile42 testfile_multi.dwz testfile-dwzstr.multi \
    test-offset-loop.alt testfile14 testfile42_noshdrs

testrun_compare  ${abs_builddir}/buildid testfile42 testfile42_noshdrs \
    testfile_multi.dwz testfile-dwzstr.multi \
    test-offset-loop.alt testfile14 <<\EOF
testfile42: build ID: d826d96c4d097bdc5c254b1f7344a907e36b0439
testfile42_noshdrs: build ID: d826d96c4d097bdc5c254b1f7344a907e36b0439
testfile_multi.dwz: build ID: a0d6c06e0d912d74033b6fe2808753cae8f6f594
testfile-dwzstr.multi: build ID: 6da22627dae55c1d62cf9122827c665e240a056b
test-offset-loop.alt: build ID: 066bbf1a7bc5676f5015ee1966a088f23bdb83ae
testfile14: <no NT_GNU_BUILD_ID note>
EOF

exit 0
