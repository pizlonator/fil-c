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

# common.h
# #define ONE 1
# #define TWO 2
# #define THREE 3
#
# #define WORLD "World"
#
# int say (const char *prefix);
#
# #define A 'a'
# #define B b
# #define C "C"
#
# #ifdef THREE
# #undef THREE
# #define THREE(ARG1,ARG2,ARG3) ARG3
# #endif

# hello.c
# #include "common.h"
#
# int
# main (int argc, char **argv)
# {
#   return say (WORLD);
# }

# world.c
# #include "common.h"
#
# int
# say (const char *prefix)
# {
#   return prefix ? ONE : TWO;
# }

# gcc -g3 -c hello.c
# gcc -g3 -c world.c
# gcc -g3 -o testfilemacro hello.o world.o

testfiles testfilemacro

testrun_compare ${abs_top_builddir}/src/readelf --debug-dump=macro testfilemacro <<\EOF

DWARF section [32] '.debug_macro' at offset 0x2480:

 Offset:             0x0
 Version:            4
 Flag:               0x2 (debug_line_offset)
 Offset length:      4
 .debug_line offset: 0x0

 #include offset 0x1a
 start_file 0, [1] /home/mark/src/tests/hello.c
  start_file 1, [2] /home/mark/src/tests/common.h
   #include offset 0x582
  end_file
 end_file

 Offset:             0x1a
 Version:            4
 Flag:               0x0
 Offset length:      4

 #define __STDC__ 1, line 1 (indirect)
 #define __STDC_HOSTED__ 1, line 1 (indirect)
 #define __GNUC__ 4, line 1 (indirect)
 #define __GNUC_MINOR__ 7, line 1 (indirect)
 #define __GNUC_PATCHLEVEL__ 1, line 1 (indirect)
 #define __VERSION__ "4.7.1 20120629 (Red Hat 4.7.1-1)", line 1 (indirect)
 #define __GNUC_RH_RELEASE__ 1, line 1 (indirect)
 #define __ATOMIC_RELAXED 0, line 1 (indirect)
 #define __ATOMIC_SEQ_CST 5, line 1 (indirect)
 #define __ATOMIC_ACQUIRE 2, line 1 (indirect)
 #define __ATOMIC_RELEASE 3, line 1 (indirect)
 #define __ATOMIC_ACQ_REL 4, line 1 (indirect)
 #define __ATOMIC_CONSUME 1, line 1 (indirect)
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
 #define __NO_INLINE__ 1, line 1 (indirect)
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
 #define __k8 1, line 1 (indirect)
 #define __k8__ 1, line 1 (indirect)
 #define __MMX__ 1, line 1 (indirect)
 #define __SSE__ 1, line 1 (indirect)
 #define __SSE2__ 1, line 1 (indirect)
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

 Offset:             0x582
 Version:            4
 Flag:               0x0
 Offset length:      4

 #define ONE 1, line 1 (indirect)
 #define TWO 2, line 2 (indirect)
 #define THREE 3, line 3 (indirect)
 #define WORLD "World", line 5 (indirect)
 #define A 'a', line 9 (indirect)
 #define B b, line 10
 #define C "C", line 11 (indirect)
 #undef THREE, line 14 (indirect)
 #define THREE(ARG1,ARG2,ARG3) ARG3, line 15 (indirect)

 Offset:             0x5bc
 Version:            4
 Flag:               0x2 (debug_line_offset)
 Offset length:      4
 .debug_line offset: 0x47

 #include offset 0x1a
 start_file 0, [1] /home/mark/src/tests/world.c
  start_file 1, [2] /home/mark/src/tests/common.h
   #include offset 0x582
  end_file
 end_file

EOF

exit 0
