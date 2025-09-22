#! /bin/sh
# Copyright (C) 2006 Red Hat, Inc.
# Written by Ulrich Drepper <drepper@redhat.com>, 2006.
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

testfiles testfile28 testfile28.rdwr

testrun ${abs_builddir}/rdwrmmap testfile28

cmp testfile28 testfile28.rdwr

test_cleanup

testfiles testfile29 testfile29.rdwr

testrun ${abs_builddir}/rdwrmmap testfile29

cmp testfile29 testfile29.rdwr

exit 0
