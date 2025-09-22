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

original=${original:-testfile19}
indexed=${indexed:-testfile19.index}

testfiles $original $indexed

testrun ${abs_top_builddir}/src/ranlib $original

if test -z "$noindex"; then
  # The date in the index is different.  The reference file has it blanked
  # out, we do the same here.
  echo "            " |
  dd of=$original seek=24 bs=1 count=12 conv=notrunc 2>/dev/null
fi

cmp $original $indexed

exit 0
