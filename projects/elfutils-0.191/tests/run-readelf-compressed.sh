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

if ! grep -q -F '#define USE_BZLIB' ${abs_top_builddir}/config.h; then
  echo "elfutils built without bzip2 support"
  exit 77
fi

# See run-strip-reloc.sh
testfiles hello_i386.ko

tempfiles hello_i386.ko.bz2 readelf.out.1 readelf.out.2

testrun ${abs_top_builddir}/src/readelf -a hello_i386.ko > readelf.out.1
bzip2 hello_i386.ko
testrun ${abs_top_builddir}/src/readelf -a hello_i386.ko.bz2 > readelf.out.2

diff -u readelf.out.1 readelf.out.2
if [ $? != 0 ]; then
  exit 1;
fi

exit 0
