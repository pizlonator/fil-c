#! /bin/sh
# Copyright (C) 2005 Red Hat, Inc.
# This file is part of elfutils.
# Written by Ulrich Drepper <drepper@redhat.com>, 2005.
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

tempfiles ranlib-test.a ranlib-test.a-copy

cat > ranlib-test.a <<"EOF"
!<arch>
foo/            1124128960  500   500   100664  4         `
foo
bar/            1124128965  500   500   100664  4         `
bar
EOF

cp ranlib-test.a ranlib-test.a-copy

testrun ${abs_top_builddir}/src/ranlib ranlib-test.a

# The ranlib call should not have changed anything.
cmp ranlib-test.a ranlib-test.a-copy

exit 0
