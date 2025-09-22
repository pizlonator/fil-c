#! /bin/sh
# Copyright (C) 2014 Red Hat, Inc.
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

# - testfile-zdebug.c
# #define UINT64_MAX 18446744073709551615UL
#
# int
# main (int argc, char **argv)
# {
#   unsigned long a = UINT64_MAX - 8;
#   unsigned long b = 42 + argc;
#
#   if ( a + b < b )
#     argc = a + argc;
#   else
#      b--;
#
#   return a - b;
# }
#
# gcc -g3 -O3 -fuse-ld=gold -Xlinker --compress-debug-sections=none \
#     -fno-asynchronous-unwind-tables -o testfile-debug testfile-zdebug.c
# gcc -g3 -O3 -fuse-ld=gold -Xlinker --compress-debug-sections=zlib \
#     -fno-asynchronous-unwind-tables -o testfile-zdebug testfile-zdebug.c

testfiles testfile-debug testfile-zdebug
tempfiles readelf.out
tempfiles loc.out aranges.out ranges.out macro.out line.out frame.out

cat > loc.out << \EOF

DWARF section [30] '.debug_loc' at offset 0xa17:

 CU [     b] base: 000000000000000000
 [     0] range 4003c0, 4003c3
           [ 0] reg5
          range 4003c3, 4003d6
           [ 0] breg5 -42
           [ 2] stack_value
          range 4003d6, 4003d9
           [ 0] GNU_entry_value:
                [ 0] reg5
           [ 3] stack_value
EOF

cat loc.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc testfile-debug

cat loc.out | sed -e "s/.debug_loc' at offset 0xa17/.zdebug_loc' at offset 0x1a27/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=loc testfile-zdebug

cat > aranges.out << \EOF

DWARF section [31] '.debug_aranges' at offset 0xa65:

Table at offset 0:

 Length:            44
 DWARF version:      2
 CU offset:          0
 Address size:       8
 Segment size:       0

   0x00000000004003c0..0x0000000000000019
EOF

cat aranges.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=aranges testfile-debug

cat aranges.out | sed -e "s/.debug_aranges' at offset 0xa65/.zdebug_aranges' at offset 0x1a5f/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=aranges testfile-zdebug

cat > ranges.out << \EOF

DWARF section [32] '.debug_ranges' at offset 0xa95:

 CU [     b] base: 000000000000000000
 [     0] range 4003c0, 4003d9
EOF

cat ranges.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=ranges testfile-debug

cat ranges.out | sed -e "s/.debug_ranges' at offset 0xa95/.zdebug_ranges' at offset 0x1a87/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=ranges testfile-zdebug

cat > macro.out << \EOF

DWARF section [33] '.debug_macro' at offset 0xab5:

 Offset:             0x0
 Version:            4
 Flag:               0x2 (debug_line_offset)
 Offset length:      4
 .debug_line offset: 0x0

 #include offset 0x17
 start_file 0, [1] /tmp/testfile-zdebug.c
  #define UINT64_MAX 18446744073709551615UL, line 1 (indirect)
 end_file

 Offset:             0x17
 Version:            4
 Flag:               0x0
 Offset length:      4

 #define __STDC__ 1, line 1 (indirect)
 #define __STDC_HOSTED__ 1, line 1 (indirect)
 #define __GNUC__ 4, line 1 (indirect)
 #define __GNUC_MINOR__ 8, line 1 (indirect)
 #define __GNUC_PATCHLEVEL__ 2, line 1 (indirect)
 #define __VERSION__ "4.8.2 20140120 (Red Hat 4.8.2-15)", line 1 (indirect)
 #define __GNUC_RH_RELEASE__ 15, line 1 (indirect)
 #define __ATOMIC_RELAXED 0, line 1 (indirect)
 #define __ATOMIC_SEQ_CST 5, line 1 (indirect)
 #define __ATOMIC_ACQUIRE 2, line 1 (indirect)
 #define __ATOMIC_RELEASE 3, line 1 (indirect)
 #define __ATOMIC_ACQ_REL 4, line 1 (indirect)
 #define __ATOMIC_CONSUME 1, line 1 (indirect)
 #define __OPTIMIZE__ 1, line 1 (indirect)
 #define __FINITE_MATH_ONLY__ 0, line 1 (indirect)
 #define _LP64 1, line 1 (indirect)
 #define __LP64__ 1, line 1 (indirect)
 #define __SIZEOF_INT__ 4, line 1 (indirect)
 #define __SIZEOF_LONG__ 8, line 1 (indirect)
 #define __SIZEOF_LONG_LONG__ 8, line 1 (indirect)
 #define __SIZEOF_SHORT__ 2, line 1 (indirect)
 #define __SIZEOF_FLOAT__ 4, line 1 (indirect)
 #define __SIZEOF_DOUBLE__ 8, line 1 (indirect)
 #define __SIZEOF_LONG_DOUBLE__ 16, line 1 (indirect)
 #define __SIZEOF_SIZE_T__ 8, line 1 (indirect)
 #define __CHAR_BIT__ 8, line 1 (indirect)
 #define __BIGGEST_ALIGNMENT__ 16, line 1 (indirect)
 #define __ORDER_LITTLE_ENDIAN__ 1234, line 1 (indirect)
 #define __ORDER_BIG_ENDIAN__ 4321, line 1 (indirect)
 #define __ORDER_PDP_ENDIAN__ 3412, line 1 (indirect)
 #define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__, line 1 (indirect)
 #define __FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__, line 1 (indirect)
 #define __SIZEOF_POINTER__ 8, line 1 (indirect)
 #define __SIZE_TYPE__ long unsigned int, line 1 (indirect)
 #define __PTRDIFF_TYPE__ long int, line 1 (indirect)
 #define __WCHAR_TYPE__ int, line 1 (indirect)
 #define __WINT_TYPE__ unsigned int, line 1 (indirect)
 #define __INTMAX_TYPE__ long int, line 1 (indirect)
 #define __UINTMAX_TYPE__ long unsigned int, line 1 (indirect)
 #define __CHAR16_TYPE__ short unsigned int, line 1 (indirect)
 #define __CHAR32_TYPE__ unsigned int, line 1 (indirect)
 #define __SIG_ATOMIC_TYPE__ int, line 1 (indirect)
 #define __INT8_TYPE__ signed char, line 1 (indirect)
 #define __INT16_TYPE__ short int, line 1 (indirect)
 #define __INT32_TYPE__ int, line 1 (indirect)
 #define __INT64_TYPE__ long int, line 1 (indirect)
 #define __UINT8_TYPE__ unsigned char, line 1 (indirect)
 #define __UINT16_TYPE__ short unsigned int, line 1 (indirect)
 #define __UINT32_TYPE__ unsigned int, line 1 (indirect)
 #define __UINT64_TYPE__ long unsigned int, line 1 (indirect)
 #define __INT_LEAST8_TYPE__ signed char, line 1 (indirect)
 #define __INT_LEAST16_TYPE__ short int, line 1 (indirect)
 #define __INT_LEAST32_TYPE__ int, line 1 (indirect)
 #define __INT_LEAST64_TYPE__ long int, line 1 (indirect)
 #define __UINT_LEAST8_TYPE__ unsigned char, line 1 (indirect)
 #define __UINT_LEAST16_TYPE__ short unsigned int, line 1 (indirect)
 #define __UINT_LEAST32_TYPE__ unsigned int, line 1 (indirect)
 #define __UINT_LEAST64_TYPE__ long unsigned int, line 1 (indirect)
 #define __INT_FAST8_TYPE__ signed char, line 1 (indirect)
 #define __INT_FAST16_TYPE__ long int, line 1 (indirect)
 #define __INT_FAST32_TYPE__ long int, line 1 (indirect)
 #define __INT_FAST64_TYPE__ long int, line 1 (indirect)
 #define __UINT_FAST8_TYPE__ unsigned char, line 1 (indirect)
 #define __UINT_FAST16_TYPE__ long unsigned int, line 1 (indirect)
 #define __UINT_FAST32_TYPE__ long unsigned int, line 1 (indirect)
 #define __UINT_FAST64_TYPE__ long unsigned int, line 1 (indirect)
 #define __INTPTR_TYPE__ long int, line 1 (indirect)
 #define __UINTPTR_TYPE__ long unsigned int, line 1 (indirect)
 #define __GXX_ABI_VERSION 1002, line 1 (indirect)
 #define __SCHAR_MAX__ 127, line 1 (indirect)
 #define __SHRT_MAX__ 32767, line 1 (indirect)
 #define __INT_MAX__ 2147483647, line 1 (indirect)
 #define __LONG_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __LONG_LONG_MAX__ 9223372036854775807LL, line 1 (indirect)
 #define __WCHAR_MAX__ 2147483647, line 1 (indirect)
 #define __WCHAR_MIN__ (-__WCHAR_MAX__ - 1), line 1 (indirect)
 #define __WINT_MAX__ 4294967295U, line 1 (indirect)
 #define __WINT_MIN__ 0U, line 1 (indirect)
 #define __PTRDIFF_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __SIZE_MAX__ 18446744073709551615UL, line 1 (indirect)
 #define __INTMAX_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __INTMAX_C(c) c ## L, line 1 (indirect)
 #define __UINTMAX_MAX__ 18446744073709551615UL, line 1 (indirect)
 #define __UINTMAX_C(c) c ## UL, line 1 (indirect)
 #define __SIG_ATOMIC_MAX__ 2147483647, line 1 (indirect)
 #define __SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1), line 1 (indirect)
 #define __INT8_MAX__ 127, line 1 (indirect)
 #define __INT16_MAX__ 32767, line 1 (indirect)
 #define __INT32_MAX__ 2147483647, line 1 (indirect)
 #define __INT64_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __UINT8_MAX__ 255, line 1 (indirect)
 #define __UINT16_MAX__ 65535, line 1 (indirect)
 #define __UINT32_MAX__ 4294967295U, line 1 (indirect)
 #define __UINT64_MAX__ 18446744073709551615UL, line 1 (indirect)
 #define __INT_LEAST8_MAX__ 127, line 1 (indirect)
 #define __INT8_C(c) c, line 1 (indirect)
 #define __INT_LEAST16_MAX__ 32767, line 1 (indirect)
 #define __INT16_C(c) c, line 1 (indirect)
 #define __INT_LEAST32_MAX__ 2147483647, line 1 (indirect)
 #define __INT32_C(c) c, line 1 (indirect)
 #define __INT_LEAST64_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __INT64_C(c) c ## L, line 1 (indirect)
 #define __UINT_LEAST8_MAX__ 255, line 1 (indirect)
 #define __UINT8_C(c) c, line 1 (indirect)
 #define __UINT_LEAST16_MAX__ 65535, line 1 (indirect)
 #define __UINT16_C(c) c, line 1 (indirect)
 #define __UINT_LEAST32_MAX__ 4294967295U, line 1 (indirect)
 #define __UINT32_C(c) c ## U, line 1 (indirect)
 #define __UINT_LEAST64_MAX__ 18446744073709551615UL, line 1 (indirect)
 #define __UINT64_C(c) c ## UL, line 1 (indirect)
 #define __INT_FAST8_MAX__ 127, line 1 (indirect)
 #define __INT_FAST16_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __INT_FAST32_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __INT_FAST64_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __UINT_FAST8_MAX__ 255, line 1 (indirect)
 #define __UINT_FAST16_MAX__ 18446744073709551615UL, line 1 (indirect)
 #define __UINT_FAST32_MAX__ 18446744073709551615UL, line 1 (indirect)
 #define __UINT_FAST64_MAX__ 18446744073709551615UL, line 1 (indirect)
 #define __INTPTR_MAX__ 9223372036854775807L, line 1 (indirect)
 #define __UINTPTR_MAX__ 18446744073709551615UL, line 1 (indirect)
 #define __FLT_EVAL_METHOD__ 0, line 1 (indirect)
 #define __DEC_EVAL_METHOD__ 2, line 1 (indirect)
 #define __FLT_RADIX__ 2, line 1 (indirect)
 #define __FLT_MANT_DIG__ 24, line 1 (indirect)
 #define __FLT_DIG__ 6, line 1 (indirect)
 #define __FLT_MIN_EXP__ (-125), line 1 (indirect)
 #define __FLT_MIN_10_EXP__ (-37), line 1 (indirect)
 #define __FLT_MAX_EXP__ 128, line 1 (indirect)
 #define __FLT_MAX_10_EXP__ 38, line 1 (indirect)
 #define __FLT_DECIMAL_DIG__ 9, line 1 (indirect)
 #define __FLT_MAX__ 3.40282346638528859812e+38F, line 1 (indirect)
 #define __FLT_MIN__ 1.17549435082228750797e-38F, line 1 (indirect)
 #define __FLT_EPSILON__ 1.19209289550781250000e-7F, line 1 (indirect)
 #define __FLT_DENORM_MIN__ 1.40129846432481707092e-45F, line 1 (indirect)
 #define __FLT_HAS_DENORM__ 1, line 1 (indirect)
 #define __FLT_HAS_INFINITY__ 1, line 1 (indirect)
 #define __FLT_HAS_QUIET_NAN__ 1, line 1 (indirect)
 #define __DBL_MANT_DIG__ 53, line 1 (indirect)
 #define __DBL_DIG__ 15, line 1 (indirect)
 #define __DBL_MIN_EXP__ (-1021), line 1 (indirect)
 #define __DBL_MIN_10_EXP__ (-307), line 1 (indirect)
 #define __DBL_MAX_EXP__ 1024, line 1 (indirect)
 #define __DBL_MAX_10_EXP__ 308, line 1 (indirect)
 #define __DBL_DECIMAL_DIG__ 17, line 1 (indirect)
 #define __DBL_MAX__ ((double)1.79769313486231570815e+308L), line 1 (indirect)
 #define __DBL_MIN__ ((double)2.22507385850720138309e-308L), line 1 (indirect)
 #define __DBL_EPSILON__ ((double)2.22044604925031308085e-16L), line 1 (indirect)
 #define __DBL_DENORM_MIN__ ((double)4.94065645841246544177e-324L), line 1 (indirect)
 #define __DBL_HAS_DENORM__ 1, line 1 (indirect)
 #define __DBL_HAS_INFINITY__ 1, line 1 (indirect)
 #define __DBL_HAS_QUIET_NAN__ 1, line 1 (indirect)
 #define __LDBL_MANT_DIG__ 64, line 1 (indirect)
 #define __LDBL_DIG__ 18, line 1 (indirect)
 #define __LDBL_MIN_EXP__ (-16381), line 1 (indirect)
 #define __LDBL_MIN_10_EXP__ (-4931), line 1 (indirect)
 #define __LDBL_MAX_EXP__ 16384, line 1 (indirect)
 #define __LDBL_MAX_10_EXP__ 4932, line 1 (indirect)
 #define __DECIMAL_DIG__ 21, line 1 (indirect)
 #define __LDBL_MAX__ 1.18973149535723176502e+4932L, line 1 (indirect)
 #define __LDBL_MIN__ 3.36210314311209350626e-4932L, line 1 (indirect)
 #define __LDBL_EPSILON__ 1.08420217248550443401e-19L, line 1 (indirect)
 #define __LDBL_DENORM_MIN__ 3.64519953188247460253e-4951L, line 1 (indirect)
 #define __LDBL_HAS_DENORM__ 1, line 1 (indirect)
 #define __LDBL_HAS_INFINITY__ 1, line 1 (indirect)
 #define __LDBL_HAS_QUIET_NAN__ 1, line 1 (indirect)
 #define __DEC32_MANT_DIG__ 7, line 1 (indirect)
 #define __DEC32_MIN_EXP__ (-94), line 1 (indirect)
 #define __DEC32_MAX_EXP__ 97, line 1 (indirect)
 #define __DEC32_MIN__ 1E-95DF, line 1 (indirect)
 #define __DEC32_MAX__ 9.999999E96DF, line 1 (indirect)
 #define __DEC32_EPSILON__ 1E-6DF, line 1 (indirect)
 #define __DEC32_SUBNORMAL_MIN__ 0.000001E-95DF, line 1 (indirect)
 #define __DEC64_MANT_DIG__ 16, line 1 (indirect)
 #define __DEC64_MIN_EXP__ (-382), line 1 (indirect)
 #define __DEC64_MAX_EXP__ 385, line 1 (indirect)
 #define __DEC64_MIN__ 1E-383DD, line 1 (indirect)
 #define __DEC64_MAX__ 9.999999999999999E384DD, line 1 (indirect)
 #define __DEC64_EPSILON__ 1E-15DD, line 1 (indirect)
 #define __DEC64_SUBNORMAL_MIN__ 0.000000000000001E-383DD, line 1 (indirect)
 #define __DEC128_MANT_DIG__ 34, line 1 (indirect)
 #define __DEC128_MIN_EXP__ (-6142), line 1 (indirect)
 #define __DEC128_MAX_EXP__ 6145, line 1 (indirect)
 #define __DEC128_MIN__ 1E-6143DL, line 1 (indirect)
 #define __DEC128_MAX__ 9.999999999999999999999999999999999E6144DL, line 1 (indirect)
 #define __DEC128_EPSILON__ 1E-33DL, line 1 (indirect)
 #define __DEC128_SUBNORMAL_MIN__ 0.000000000000000000000000000000001E-6143DL, line 1 (indirect)
 #define __REGISTER_PREFIX__ , line 1 (indirect)
 #define __USER_LABEL_PREFIX__ , line 1 (indirect)
 #define __GNUC_GNU_INLINE__ 1, line 1 (indirect)
 #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1, line 1 (indirect)
 #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1, line 1 (indirect)
 #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1, line 1 (indirect)
 #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1, line 1 (indirect)
 #define __GCC_ATOMIC_BOOL_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_CHAR_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_CHAR16_T_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_CHAR32_T_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_WCHAR_T_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_SHORT_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_INT_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_LONG_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_LLONG_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1, line 1 (indirect)
 #define __GCC_ATOMIC_POINTER_LOCK_FREE 2, line 1 (indirect)
 #define __GCC_HAVE_DWARF2_CFI_ASM 1, line 1 (indirect)
 #define __PRAGMA_REDEFINE_EXTNAME 1, line 1 (indirect)
 #define __SIZEOF_INT128__ 16, line 1 (indirect)
 #define __SIZEOF_WCHAR_T__ 4, line 1 (indirect)
 #define __SIZEOF_WINT_T__ 4, line 1 (indirect)
 #define __SIZEOF_PTRDIFF_T__ 8, line 1 (indirect)
 #define __amd64 1, line 1 (indirect)
 #define __amd64__ 1, line 1 (indirect)
 #define __x86_64 1, line 1 (indirect)
 #define __x86_64__ 1, line 1 (indirect)
 #define __ATOMIC_HLE_ACQUIRE 65536, line 1 (indirect)
 #define __ATOMIC_HLE_RELEASE 131072, line 1 (indirect)
 #define __k8 1, line 1 (indirect)
 #define __k8__ 1, line 1 (indirect)
 #define __code_model_small__ 1, line 1 (indirect)
 #define __MMX__ 1, line 1 (indirect)
 #define __SSE__ 1, line 1 (indirect)
 #define __SSE2__ 1, line 1 (indirect)
 #define __FXSR__ 1, line 1 (indirect)
 #define __SSE_MATH__ 1, line 1 (indirect)
 #define __SSE2_MATH__ 1, line 1 (indirect)
 #define __gnu_linux__ 1, line 1 (indirect)
 #define __linux 1, line 1 (indirect)
 #define __linux__ 1, line 1 (indirect)
 #define linux 1, line 1 (indirect)
 #define __unix 1, line 1 (indirect)
 #define __unix__ 1, line 1 (indirect)
 #define unix 1, line 1 (indirect)
 #define __ELF__ 1, line 1 (indirect)
 #define __DECIMAL_BID_FORMAT__ 1, line 1 (indirect)

EOF

cat macro.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=macro testfile-debug

cat macro.out | sed -e "s/.debug_macro' at offset 0xab5/.zdebug_macro' at offset 0x1aa7/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=macro testfile-zdebug

cat > line.out << \EOF

DWARF section [34] '.debug_line' at offset 0x104c:

Table at offset 0:

 Length:                         70
 DWARF version:                  2
 Prologue length:                40
 Address size:                   8
 Segment selector size:          0
 Min instruction length:         1
 Max operations per instruction: 1
 Initial value if 'is_stmt':     1
 Line base:                      -5
 Line range:                     14
 Opcode base:                    13

Opcodes:
  [ 1]  0 arguments
  [ 2]  1 argument
  [ 3]  1 argument
  [ 4]  1 argument
  [ 5]  1 argument
  [ 6]  0 arguments
  [ 7]  0 arguments
  [ 8]  0 arguments
  [ 9]  1 argument
  [10]  0 arguments
  [11]  0 arguments
  [12]  1 argument

Directory table:

File name table:
 Entry Dir   Time      Size      Name
 1     0     0         0         testfile-zdebug.c

Line number statements:
 [    32] extended opcode 2:  set address to 0x4003c0
 [    3d] special opcode 22: address+0 = 0x4003c0, line+4 = 5
 [    3e] special opcode 20: address+0 = 0x4003c0, line+2 = 7
 [    3f] special opcode 104: address+6 = 0x4003c6, line+2 = 9
 [    40] special opcode 77: address+4 = 0x4003ca, line+3 = 12
 [    41] special opcode 62: address+3 = 0x4003cd, line+2 = 14
 [    42] special opcode 86: address+5 = 0x4003d2, line-2 = 12
 [    43] special opcode 76: address+4 = 0x4003d6, line+2 = 14
 [    44] special opcode 47: address+2 = 0x4003d8, line+1 = 15
 [    45] advance address by 1 to 0x4003d9
 [    47] extended opcode 1:  end of sequence
EOF

cat line.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=line testfile-debug

cat line.out | sed -e "s/.debug_line' at offset 0x104c/.zdebug_line' at offset 0x1d53/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=line testfile-zdebug


cat > frame.out << \EOF

Call frame information section [16] '.eh_frame' at offset 0x5b8:

 [     0] CIE length=20
   CIE_id:                   0
   version:                  1
   augmentation:             "zR"
   code_alignment_factor:    1
   data_alignment_factor:    -8
   return_address_register:  16
   Augmentation data:        0x1b (FDE address encoding: sdata4 pcrel)

   Program:
     def_cfa r7 (rsp) at offset 8
     offset r16 (rip) at cfa-8
     nop
     nop

 [    18] FDE length=20 cie=[     0]
   CIE_pointer:              28
   initial_location:         0x00000000ffffff08 (offset: 0x4e0)
   address_range:            0x2 (end offset: 0x4e2)

   Program:
     nop
     nop
     nop
     nop
     nop
     nop
     nop

 [    30] FDE length=44 cie=[     0]
   CIE_pointer:              52
   initial_location:         0x00000000ffffff00 (offset: 0x4f0)
   address_range:            0x89 (end offset: 0x579)

   Program:
     advance_loc 17 to 0x501
     offset r12 (r12) at cfa-40
     offset r6 (rbp) at cfa-48
     advance_loc 31 to 0x520
     def_cfa_offset 64
     offset r3 (rbx) at cfa-56
     offset r15 (r15) at cfa-16
     offset r14 (r14) at cfa-24
     offset r13 (r13) at cfa-32
     advance_loc1 88 to 0x578
     def_cfa_offset 8
     nop
     nop
     nop
     nop
     nop
     nop
     nop
     nop
     nop
     nop
     nop

 [    60] FDE length=36 cie=[     0]
   CIE_pointer:              100
   initial_location:         0x00000000fffffd80 (offset: 0x3a0)
   address_range:            0x20 (end offset: 0x3c0)

   Program:
     def_cfa_offset 16
     advance_loc 6 to 0x3a6
     def_cfa_offset 24
     advance_loc 10 to 0x3b0
     def_cfa_expression 11
          [ 0] breg7 8
          [ 2] breg16 0
          [ 4] lit15
          [ 5] and
          [ 6] lit11
          [ 7] ge
          [ 8] lit3
          [ 9] shl
          [10] plus
     nop
     nop
     nop
     nop

 [    88] Zero terminator

Call frame search table section [17] '.eh_frame_hdr':
 version:          1
 eh_frame_ptr_enc: 0x1b (sdata4 pcrel)
 fde_count_enc:    0x3 (udata4)
 table_enc:        0x3b (sdata4 datarel)
 eh_frame_ptr:     0xffffffffffffff70 (offset: 0x5b8)
 fde_count:        3
 Table:
  0xfffffd5c (offset:  0x3a0) -> 0xffffffd4 fde=[    60]
  0xfffffe9c (offset:  0x4e0) -> 0xffffff8c fde=[    18]
  0xfffffeac (offset:  0x4f0) -> 0xffffffa4 fde=[    30]

DWARF section [36] '.debug_frame' at offset 0x29b8:

 [     0] CIE length=20
   CIE_id:                   18446744073709551615
   version:                  1
   augmentation:             ""
   code_alignment_factor:    1
   data_alignment_factor:    -8
   return_address_register:  16

   Program:
     def_cfa r7 (rsp) at offset 8
     offset r16 (rip) at cfa-8
     nop
     nop
     nop
     nop
     nop
     nop

 [    18] FDE length=20 cie=[     0]
   CIE_pointer:              0
   initial_location:         0x00000000004003c0
   address_range:            0x19

   Program:
EOF

cat frame.out | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=frame testfile-debug

cat frame.out | sed -e "s/.debug_frame' at offset 0x29b8/.zdebug_frame' at offset 0x2728/" | testrun_compare ${abs_top_builddir}/src/readelf -U --debug-dump=frame testfile-zdebug


