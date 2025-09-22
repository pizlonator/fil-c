#! /bin/sh
# Copyright (C) 1999, 2000, 2002, 2006 Red Hat, Inc.
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

lib=${abs_top_builddir}/libelf/libelf.a
okfile=arsymtest.ok
tmpfile=arsymtest.tmp
testfile=arsymtest.test

tempfiles $okfile $tmpfile $testfile

result=77
if test -f $lib; then
    # Generate list using `nm' we check against.
    ${NM} -s $lib |
    sed -e '1,/^Arch/d' -e '/^$/,$d' |
    sort > $okfile

    # Now run our program using libelf.
    testrun ${abs_builddir}/arsymtest $lib $tmpfile || exit 1
    sort $tmpfile > $testfile

    # Compare the outputs.
    if cmp $okfile $testfile; then
	result=0
    else
	result=1
    fi
fi

exit $result
