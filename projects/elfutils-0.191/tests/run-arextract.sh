#! /bin/sh
# Copyright (C) 1999, 2000, 2002, 2005, 2006 Red Hat, Inc.
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

tempfiles arextract.test

archive=${abs_top_builddir}/libelf/libelf.a
if test -f $archive; then
    # The file is really available (i.e., no shared-only built).
    echo -n "Extracting symbols... $ac_c"

    # The files we are looking at.
    for f in ${abs_top_builddir}/libelf/*.o; do
	testrun ${abs_builddir}/arextract $archive `basename $f` arextract.test || exit 1
	cmp $f arextract.test || {
	    echo "Extraction of $1 failed"
	    exit 1
	}
    done

    echo "done"
fi

exit 0
