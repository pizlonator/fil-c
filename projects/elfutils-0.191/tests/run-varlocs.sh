#! /bin/sh
# Copyright (C) 2013 Red Hat, Inc.
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

# See the source files testfile_const_type.c testfile_implicit_value.c
# testfile_entry_value.c testfile_parameter_ref.c testfile_implicit_pointer.c
# how to regenerate the test files (needs GCC 4.8+).

testfiles testfile_const_type testfile_implicit_value testfile_entry_value
testfiles testfile_parameter_ref testfile_implicit_pointer

testrun_compare ${abs_top_builddir}/tests/varlocs -e testfile_const_type <<\EOF
module 'testfile_const_type'
[b] CU 'const_type.c'@0
  [33] function 'f1'@80483f0
    frame_base: {call_frame_cfa {bregx(4,4)}}
    [4b] parameter 'd'
      [80483f0,804841b) {fbreg(0)}
    [57] variable 'w'
      [80483f0,804841b) {fbreg(0), GNU_deref_type(8){long long int,signed,64@[25]}, GNU_const_type{long long int,signed,64@[25]}(8)[0000806745230100], div, GNU_convert{long long unsigned int,unsigned,64@[2c]}, stack_value}
  [7d] function 'main'@80482f0
    frame_base: {call_frame_cfa {bregx(4,4)}}
EOF

testrun_compare ${abs_top_builddir}/tests/varlocs -e testfile_implicit_value <<\EOF
module 'testfile_implicit_value'
[b] CU 'implicit_value.c'@0
  [25] function 'foo'@80483f0
    frame_base: {call_frame_cfa {bregx(4,4)}}
    [3e] variable 'a'
      [80483f0,80483f6) {implicit_value(8){0200000000000000}, piece(8), implicit_value(8){1500000000000000}, piece(8)}
  [86] function 'main'@80482f0
    frame_base: {call_frame_cfa {bregx(4,4)}}
EOF

testrun_compare ${abs_top_builddir}/tests/varlocs -e testfile_entry_value <<\EOF
module 'testfile_entry_value'
[b] CU 'entry_value.c'@0
  [29] function 'foo'@400500
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [4a] parameter 'x'
      [400500,400504) {reg5}
    [55] parameter 'y'
      [400500,400504) {reg4}
  [68] function 'bar'@400510
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [89] parameter 'x'
      [400510,40051c) {reg5}
      [40051c,40052b) {reg6}
      [40052b,400531) {GNU_entry_value(1) {reg5}, stack_value}
    [96] parameter 'y'
      [400510,40051c) {reg4}
      [40051c,40052a) {reg3}
      [40052a,400531) {GNU_entry_value(1) {reg4}, stack_value}
    [a3] variable 'z'
      [400524,400528) {reg0}
      [400528,400529) {reg12}
      [400529,40052e) {breg0(0), breg12(0), plus, stack_value}
      [40052e,400531) {reg0}
  [e9] function 'main'@400400
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [10a] parameter 'argc'
      [400400,400406) {reg5}
      [400406,40040a) {breg5(-1), stack_value}
      [40040a,40040b) {GNU_entry_value(1) {reg5}, stack_value}
    [119] parameter 'argv'
      [400400,400403) {reg4}
      [400403,40040b) {GNU_entry_value(1) {reg4}, stack_value}
EOF

testrun_compare ${abs_top_builddir}/tests/varlocs -e testfile_parameter_ref <<\EOF
module 'testfile_parameter_ref'
[b] CU 'parameter_ref.c'@0
  [77] function 'foo'@400510
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [92] parameter 'x'
      [400510,400523) {reg5}
    [99] parameter 'y'
      [400510,400523) {GNU_parameter_ref[42], stack_value}
    [a5] variable 'a'
      [400510,400523) {breg5(0), lit1, shl, stack_value}
    [b0] variable 'b'
      [400510,400523) {GNU_parameter_ref[42], lit1, shl, stack_value}
    [be] variable 'c'
      <constant value>
    [c4] parameter 'z'
      <constant value>
  [cb] function 'main'@400400
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [ec] parameter 'x'
      [400400,400408) {reg5}
      [400408,400421) {reg3}
      [400421,400423) {GNU_entry_value(1) {reg5}, stack_value}
    [f9] parameter 'argv'
      [400400,400408) {reg4}
      [400408,400423) {GNU_entry_value(1) {reg4}, stack_value}
EOF

testrun_compare ${abs_top_builddir}/tests/varlocs -e testfile_implicit_pointer <<\EOF
module 'testfile_implicit_pointer'
[b] CU 'implicit_pointer.c'@0
  [29] function 'foo'@400500
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [4a] parameter 'i'
      [400500,400503) {reg5}
    [55] variable 'p'
      [400500,400503) {GNU_implicit_pointer([4a],0) {reg5}}
  [73] function 'main'@400400
    frame_base: {call_frame_cfa {bregx(7,8)}}
EOF

# Multi CU DWARF5. See run-dwarf-ranges.sh.
testfiles testfileranges5.debug
testrun_compare ${abs_top_builddir}/tests/varlocs --debug -e testfileranges5.debug <<\EOF
module 'testfileranges5.debug'
[c] CU 'hello.c'@0
  [2a] function 'no_say'@401160
    frame_base: {call_frame_cfa {...}}
    [4a] parameter 'prefix'
      [401160,401169) {reg5}
      [401169,40116a) {entry_value(1) {reg5}, stack_value}
      [40116a,401175) {reg5}
      [401175,40117a) {entry_value(1) {reg5}, stack_value}
    [59] variable 'world'
      [401160,40117a) {addr(0x402004), stack_value}
  [bd] function 'main'@401050
    frame_base: {call_frame_cfa {...}}
    [dd] parameter 'argc'
      [401050,401062) {reg5}
      [401062,401067) {entry_value(1) {reg5}, stack_value}
    [ec] parameter 'argv'
      [401050,401066) {reg4}
      [401066,401067) {entry_value(1) {reg4}, stack_value}
  [fb] inlined function 'subject'@401053
    [117] parameter 'count'
      [401053,40105f) {reg5}
    [120] parameter 'word'
      [401053,40105f) {reg0}
  [168] function 'subject'@401150
    frame_base: {call_frame_cfa {...}}
    [183] parameter 'word'
      [401150,401160) {reg5}
    [18a] parameter 'count'
      [401150,401160) {reg4}
module 'testfileranges5.debug'
[1ab] CU 'world.c'@401180
  [1cd] function 'no_main'@4011d0
    frame_base: {call_frame_cfa {...}}
    [1ef] parameter 'argc'
      [4011d0,4011e2) {reg5}
      [4011e2,4011e7) {entry_value(1) {reg5}, stack_value}
    [1fe] parameter 'argv'
      [4011d0,4011e6) {reg4}
      [4011e6,4011e7) {entry_value(1) {reg4}, stack_value}
  [20d] inlined function 'no_subject'@4011d3
    [229] parameter 'count'
      [4011d3,4011df) {reg5}
    [232] parameter 'word'
      [4011d3,4011df) {reg0}
  [28d] function 'say'@401180
    frame_base: {call_frame_cfa {...}}
    [2af] parameter 'prefix'
      [401180,40118e) {reg5}
      [40118e,40119c) {reg3}
      [40119c,4011a7) {entry_value(1) {reg5}, stack_value}
      [4011a7,4011b5) {reg3}
      [4011b5,4011c0) {entry_value(1) {reg5}, stack_value}
    [2be] variable 'world'
      [401193,40119b) {reg0}
      [4011a7,4011b4) {reg0}
  [2ce] inlined function 'happy'@40119b
    [2e6] parameter 'w'
      [4011a7,4011b4) {reg0}
  [2ef] inlined function 'sad'@40119b
    [303] parameter 'c'
      [40119b,4011a6) {reg0}
      [4011a6,4011a7) {entry_value(1) {reg5}}
      [4011b4,4011bf) {reg0}
  [36b] function 'no_subject'@4011c0
    frame_base: {call_frame_cfa {...}}
    [386] parameter 'word'
      [4011c0,4011d0) {reg5}
    [38d] parameter 'count'
      [4011c0,4011d0) {reg4}
EOF

# Multi CU Split DWARF5. See run-dwarf-ranges.sh.
# Note that the DIE numbers change, but the actual location addresses are
# the same as above, even though the representation is totally different.
testfiles testfilesplitranges5.debug
testfiles testfile-ranges-hello5.dwo testfile-ranges-world5.dwo
testrun_compare ${abs_top_builddir}/tests/varlocs --debug -e testfilesplitranges5.debug <<\EOF
module 'testfilesplitranges5.debug'
[14] CU 'hello.c'@0
  [1d] function 'no_say'@401160
    frame_base: {call_frame_cfa {...}}
    [33] parameter 'prefix'
      [401160,401169) {reg5}
      [401169,40116a) {entry_value(1) {reg5}, stack_value}
      [40116a,401175) {reg5}
      [401175,40117a) {entry_value(1) {reg5}, stack_value}
    [3c] variable 'world'
      [401160,40117a) {addr: 0x402004, stack_value}
  [7e] function 'main'@401050
    frame_base: {call_frame_cfa {...}}
    [94] parameter 'argc'
      [401050,401062) {reg5}
      [401062,401067) {entry_value(1) {reg5}, stack_value}
    [9d] parameter 'argv'
      [401050,401066) {reg4}
      [401066,401067) {entry_value(1) {reg4}, stack_value}
  [a6] inlined function 'subject'@401053
    [bb] parameter 'count'
      [401053,40105f) {reg5}
    [c1] parameter 'word'
      [401053,40105f) {reg0}
  [f6] function 'subject'@401150
    frame_base: {call_frame_cfa {...}}
    [10a] parameter 'word'
      [401150,401160) {reg5}
    [111] parameter 'count'
      [401150,401160) {reg4}
module 'testfilesplitranges5.debug'
[14] CU 'world.c'@401180
  [1d] function 'no_main'@4011d0
    frame_base: {call_frame_cfa {...}}
    [35] parameter 'argc'
      [4011d0,4011e2) {reg5}
      [4011e2,4011e7) {entry_value(1) {reg5}, stack_value}
    [3e] parameter 'argv'
      [4011d0,4011e6) {reg4}
      [4011e6,4011e7) {entry_value(1) {reg4}, stack_value}
  [47] inlined function 'no_subject'@4011d3
    [5c] parameter 'count'
      [4011d3,4011df) {reg5}
    [62] parameter 'word'
      [4011d3,4011df) {reg0}
  [a7] function 'say'@401180
    frame_base: {call_frame_cfa {...}}
    [c2] parameter 'prefix'
      [401180,40118e) {reg5}
      [40118e,40119c) {reg3}
      [40119c,4011a7) {entry_value(1) {reg5}, stack_value}
      [4011a7,4011b5) {reg3}
      [4011b5,4011c0) {entry_value(1) {reg5}, stack_value}
    [cb] variable 'world'
      [401193,40119b) {reg0}
      [4011a7,4011b4) {reg0}
  [d5] inlined function 'happy'@40119b
    [e3] parameter 'w'
      [4011a7,4011b4) {reg0}
  [e9] inlined function 'sad'@40119b
    [f3] parameter 'c'
      [40119b,4011a6) {reg0}
      [4011a6,4011a7) {entry_value(1) {reg5}}
      [4011b4,4011bf) {reg0}
  [147] function 'no_subject'@4011c0
    frame_base: {call_frame_cfa {...}}
    [15b] parameter 'word'
      [4011c0,4011d0) {reg5}
    [162] parameter 'count'
      [4011c0,4011d0) {reg4}
EOF

# GNU DebugFissuon Multi CU Split DWARF. See run-dwarf-ranges.sh.
testfiles testfilesplitranges4.debug
testfiles testfile-ranges-hello.dwo testfile-ranges-world.dwo
testrun_compare ${abs_top_builddir}/tests/varlocs --debug -e testfilesplitranges4.debug <<\EOF
module 'testfilesplitranges4.debug'
[b] CU 'hello.c'@0
  [18] function 'no_say'@4004f0
    frame_base: {call_frame_cfa {...}}
    [2f] parameter 'prefix'
      [4004f0,4004fa) {reg5}
      [4004fa,4004ff) {GNU_entry_value(1) {reg5}, stack_value}
    [3b] variable 'world'
      <no value>
  [60] function 'main'@4003e0
    frame_base: {call_frame_cfa {...}}
    [77] parameter 'argc'
      [4003e0,4003f2) {reg5}
      [4003f2,4003f7) {GNU_entry_value(1) {reg5}, stack_value}
    [83] parameter 'argv'
      [4003e0,4003f6) {reg4}
      [4003f6,1004003f5) {GNU_entry_value(1) {reg4}, stack_value}
  [8f] inlined function 'subject'@4003e3
    [a3] parameter 'count'
      [4003e3,4003ef) {reg5}
    [ac] parameter 'word'
      [4003e3,4003ef) {reg0}
  [e7] function 'subject'@4004e0
    frame_base: {call_frame_cfa {...}}
    [fb] parameter 'word'
      [4004e0,4004f0) {reg5}
    [102] parameter 'count'
      [4004e0,4004f0) {reg4}
module 'testfilesplitranges4.debug'
[b] CU 'world.c'@400500
  [18] function 'no_main'@400550
    frame_base: {call_frame_cfa {...}}
    [2f] parameter 'argc'
      [400550,400562) {reg5}
      [400562,400567) {GNU_entry_value(1) {reg5}, stack_value}
    [3b] parameter 'argv'
      [400550,400566) {reg4}
      [400566,100400565) {GNU_entry_value(1) {reg4}, stack_value}
  [47] inlined function 'no_subject'@400553
    [5b] parameter 'count'
      [400553,40055f) {reg5}
    [64] parameter 'word'
      [400553,40055f) {reg0}
  [af] function 'say'@400500
    frame_base: {call_frame_cfa {...}}
    [c9] parameter 'prefix'
      [400500,40050e) {reg5}
      [40050e,40051c) {reg3}
      [40051c,400527) {GNU_entry_value(1) {reg5}, stack_value}
      [400527,400535) {reg3}
      [400535,400540) {GNU_entry_value(1) {reg5}, stack_value}
    [d5] variable 'world'
      [400513,40051b) {reg0}
      [400527,400534) {reg0}
  [e1] inlined function 'happy'@40051c
    [f1] parameter 'w'
      [400527,400534) {reg0}
  [fa] inlined function 'sad'@40051c
    [106] parameter 'c'
      [40051b,400526) {reg0}
      [400526,400527) {GNU_entry_value(1) {reg5}}
      [400534,40053f) {reg0}
  [15c] function 'no_subject'@400540
    frame_base: {call_frame_cfa {...}}
    [170] parameter 'word'
      [400540,400550) {reg5}
    [177] parameter 'count'
      [400540,400550) {reg4}
EOF

# DW_OP_addrx and DW_OP_constx testcases.
#
# int i, j, k;
# __thread int l, m, n;
#
# int main ()
# {
#   int r1 = i + j + k;
#   int r2 = l + m + n;
#   int res = r1 + r2;
#
#   return res;
# }
#
# gcc -O2 -gdwarf-5 -gsplit-dwarf -o addrx_constx-5.o -c addrx_constx.c
# gcc -O2 -gdwarf-5 -gsplit-dwarf -o testfile-addrx_constx-5 addrx_constx-5.o
# gcc -O2 -gdwarf-4 -gsplit-dwarf -o addrx_constx-4.o -c addrx_constx.c
# gcc -O2 -gdwarf-4 -gsplit-dwarf -o testfile-addrx_constx-4 addrx_constx-4.o

testfiles testfile-addrx_constx-5 addrx_constx-5.dwo
testrun_compare ${abs_top_builddir}/tests/varlocs --exprlocs -e testfile-addrx_constx-5 <<\EOF
module 'testfile-addrx_constx-5'
[14] CU 'addrx_constx.c'@0
  producer (strx)
  language (data1)
  name (strx)
  comp_dir (strx)
  [19] variable "i"
    name (string)
    decl_file (implicit_const)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {addr: 0x404038}
  [25] base_type "int"
    byte_size (data1)
    encoding (data1)
    name (string)
  [2c] variable "j"
    name (string)
    decl_file (implicit_const)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {addr: 0x404034}
  [38] variable "k"
    name (string)
    decl_file (implicit_const)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {addr: 0x40403c}
  [44] variable "l"
    name (string)
    decl_file (implicit_const)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {const: 0x403e10, form_tls_address}
  [51] variable "m"
    name (string)
    decl_file (implicit_const)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {const: 0x403e0c, form_tls_address}
  [5e] variable "n"
    name (string)
    decl_file (implicit_const)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {const: 0x403e08, form_tls_address}
  [6b] subprogram "main"
    external (flag_present)
    name (strx)
    decl_file (data1)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    low_pc (addrx)
    high_pc (data8)
    frame_base (exprloc) {call_frame_cfa {bregx(7,8)}}
    call_all_calls (flag_present)
    [7f] variable "r1"
      name (string)
      decl_file (implicit_const)
      decl_line (data1)
      decl_column (implicit_const)
      type (ref4)
      location (exprloc) {addr: 0x404038, deref_size(4), addr: 0x404034, deref_size(4), plus, addr: 0x40403c, deref_size(4), plus, stack_value}
    [98] variable "r2"
      name (string)
      decl_file (implicit_const)
      decl_line (data1)
      decl_column (implicit_const)
      type (ref4)
      location (exprloc) {form_tls_address, const: 0x403e10, deref_size(4), form_tls_address, const: 0x403e0c, deref_size(4), plus, form_tls_address, const: 0x403e08, deref_size(4), plus, stack_value}
    [b4] variable "res"
      name (string)
      decl_file (implicit_const)
      decl_line (data1)
      decl_column (implicit_const)
      type (ref4)
      location (exprloc) {addr: 0x404038, deref_size(4), form_tls_address, const: 0x403e08, deref_size(4), plus, form_tls_address, const: 0x403e0c, deref_size(4), plus, form_tls_address, const: 0x403e10, deref_size(4), plus, addr: 0x404034, deref_size(4), plus, addr: 0x40403c, deref_size(4), plus, stack_value}
EOF

testfiles testfile-addrx_constx-4 addrx_constx-4.dwo
testrun_compare ${abs_top_builddir}/tests/varlocs --exprlocs -e testfile-addrx_constx-4 <<\EOF
module 'testfile-addrx_constx-4'
[b] CU 'addrx_constx.c'@0
  producer (GNU_str_index)
  language (data1)
  name (GNU_str_index)
  comp_dir (GNU_str_index)
  GNU_dwo_id (data8)
  [18] variable "i"
    name (string)
    decl_file (data1)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {addr: 0x404038}
  [25] base_type "int"
    byte_size (data1)
    encoding (data1)
    name (string)
  [2c] variable "j"
    name (string)
    decl_file (data1)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {addr: 0x404034}
  [39] variable "k"
    name (string)
    decl_file (data1)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {addr: 0x40403c}
  [46] variable "l"
    name (string)
    decl_file (data1)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {const: 0x403e10, GNU_push_tls_address}
  [54] variable "m"
    name (string)
    decl_file (data1)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {const: 0x403e0c, GNU_push_tls_address}
  [62] variable "n"
    name (string)
    decl_file (data1)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    external (flag_present)
    location (exprloc) {const: 0x403e08, GNU_push_tls_address}
  [70] subprogram "main"
    external (flag_present)
    name (GNU_str_index)
    decl_file (data1)
    decl_line (data1)
    decl_column (data1)
    type (ref4)
    low_pc (GNU_addr_index)
    high_pc (data8)
    frame_base (exprloc) {call_frame_cfa {bregx(7,8)}}
    GNU_all_call_sites (flag_present)
    [84] variable "r1"
      name (string)
      decl_file (data1)
      decl_line (data1)
      decl_column (data1)
      type (ref4)
      location (exprloc) {addr: 0x404038, deref_size(4), addr: 0x404034, deref_size(4), plus, addr: 0x40403c, deref_size(4), plus, stack_value}
    [9f] variable "r2"
      name (string)
      decl_file (data1)
      decl_line (data1)
      decl_column (data1)
      type (ref4)
      location (exprloc) {GNU_push_tls_address, const: 0x403e10, deref_size(4), GNU_push_tls_address, const: 0x403e0c, deref_size(4), plus, GNU_push_tls_address, const: 0x403e08, deref_size(4), plus, stack_value}
    [bd] variable "res"
      name (string)
      decl_file (data1)
      decl_line (data1)
      decl_column (data1)
      type (ref4)
      location (exprloc) {addr: 0x404038, deref_size(4), GNU_push_tls_address, const: 0x403e08, deref_size(4), plus, GNU_push_tls_address, const: 0x403e0c, deref_size(4), plus, GNU_push_tls_address, const: 0x403e10, deref_size(4), plus, addr: 0x404034, deref_size(4), plus, addr: 0x40403c, deref_size(4), plus, stack_value}
EOF

# See run-readelf-loc.sh
testfiles testfile-splitdwarf4-not-split4.debug
testfiles splitdwarf4-not-split4.dwo

testrun_compare ${abs_top_builddir}/tests/varlocs --debug -e testfile-splitdwarf4-not-split4.debug <<\EOF
module 'testfile-splitdwarf4-not-split4.debug'
[b] CU 'splitdwarf4-not-split4.c'@0
  [18] function 'main'@401050
    frame_base: {call_frame_cfa {...}}
    [30] parameter 'argc'
      [401050,40106e) {reg5}
      [40106e,401086) {reg12}
      [401086,401095) {GNU_entry_value(1) {reg5}, stack_value}
      [401095,40109c) {reg5}
    [3d] parameter 'argv'
      [401050,40106e) {reg4}
      [40106e,401095) {GNU_entry_value(1) {reg4}, stack_value}
      [401095,40109c) {reg4}
    [4a] variable 'i'
      [401050,40106e) {lit0, stack_value}
      [401086,40108e) {breg12(0), breg6(0), plus, stack_value}
      [40108e,401095) {reg0}
      [401095,40109c) {lit0, stack_value}
    [58] variable 'p'
      [401050,40106e) {reg5}
      [40106e,401090) {reg6}
      [401095,40109c) {reg5}
module 'testfile-splitdwarf4-not-split4.debug'
[3f] CU 'popcount.c'@401180
  [61] function 'popcount'@401180
    frame_base: {call_frame_cfa {...}}
    [83] parameter 'u'
      [401180,401189) {reg5}
      [401189,40119b) {reg1}
      [40119b,40119d) {breg1(0), lit1, shr, stack_value}
      [40119d,4011a1) {reg1}
    [91] variable 'c'
      [401180,401189) {lit0, stack_value}
      [401189,4011a0) {reg0}
      [4011a0,4011a1) {lit0, stack_value}
EOF

# See testfile-dwp.source.
testfiles testfile-dwp-5 testfile-dwp-5.dwp
testfiles testfile-dwp-4 testfile-dwp-4.dwp

testrun_compare ${abs_builddir}/varlocs -e testfile-dwp-5 << EOF
module 'testfile-dwp-5'
[84] CU 'foo.cc'@401190
  [c6] function 'foo'@4011c0
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [e1] parameter 'this'
      [4011c0,401200) {reg5}
    [ea] variable 'x'
      [4011c2,4011d4) {reg0}
      [4011d4,4011d7) {reg1}
      [4011d7,4011d9) {breg1(1), stack_value}
      [4011f1,401200) {reg0}
  [f9] inlined function 'x_x'@4011cb
    [104] parameter 'x'
      [4011cb,4011eb) {reg0}
      [4011f1,401200) {reg0}
  [14a] function 'x_x'@401190
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [15b] parameter 'x'
      [401190,4011a1) {reg5}
      [4011a1,4011bd) {reg0}
module 'testfile-dwp-5'
[1fa] CU 'bar.cc'@401200
  [23c] function 'bar'@401200
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [253] parameter 'this'
      [401200,40121b) {reg5}
module 'testfile-dwp-5'
[272] CU 'main.cc'@0
  [2c7] function 'main'@401020
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [2e0] parameter 'argc'
      [401020,401068) {reg5}
      [401068,401080) {fbreg(-40)}
      [401080,401084) {breg5(0)}
      [401084,401099) {entry_value(1) {reg5}, stack_value}
      [401099,4010a0) {reg5}
    [2ec] parameter 'argv'
      [401020,40104b) {reg4}
      [40104b,401099) {entry_value(1) {reg4}, stack_value}
      [401099,4010a0) {reg4}
    [2f8] variable 'myfoo'
      [401020,4010a0) {fbreg(-40)}
    [303] variable 'mybar'
      [401020,4010a0) {fbreg(-32)}
  [30d] inlined function 'fibonacci'@401030
    [31c] parameter 'n'
      [401030,401060) {reg5}
      [401099,4010a0) {reg5}
  [326] inlined function 'fibonacci'
    [32f] parameter 'n'
      <no value>
EOF

testrun_compare ${abs_builddir}/varlocs -e testfile-dwp-4 << EOF
module 'testfile-dwp-4'
[b] CU 'foo.cc'@401190
  [54] function 'foo'@4011c0
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [6f] parameter 'this'
      [4011c0,401200) {reg5}
    [78] variable 'x'
      [4011c2,4011d4) {reg0}
      [4011d4,4011d7) {reg1}
      [4011d7,4011d9) {breg1(1), stack_value}
      [4011f1,401200) {reg0}
  [8a] inlined function 'x_x'@4011cb
    [98] parameter 'x'
      [4011cb,4011eb) {reg0}
      [4011f1,401200) {reg0}
  [e7] function 'x_x'@401190
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [f8] parameter 'x'
      [401190,4011a1) {reg5}
      [4011a1,4011bd) {reg0}
module 'testfile-dwp-4'
[129] CU 'bar.cc'@401200
  [172] function 'bar'@401200
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [189] parameter 'this'
      [401200,40121b) {reg5}
module 'testfile-dwp-4'
[19f] CU 'main.cc'@0
  [1fd] function 'main'@401020
    frame_base: {call_frame_cfa {bregx(7,8)}}
    [216] parameter 'argc'
      [401020,401068) {reg5}
      [401068,401080) {fbreg(-40)}
      [401080,401084) {breg5(0)}
      [401084,401097) {GNU_entry_value(1) {reg5}, stack_value}
      [401099,4010a0) {reg5}
    [228] parameter 'argv'
      [401020,40104b) {reg4}
      [40104b,401099) {GNU_entry_value(1) {reg4}, stack_value}
      [401099,4010a0) {reg4}
    [23a] variable 'myfoo'
      [401020,4010a0) {fbreg(-40)}
    [247] variable 'mybar'
      [401020,4010a0) {fbreg(-32)}
  [253] inlined function 'fibonacci'@401030
    [265] parameter 'n'
      [401030,401060) {reg5}
      [401099,4010a0) {reg5}
  [272] inlined function 'fibonacci'
    [27e] parameter 'n'
      <no value>
EOF

exit 0
