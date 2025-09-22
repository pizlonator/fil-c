#! /bin/sh
# Copyright (C) 2019 Red Hat, Inc.
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

# = rng.ads =
# package Rng is
# 
#    type Rec (I : Integer) is record
#       case I is
#  when Positive =>
#     case I is
#        when 1..15 | 17 | 23 =>
#   null;
#        when others =>
#   J : Integer;
#     end case;
#  when -52..-1 =>
#     Q: Integer;
#  when -64 =>
#     R: Boolean;
#  when others =>
#     null;
#       end case;
#    end record;
# 
#    R : Rec (1);
# 
# end Rng;

# = urng.ads =
#
# package Urng is
# 
#    type Unsigned is mod 65536;
#    type Rec (U : Unsigned) is record
#       case U is
#  when 17 | 23 | 32768..65535 =>
#     null;
#  when 256 => 
#     B: Boolean;
#  when others =>
#     I : Integer;
#       end case;
#    end record;
# 
#    R : Rec (1);
# 
# end Urng;

# gcc -c -g -fgnat-encodings=minimal -gstrict-dwarf rng.ads
# eu-strip -g -f rng.debug rng.o 
# gcc -c -g -fgnat-encodings=minimal -gstrict-dwarf urng.ads
# eu-strip -g -f urng.debug urng.o 

testfiles testfile-rng.debug testfile-urng.debug

testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=info testfile-rng.debug testfile-urng.debug <<EOF

testfile-rng.debug:


DWARF section [ 5] '.debug_info' at offset 0x40:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 1
           producer             (strp) "GNU Ada 9.1.1 20190503 (Red Hat 9.1.1-1) -g -fgnat-encodings=minimal -gstrict-dwarf -mtune=generic -march=x86-64"
           language             (data1) Ada95 (13)
           name                 (strp) "rng.ads"
           comp_dir             (strp) "/home/mark"
           low_pc               (addr) 000000000000000000
           high_pc              (data8) 2016 (0x00000000000007e0)
           stmt_list            (sec_offset) 0
 [    2d]    structure_type       abbrev: 2
             name                 (strp) "rng__rec"
             byte_size            (exprloc) 
              [ 0] push_object_address
              [ 1] deref_size 4
              [ 3] call4 [    c6]
              [ 8] plus_uconst 7
              [10] const1s -4
              [12] and
             decl_file            (data1) rng.ads (1)
             decl_line            (data1) 3
             decl_column          (data1) 9
             sibling              (ref4) [    ab]
 [    47]      member               abbrev: 3
               name                 (string) "i"
               decl_file            (data1) rng.ads (1)
               decl_line            (data1) 3
               decl_column          (data1) 14
               type                 (ref4) [    fe]
               data_member_location (data1) 0
 [    52]      variant_part         abbrev: 4
               discr                (ref4) [    47]
 [    57]        variant              abbrev: 5
                 discr_list           (block1) range 1..2147483647
                 sibling              (ref4) [    81]
 [    64]          variant_part         abbrev: 4
                   discr                (ref4) [    47]
 [    69]            variant              abbrev: 6
                     discr_list           (block1) range 1..15, label 17, label 23
 [    72]            variant              abbrev: 7
 [    73]              member               abbrev: 3
                       name                 (string) "j"
                       decl_file            (data1) rng.ads (1)
                       decl_line            (data1) 10
                       decl_column          (data1) 19
                       type                 (ref4) [    fe]
                       data_member_location (data1) 4
 [    81]        variant              abbrev: 5
                 discr_list           (block1) range -52..-1
                 sibling              (ref4) [    96]
 [    8a]          member               abbrev: 3
                   name                 (string) "q"
                   decl_file            (data1) rng.ads (1)
                   decl_line            (data1) 13
                   decl_column          (data1) 13
                   type                 (ref4) [    fe]
                   data_member_location (data1) 4
 [    96]        variant              abbrev: 8
                 discr_value          (sdata) -64
                 sibling              (ref4) [    a8]
 [    9c]          member               abbrev: 3
                   name                 (string) "r"
                   decl_file            (data1) rng.ads (1)
                   decl_line            (data1) 15
                   decl_column          (data1) 13
                   type                 (ref4) [   105]
                   data_member_location (data1) 4
 [    a8]        variant              abbrev: 9
 [    ab]    dwarf_procedure      abbrev: 10
             location             (exprloc) 
              [ 0] dup
              [ 1] lit0
              [ 2] gt
              [ 3] over
              [ 4] lit15
              [ 5] le
              [ 6] and
              [ 7] over
              [ 8] lit17
              [ 9] eq
              [10] or
              [11] over
              [12] lit23
              [13] eq
              [14] or
              [15] bra 22
              [18] lit4
              [19] skip 23
              [22] lit0
              [23] swap
              [24] drop
 [    c6]    dwarf_procedure      abbrev: 10
             location             (exprloc) 
              [ 0] dup
              [ 1] lit0
              [ 2] gt
              [ 3] bra 36
              [ 6] dup
              [ 7] const1s -52
              [ 9] lt
              [10] over
              [11] lit0
              [12] ge
              [13] or
              [14] bra 21
              [17] lit4
              [18] skip 33
              [21] dup
              [22] const1s -64
              [24] eq
              [25] bra 32
              [28] lit0
              [29] skip 33
              [32] lit4
              [33] skip 52
              [36] dup
              [37] call4 [    ab]
              [42] plus_uconst 3
              [44] const1s -4
              [46] and
              [47] plus_uconst 3
              [49] const1s -4
              [51] and
              [52] swap
              [53] drop
 [    fe]    base_type            abbrev: 11
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (strp) "integer"
             artificial           (flag_present) yes
 [   105]    base_type            abbrev: 12
             byte_size            (data1) 1
             encoding             (data1) boolean (2)
             name                 (strp) "boolean"
 [   10c]    variable             abbrev: 13
             name                 (strp) "rng__r"
             decl_file            (data1) rng.ads (1)
             decl_line            (data1) 21
             decl_column          (data1) 4
             type                 (ref4) [    2d]
             external             (flag_present) yes
             location             (exprloc) 
              [ 0] addr 0x7e4
 [   122]    subprogram           abbrev: 14
             external             (flag_present) yes
             name                 (strp) "rng___elabs"
             artificial           (flag_present) yes
             low_pc               (addr) 0x0000000000000734
             high_pc              (data8) 22 (0x000000000000074a)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa

testfile-urng.debug:


DWARF section [ 5] '.debug_info' at offset 0x40:
 [Offset]
 Compilation unit at offset 0:
 Version: 4, Abbreviation section offset: 0, Address size: 8, Offset size: 4
 [     b]  compile_unit         abbrev: 1
           producer             (strp) "GNU Ada 9.1.1 20190503 (Red Hat 9.1.1-1) -g -fgnat-encodings=minimal -gstrict-dwarf -mtune=generic -march=x86-64"
           language             (data1) Ada95 (13)
           name                 (strp) "urng.ads"
           comp_dir             (strp) "/home/mark"
           low_pc               (addr) 000000000000000000
           high_pc              (data8) 977 (0x00000000000003d1)
           stmt_list            (sec_offset) 0
 [    2d]    base_type            abbrev: 2
             byte_size            (data1) 2
             encoding             (data1) unsigned (7)
             name                 (strp) "urng__unsigned"
 [    34]    structure_type       abbrev: 3
             name                 (strp) "urng__rec"
             byte_size            (exprloc) 
              [ 0] push_object_address
              [ 1] deref_size 2
              [ 3] call4 [    8d]
              [ 8] plus_uconst 7
              [10] const1s -4
              [12] and
             decl_file            (data1) urng.ads (1)
             decl_line            (data1) 4
             decl_column          (data1) 9
             sibling              (ref4) [    8d]
 [    4e]      member               abbrev: 4
               name                 (string) "u"
               decl_file            (data1) urng.ads (1)
               decl_line            (data1) 4
               decl_column          (data1) 14
               type                 (ref4) [    2d]
               data_member_location (data1) 0
 [    59]      variant_part         abbrev: 5
               discr                (ref4) [    4e]
 [    5e]        variant              abbrev: 6
                 discr_list           (block1) label 17, label 23, range 32768..65535
 [    6b]        variant              abbrev: 7
                 discr_value          (udata) 256
                 sibling              (ref4) [    7e]
 [    72]          member               abbrev: 4
                   name                 (string) "b"
                   decl_file            (data1) urng.ads (1)
                   decl_line            (data1) 9
                   decl_column          (data1) 13
                   type                 (ref4) [    a4]
                   data_member_location (data1) 4
 [    7e]        variant              abbrev: 8
 [    7f]          member               abbrev: 4
                   name                 (string) "i"
                   decl_file            (data1) urng.ads (1)
                   decl_line            (data1) 11
                   decl_column          (data1) 13
                   type                 (ref4) [    ab]
                   data_member_location (data1) 4
 [    8d]    dwarf_procedure      abbrev: 9
             location             (exprloc) 
              [ 0] dup
              [ 1] lit17
              [ 2] ne
              [ 3] over
              [ 4] lit23
              [ 5] ne
              [ 6] and
              [ 7] over
              [ 8] lit0
              [ 9] ge
              [10] and
              [11] bra 18
              [14] lit0
              [15] skip 19
              [18] lit4
              [19] swap
              [20] drop
 [    a4]    base_type            abbrev: 2
             byte_size            (data1) 1
             encoding             (data1) boolean (2)
             name                 (strp) "boolean"
 [    ab]    base_type            abbrev: 10
             byte_size            (data1) 4
             encoding             (data1) signed (5)
             name                 (strp) "integer"
             artificial           (flag_present) yes
 [    b2]    variable             abbrev: 11
             name                 (strp) "urng__r"
             decl_file            (data1) urng.ads (1)
             decl_line            (data1) 15
             decl_column          (data1) 4
             type                 (ref4) [    34]
             external             (flag_present) yes
             location             (exprloc) 
              [ 0] addr 0x3d8
 [    c8]    subprogram           abbrev: 12
             external             (flag_present) yes
             name                 (strp) "urng___elabs"
             artificial           (flag_present) yes
             low_pc               (addr) 0x0000000000000386
             high_pc              (data8) 22 (0x000000000000039c)
             frame_base           (exprloc) 
              [ 0] call_frame_cfa
EOF
