#! /bin/sh
# Copyright (C) 2012 Red Hat, Inc.
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

# #include <string.h>
#
# #define HELLO "world"
#
# int
# main(int argc, char ** argv)
# {
#   return strlen (HELLO);
# }
#
# gcc -gdwarf-4 -g3 -o testfile-macros macro.c
# gcc -gstrict-dwarf -gdwarf-4 -g3 -o testfile-macinfo macro.c

testfiles testfile-macinfo testfile-macros
tempfiles readelf.macros.out

status=0

testrun ${abs_top_builddir}/src/readelf --debug-dump=info testfile-macinfo \
	| grep macro_info > readelf.macros.out ||
  { echo "*** failure readelf --debug-dump=info testfile-macinfo"; status=1; }
testrun_compare cat readelf.macros.out <<\EOF
           macro_info           (sec_offset) 0
EOF

testrun ${abs_top_builddir}/src/readelf --debug-dump=info testfile-macros \
	| grep GNU_macros > readelf.macros.out ||
  { echo "*** failure readelf --debug-dump=info testfile-macros"; status=1; }
testrun_compare cat readelf.macros.out <<\EOF
           GNU_macros           (sec_offset) 0
EOF

exit $status
