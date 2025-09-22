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

# See tests/testfile-dwarf-45.source
testfiles testfile-splitdwarf-4 testfile-splitdwarf-5
testfiles testfile-hello4.dwo testfile-hello5.dwo
testfiles testfile-world4.dwo testfile-world5.dwo

# DWARF4 GNU DebugFission No real table header.
# We don't really need the skeleton, but we don't want any Warnings.
testrun_compare ${abs_top_builddir}/src/readelf --dwarf-skeleton testfile-splitdwarf-4 --debug-dump=str testfile-hello4.dwo testfile-world4.dwo<<\EOF

testfile-hello4.dwo:


DWARF section [ 5] '.debug_str_offsets.dwo' at offset 0x335:
Table at offset 0 
 Offsets start at 0x0:
 [ 0] [       0]  "long long int"
 [ 1] [       e]  "frob"
 [ 2] [      13]  "long unsigned int"
 [ 3] [      25]  "/home/mark/src/elfutils/tests"
 [ 4] [      43]  "wchar_t"
 [ 5] [      4b]  "main"
 [ 6] [      50]  "long int"
 [ 7] [      59]  "GNU C17 9.0.0 20180515 (experimental) -mtune=generic -march=x86-64 -gdwarf-4 -gsplit-dwarf -gno-as-loc-support -gno-variable-location-views -O2"
 [ 8] [      e9]  "long double"
 [ 9] [      f5]  "hello.c"


DWARF section [ 6] '.debug_str.dwo' at offset 0x35d:
 Offset  String
 [   0]  "long long int"
 [   e]  "frob"
 [  13]  "long unsigned int"
 [  25]  "/home/mark/src/elfutils/tests"
 [  43]  "wchar_t"
 [  4b]  "main"
 [  50]  "long int"
 [  59]  "GNU C17 9.0.0 20180515 (experimental) -mtune=generic -march=x86-64 -gdwarf-4 -gsplit-dwarf -gno-as-loc-support -gno-variable-location-views -O2"
 [  e9]  "long double"
 [  f5]  "hello.c"

testfile-world4.dwo:


DWARF section [ 5] '.debug_str_offsets.dwo' at offset 0x2e7:
Table at offset 0 
 Offsets start at 0x0:
 [ 0] [       0]  "long long unsigned int"
 [ 1] [      17]  "/home/mark/src/elfutils/tests"
 [ 2] [      35]  "long long int"
 [ 3] [      43]  "signed char"
 [ 4] [      4f]  "long int"
 [ 5] [      58]  "world.c"
 [ 6] [      60]  "unsigned int"
 [ 7] [      6d]  "long unsigned int"
 [ 8] [      7f]  "short unsigned int"
 [ 9] [      92]  "frob"
 [10] [      97]  "calc"
 [11] [      9c]  "unsigned char"
 [12] [      aa]  "short int"
 [13] [      b4]  "exit"
 [14] [      b9]  "GNU C17 9.0.0 20180515 (experimental) -mtune=generic -march=x86-64 -gdwarf-4 -gsplit-dwarf -gno-as-loc-support -gno-variable-location-views -O2"
 [15] [     149]  "char"
 [16] [     14e]  "word"
 [17] [     153]  "argv"
 [18] [     158]  "argc"
 [19] [     15d]  "main"


DWARF section [ 6] '.debug_str.dwo' at offset 0x337:
 Offset  String
 [   0]  "long long unsigned int"
 [  17]  "/home/mark/src/elfutils/tests"
 [  35]  "long long int"
 [  43]  "signed char"
 [  4f]  "long int"
 [  58]  "world.c"
 [  60]  "unsigned int"
 [  6d]  "long unsigned int"
 [  7f]  "short unsigned int"
 [  92]  "frob"
 [  97]  "calc"
 [  9c]  "unsigned char"
 [  aa]  "short int"
 [  b4]  "exit"
 [  b9]  "GNU C17 9.0.0 20180515 (experimental) -mtune=generic -march=x86-64 -gdwarf-4 -gsplit-dwarf -gno-as-loc-support -gno-variable-location-views -O2"
 [ 149]  "char"
 [ 14e]  "word"
 [ 153]  "argv"
 [ 158]  "argc"
 [ 15d]  "main"
EOF

# DWARF5 Real table header.
# We don't really need the skeleton, but we don't want any Warnings.
testrun_compare ${abs_top_builddir}/src/readelf --dwarf-skeleton testfile-splitdwarf-5 --debug-dump=str testfile-hello5.dwo testfile-world5.dwo<<\EOF

testfile-hello5.dwo:


DWARF section [ 5] '.debug_str_offsets.dwo' at offset 0x353:
Table at offset 0 

 Length:              44
 Offset size:          4
 DWARF version:        5
 Padding:              0

 Offsets start at 0x8:
 [ 0] [       0]  "long long int"
 [ 1] [       e]  "frob"
 [ 2] [      13]  "long unsigned int"
 [ 3] [      25]  "/home/mark/src/elfutils/tests"
 [ 4] [      43]  "wchar_t"
 [ 5] [      4b]  "main"
 [ 6] [      50]  "long int"
 [ 7] [      59]  "GNU C17 9.0.0 20180515 (experimental) -mtune=generic -march=x86-64 -gdwarf-5 -gsplit-dwarf -gno-as-loc-support -gno-variable-location-views -O2"
 [ 8] [      e9]  "long double"
 [ 9] [      f5]  "hello.c"


DWARF section [ 6] '.debug_str.dwo' at offset 0x383:
 Offset  String
 [   0]  "long long int"
 [   e]  "frob"
 [  13]  "long unsigned int"
 [  25]  "/home/mark/src/elfutils/tests"
 [  43]  "wchar_t"
 [  4b]  "main"
 [  50]  "long int"
 [  59]  "GNU C17 9.0.0 20180515 (experimental) -mtune=generic -march=x86-64 -gdwarf-5 -gsplit-dwarf -gno-as-loc-support -gno-variable-location-views -O2"
 [  e9]  "long double"
 [  f5]  "hello.c"

testfile-world5.dwo:


DWARF section [ 5] '.debug_str_offsets.dwo' at offset 0x313:
Table at offset 0 

 Length:              84
 Offset size:          4
 DWARF version:        5
 Padding:              0

 Offsets start at 0x8:
 [ 0] [       0]  "long long unsigned int"
 [ 1] [      17]  "GNU C17 9.0.0 20180515 (experimental) -mtune=generic -march=x86-64 -gdwarf-5 -gsplit-dwarf -gno-as-loc-support -gno-variable-location-views -O2"
 [ 2] [      a7]  "/home/mark/src/elfutils/tests"
 [ 3] [      c5]  "long long int"
 [ 4] [      d3]  "signed char"
 [ 5] [      df]  "long int"
 [ 6] [      e8]  "world.c"
 [ 7] [      f0]  "unsigned int"
 [ 8] [      fd]  "long unsigned int"
 [ 9] [     10f]  "short unsigned int"
 [10] [     122]  "frob"
 [11] [     127]  "calc"
 [12] [     12c]  "unsigned char"
 [13] [     13a]  "short int"
 [14] [     144]  "exit"
 [15] [     149]  "char"
 [16] [     14e]  "word"
 [17] [     153]  "argv"
 [18] [     158]  "argc"
 [19] [     15d]  "main"


DWARF section [ 6] '.debug_str.dwo' at offset 0x36b:
 Offset  String
 [   0]  "long long unsigned int"
 [  17]  "GNU C17 9.0.0 20180515 (experimental) -mtune=generic -march=x86-64 -gdwarf-5 -gsplit-dwarf -gno-as-loc-support -gno-variable-location-views -O2"
 [  a7]  "/home/mark/src/elfutils/tests"
 [  c5]  "long long int"
 [  d3]  "signed char"
 [  df]  "long int"
 [  e8]  "world.c"
 [  f0]  "unsigned int"
 [  fd]  "long unsigned int"
 [ 10f]  "short unsigned int"
 [ 122]  "frob"
 [ 127]  "calc"
 [ 12c]  "unsigned char"
 [ 13a]  "short int"
 [ 144]  "exit"
 [ 149]  "char"
 [ 14e]  "word"
 [ 153]  "argv"
 [ 158]  "argc"
 [ 15d]  "main"
EOF

exit 0
