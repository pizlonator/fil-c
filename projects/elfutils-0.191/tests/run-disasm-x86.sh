#! /bin/sh
# Copyright (C) 2007, 2008 Red Hat, Inc.
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

# Run x86 test.
case "`uname -m`" in
  x86_64 | i?86 )
    tempfiles testfile44.o
    testfiles testfile44.S testfile44.expect
    ${CC} -m32 -c -o testfile44.o testfile44.S
    testrun_compare ${abs_top_builddir}/src/objdump -d testfile44.o < testfile44.expect
    ;;
esac
