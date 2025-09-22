#! /bin/sh
# Copyright (C) 2015 Red Hat, Inc.
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

# See run-readelf-zdebug.sh for testfile.

testfiles testfile-zdebug
testrun_compare ${abs_top_builddir}/src/readelf -z -p.zdebug_str testfile-zdebug <<\EOF

String section [35] '.zdebug_str' contains 2431 bytes (6433 uncompressed) at offset 0x1da3:
  [     0]  UINT64_MAX 18446744073709551615UL
  [    22]  __DBL_DENORM_MIN__ ((double)4.94065645841246544177e-324L)
  [    5c]  __linux 1
  [    66]  __SIZEOF_SIZE_T__ 8
  [    7a]  __UINTPTR_TYPE__ long unsigned int
  [    9d]  __SIZEOF_POINTER__ 8
  [    b2]  __UINT8_MAX__ 255
  [    c4]  __PTRDIFF_MAX__ 9223372036854775807L
  [    e9]  __DEC64_MANT_DIG__ 16
  [    ff]  __FLT_RADIX__ 2
  [   10f]  __DEC32_MIN__ 1E-95DF
  [   125]  __unix__ 1
  [   130]  testfile-zdebug.c
  [   142]  __UINT_LEAST64_MAX__ 18446744073709551615UL
  [   16e]  __SIZEOF_WINT_T__ 4
  [   182]  __LONG_MAX__ 9223372036854775807L
  [   1a4]  __LDBL_MIN__ 3.36210314311209350626e-4932L
  [   1cf]  __GCC_ATOMIC_SHORT_LOCK_FREE 2
  [   1ee]  __LP64__ 1
  [   1f9]  __UINT64_C(c) c ## UL
  [   20f]  __DBL_HAS_INFINITY__ 1
  [   226]  __SSE2_MATH__ 1
  [   236]  __linux__ 1
  [   242]  __STDC_HOSTED__ 1
  [   254]  __WINT_MIN__ 0U
  [   264]  __x86_64__ 1
  [   271]  __UINT32_TYPE__ unsigned int
  [   28e]  __UINT_LEAST8_MAX__ 255
  [   2a6]  __DEC64_SUBNORMAL_MIN__ 0.000000000000001E-383DD
  [   2d7]  __FLT_MAX__ 3.40282346638528859812e+38F
  [   2ff]  long unsigned int
  [   311]  __DBL_MANT_DIG__ 53
  [   325]  linux 1
  [   32d]  __DBL_HAS_QUIET_NAN__ 1
  [   345]  __UINT8_TYPE__ unsigned char
  [   362]  __DEC32_MAX_EXP__ 97
  [   377]  __INT32_TYPE__ int
  [   38a]  __SIG_ATOMIC_TYPE__ int
  [   3a2]  __DEC64_MAX_EXP__ 385
  [   3b8]  __DBL_MIN_EXP__ (-1021)
  [   3d0]  _LP64 1
  [   3d8]  __LDBL_HAS_INFINITY__ 1
  [   3f0]  __INT_FAST64_TYPE__ long int
  [   40d]  __gnu_linux__ 1
  [   41d]  __GCC_ATOMIC_WCHAR_T_LOCK_FREE 2
  [   43e]  __UINT_FAST64_TYPE__ long unsigned int
  [   465]  __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
  [   48c]  __UINT16_MAX__ 65535
  [   4a1]  __FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__
  [   4ce]  __PRAGMA_REDEFINE_EXTNAME 1
  [   4ea]  __INT_LEAST16_TYPE__ short int
  [   509]  __k8__ 1
  [   512]  __DECIMAL_DIG__ 21
  [   525]  main
  [   52a]  __DBL_MAX__ ((double)1.79769313486231570815e+308L)
  [   55d]  __INT16_TYPE__ short int
  [   576]  __LDBL_HAS_QUIET_NAN__ 1
  [   58f]  __SIZEOF_DOUBLE__ 8
  [   5a3]  __DEC32_SUBNORMAL_MIN__ 0.000001E-95DF
  [   5ca]  __ATOMIC_SEQ_CST 5
  [   5dd]  __UINT64_TYPE__ long unsigned int
  [   5ff]  __INT_LEAST32_TYPE__ int
  [   618]  __INT_LEAST64_MAX__ 9223372036854775807L
  [   641]  __OPTIMIZE__ 1
  [   650]  __INTMAX_C(c) c ## L
  [   665]  __GCC_ATOMIC_CHAR32_T_LOCK_FREE 2
  [   687]  __INT_FAST8_MAX__ 127
  [   69d]  __PTRDIFF_TYPE__ long int
  [   6b7]  __LDBL_MIN_EXP__ (-16381)
  [   6d1]  __SIZEOF_LONG_LONG__ 8
  [   6e8]  __FLT_DIG__ 6
  [   6f6]  __UINTMAX_MAX__ 18446744073709551615UL
  [   71d]  __SIZEOF_WCHAR_T__ 4
  [   732]  __INT64_C(c) c ## L
  [   746]  __UINTPTR_MAX__ 18446744073709551615UL
  [   76d]  __FLT_MAX_10_EXP__ 38
  [   783]  __FLT_MIN__ 1.17549435082228750797e-38F
  [   7ab]  __UINT_LEAST64_TYPE__ long unsigned int
  [   7d3]  __SIZEOF_LONG_DOUBLE__ 16
  [   7ed]  __SIZE_MAX__ 18446744073709551615UL
  [   811]  __INT8_C(c) c
  [   81f]  __amd64__ 1
  [   82b]  __INT_LEAST64_TYPE__ long int
  [   849]  __INT_FAST64_MAX__ 9223372036854775807L
  [   871]  __DEC_EVAL_METHOD__ 2
  [   887]  __DEC32_MAX__ 9.999999E96DF
  [   8a3]  __GNUC_MINOR__ 8
  [   8b4]  __WCHAR_MAX__ 2147483647
  [   8cd]  __SIZE_TYPE__ long unsigned int
  [   8ed]  __INT8_MAX__ 127
  [   8fe]  __INTMAX_MAX__ 9223372036854775807L
  [   922]  __ATOMIC_HLE_RELEASE 131072
  [   93e]  __FLT_HAS_QUIET_NAN__ 1
  [   956]  __DBL_EPSILON__ ((double)2.22044604925031308085e-16L)
  [   98c]  __FLT_MIN_EXP__ (-125)
  [   9a3]  __INT_LEAST8_MAX__ 127
  [   9ba]  __SIZEOF_INT128__ 16
  [   9cf]  __INTPTR_MAX__ 9223372036854775807L
  [   9f3]  __INTPTR_TYPE__ long int
  [   a0c]  __LDBL_MIN_10_EXP__ (-4931)
  [   a28]  __GCC_ATOMIC_POINTER_LOCK_FREE 2
  [   a49]  __UINT_LEAST32_MAX__ 4294967295U
  [   a6a]  __SIZEOF_SHORT__ 2
  [   a7d]  __LDBL_MAX_10_EXP__ 4932
  [   a96]  __INT16_C(c) c
  [   aa5]  __MMX__ 1
  [   aaf]  unix 1
  [   ab6]  __FLT_MAX_EXP__ 128
  [   aca]  __DEC64_MAX__ 9.999999999999999E384DD
  [   af0]  __FLT_EPSILON__ 1.19209289550781250000e-7F
  [   b1b]  __INT_FAST16_TYPE__ long int
  [   b38]  __VERSION__ "4.8.2 20140120 (Red Hat 4.8.2-15)"
  [   b68]  __GCC_ATOMIC_LLONG_LOCK_FREE 2
  [   b87]  __DEC128_MIN_EXP__ (-6142)
  [   ba2]  __ATOMIC_RELEASE 3
  [   bb5]  __GNUC_PATCHLEVEL__ 2
  [   bcb]  __UINT_FAST64_MAX__ 18446744073709551615UL
  [   bf6]  __DBL_DECIMAL_DIG__ 17
  [   c0d]  __DBL_DIG__ 15
  [   c1c]  __FLT_MANT_DIG__ 24
  [   c30]  __FLT_DECIMAL_DIG__ 9
  [   c46]  __INT16_MAX__ 32767
  [   c5a]  __DEC128_MIN__ 1E-6143DL
  [   c73]  __BIGGEST_ALIGNMENT__ 16
  [   c8c]  __INT64_MAX__ 9223372036854775807L
  [   caf]  __INT_FAST32_TYPE__ long int
  [   ccc]  __GCC_ATOMIC_INT_LOCK_FREE 2
  [   ce9]  __DEC128_MAX_EXP__ 6145
  [   d01]  __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
  [   d26]  __FXSR__ 1
  [   d31]  __INT8_TYPE__ signed char
  [   d4b]  __ATOMIC_ACQ_REL 4
  [   d5e]  __UINT_LEAST16_MAX__ 65535
  [   d79]  __UINTMAX_TYPE__ long unsigned int
  [   d9c]  __UINT_FAST8_MAX__ 255
  [   db3]  __ORDER_BIG_ENDIAN__ 4321
  [   dcd]  __INT_LEAST32_MAX__ 2147483647
  [   dec]  __UINT_LEAST16_TYPE__ short unsigned int
  [   e15]  __INT_FAST8_TYPE__ signed char
  [   e34]  __DBL_MAX_EXP__ 1024
  [   e49]  __STDC__ 1
  [   e54]  __ELF__ 1
  [   e5e]  __FLT_EVAL_METHOD__ 0
  [   e74]  __ATOMIC_ACQUIRE 2
  [   e87]  __DEC64_EPSILON__ 1E-15DD
  [   ea1]  __INT32_MAX__ 2147483647
  [   eba]  __GCC_ATOMIC_CHAR_LOCK_FREE 2
  [   ed8]  __DEC128_EPSILON__ 1E-33DL
  [   ef3]  __UINT_FAST8_TYPE__ unsigned char
  [   f15]  __amd64 1
  [   f1f]  __DEC32_MIN_EXP__ (-94)
  [   f37]  __GCC_HAVE_DWARF2_CFI_ASM 1
  [   f53]  __LDBL_DIG__ 18
  [   f63]  __UINT32_MAX__ 4294967295U
  [   f7e]  __GNUC_GNU_INLINE__ 1
  [   f94]  __SSE2__ 1
  [   f9f]  __ATOMIC_HLE_ACQUIRE 65536
  [   fba]  __SSE_MATH__ 1
  [   fc9]  __INT_FAST16_MAX__ 9223372036854775807L
  [   ff1]  __LDBL_MAX__ 1.18973149535723176502e+4932L
  [  101c]  __DBL_MIN__ ((double)2.22507385850720138309e-308L)
  [  104f]  __DEC128_MANT_DIG__ 34
  [  1066]  __INT32_C(c) c
  [  1075]  __DEC64_MIN_EXP__ (-382)
  [  108e]  __WCHAR_MIN__ (-__WCHAR_MAX__ - 1)
  [  10b1]  __GCC_ATOMIC_CHAR16_T_LOCK_FREE 2
  [  10d3]  __LDBL_MAX_EXP__ 16384
  [  10ea]  __DEC32_MANT_DIG__ 7
  [  10ff]  __DEC128_MAX__ 9.999999999999999999999999999999999E6144DL
  [  1139]  __CHAR32_TYPE__ unsigned int
  [  1156]  __INT_LEAST8_TYPE__ signed char
  [  1176]  __UINT16_C(c) c
  [  1186]  __GCC_ATOMIC_BOOL_LOCK_FREE 2
  [  11a4]  __SIZEOF_FLOAT__ 4
  [  11b7]  __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1
  [  11dc]  __DBL_MAX_10_EXP__ 308
  [  11f3]  __LDBL_EPSILON__ 1.08420217248550443401e-19L
  [  1220]  __ORDER_PDP_ENDIAN__ 3412
  [  123a]  __ORDER_LITTLE_ENDIAN__ 1234
  [  1257]  __WINT_TYPE__ unsigned int
  [  1272]  __unix 1
  [  127b]  __ATOMIC_RELAXED 0
  [  128e]  __UINT_FAST32_MAX__ 18446744073709551615UL
  [  12b9]  __INT_FAST32_MAX__ 9223372036854775807L
  [  12e1]  __SIG_ATOMIC_MAX__ 2147483647
  [  12ff]  __UINT_FAST32_TYPE__ long unsigned int
  [  1326]  __INT_MAX__ 2147483647
  [  133d]  __GXX_ABI_VERSION 1002
  [  1354]  __SIZEOF_INT__ 4
  [  1365]  char
  [  136a]  __UINT_FAST16_TYPE__ long unsigned int
  [  1391]  __LDBL_DENORM_MIN__ 3.64519953188247460253e-4951L
  [  13c3]  __WINT_MAX__ 4294967295U
  [  13dc]  __FLT_HAS_INFINITY__ 1
  [  13f3]  __SHRT_MAX__ 32767
  [  1406]  __INT_LEAST16_MAX__ 32767
  [  1420]  __LONG_LONG_MAX__ 9223372036854775807LL
  [  1448]  __SIZEOF_LONG__ 8
  [  145a]  __INTMAX_TYPE__ long int
  [  1473]  __LDBL_HAS_DENORM__ 1
  [  1489]  __code_model_small__ 1
  [  14a0]  __REGISTER_PREFIX__ 
  [  14b5]  __ATOMIC_CONSUME 1
  [  14c8]  __DEC128_SUBNORMAL_MIN__ 0.000000000000000000000000000000001E-6143DL
  [  150d]  __GNUC__ 4
  [  1518]  __UINT16_TYPE__ short unsigned int
  [  153b]  __SSE__ 1
  [  1545]  __UINT32_C(c) c ## U
  [  155a]  __k8 1
  [  1561]  __UINTMAX_C(c) c ## UL
  [  1578]  __GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1
  [  159d]  __SIZEOF_PTRDIFF_T__ 8
  [  15b4]  __CHAR_BIT__ 8
  [  15c3]  __SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)
  [  15f0]  __DEC32_EPSILON__ 1E-6DF
  [  1609]  __UINT_LEAST32_TYPE__ unsigned int
  [  162c]  __DBL_HAS_DENORM__ 1
  [  1641]  /tmp
  [  1646]  __LDBL_MANT_DIG__ 64
  [  165b]  __GCC_ATOMIC_LONG_LOCK_FREE 2
  [  1679]  __DECIMAL_BID_FORMAT__ 1
  [  1692]  __FLT_MIN_10_EXP__ (-37)
  [  16ab]  __GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1
  [  16cf]  __WCHAR_TYPE__ int
  [  16e2]  __FINITE_MATH_ONLY__ 0
  [  16f9]  argc
  [  16fe]  __USER_LABEL_PREFIX__ 
  [  1715]  __CHAR16_TYPE__ short unsigned int
  [  1738]  __UINT64_MAX__ 18446744073709551615UL
  [  175e]  __UINT8_C(c) c
  [  176d]  __x86_64 1
  [  1778]  __UINT_LEAST8_TYPE__ unsigned char
  [  179b]  __INT64_TYPE__ long int
  [  17b3]  __GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1
  [  17d8]  argv
  [  17dd]  __GNUC_RH_RELEASE__ 15
  [  17f4]  __UINT_FAST16_MAX__ 18446744073709551615UL
  [  181f]  __FLT_HAS_DENORM__ 1
  [  1834]  __DEC64_MIN__ 1E-383DD
  [  184b]  __DBL_MIN_10_EXP__ (-307)
  [  1865]  __FLT_DENORM_MIN__ 1.40129846432481707092e-45F
  [  1894]  GNU C 4.8.2 20140120 (Red Hat 4.8.2-15) -mtune=generic -march=x86-64 -g3 -O3 -fuse-ld=gold -fno-asynchronous-unwind-tables
  [  190f]  __SCHAR_MAX__ 127
EOF

exit 0
