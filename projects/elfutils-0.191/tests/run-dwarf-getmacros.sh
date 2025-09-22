#! /bin/sh
# Copyright (C) 2009, 2014 Red Hat, Inc.
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

testfiles testfile51

testrun_compare ${abs_builddir}/dwarf-getmacros testfile51 0xb <<\EOF
file /home/petr/proj/elfutils/pending/elfutils/tests/decom/x.c
 __STDC__ 1
 __STDC_HOSTED__ 1
 __GNUC__ 4
 __GNUC_MINOR__ 3
 __GNUC_PATCHLEVEL__ 2
 __GNUC_RH_RELEASE__ 7
 __SIZE_TYPE__ long unsigned int
 __PTRDIFF_TYPE__ long int
 __WCHAR_TYPE__ int
 __WINT_TYPE__ unsigned int
 __INTMAX_TYPE__ long int
 __UINTMAX_TYPE__ long unsigned int
 __GXX_ABI_VERSION 1002
 __SCHAR_MAX__ 127
 __SHRT_MAX__ 32767
 __INT_MAX__ 2147483647
 __LONG_MAX__ 9223372036854775807L
 __LONG_LONG_MAX__ 9223372036854775807LL
 __WCHAR_MAX__ 2147483647
 __CHAR_BIT__ 8
 __INTMAX_MAX__ 9223372036854775807L
 __FLT_EVAL_METHOD__ 0
 __DEC_EVAL_METHOD__ 2
 __FLT_RADIX__ 2
 __FLT_MANT_DIG__ 24
 __FLT_DIG__ 6
 __FLT_MIN_EXP__ (-125)
 __FLT_MIN_10_EXP__ (-37)
 __FLT_MAX_EXP__ 128
 __FLT_MAX_10_EXP__ 38
 __FLT_MAX__ 3.40282347e+38F
 __FLT_MIN__ 1.17549435e-38F
 __FLT_EPSILON__ 1.19209290e-7F
 __FLT_DENORM_MIN__ 1.40129846e-45F
 __FLT_HAS_DENORM__ 1
 __FLT_HAS_INFINITY__ 1
 __FLT_HAS_QUIET_NAN__ 1
 __DBL_MANT_DIG__ 53
 __DBL_DIG__ 15
 __DBL_MIN_EXP__ (-1021)
 __DBL_MIN_10_EXP__ (-307)
 __DBL_MAX_EXP__ 1024
 __DBL_MAX_10_EXP__ 308
 __DBL_MAX__ 1.7976931348623157e+308
 __DBL_MIN__ 2.2250738585072014e-308
 __DBL_EPSILON__ 2.2204460492503131e-16
 __DBL_DENORM_MIN__ 4.9406564584124654e-324
 __DBL_HAS_DENORM__ 1
 __DBL_HAS_INFINITY__ 1
 __DBL_HAS_QUIET_NAN__ 1
 __LDBL_MANT_DIG__ 64
 __LDBL_DIG__ 18
 __LDBL_MIN_EXP__ (-16381)
 __LDBL_MIN_10_EXP__ (-4931)
 __LDBL_MAX_EXP__ 16384
 __LDBL_MAX_10_EXP__ 4932
 __DECIMAL_DIG__ 21
 __LDBL_MAX__ 1.18973149535723176502e+4932L
 __LDBL_MIN__ 3.36210314311209350626e-4932L
 __LDBL_EPSILON__ 1.08420217248550443401e-19L
 __LDBL_DENORM_MIN__ 3.64519953188247460253e-4951L
 __LDBL_HAS_DENORM__ 1
 __LDBL_HAS_INFINITY__ 1
 __LDBL_HAS_QUIET_NAN__ 1
 __DEC32_MANT_DIG__ 7
 __DEC32_MIN_EXP__ (-95)
 __DEC32_MAX_EXP__ 96
 __DEC32_MIN__ 1E-95DF
 __DEC32_MAX__ 9.999999E96DF
 __DEC32_EPSILON__ 1E-6DF
 __DEC32_DEN__ 0.000001E-95DF
 __DEC64_MANT_DIG__ 16
 __DEC64_MIN_EXP__ (-383)
 __DEC64_MAX_EXP__ 384
 __DEC64_MIN__ 1E-383DD
 __DEC64_MAX__ 9.999999999999999E384DD
 __DEC64_EPSILON__ 1E-15DD
 __DEC64_DEN__ 0.000000000000001E-383DD
 __DEC128_MANT_DIG__ 34
 __DEC128_MIN_EXP__ (-6143)
 __DEC128_MAX_EXP__ 6144
 __DEC128_MIN__ 1E-6143DL
 __DEC128_MAX__ 9.999999999999999999999999999999999E6144DL
 __DEC128_EPSILON__ 1E-33DL
 __DEC128_DEN__ 0.000000000000000000000000000000001E-6143DL
 __REGISTER_PREFIX__ 
 __USER_LABEL_PREFIX__ 
 __VERSION__ "4.3.2 20081105 (Red Hat 4.3.2-7)"
 __GNUC_GNU_INLINE__ 1
 _LP64 1
 __LP64__ 1
 __NO_INLINE__ 1
 __FINITE_MATH_ONLY__ 0
 __GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1
 __GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1
 __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
 __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1
 __SIZEOF_INT__ 4
 __SIZEOF_LONG__ 8
 __SIZEOF_LONG_LONG__ 8
 __SIZEOF_SHORT__ 2
 __SIZEOF_FLOAT__ 4
 __SIZEOF_DOUBLE__ 8
 __SIZEOF_LONG_DOUBLE__ 16
 __SIZEOF_SIZE_T__ 8
 __SIZEOF_WCHAR_T__ 4
 __SIZEOF_WINT_T__ 4
 __SIZEOF_PTRDIFF_T__ 8
 __SIZEOF_POINTER__ 8
 __amd64 1
 __amd64__ 1
 __x86_64 1
 __x86_64__ 1
 __k8 1
 __k8__ 1
 __MMX__ 1
 __SSE__ 1
 __SSE2__ 1
 __SSE_MATH__ 1
 __SSE2_MATH__ 1
 __gnu_linux__ 1
 __linux 1
 __linux__ 1
 linux 1
 __unix 1
 __unix__ 1
 unix 1
 __ELF__ 1
 __DECIMAL_BID_FORMAT__ 1
 macro1 ble
/file
EOF

testrun_compare ${abs_builddir}/dwarf-getmacros testfile51 0x84 <<\EOF
file /home/petr/proj/elfutils/pending/elfutils/tests/decom/y.c
 __STDC__ 1
 __STDC_HOSTED__ 1
 __GNUC__ 4
 __GNUC_MINOR__ 3
 __GNUC_PATCHLEVEL__ 2
 __GNUC_RH_RELEASE__ 7
 __SIZE_TYPE__ long unsigned int
 __PTRDIFF_TYPE__ long int
 __WCHAR_TYPE__ int
 __WINT_TYPE__ unsigned int
 __INTMAX_TYPE__ long int
 __UINTMAX_TYPE__ long unsigned int
 __GXX_ABI_VERSION 1002
 __SCHAR_MAX__ 127
 __SHRT_MAX__ 32767
 __INT_MAX__ 2147483647
 __LONG_MAX__ 9223372036854775807L
 __LONG_LONG_MAX__ 9223372036854775807LL
 __WCHAR_MAX__ 2147483647
 __CHAR_BIT__ 8
 __INTMAX_MAX__ 9223372036854775807L
 __FLT_EVAL_METHOD__ 0
 __DEC_EVAL_METHOD__ 2
 __FLT_RADIX__ 2
 __FLT_MANT_DIG__ 24
 __FLT_DIG__ 6
 __FLT_MIN_EXP__ (-125)
 __FLT_MIN_10_EXP__ (-37)
 __FLT_MAX_EXP__ 128
 __FLT_MAX_10_EXP__ 38
 __FLT_MAX__ 3.40282347e+38F
 __FLT_MIN__ 1.17549435e-38F
 __FLT_EPSILON__ 1.19209290e-7F
 __FLT_DENORM_MIN__ 1.40129846e-45F
 __FLT_HAS_DENORM__ 1
 __FLT_HAS_INFINITY__ 1
 __FLT_HAS_QUIET_NAN__ 1
 __DBL_MANT_DIG__ 53
 __DBL_DIG__ 15
 __DBL_MIN_EXP__ (-1021)
 __DBL_MIN_10_EXP__ (-307)
 __DBL_MAX_EXP__ 1024
 __DBL_MAX_10_EXP__ 308
 __DBL_MAX__ 1.7976931348623157e+308
 __DBL_MIN__ 2.2250738585072014e-308
 __DBL_EPSILON__ 2.2204460492503131e-16
 __DBL_DENORM_MIN__ 4.9406564584124654e-324
 __DBL_HAS_DENORM__ 1
 __DBL_HAS_INFINITY__ 1
 __DBL_HAS_QUIET_NAN__ 1
 __LDBL_MANT_DIG__ 64
 __LDBL_DIG__ 18
 __LDBL_MIN_EXP__ (-16381)
 __LDBL_MIN_10_EXP__ (-4931)
 __LDBL_MAX_EXP__ 16384
 __LDBL_MAX_10_EXP__ 4932
 __DECIMAL_DIG__ 21
 __LDBL_MAX__ 1.18973149535723176502e+4932L
 __LDBL_MIN__ 3.36210314311209350626e-4932L
 __LDBL_EPSILON__ 1.08420217248550443401e-19L
 __LDBL_DENORM_MIN__ 3.64519953188247460253e-4951L
 __LDBL_HAS_DENORM__ 1
 __LDBL_HAS_INFINITY__ 1
 __LDBL_HAS_QUIET_NAN__ 1
 __DEC32_MANT_DIG__ 7
 __DEC32_MIN_EXP__ (-95)
 __DEC32_MAX_EXP__ 96
 __DEC32_MIN__ 1E-95DF
 __DEC32_MAX__ 9.999999E96DF
 __DEC32_EPSILON__ 1E-6DF
 __DEC32_DEN__ 0.000001E-95DF
 __DEC64_MANT_DIG__ 16
 __DEC64_MIN_EXP__ (-383)
 __DEC64_MAX_EXP__ 384
 __DEC64_MIN__ 1E-383DD
 __DEC64_MAX__ 9.999999999999999E384DD
 __DEC64_EPSILON__ 1E-15DD
 __DEC64_DEN__ 0.000000000000001E-383DD
 __DEC128_MANT_DIG__ 34
 __DEC128_MIN_EXP__ (-6143)
 __DEC128_MAX_EXP__ 6144
 __DEC128_MIN__ 1E-6143DL
 __DEC128_MAX__ 9.999999999999999999999999999999999E6144DL
 __DEC128_EPSILON__ 1E-33DL
 __DEC128_DEN__ 0.000000000000000000000000000000001E-6143DL
 __REGISTER_PREFIX__ 
 __USER_LABEL_PREFIX__ 
 __VERSION__ "4.3.2 20081105 (Red Hat 4.3.2-7)"
 __GNUC_GNU_INLINE__ 1
 _LP64 1
 __LP64__ 1
 __NO_INLINE__ 1
 __FINITE_MATH_ONLY__ 0
 __GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1
 __GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1
 __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
 __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1
 __SIZEOF_INT__ 4
 __SIZEOF_LONG__ 8
 __SIZEOF_LONG_LONG__ 8
 __SIZEOF_SHORT__ 2
 __SIZEOF_FLOAT__ 4
 __SIZEOF_DOUBLE__ 8
 __SIZEOF_LONG_DOUBLE__ 16
 __SIZEOF_SIZE_T__ 8
 __SIZEOF_WCHAR_T__ 4
 __SIZEOF_WINT_T__ 4
 __SIZEOF_PTRDIFF_T__ 8
 __SIZEOF_POINTER__ 8
 __amd64 1
 __amd64__ 1
 __x86_64 1
 __x86_64__ 1
 __k8 1
 __k8__ 1
 __MMX__ 1
 __SSE__ 1
 __SSE2__ 1
 __SSE_MATH__ 1
 __SSE2_MATH__ 1
 __gnu_linux__ 1
 __linux 1
 __linux__ 1
 linux 1
 __unix 1
 __unix__ 1
 unix 1
 __ELF__ 1
 __DECIMAL_BID_FORMAT__ 1
 macro2 ble
/file
EOF

testfiles testfile-macros

testrun_compare ${abs_builddir}/dwarf-getmacros testfile-macros 0xb <<\EOF
__STDC__ 1
__STDC_HOSTED__ 1
__GNUC__ 4
__GNUC_MINOR__ 7
__GNUC_PATCHLEVEL__ 0
__VERSION__ "4.7.0 20120507 (Red Hat 4.7.0-5)"
__GNUC_RH_RELEASE__ 5
__ATOMIC_RELAXED 0
__ATOMIC_SEQ_CST 5
__ATOMIC_ACQUIRE 2
__ATOMIC_RELEASE 3
__ATOMIC_ACQ_REL 4
__ATOMIC_CONSUME 1
__FINITE_MATH_ONLY__ 0
_LP64 1
__LP64__ 1
__SIZEOF_INT__ 4
__SIZEOF_LONG__ 8
__SIZEOF_LONG_LONG__ 8
__SIZEOF_SHORT__ 2
__SIZEOF_FLOAT__ 4
__SIZEOF_DOUBLE__ 8
__SIZEOF_LONG_DOUBLE__ 16
__SIZEOF_SIZE_T__ 8
__CHAR_BIT__ 8
__BIGGEST_ALIGNMENT__ 16
__ORDER_LITTLE_ENDIAN__ 1234
__ORDER_BIG_ENDIAN__ 4321
__ORDER_PDP_ENDIAN__ 3412
__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
__FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__
__SIZEOF_POINTER__ 8
__SIZE_TYPE__ long unsigned int
__PTRDIFF_TYPE__ long int
__WCHAR_TYPE__ int
__WINT_TYPE__ unsigned int
__INTMAX_TYPE__ long int
__UINTMAX_TYPE__ long unsigned int
__CHAR16_TYPE__ short unsigned int
__CHAR32_TYPE__ unsigned int
__SIG_ATOMIC_TYPE__ int
__INT8_TYPE__ signed char
__INT16_TYPE__ short int
__INT32_TYPE__ int
__INT64_TYPE__ long int
__UINT8_TYPE__ unsigned char
__UINT16_TYPE__ short unsigned int
__UINT32_TYPE__ unsigned int
__UINT64_TYPE__ long unsigned int
__INT_LEAST8_TYPE__ signed char
__INT_LEAST16_TYPE__ short int
__INT_LEAST32_TYPE__ int
__INT_LEAST64_TYPE__ long int
__UINT_LEAST8_TYPE__ unsigned char
__UINT_LEAST16_TYPE__ short unsigned int
__UINT_LEAST32_TYPE__ unsigned int
__UINT_LEAST64_TYPE__ long unsigned int
__INT_FAST8_TYPE__ signed char
__INT_FAST16_TYPE__ long int
__INT_FAST32_TYPE__ long int
__INT_FAST64_TYPE__ long int
__UINT_FAST8_TYPE__ unsigned char
__UINT_FAST16_TYPE__ long unsigned int
__UINT_FAST32_TYPE__ long unsigned int
__UINT_FAST64_TYPE__ long unsigned int
__INTPTR_TYPE__ long int
__UINTPTR_TYPE__ long unsigned int
__GXX_ABI_VERSION 1002
__SCHAR_MAX__ 127
__SHRT_MAX__ 32767
__INT_MAX__ 2147483647
__LONG_MAX__ 9223372036854775807L
__LONG_LONG_MAX__ 9223372036854775807LL
__WCHAR_MAX__ 2147483647
__WCHAR_MIN__ (-__WCHAR_MAX__ - 1)
__WINT_MAX__ 4294967295U
__WINT_MIN__ 0U
__PTRDIFF_MAX__ 9223372036854775807L
__SIZE_MAX__ 18446744073709551615UL
__INTMAX_MAX__ 9223372036854775807L
__INTMAX_C(c) c ## L
__UINTMAX_MAX__ 18446744073709551615UL
__UINTMAX_C(c) c ## UL
__SIG_ATOMIC_MAX__ 2147483647
__SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)
__INT8_MAX__ 127
__INT16_MAX__ 32767
__INT32_MAX__ 2147483647
__INT64_MAX__ 9223372036854775807L
__UINT8_MAX__ 255
__UINT16_MAX__ 65535
__UINT32_MAX__ 4294967295U
__UINT64_MAX__ 18446744073709551615UL
__INT_LEAST8_MAX__ 127
__INT8_C(c) c
__INT_LEAST16_MAX__ 32767
__INT16_C(c) c
__INT_LEAST32_MAX__ 2147483647
__INT32_C(c) c
__INT_LEAST64_MAX__ 9223372036854775807L
__INT64_C(c) c ## L
__UINT_LEAST8_MAX__ 255
__UINT8_C(c) c
__UINT_LEAST16_MAX__ 65535
__UINT16_C(c) c
__UINT_LEAST32_MAX__ 4294967295U
__UINT32_C(c) c ## U
__UINT_LEAST64_MAX__ 18446744073709551615UL
__UINT64_C(c) c ## UL
__INT_FAST8_MAX__ 127
__INT_FAST16_MAX__ 9223372036854775807L
__INT_FAST32_MAX__ 9223372036854775807L
__INT_FAST64_MAX__ 9223372036854775807L
__UINT_FAST8_MAX__ 255
__UINT_FAST16_MAX__ 18446744073709551615UL
__UINT_FAST32_MAX__ 18446744073709551615UL
__UINT_FAST64_MAX__ 18446744073709551615UL
__INTPTR_MAX__ 9223372036854775807L
__UINTPTR_MAX__ 18446744073709551615UL
__FLT_EVAL_METHOD__ 0
__DEC_EVAL_METHOD__ 2
__FLT_RADIX__ 2
__FLT_MANT_DIG__ 24
__FLT_DIG__ 6
__FLT_MIN_EXP__ (-125)
__FLT_MIN_10_EXP__ (-37)
__FLT_MAX_EXP__ 128
__FLT_MAX_10_EXP__ 38
__FLT_DECIMAL_DIG__ 9
__FLT_MAX__ 3.40282346638528859812e+38F
__FLT_MIN__ 1.17549435082228750797e-38F
__FLT_EPSILON__ 1.19209289550781250000e-7F
__FLT_DENORM_MIN__ 1.40129846432481707092e-45F
__FLT_HAS_DENORM__ 1
__FLT_HAS_INFINITY__ 1
__FLT_HAS_QUIET_NAN__ 1
__DBL_MANT_DIG__ 53
__DBL_DIG__ 15
__DBL_MIN_EXP__ (-1021)
__DBL_MIN_10_EXP__ (-307)
__DBL_MAX_EXP__ 1024
__DBL_MAX_10_EXP__ 308
__DBL_DECIMAL_DIG__ 17
__DBL_MAX__ ((double)1.79769313486231570815e+308L)
__DBL_MIN__ ((double)2.22507385850720138309e-308L)
__DBL_EPSILON__ ((double)2.22044604925031308085e-16L)
__DBL_DENORM_MIN__ ((double)4.94065645841246544177e-324L)
__DBL_HAS_DENORM__ 1
__DBL_HAS_INFINITY__ 1
__DBL_HAS_QUIET_NAN__ 1
__LDBL_MANT_DIG__ 64
__LDBL_DIG__ 18
__LDBL_MIN_EXP__ (-16381)
__LDBL_MIN_10_EXP__ (-4931)
__LDBL_MAX_EXP__ 16384
__LDBL_MAX_10_EXP__ 4932
__DECIMAL_DIG__ 21
__LDBL_MAX__ 1.18973149535723176502e+4932L
__LDBL_MIN__ 3.36210314311209350626e-4932L
__LDBL_EPSILON__ 1.08420217248550443401e-19L
__LDBL_DENORM_MIN__ 3.64519953188247460253e-4951L
__LDBL_HAS_DENORM__ 1
__LDBL_HAS_INFINITY__ 1
__LDBL_HAS_QUIET_NAN__ 1
__DEC32_MANT_DIG__ 7
__DEC32_MIN_EXP__ (-94)
__DEC32_MAX_EXP__ 97
__DEC32_MIN__ 1E-95DF
__DEC32_MAX__ 9.999999E96DF
__DEC32_EPSILON__ 1E-6DF
__DEC32_SUBNORMAL_MIN__ 0.000001E-95DF
__DEC64_MANT_DIG__ 16
__DEC64_MIN_EXP__ (-382)
__DEC64_MAX_EXP__ 385
__DEC64_MIN__ 1E-383DD
__DEC64_MAX__ 9.999999999999999E384DD
__DEC64_EPSILON__ 1E-15DD
__DEC64_SUBNORMAL_MIN__ 0.000000000000001E-383DD
__DEC128_MANT_DIG__ 34
__DEC128_MIN_EXP__ (-6142)
__DEC128_MAX_EXP__ 6145
__DEC128_MIN__ 1E-6143DL
__DEC128_MAX__ 9.999999999999999999999999999999999E6144DL
__DEC128_EPSILON__ 1E-33DL
__DEC128_SUBNORMAL_MIN__ 0.000000000000000000000000000000001E-6143DL
__REGISTER_PREFIX__ 
__USER_LABEL_PREFIX__ 
__GNUC_GNU_INLINE__ 1
__NO_INLINE__ 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1
__GCC_ATOMIC_BOOL_LOCK_FREE 2
__GCC_ATOMIC_CHAR_LOCK_FREE 2
__GCC_ATOMIC_CHAR16_T_LOCK_FREE 2
__GCC_ATOMIC_CHAR32_T_LOCK_FREE 2
__GCC_ATOMIC_WCHAR_T_LOCK_FREE 2
__GCC_ATOMIC_SHORT_LOCK_FREE 2
__GCC_ATOMIC_INT_LOCK_FREE 2
__GCC_ATOMIC_LONG_LOCK_FREE 2
__GCC_ATOMIC_LLONG_LOCK_FREE 2
__GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1
__GCC_ATOMIC_POINTER_LOCK_FREE 2
__GCC_HAVE_DWARF2_CFI_ASM 1
__PRAGMA_REDEFINE_EXTNAME 1
__SIZEOF_INT128__ 16
__SIZEOF_WCHAR_T__ 4
__SIZEOF_WINT_T__ 4
__SIZEOF_PTRDIFF_T__ 8
__amd64 1
__amd64__ 1
__x86_64 1
__x86_64__ 1
__k8 1
__k8__ 1
__MMX__ 1
__SSE__ 1
__SSE2__ 1
__SSE_MATH__ 1
__SSE2_MATH__ 1
__gnu_linux__ 1
__linux 1
__linux__ 1
linux 1
__unix 1
__unix__ 1
unix 1
__ELF__ 1
__DECIMAL_BID_FORMAT__ 1
file /home/mark/src/tests/macro.c
 file /usr/include/string.h
  _STRING_H 1
  file /usr/include/features.h
   include 0x5d8
    _FEATURES_H 1
    __KERNEL_STRICT_NAMES 
    __USE_ANSI 1
    __GNUC_PREREQ(maj,min) ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
    _BSD_SOURCE 1
    _SVID_SOURCE 1
    _POSIX_SOURCE 1
    _POSIX_C_SOURCE 200809L
    __USE_POSIX_IMPLICITLY 1
    __USE_POSIX 1
    __USE_POSIX2 1
    __USE_POSIX199309 1
    __USE_POSIX199506 1
    __USE_XOPEN2K 1
    __USE_ISOC95 1
    __USE_ISOC99 1
    __USE_XOPEN2K8 1
    _ATFILE_SOURCE 1
    __USE_MISC 1
    __USE_BSD 1
    __USE_SVID 1
    __USE_ATFILE 1
    __USE_FORTIFY_LEVEL 0
   /include
   file /usr/include/stdc-predef.h
    include 0x733
     _STDC_PREDEF_H 1
     __STDC_IEC_559__ 1
     __STDC_IEC_559_COMPLEX__ 1
     __STDC_ISO_10646__ 201103L
     __STDC_NO_THREADS__ 1
    /include
   /file
   include 0x755
    __GNU_LIBRARY__ 6
    __GLIBC__ 2
    __GLIBC_MINOR__ 15
    __GLIBC_PREREQ(maj,min) ((__GLIBC__ << 16) + __GLIBC_MINOR__ >= ((maj) << 16) + (min))
    __GLIBC_HAVE_LONG_LONG 1
   /include
   file /usr/include/sys/cdefs.h
    include 0x783
     _SYS_CDEFS_H 1
     __LEAF , __leaf__
     __LEAF_ATTR __attribute__ ((__leaf__))
     __THROW __attribute__ ((__nothrow__ __LEAF))
     __THROWNL __attribute__ ((__nothrow__))
     __NTH(fct) __attribute__ ((__nothrow__ __LEAF)) fct
     __P(args) args
     __PMT(args) args
     __CONCAT(x,y) x ## y
     __STRING(x) #x
     __ptr_t void *
     __long_double_t long double
     __BEGIN_DECLS 
     __END_DECLS 
     __BEGIN_NAMESPACE_STD 
     __END_NAMESPACE_STD 
     __USING_NAMESPACE_STD(name) 
     __BEGIN_NAMESPACE_C99 
     __END_NAMESPACE_C99 
     __USING_NAMESPACE_C99(name) 
     __bounded 
     __unbounded 
     __ptrvalue 
     __bos(ptr) __builtin_object_size (ptr, __USE_FORTIFY_LEVEL > 1)
     __bos0(ptr) __builtin_object_size (ptr, 0)
     __fortify_function __extern_always_inline __attribute_artificial__
     __warndecl(name,msg) extern void name (void) __attribute__((__warning__ (msg)))
     __warnattr(msg) __attribute__((__warning__ (msg)))
     __errordecl(name,msg) extern void name (void) __attribute__((__error__ (msg)))
     __flexarr []
     __REDIRECT(name,proto,alias) name proto __asm__ (__ASMNAME (#alias))
     __REDIRECT_NTH(name,proto,alias) name proto __asm__ (__ASMNAME (#alias)) __THROW
     __REDIRECT_NTHNL(name,proto,alias) name proto __asm__ (__ASMNAME (#alias)) __THROWNL
     __ASMNAME(cname) __ASMNAME2 (__USER_LABEL_PREFIX__, cname)
     __ASMNAME2(prefix,cname) __STRING (prefix) cname
     __attribute_malloc__ __attribute__ ((__malloc__))
     __attribute_pure__ __attribute__ ((__pure__))
     __attribute_const__ __attribute__ ((__const__))
     __attribute_used__ __attribute__ ((__used__))
     __attribute_noinline__ __attribute__ ((__noinline__))
     __attribute_deprecated__ __attribute__ ((__deprecated__))
     __attribute_format_arg__(x) __attribute__ ((__format_arg__ (x)))
     __attribute_format_strfmon__(a,b) __attribute__ ((__format__ (__strfmon__, a, b)))
     __nonnull(params) __attribute__ ((__nonnull__ params))
     __attribute_warn_unused_result__ __attribute__ ((__warn_unused_result__))
     __wur 
     __always_inline __inline __attribute__ ((__always_inline__))
     __attribute_artificial__ __attribute__ ((__artificial__))
     __extern_inline extern __inline
     __extern_always_inline extern __always_inline
     __va_arg_pack() __builtin_va_arg_pack ()
     __va_arg_pack_len() __builtin_va_arg_pack_len ()
     __restrict_arr __restrict
     __glibc_unlikely(cond) __builtin_expect((cond), 0)
    /include
    file /usr/include/bits/wordsize.h
     include 0x8fa
      __WORDSIZE 64
      __WORDSIZE_TIME64_COMPAT32 1
      __SYSCALL_WORDSIZE 64
     /include
    /file
    include 0x910
     __LDBL_REDIR1(name,proto,alias) name proto
     __LDBL_REDIR(name,proto) name proto
     __LDBL_REDIR1_NTH(name,proto,alias) name proto __THROW
     __LDBL_REDIR_NTH(name,proto) name proto __THROW
     __LDBL_REDIR_DECL(name) 
     __REDIRECT_LDBL(name,proto,alias) __REDIRECT (name, proto, alias)
     __REDIRECT_NTH_LDBL(name,proto,alias) __REDIRECT_NTH (name, proto, alias)
    /include
   /file
   file /usr/include/gnu/stubs.h
    file /usr/include/gnu/stubs-64.h
     include 0x945
      __stub_bdflush 
      __stub_chflags 
      __stub_fattach 
      __stub_fchflags 
      __stub_fdetach 
      __stub_getmsg 
      __stub_gtty 
      __stub_lchmod 
      __stub_putmsg 
      __stub_revoke 
      __stub_setlogin 
      __stub_sigreturn 
      __stub_sstk 
      __stub_stty 
     /include
    /file
   /file
  /file
  include 0x99d
   __need_size_t 
   __need_NULL 
  /include
  file /usr/lib/gcc/x86_64-redhat-linux/4.7.0/include/stddef.h
   include 0x9ad
    __size_t__ 
    __SIZE_T__ 
    _SIZE_T 
    _SYS_SIZE_T_H 
    _T_SIZE_ 
    _T_SIZE 
    __SIZE_T 
    _SIZE_T_ 
    _BSD_SIZE_T_ 
    _SIZE_T_DEFINED_ 
    _SIZE_T_DEFINED 
    _BSD_SIZE_T_DEFINED_ 
    _SIZE_T_DECLARED 
    ___int_size_t_h 
    _GCC_SIZE_T 
    _SIZET_ 
    __size_t 
    NULL ((void *)0)
   /include
  /file
  file /usr/include/xlocale.h
   _XLOCALE_H 1
  /file
 /file
 HELLO "world"
/file
EOF

testfiles testfile-macros-0xff
testrun_compare ${abs_builddir}/dwarf-getmacros testfile-macros-0xff 0xb <<\EOF
invalid opcode
EOF
testrun_compare ${abs_builddir}/dwarf-getmacros testfile-macros-0xff 0xb '' <<\EOF
opcode 255 with 0 arguments
file /home/petr/proj/elfutils/master/elfutils/x.c
 FOO 0
/file
EOF

# See testfile-dwp.source.
testfiles testfile-dwp-5 testfile-dwp-5.dwp
testfiles testfile-dwp-4-strict testfile-dwp-4-strict.dwp

# Not testfile-dwp-4 because it's unclear what to do about
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99319.
for file in testfile-dwp-5 testfile-dwp-4-strict; do
	testrun_compare ${abs_builddir}/dwarf-getmacros "$file" '' '' << EOF
CU foo.cc
__STDC__ 1
__cplusplus 201703L
__STDC_UTF_16__ 1
__STDC_UTF_32__ 1
__STDC_HOSTED__ 1
__GNUC__ 14
__GNUC_MINOR__ 0
__GNUC_PATCHLEVEL__ 0
__VERSION__ "14.0.0 20230920 (experimental)"
__ATOMIC_RELAXED 0
__ATOMIC_SEQ_CST 5
__ATOMIC_ACQUIRE 2
__ATOMIC_RELEASE 3
__ATOMIC_ACQ_REL 4
__ATOMIC_CONSUME 1
__OPTIMIZE__ 1
__FINITE_MATH_ONLY__ 0
_LP64 1
__LP64__ 1
__SIZEOF_INT__ 4
__SIZEOF_LONG__ 8
__SIZEOF_LONG_LONG__ 8
__SIZEOF_SHORT__ 2
__SIZEOF_FLOAT__ 4
__SIZEOF_DOUBLE__ 8
__SIZEOF_LONG_DOUBLE__ 16
__SIZEOF_SIZE_T__ 8
__CHAR_BIT__ 8
__BIGGEST_ALIGNMENT__ 16
__ORDER_LITTLE_ENDIAN__ 1234
__ORDER_BIG_ENDIAN__ 4321
__ORDER_PDP_ENDIAN__ 3412
__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
__FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__
__SIZEOF_POINTER__ 8
__GNUC_EXECUTION_CHARSET_NAME "UTF-8"
__GNUC_WIDE_EXECUTION_CHARSET_NAME "UTF-32LE"
__GNUG__ 14
__SIZE_TYPE__ long unsigned int
__PTRDIFF_TYPE__ long int
__WCHAR_TYPE__ int
__WINT_TYPE__ unsigned int
__INTMAX_TYPE__ long int
__UINTMAX_TYPE__ long unsigned int
__CHAR16_TYPE__ short unsigned int
__CHAR32_TYPE__ unsigned int
__SIG_ATOMIC_TYPE__ int
__INT8_TYPE__ signed char
__INT16_TYPE__ short int
__INT32_TYPE__ int
__INT64_TYPE__ long int
__UINT8_TYPE__ unsigned char
__UINT16_TYPE__ short unsigned int
__UINT32_TYPE__ unsigned int
__UINT64_TYPE__ long unsigned int
__INT_LEAST8_TYPE__ signed char
__INT_LEAST16_TYPE__ short int
__INT_LEAST32_TYPE__ int
__INT_LEAST64_TYPE__ long int
__UINT_LEAST8_TYPE__ unsigned char
__UINT_LEAST16_TYPE__ short unsigned int
__UINT_LEAST32_TYPE__ unsigned int
__UINT_LEAST64_TYPE__ long unsigned int
__INT_FAST8_TYPE__ signed char
__INT_FAST16_TYPE__ long int
__INT_FAST32_TYPE__ long int
__INT_FAST64_TYPE__ long int
__UINT_FAST8_TYPE__ unsigned char
__UINT_FAST16_TYPE__ long unsigned int
__UINT_FAST32_TYPE__ long unsigned int
__UINT_FAST64_TYPE__ long unsigned int
__INTPTR_TYPE__ long int
__UINTPTR_TYPE__ long unsigned int
__GXX_WEAK__ 1
__DEPRECATED 1
__GXX_RTTI 1
__cpp_rtti 199711L
__GXX_EXPERIMENTAL_CXX0X__ 1
__cpp_binary_literals 201304L
__cpp_hex_float 201603L
__cpp_runtime_arrays 198712L
__cpp_raw_strings 200710L
__cpp_unicode_literals 200710L
__cpp_user_defined_literals 200809L
__cpp_lambdas 200907L
__cpp_decltype 200707L
__cpp_attributes 200809L
__cpp_rvalue_reference 200610L
__cpp_rvalue_references 200610L
__cpp_variadic_templates 200704L
__cpp_initializer_lists 200806L
__cpp_delegating_constructors 200604L
__cpp_nsdmi 200809L
__cpp_inheriting_constructors 201511L
__cpp_ref_qualifiers 200710L
__cpp_alias_templates 200704L
__cpp_return_type_deduction 201304L
__cpp_init_captures 201304L
__cpp_generic_lambdas 201304L
__cpp_decltype_auto 201304L
__cpp_aggregate_nsdmi 201304L
__cpp_variable_templates 201304L
__cpp_digit_separators 201309L
__cpp_unicode_characters 201411L
__cpp_static_assert 201411L
__cpp_namespace_attributes 201411L
__cpp_enumerator_attributes 201411L
__cpp_nested_namespace_definitions 201411L
__cpp_fold_expressions 201603L
__cpp_nontype_template_args 201411L
__cpp_range_based_for 201603L
__cpp_constexpr 201603L
__cpp_if_constexpr 201606L
__cpp_capture_star_this 201603L
__cpp_inline_variables 201606L
__cpp_aggregate_bases 201603L
__cpp_deduction_guides 201703L
__cpp_noexcept_function_type 201510L
__cpp_template_auto 201606L
__cpp_structured_bindings 201606L
__cpp_variadic_using 201611L
__cpp_guaranteed_copy_elision 201606L
__cpp_nontype_template_parameter_auto 201606L
__cpp_sized_deallocation 201309L
__cpp_aligned_new 201606L
__STDCPP_DEFAULT_NEW_ALIGNMENT__ 16
__cpp_template_template_args 201611L
__cpp_threadsafe_static_init 200806L
__STDCPP_THREADS__ 1
__EXCEPTIONS 1
__cpp_exceptions 199711L
__GXX_ABI_VERSION 1019
__SCHAR_MAX__ 0x7f
__SHRT_MAX__ 0x7fff
__INT_MAX__ 0x7fffffff
__LONG_MAX__ 0x7fffffffffffffffL
__LONG_LONG_MAX__ 0x7fffffffffffffffLL
__WCHAR_MAX__ 0x7fffffff
__WCHAR_MIN__ (-__WCHAR_MAX__ - 1)
__WINT_MAX__ 0xffffffffU
__WINT_MIN__ 0U
__PTRDIFF_MAX__ 0x7fffffffffffffffL
__SIZE_MAX__ 0xffffffffffffffffUL
__SCHAR_WIDTH__ 8
__SHRT_WIDTH__ 16
__INT_WIDTH__ 32
__LONG_WIDTH__ 64
__LONG_LONG_WIDTH__ 64
__WCHAR_WIDTH__ 32
__WINT_WIDTH__ 32
__PTRDIFF_WIDTH__ 64
__SIZE_WIDTH__ 64
__GLIBCXX_TYPE_INT_N_0 __int128
__GLIBCXX_BITSIZE_INT_N_0 128
__INTMAX_MAX__ 0x7fffffffffffffffL
__INTMAX_C(c) c ## L
__UINTMAX_MAX__ 0xffffffffffffffffUL
__UINTMAX_C(c) c ## UL
__INTMAX_WIDTH__ 64
__SIG_ATOMIC_MAX__ 0x7fffffff
__SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)
__SIG_ATOMIC_WIDTH__ 32
__INT8_MAX__ 0x7f
__INT16_MAX__ 0x7fff
__INT32_MAX__ 0x7fffffff
__INT64_MAX__ 0x7fffffffffffffffL
__UINT8_MAX__ 0xff
__UINT16_MAX__ 0xffff
__UINT32_MAX__ 0xffffffffU
__UINT64_MAX__ 0xffffffffffffffffUL
__INT_LEAST8_MAX__ 0x7f
__INT8_C(c) c
__INT_LEAST8_WIDTH__ 8
__INT_LEAST16_MAX__ 0x7fff
__INT16_C(c) c
__INT_LEAST16_WIDTH__ 16
__INT_LEAST32_MAX__ 0x7fffffff
__INT32_C(c) c
__INT_LEAST32_WIDTH__ 32
__INT_LEAST64_MAX__ 0x7fffffffffffffffL
__INT64_C(c) c ## L
__INT_LEAST64_WIDTH__ 64
__UINT_LEAST8_MAX__ 0xff
__UINT8_C(c) c
__UINT_LEAST16_MAX__ 0xffff
__UINT16_C(c) c
__UINT_LEAST32_MAX__ 0xffffffffU
__UINT32_C(c) c ## U
__UINT_LEAST64_MAX__ 0xffffffffffffffffUL
__UINT64_C(c) c ## UL
__INT_FAST8_MAX__ 0x7f
__INT_FAST8_WIDTH__ 8
__INT_FAST16_MAX__ 0x7fffffffffffffffL
__INT_FAST16_WIDTH__ 64
__INT_FAST32_MAX__ 0x7fffffffffffffffL
__INT_FAST32_WIDTH__ 64
__INT_FAST64_MAX__ 0x7fffffffffffffffL
__INT_FAST64_WIDTH__ 64
__UINT_FAST8_MAX__ 0xff
__UINT_FAST16_MAX__ 0xffffffffffffffffUL
__UINT_FAST32_MAX__ 0xffffffffffffffffUL
__UINT_FAST64_MAX__ 0xffffffffffffffffUL
__INTPTR_MAX__ 0x7fffffffffffffffL
__INTPTR_WIDTH__ 64
__UINTPTR_MAX__ 0xffffffffffffffffUL
__GCC_IEC_559 2
__GCC_IEC_559_COMPLEX 2
__FLT_EVAL_METHOD__ 0
__FLT_EVAL_METHOD_TS_18661_3__ 0
__DEC_EVAL_METHOD__ 2
__FLT_RADIX__ 2
__FLT_MANT_DIG__ 24
__FLT_DIG__ 6
__FLT_MIN_EXP__ (-125)
__FLT_MIN_10_EXP__ (-37)
__FLT_MAX_EXP__ 128
__FLT_MAX_10_EXP__ 38
__FLT_DECIMAL_DIG__ 9
__FLT_MAX__ 3.40282346638528859811704183484516925e+38F
__FLT_NORM_MAX__ 3.40282346638528859811704183484516925e+38F
__FLT_MIN__ 1.17549435082228750796873653722224568e-38F
__FLT_EPSILON__ 1.19209289550781250000000000000000000e-7F
__FLT_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F
__FLT_HAS_DENORM__ 1
__FLT_HAS_INFINITY__ 1
__FLT_HAS_QUIET_NAN__ 1
__FLT_IS_IEC_60559__ 1
__DBL_MANT_DIG__ 53
__DBL_DIG__ 15
__DBL_MIN_EXP__ (-1021)
__DBL_MIN_10_EXP__ (-307)
__DBL_MAX_EXP__ 1024
__DBL_MAX_10_EXP__ 308
__DBL_DECIMAL_DIG__ 17
__DBL_MAX__ double(1.79769313486231570814527423731704357e+308L)
__DBL_NORM_MAX__ double(1.79769313486231570814527423731704357e+308L)
__DBL_MIN__ double(2.22507385850720138309023271733240406e-308L)
__DBL_EPSILON__ double(2.22044604925031308084726333618164062e-16L)
__DBL_DENORM_MIN__ double(4.94065645841246544176568792868221372e-324L)
__DBL_HAS_DENORM__ 1
__DBL_HAS_INFINITY__ 1
__DBL_HAS_QUIET_NAN__ 1
__DBL_IS_IEC_60559__ 1
__LDBL_MANT_DIG__ 64
__LDBL_DIG__ 18
__LDBL_MIN_EXP__ (-16381)
__LDBL_MIN_10_EXP__ (-4931)
__LDBL_MAX_EXP__ 16384
__LDBL_MAX_10_EXP__ 4932
__DECIMAL_DIG__ 21
__LDBL_DECIMAL_DIG__ 21
__LDBL_MAX__ 1.18973149535723176502126385303097021e+4932L
__LDBL_NORM_MAX__ 1.18973149535723176502126385303097021e+4932L
__LDBL_MIN__ 3.36210314311209350626267781732175260e-4932L
__LDBL_EPSILON__ 1.08420217248550443400745280086994171e-19L
__LDBL_DENORM_MIN__ 3.64519953188247460252840593361941982e-4951L
__LDBL_HAS_DENORM__ 1
__LDBL_HAS_INFINITY__ 1
__LDBL_HAS_QUIET_NAN__ 1
__LDBL_IS_IEC_60559__ 1
__FLT16_MANT_DIG__ 11
__FLT16_DIG__ 3
__FLT16_MIN_EXP__ (-13)
__FLT16_MIN_10_EXP__ (-4)
__FLT16_MAX_EXP__ 16
__FLT16_MAX_10_EXP__ 4
__FLT16_DECIMAL_DIG__ 5
__FLT16_MAX__ 6.55040000000000000000000000000000000e+4F16
__FLT16_NORM_MAX__ 6.55040000000000000000000000000000000e+4F16
__FLT16_MIN__ 6.10351562500000000000000000000000000e-5F16
__FLT16_EPSILON__ 9.76562500000000000000000000000000000e-4F16
__FLT16_DENORM_MIN__ 5.96046447753906250000000000000000000e-8F16
__FLT16_HAS_DENORM__ 1
__FLT16_HAS_INFINITY__ 1
__FLT16_HAS_QUIET_NAN__ 1
__FLT16_IS_IEC_60559__ 1
__FLT32_MANT_DIG__ 24
__FLT32_DIG__ 6
__FLT32_MIN_EXP__ (-125)
__FLT32_MIN_10_EXP__ (-37)
__FLT32_MAX_EXP__ 128
__FLT32_MAX_10_EXP__ 38
__FLT32_DECIMAL_DIG__ 9
__FLT32_MAX__ 3.40282346638528859811704183484516925e+38F32
__FLT32_NORM_MAX__ 3.40282346638528859811704183484516925e+38F32
__FLT32_MIN__ 1.17549435082228750796873653722224568e-38F32
__FLT32_EPSILON__ 1.19209289550781250000000000000000000e-7F32
__FLT32_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F32
__FLT32_HAS_DENORM__ 1
__FLT32_HAS_INFINITY__ 1
__FLT32_HAS_QUIET_NAN__ 1
__FLT32_IS_IEC_60559__ 1
__FLT64_MANT_DIG__ 53
__FLT64_DIG__ 15
__FLT64_MIN_EXP__ (-1021)
__FLT64_MIN_10_EXP__ (-307)
__FLT64_MAX_EXP__ 1024
__FLT64_MAX_10_EXP__ 308
__FLT64_DECIMAL_DIG__ 17
__FLT64_MAX__ 1.79769313486231570814527423731704357e+308F64
__FLT64_NORM_MAX__ 1.79769313486231570814527423731704357e+308F64
__FLT64_MIN__ 2.22507385850720138309023271733240406e-308F64
__FLT64_EPSILON__ 2.22044604925031308084726333618164062e-16F64
__FLT64_DENORM_MIN__ 4.94065645841246544176568792868221372e-324F64
__FLT64_HAS_DENORM__ 1
__FLT64_HAS_INFINITY__ 1
__FLT64_HAS_QUIET_NAN__ 1
__FLT64_IS_IEC_60559__ 1
__FLT128_MANT_DIG__ 113
__FLT128_DIG__ 33
__FLT128_MIN_EXP__ (-16381)
__FLT128_MIN_10_EXP__ (-4931)
__FLT128_MAX_EXP__ 16384
__FLT128_MAX_10_EXP__ 4932
__FLT128_DECIMAL_DIG__ 36
__FLT128_MAX__ 1.18973149535723176508575932662800702e+4932F128
__FLT128_NORM_MAX__ 1.18973149535723176508575932662800702e+4932F128
__FLT128_MIN__ 3.36210314311209350626267781732175260e-4932F128
__FLT128_EPSILON__ 1.92592994438723585305597794258492732e-34F128
__FLT128_DENORM_MIN__ 6.47517511943802511092443895822764655e-4966F128
__FLT128_HAS_DENORM__ 1
__FLT128_HAS_INFINITY__ 1
__FLT128_HAS_QUIET_NAN__ 1
__FLT128_IS_IEC_60559__ 1
__FLT32X_MANT_DIG__ 53
__FLT32X_DIG__ 15
__FLT32X_MIN_EXP__ (-1021)
__FLT32X_MIN_10_EXP__ (-307)
__FLT32X_MAX_EXP__ 1024
__FLT32X_MAX_10_EXP__ 308
__FLT32X_DECIMAL_DIG__ 17
__FLT32X_MAX__ 1.79769313486231570814527423731704357e+308F32x
__FLT32X_NORM_MAX__ 1.79769313486231570814527423731704357e+308F32x
__FLT32X_MIN__ 2.22507385850720138309023271733240406e-308F32x
__FLT32X_EPSILON__ 2.22044604925031308084726333618164062e-16F32x
__FLT32X_DENORM_MIN__ 4.94065645841246544176568792868221372e-324F32x
__FLT32X_HAS_DENORM__ 1
__FLT32X_HAS_INFINITY__ 1
__FLT32X_HAS_QUIET_NAN__ 1
__FLT32X_IS_IEC_60559__ 1
__FLT64X_MANT_DIG__ 64
__FLT64X_DIG__ 18
__FLT64X_MIN_EXP__ (-16381)
__FLT64X_MIN_10_EXP__ (-4931)
__FLT64X_MAX_EXP__ 16384
__FLT64X_MAX_10_EXP__ 4932
__FLT64X_DECIMAL_DIG__ 21
__FLT64X_MAX__ 1.18973149535723176502126385303097021e+4932F64x
__FLT64X_NORM_MAX__ 1.18973149535723176502126385303097021e+4932F64x
__FLT64X_MIN__ 3.36210314311209350626267781732175260e-4932F64x
__FLT64X_EPSILON__ 1.08420217248550443400745280086994171e-19F64x
__FLT64X_DENORM_MIN__ 3.64519953188247460252840593361941982e-4951F64x
__FLT64X_HAS_DENORM__ 1
__FLT64X_HAS_INFINITY__ 1
__FLT64X_HAS_QUIET_NAN__ 1
__FLT64X_IS_IEC_60559__ 1
__BFLT16_MANT_DIG__ 8
__BFLT16_DIG__ 2
__BFLT16_MIN_EXP__ (-125)
__BFLT16_MIN_10_EXP__ (-37)
__BFLT16_MAX_EXP__ 128
__BFLT16_MAX_10_EXP__ 38
__BFLT16_DECIMAL_DIG__ 4
__BFLT16_MAX__ 3.38953138925153547590470800371487867e+38BF16
__BFLT16_NORM_MAX__ 3.38953138925153547590470800371487867e+38BF16
__BFLT16_MIN__ 1.17549435082228750796873653722224568e-38BF16
__BFLT16_EPSILON__ 7.81250000000000000000000000000000000e-3BF16
__BFLT16_DENORM_MIN__ 9.18354961579912115600575419704879436e-41BF16
__BFLT16_HAS_DENORM__ 1
__BFLT16_HAS_INFINITY__ 1
__BFLT16_HAS_QUIET_NAN__ 1
__BFLT16_IS_IEC_60559__ 0
__DEC32_MANT_DIG__ 7
__DEC32_MIN_EXP__ (-94)
__DEC32_MAX_EXP__ 97
__DEC32_MIN__ 1E-95DF
__DEC32_MAX__ 9.999999E96DF
__DEC32_EPSILON__ 1E-6DF
__DEC32_SUBNORMAL_MIN__ 0.000001E-95DF
__DEC64_MANT_DIG__ 16
__DEC64_MIN_EXP__ (-382)
__DEC64_MAX_EXP__ 385
__DEC64_MIN__ 1E-383DD
__DEC64_MAX__ 9.999999999999999E384DD
__DEC64_EPSILON__ 1E-15DD
__DEC64_SUBNORMAL_MIN__ 0.000000000000001E-383DD
__DEC128_MANT_DIG__ 34
__DEC128_MIN_EXP__ (-6142)
__DEC128_MAX_EXP__ 6145
__DEC128_MIN__ 1E-6143DL
__DEC128_MAX__ 9.999999999999999999999999999999999E6144DL
__DEC128_EPSILON__ 1E-33DL
__DEC128_SUBNORMAL_MIN__ 0.000000000000000000000000000000001E-6143DL
__REGISTER_PREFIX__ 
__USER_LABEL_PREFIX__ 
__GNUC_STDC_INLINE__ 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1
__GCC_ATOMIC_BOOL_LOCK_FREE 2
__GCC_ATOMIC_CHAR_LOCK_FREE 2
__GCC_ATOMIC_CHAR16_T_LOCK_FREE 2
__GCC_ATOMIC_CHAR32_T_LOCK_FREE 2
__GCC_ATOMIC_WCHAR_T_LOCK_FREE 2
__GCC_ATOMIC_SHORT_LOCK_FREE 2
__GCC_ATOMIC_INT_LOCK_FREE 2
__GCC_ATOMIC_LONG_LOCK_FREE 2
__GCC_ATOMIC_LLONG_LOCK_FREE 2
__GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1
__GCC_DESTRUCTIVE_SIZE 64
__GCC_CONSTRUCTIVE_SIZE 64
__GCC_ATOMIC_POINTER_LOCK_FREE 2
__HAVE_SPECULATION_SAFE_VALUE 1
__GCC_HAVE_DWARF2_CFI_ASM 1
__PRAGMA_REDEFINE_EXTNAME 1
__SIZEOF_INT128__ 16
__SIZEOF_WCHAR_T__ 4
__SIZEOF_WINT_T__ 4
__SIZEOF_PTRDIFF_T__ 8
__amd64 1
__amd64__ 1
__x86_64 1
__x86_64__ 1
__SIZEOF_FLOAT80__ 16
__SIZEOF_FLOAT128__ 16
__ATOMIC_HLE_ACQUIRE 65536
__ATOMIC_HLE_RELEASE 131072
__GCC_ASM_FLAG_OUTPUTS__ 1
__k8 1
__k8__ 1
__code_model_small__ 1
__MMX__ 1
__SSE__ 1
__SSE2__ 1
__FXSR__ 1
__SSE_MATH__ 1
__SSE2_MATH__ 1
__MMX_WITH_SSE__ 1
__SEG_FS 1
__SEG_GS 1
__gnu_linux__ 1
__linux 1
__linux__ 1
linux 1
__unix 1
__unix__ 1
unix 1
__ELF__ 1
__DECIMAL_BID_FORMAT__ 1
_GNU_SOURCE 1
file /home/osandov/src/elfutils/tests/foo.cc
 file /usr/include/stdc-predef.h
  _STDC_PREDEF_H 1
  __STDC_IEC_559__ 1
  __STDC_IEC_60559_BFP__ 201404L
  __STDC_IEC_559_COMPLEX__ 1
  __STDC_IEC_60559_COMPLEX__ 201404L
  __STDC_ISO_10646__ 201706L
 /file
 file /home/osandov/src/elfutils/tests/foobar.h
  FROB(x) ((x) ^ 0x2a2a2a2a)
  FRY(x) ((x) * 0x100000001b3)
 /file
 ZERO() (1 - 1)
/file
CU bar.cc
__STDC__ 1
__cplusplus 201703L
__STDC_UTF_16__ 1
__STDC_UTF_32__ 1
__STDC_HOSTED__ 1
__GNUC__ 14
__GNUC_MINOR__ 0
__GNUC_PATCHLEVEL__ 0
__VERSION__ "14.0.0 20230920 (experimental)"
__ATOMIC_RELAXED 0
__ATOMIC_SEQ_CST 5
__ATOMIC_ACQUIRE 2
__ATOMIC_RELEASE 3
__ATOMIC_ACQ_REL 4
__ATOMIC_CONSUME 1
__OPTIMIZE__ 1
__FINITE_MATH_ONLY__ 0
_LP64 1
__LP64__ 1
__SIZEOF_INT__ 4
__SIZEOF_LONG__ 8
__SIZEOF_LONG_LONG__ 8
__SIZEOF_SHORT__ 2
__SIZEOF_FLOAT__ 4
__SIZEOF_DOUBLE__ 8
__SIZEOF_LONG_DOUBLE__ 16
__SIZEOF_SIZE_T__ 8
__CHAR_BIT__ 8
__BIGGEST_ALIGNMENT__ 16
__ORDER_LITTLE_ENDIAN__ 1234
__ORDER_BIG_ENDIAN__ 4321
__ORDER_PDP_ENDIAN__ 3412
__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
__FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__
__SIZEOF_POINTER__ 8
__GNUC_EXECUTION_CHARSET_NAME "UTF-8"
__GNUC_WIDE_EXECUTION_CHARSET_NAME "UTF-32LE"
__GNUG__ 14
__SIZE_TYPE__ long unsigned int
__PTRDIFF_TYPE__ long int
__WCHAR_TYPE__ int
__WINT_TYPE__ unsigned int
__INTMAX_TYPE__ long int
__UINTMAX_TYPE__ long unsigned int
__CHAR16_TYPE__ short unsigned int
__CHAR32_TYPE__ unsigned int
__SIG_ATOMIC_TYPE__ int
__INT8_TYPE__ signed char
__INT16_TYPE__ short int
__INT32_TYPE__ int
__INT64_TYPE__ long int
__UINT8_TYPE__ unsigned char
__UINT16_TYPE__ short unsigned int
__UINT32_TYPE__ unsigned int
__UINT64_TYPE__ long unsigned int
__INT_LEAST8_TYPE__ signed char
__INT_LEAST16_TYPE__ short int
__INT_LEAST32_TYPE__ int
__INT_LEAST64_TYPE__ long int
__UINT_LEAST8_TYPE__ unsigned char
__UINT_LEAST16_TYPE__ short unsigned int
__UINT_LEAST32_TYPE__ unsigned int
__UINT_LEAST64_TYPE__ long unsigned int
__INT_FAST8_TYPE__ signed char
__INT_FAST16_TYPE__ long int
__INT_FAST32_TYPE__ long int
__INT_FAST64_TYPE__ long int
__UINT_FAST8_TYPE__ unsigned char
__UINT_FAST16_TYPE__ long unsigned int
__UINT_FAST32_TYPE__ long unsigned int
__UINT_FAST64_TYPE__ long unsigned int
__INTPTR_TYPE__ long int
__UINTPTR_TYPE__ long unsigned int
__GXX_WEAK__ 1
__DEPRECATED 1
__GXX_RTTI 1
__cpp_rtti 199711L
__GXX_EXPERIMENTAL_CXX0X__ 1
__cpp_binary_literals 201304L
__cpp_hex_float 201603L
__cpp_runtime_arrays 198712L
__cpp_raw_strings 200710L
__cpp_unicode_literals 200710L
__cpp_user_defined_literals 200809L
__cpp_lambdas 200907L
__cpp_decltype 200707L
__cpp_attributes 200809L
__cpp_rvalue_reference 200610L
__cpp_rvalue_references 200610L
__cpp_variadic_templates 200704L
__cpp_initializer_lists 200806L
__cpp_delegating_constructors 200604L
__cpp_nsdmi 200809L
__cpp_inheriting_constructors 201511L
__cpp_ref_qualifiers 200710L
__cpp_alias_templates 200704L
__cpp_return_type_deduction 201304L
__cpp_init_captures 201304L
__cpp_generic_lambdas 201304L
__cpp_decltype_auto 201304L
__cpp_aggregate_nsdmi 201304L
__cpp_variable_templates 201304L
__cpp_digit_separators 201309L
__cpp_unicode_characters 201411L
__cpp_static_assert 201411L
__cpp_namespace_attributes 201411L
__cpp_enumerator_attributes 201411L
__cpp_nested_namespace_definitions 201411L
__cpp_fold_expressions 201603L
__cpp_nontype_template_args 201411L
__cpp_range_based_for 201603L
__cpp_constexpr 201603L
__cpp_if_constexpr 201606L
__cpp_capture_star_this 201603L
__cpp_inline_variables 201606L
__cpp_aggregate_bases 201603L
__cpp_deduction_guides 201703L
__cpp_noexcept_function_type 201510L
__cpp_template_auto 201606L
__cpp_structured_bindings 201606L
__cpp_variadic_using 201611L
__cpp_guaranteed_copy_elision 201606L
__cpp_nontype_template_parameter_auto 201606L
__cpp_sized_deallocation 201309L
__cpp_aligned_new 201606L
__STDCPP_DEFAULT_NEW_ALIGNMENT__ 16
__cpp_template_template_args 201611L
__cpp_threadsafe_static_init 200806L
__STDCPP_THREADS__ 1
__EXCEPTIONS 1
__cpp_exceptions 199711L
__GXX_ABI_VERSION 1019
__SCHAR_MAX__ 0x7f
__SHRT_MAX__ 0x7fff
__INT_MAX__ 0x7fffffff
__LONG_MAX__ 0x7fffffffffffffffL
__LONG_LONG_MAX__ 0x7fffffffffffffffLL
__WCHAR_MAX__ 0x7fffffff
__WCHAR_MIN__ (-__WCHAR_MAX__ - 1)
__WINT_MAX__ 0xffffffffU
__WINT_MIN__ 0U
__PTRDIFF_MAX__ 0x7fffffffffffffffL
__SIZE_MAX__ 0xffffffffffffffffUL
__SCHAR_WIDTH__ 8
__SHRT_WIDTH__ 16
__INT_WIDTH__ 32
__LONG_WIDTH__ 64
__LONG_LONG_WIDTH__ 64
__WCHAR_WIDTH__ 32
__WINT_WIDTH__ 32
__PTRDIFF_WIDTH__ 64
__SIZE_WIDTH__ 64
__GLIBCXX_TYPE_INT_N_0 __int128
__GLIBCXX_BITSIZE_INT_N_0 128
__INTMAX_MAX__ 0x7fffffffffffffffL
__INTMAX_C(c) c ## L
__UINTMAX_MAX__ 0xffffffffffffffffUL
__UINTMAX_C(c) c ## UL
__INTMAX_WIDTH__ 64
__SIG_ATOMIC_MAX__ 0x7fffffff
__SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)
__SIG_ATOMIC_WIDTH__ 32
__INT8_MAX__ 0x7f
__INT16_MAX__ 0x7fff
__INT32_MAX__ 0x7fffffff
__INT64_MAX__ 0x7fffffffffffffffL
__UINT8_MAX__ 0xff
__UINT16_MAX__ 0xffff
__UINT32_MAX__ 0xffffffffU
__UINT64_MAX__ 0xffffffffffffffffUL
__INT_LEAST8_MAX__ 0x7f
__INT8_C(c) c
__INT_LEAST8_WIDTH__ 8
__INT_LEAST16_MAX__ 0x7fff
__INT16_C(c) c
__INT_LEAST16_WIDTH__ 16
__INT_LEAST32_MAX__ 0x7fffffff
__INT32_C(c) c
__INT_LEAST32_WIDTH__ 32
__INT_LEAST64_MAX__ 0x7fffffffffffffffL
__INT64_C(c) c ## L
__INT_LEAST64_WIDTH__ 64
__UINT_LEAST8_MAX__ 0xff
__UINT8_C(c) c
__UINT_LEAST16_MAX__ 0xffff
__UINT16_C(c) c
__UINT_LEAST32_MAX__ 0xffffffffU
__UINT32_C(c) c ## U
__UINT_LEAST64_MAX__ 0xffffffffffffffffUL
__UINT64_C(c) c ## UL
__INT_FAST8_MAX__ 0x7f
__INT_FAST8_WIDTH__ 8
__INT_FAST16_MAX__ 0x7fffffffffffffffL
__INT_FAST16_WIDTH__ 64
__INT_FAST32_MAX__ 0x7fffffffffffffffL
__INT_FAST32_WIDTH__ 64
__INT_FAST64_MAX__ 0x7fffffffffffffffL
__INT_FAST64_WIDTH__ 64
__UINT_FAST8_MAX__ 0xff
__UINT_FAST16_MAX__ 0xffffffffffffffffUL
__UINT_FAST32_MAX__ 0xffffffffffffffffUL
__UINT_FAST64_MAX__ 0xffffffffffffffffUL
__INTPTR_MAX__ 0x7fffffffffffffffL
__INTPTR_WIDTH__ 64
__UINTPTR_MAX__ 0xffffffffffffffffUL
__GCC_IEC_559 2
__GCC_IEC_559_COMPLEX 2
__FLT_EVAL_METHOD__ 0
__FLT_EVAL_METHOD_TS_18661_3__ 0
__DEC_EVAL_METHOD__ 2
__FLT_RADIX__ 2
__FLT_MANT_DIG__ 24
__FLT_DIG__ 6
__FLT_MIN_EXP__ (-125)
__FLT_MIN_10_EXP__ (-37)
__FLT_MAX_EXP__ 128
__FLT_MAX_10_EXP__ 38
__FLT_DECIMAL_DIG__ 9
__FLT_MAX__ 3.40282346638528859811704183484516925e+38F
__FLT_NORM_MAX__ 3.40282346638528859811704183484516925e+38F
__FLT_MIN__ 1.17549435082228750796873653722224568e-38F
__FLT_EPSILON__ 1.19209289550781250000000000000000000e-7F
__FLT_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F
__FLT_HAS_DENORM__ 1
__FLT_HAS_INFINITY__ 1
__FLT_HAS_QUIET_NAN__ 1
__FLT_IS_IEC_60559__ 1
__DBL_MANT_DIG__ 53
__DBL_DIG__ 15
__DBL_MIN_EXP__ (-1021)
__DBL_MIN_10_EXP__ (-307)
__DBL_MAX_EXP__ 1024
__DBL_MAX_10_EXP__ 308
__DBL_DECIMAL_DIG__ 17
__DBL_MAX__ double(1.79769313486231570814527423731704357e+308L)
__DBL_NORM_MAX__ double(1.79769313486231570814527423731704357e+308L)
__DBL_MIN__ double(2.22507385850720138309023271733240406e-308L)
__DBL_EPSILON__ double(2.22044604925031308084726333618164062e-16L)
__DBL_DENORM_MIN__ double(4.94065645841246544176568792868221372e-324L)
__DBL_HAS_DENORM__ 1
__DBL_HAS_INFINITY__ 1
__DBL_HAS_QUIET_NAN__ 1
__DBL_IS_IEC_60559__ 1
__LDBL_MANT_DIG__ 64
__LDBL_DIG__ 18
__LDBL_MIN_EXP__ (-16381)
__LDBL_MIN_10_EXP__ (-4931)
__LDBL_MAX_EXP__ 16384
__LDBL_MAX_10_EXP__ 4932
__DECIMAL_DIG__ 21
__LDBL_DECIMAL_DIG__ 21
__LDBL_MAX__ 1.18973149535723176502126385303097021e+4932L
__LDBL_NORM_MAX__ 1.18973149535723176502126385303097021e+4932L
__LDBL_MIN__ 3.36210314311209350626267781732175260e-4932L
__LDBL_EPSILON__ 1.08420217248550443400745280086994171e-19L
__LDBL_DENORM_MIN__ 3.64519953188247460252840593361941982e-4951L
__LDBL_HAS_DENORM__ 1
__LDBL_HAS_INFINITY__ 1
__LDBL_HAS_QUIET_NAN__ 1
__LDBL_IS_IEC_60559__ 1
__FLT16_MANT_DIG__ 11
__FLT16_DIG__ 3
__FLT16_MIN_EXP__ (-13)
__FLT16_MIN_10_EXP__ (-4)
__FLT16_MAX_EXP__ 16
__FLT16_MAX_10_EXP__ 4
__FLT16_DECIMAL_DIG__ 5
__FLT16_MAX__ 6.55040000000000000000000000000000000e+4F16
__FLT16_NORM_MAX__ 6.55040000000000000000000000000000000e+4F16
__FLT16_MIN__ 6.10351562500000000000000000000000000e-5F16
__FLT16_EPSILON__ 9.76562500000000000000000000000000000e-4F16
__FLT16_DENORM_MIN__ 5.96046447753906250000000000000000000e-8F16
__FLT16_HAS_DENORM__ 1
__FLT16_HAS_INFINITY__ 1
__FLT16_HAS_QUIET_NAN__ 1
__FLT16_IS_IEC_60559__ 1
__FLT32_MANT_DIG__ 24
__FLT32_DIG__ 6
__FLT32_MIN_EXP__ (-125)
__FLT32_MIN_10_EXP__ (-37)
__FLT32_MAX_EXP__ 128
__FLT32_MAX_10_EXP__ 38
__FLT32_DECIMAL_DIG__ 9
__FLT32_MAX__ 3.40282346638528859811704183484516925e+38F32
__FLT32_NORM_MAX__ 3.40282346638528859811704183484516925e+38F32
__FLT32_MIN__ 1.17549435082228750796873653722224568e-38F32
__FLT32_EPSILON__ 1.19209289550781250000000000000000000e-7F32
__FLT32_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F32
__FLT32_HAS_DENORM__ 1
__FLT32_HAS_INFINITY__ 1
__FLT32_HAS_QUIET_NAN__ 1
__FLT32_IS_IEC_60559__ 1
__FLT64_MANT_DIG__ 53
__FLT64_DIG__ 15
__FLT64_MIN_EXP__ (-1021)
__FLT64_MIN_10_EXP__ (-307)
__FLT64_MAX_EXP__ 1024
__FLT64_MAX_10_EXP__ 308
__FLT64_DECIMAL_DIG__ 17
__FLT64_MAX__ 1.79769313486231570814527423731704357e+308F64
__FLT64_NORM_MAX__ 1.79769313486231570814527423731704357e+308F64
__FLT64_MIN__ 2.22507385850720138309023271733240406e-308F64
__FLT64_EPSILON__ 2.22044604925031308084726333618164062e-16F64
__FLT64_DENORM_MIN__ 4.94065645841246544176568792868221372e-324F64
__FLT64_HAS_DENORM__ 1
__FLT64_HAS_INFINITY__ 1
__FLT64_HAS_QUIET_NAN__ 1
__FLT64_IS_IEC_60559__ 1
__FLT128_MANT_DIG__ 113
__FLT128_DIG__ 33
__FLT128_MIN_EXP__ (-16381)
__FLT128_MIN_10_EXP__ (-4931)
__FLT128_MAX_EXP__ 16384
__FLT128_MAX_10_EXP__ 4932
__FLT128_DECIMAL_DIG__ 36
__FLT128_MAX__ 1.18973149535723176508575932662800702e+4932F128
__FLT128_NORM_MAX__ 1.18973149535723176508575932662800702e+4932F128
__FLT128_MIN__ 3.36210314311209350626267781732175260e-4932F128
__FLT128_EPSILON__ 1.92592994438723585305597794258492732e-34F128
__FLT128_DENORM_MIN__ 6.47517511943802511092443895822764655e-4966F128
__FLT128_HAS_DENORM__ 1
__FLT128_HAS_INFINITY__ 1
__FLT128_HAS_QUIET_NAN__ 1
__FLT128_IS_IEC_60559__ 1
__FLT32X_MANT_DIG__ 53
__FLT32X_DIG__ 15
__FLT32X_MIN_EXP__ (-1021)
__FLT32X_MIN_10_EXP__ (-307)
__FLT32X_MAX_EXP__ 1024
__FLT32X_MAX_10_EXP__ 308
__FLT32X_DECIMAL_DIG__ 17
__FLT32X_MAX__ 1.79769313486231570814527423731704357e+308F32x
__FLT32X_NORM_MAX__ 1.79769313486231570814527423731704357e+308F32x
__FLT32X_MIN__ 2.22507385850720138309023271733240406e-308F32x
__FLT32X_EPSILON__ 2.22044604925031308084726333618164062e-16F32x
__FLT32X_DENORM_MIN__ 4.94065645841246544176568792868221372e-324F32x
__FLT32X_HAS_DENORM__ 1
__FLT32X_HAS_INFINITY__ 1
__FLT32X_HAS_QUIET_NAN__ 1
__FLT32X_IS_IEC_60559__ 1
__FLT64X_MANT_DIG__ 64
__FLT64X_DIG__ 18
__FLT64X_MIN_EXP__ (-16381)
__FLT64X_MIN_10_EXP__ (-4931)
__FLT64X_MAX_EXP__ 16384
__FLT64X_MAX_10_EXP__ 4932
__FLT64X_DECIMAL_DIG__ 21
__FLT64X_MAX__ 1.18973149535723176502126385303097021e+4932F64x
__FLT64X_NORM_MAX__ 1.18973149535723176502126385303097021e+4932F64x
__FLT64X_MIN__ 3.36210314311209350626267781732175260e-4932F64x
__FLT64X_EPSILON__ 1.08420217248550443400745280086994171e-19F64x
__FLT64X_DENORM_MIN__ 3.64519953188247460252840593361941982e-4951F64x
__FLT64X_HAS_DENORM__ 1
__FLT64X_HAS_INFINITY__ 1
__FLT64X_HAS_QUIET_NAN__ 1
__FLT64X_IS_IEC_60559__ 1
__BFLT16_MANT_DIG__ 8
__BFLT16_DIG__ 2
__BFLT16_MIN_EXP__ (-125)
__BFLT16_MIN_10_EXP__ (-37)
__BFLT16_MAX_EXP__ 128
__BFLT16_MAX_10_EXP__ 38
__BFLT16_DECIMAL_DIG__ 4
__BFLT16_MAX__ 3.38953138925153547590470800371487867e+38BF16
__BFLT16_NORM_MAX__ 3.38953138925153547590470800371487867e+38BF16
__BFLT16_MIN__ 1.17549435082228750796873653722224568e-38BF16
__BFLT16_EPSILON__ 7.81250000000000000000000000000000000e-3BF16
__BFLT16_DENORM_MIN__ 9.18354961579912115600575419704879436e-41BF16
__BFLT16_HAS_DENORM__ 1
__BFLT16_HAS_INFINITY__ 1
__BFLT16_HAS_QUIET_NAN__ 1
__BFLT16_IS_IEC_60559__ 0
__DEC32_MANT_DIG__ 7
__DEC32_MIN_EXP__ (-94)
__DEC32_MAX_EXP__ 97
__DEC32_MIN__ 1E-95DF
__DEC32_MAX__ 9.999999E96DF
__DEC32_EPSILON__ 1E-6DF
__DEC32_SUBNORMAL_MIN__ 0.000001E-95DF
__DEC64_MANT_DIG__ 16
__DEC64_MIN_EXP__ (-382)
__DEC64_MAX_EXP__ 385
__DEC64_MIN__ 1E-383DD
__DEC64_MAX__ 9.999999999999999E384DD
__DEC64_EPSILON__ 1E-15DD
__DEC64_SUBNORMAL_MIN__ 0.000000000000001E-383DD
__DEC128_MANT_DIG__ 34
__DEC128_MIN_EXP__ (-6142)
__DEC128_MAX_EXP__ 6145
__DEC128_MIN__ 1E-6143DL
__DEC128_MAX__ 9.999999999999999999999999999999999E6144DL
__DEC128_EPSILON__ 1E-33DL
__DEC128_SUBNORMAL_MIN__ 0.000000000000000000000000000000001E-6143DL
__REGISTER_PREFIX__ 
__USER_LABEL_PREFIX__ 
__GNUC_STDC_INLINE__ 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1
__GCC_ATOMIC_BOOL_LOCK_FREE 2
__GCC_ATOMIC_CHAR_LOCK_FREE 2
__GCC_ATOMIC_CHAR16_T_LOCK_FREE 2
__GCC_ATOMIC_CHAR32_T_LOCK_FREE 2
__GCC_ATOMIC_WCHAR_T_LOCK_FREE 2
__GCC_ATOMIC_SHORT_LOCK_FREE 2
__GCC_ATOMIC_INT_LOCK_FREE 2
__GCC_ATOMIC_LONG_LOCK_FREE 2
__GCC_ATOMIC_LLONG_LOCK_FREE 2
__GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1
__GCC_DESTRUCTIVE_SIZE 64
__GCC_CONSTRUCTIVE_SIZE 64
__GCC_ATOMIC_POINTER_LOCK_FREE 2
__HAVE_SPECULATION_SAFE_VALUE 1
__GCC_HAVE_DWARF2_CFI_ASM 1
__PRAGMA_REDEFINE_EXTNAME 1
__SIZEOF_INT128__ 16
__SIZEOF_WCHAR_T__ 4
__SIZEOF_WINT_T__ 4
__SIZEOF_PTRDIFF_T__ 8
__amd64 1
__amd64__ 1
__x86_64 1
__x86_64__ 1
__SIZEOF_FLOAT80__ 16
__SIZEOF_FLOAT128__ 16
__ATOMIC_HLE_ACQUIRE 65536
__ATOMIC_HLE_RELEASE 131072
__GCC_ASM_FLAG_OUTPUTS__ 1
__k8 1
__k8__ 1
__code_model_small__ 1
__MMX__ 1
__SSE__ 1
__SSE2__ 1
__FXSR__ 1
__SSE_MATH__ 1
__SSE2_MATH__ 1
__MMX_WITH_SSE__ 1
__SEG_FS 1
__SEG_GS 1
__gnu_linux__ 1
__linux 1
__linux__ 1
linux 1
__unix 1
__unix__ 1
unix 1
__ELF__ 1
__DECIMAL_BID_FORMAT__ 1
_GNU_SOURCE 1
file /home/osandov/src/elfutils/tests/bar.cc
 file /usr/include/stdc-predef.h
  _STDC_PREDEF_H 1
  __STDC_IEC_559__ 1
  __STDC_IEC_60559_BFP__ 201404L
  __STDC_IEC_559_COMPLEX__ 1
  __STDC_IEC_60559_COMPLEX__ 201404L
  __STDC_ISO_10646__ 201706L
 /file
 file /home/osandov/src/elfutils/tests/foobar.h
  FROB(x) ((x) ^ 0x2a2a2a2a)
  FRY(x) ((x) * 0x100000001b3)
 /file
 ONE 1
/file
CU main.cc
__STDC__ 1
__cplusplus 201703L
__STDC_UTF_16__ 1
__STDC_UTF_32__ 1
__STDC_HOSTED__ 1
__GNUC__ 14
__GNUC_MINOR__ 0
__GNUC_PATCHLEVEL__ 0
__VERSION__ "14.0.0 20230920 (experimental)"
__ATOMIC_RELAXED 0
__ATOMIC_SEQ_CST 5
__ATOMIC_ACQUIRE 2
__ATOMIC_RELEASE 3
__ATOMIC_ACQ_REL 4
__ATOMIC_CONSUME 1
__OPTIMIZE__ 1
__FINITE_MATH_ONLY__ 0
_LP64 1
__LP64__ 1
__SIZEOF_INT__ 4
__SIZEOF_LONG__ 8
__SIZEOF_LONG_LONG__ 8
__SIZEOF_SHORT__ 2
__SIZEOF_FLOAT__ 4
__SIZEOF_DOUBLE__ 8
__SIZEOF_LONG_DOUBLE__ 16
__SIZEOF_SIZE_T__ 8
__CHAR_BIT__ 8
__BIGGEST_ALIGNMENT__ 16
__ORDER_LITTLE_ENDIAN__ 1234
__ORDER_BIG_ENDIAN__ 4321
__ORDER_PDP_ENDIAN__ 3412
__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
__FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__
__SIZEOF_POINTER__ 8
__GNUC_EXECUTION_CHARSET_NAME "UTF-8"
__GNUC_WIDE_EXECUTION_CHARSET_NAME "UTF-32LE"
__GNUG__ 14
__SIZE_TYPE__ long unsigned int
__PTRDIFF_TYPE__ long int
__WCHAR_TYPE__ int
__WINT_TYPE__ unsigned int
__INTMAX_TYPE__ long int
__UINTMAX_TYPE__ long unsigned int
__CHAR16_TYPE__ short unsigned int
__CHAR32_TYPE__ unsigned int
__SIG_ATOMIC_TYPE__ int
__INT8_TYPE__ signed char
__INT16_TYPE__ short int
__INT32_TYPE__ int
__INT64_TYPE__ long int
__UINT8_TYPE__ unsigned char
__UINT16_TYPE__ short unsigned int
__UINT32_TYPE__ unsigned int
__UINT64_TYPE__ long unsigned int
__INT_LEAST8_TYPE__ signed char
__INT_LEAST16_TYPE__ short int
__INT_LEAST32_TYPE__ int
__INT_LEAST64_TYPE__ long int
__UINT_LEAST8_TYPE__ unsigned char
__UINT_LEAST16_TYPE__ short unsigned int
__UINT_LEAST32_TYPE__ unsigned int
__UINT_LEAST64_TYPE__ long unsigned int
__INT_FAST8_TYPE__ signed char
__INT_FAST16_TYPE__ long int
__INT_FAST32_TYPE__ long int
__INT_FAST64_TYPE__ long int
__UINT_FAST8_TYPE__ unsigned char
__UINT_FAST16_TYPE__ long unsigned int
__UINT_FAST32_TYPE__ long unsigned int
__UINT_FAST64_TYPE__ long unsigned int
__INTPTR_TYPE__ long int
__UINTPTR_TYPE__ long unsigned int
__GXX_WEAK__ 1
__DEPRECATED 1
__GXX_RTTI 1
__cpp_rtti 199711L
__GXX_EXPERIMENTAL_CXX0X__ 1
__cpp_binary_literals 201304L
__cpp_hex_float 201603L
__cpp_runtime_arrays 198712L
__cpp_raw_strings 200710L
__cpp_unicode_literals 200710L
__cpp_user_defined_literals 200809L
__cpp_lambdas 200907L
__cpp_decltype 200707L
__cpp_attributes 200809L
__cpp_rvalue_reference 200610L
__cpp_rvalue_references 200610L
__cpp_variadic_templates 200704L
__cpp_initializer_lists 200806L
__cpp_delegating_constructors 200604L
__cpp_nsdmi 200809L
__cpp_inheriting_constructors 201511L
__cpp_ref_qualifiers 200710L
__cpp_alias_templates 200704L
__cpp_return_type_deduction 201304L
__cpp_init_captures 201304L
__cpp_generic_lambdas 201304L
__cpp_decltype_auto 201304L
__cpp_aggregate_nsdmi 201304L
__cpp_variable_templates 201304L
__cpp_digit_separators 201309L
__cpp_unicode_characters 201411L
__cpp_static_assert 201411L
__cpp_namespace_attributes 201411L
__cpp_enumerator_attributes 201411L
__cpp_nested_namespace_definitions 201411L
__cpp_fold_expressions 201603L
__cpp_nontype_template_args 201411L
__cpp_range_based_for 201603L
__cpp_constexpr 201603L
__cpp_if_constexpr 201606L
__cpp_capture_star_this 201603L
__cpp_inline_variables 201606L
__cpp_aggregate_bases 201603L
__cpp_deduction_guides 201703L
__cpp_noexcept_function_type 201510L
__cpp_template_auto 201606L
__cpp_structured_bindings 201606L
__cpp_variadic_using 201611L
__cpp_guaranteed_copy_elision 201606L
__cpp_nontype_template_parameter_auto 201606L
__cpp_sized_deallocation 201309L
__cpp_aligned_new 201606L
__STDCPP_DEFAULT_NEW_ALIGNMENT__ 16
__cpp_template_template_args 201611L
__cpp_threadsafe_static_init 200806L
__STDCPP_THREADS__ 1
__EXCEPTIONS 1
__cpp_exceptions 199711L
__GXX_ABI_VERSION 1019
__SCHAR_MAX__ 0x7f
__SHRT_MAX__ 0x7fff
__INT_MAX__ 0x7fffffff
__LONG_MAX__ 0x7fffffffffffffffL
__LONG_LONG_MAX__ 0x7fffffffffffffffLL
__WCHAR_MAX__ 0x7fffffff
__WCHAR_MIN__ (-__WCHAR_MAX__ - 1)
__WINT_MAX__ 0xffffffffU
__WINT_MIN__ 0U
__PTRDIFF_MAX__ 0x7fffffffffffffffL
__SIZE_MAX__ 0xffffffffffffffffUL
__SCHAR_WIDTH__ 8
__SHRT_WIDTH__ 16
__INT_WIDTH__ 32
__LONG_WIDTH__ 64
__LONG_LONG_WIDTH__ 64
__WCHAR_WIDTH__ 32
__WINT_WIDTH__ 32
__PTRDIFF_WIDTH__ 64
__SIZE_WIDTH__ 64
__GLIBCXX_TYPE_INT_N_0 __int128
__GLIBCXX_BITSIZE_INT_N_0 128
__INTMAX_MAX__ 0x7fffffffffffffffL
__INTMAX_C(c) c ## L
__UINTMAX_MAX__ 0xffffffffffffffffUL
__UINTMAX_C(c) c ## UL
__INTMAX_WIDTH__ 64
__SIG_ATOMIC_MAX__ 0x7fffffff
__SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)
__SIG_ATOMIC_WIDTH__ 32
__INT8_MAX__ 0x7f
__INT16_MAX__ 0x7fff
__INT32_MAX__ 0x7fffffff
__INT64_MAX__ 0x7fffffffffffffffL
__UINT8_MAX__ 0xff
__UINT16_MAX__ 0xffff
__UINT32_MAX__ 0xffffffffU
__UINT64_MAX__ 0xffffffffffffffffUL
__INT_LEAST8_MAX__ 0x7f
__INT8_C(c) c
__INT_LEAST8_WIDTH__ 8
__INT_LEAST16_MAX__ 0x7fff
__INT16_C(c) c
__INT_LEAST16_WIDTH__ 16
__INT_LEAST32_MAX__ 0x7fffffff
__INT32_C(c) c
__INT_LEAST32_WIDTH__ 32
__INT_LEAST64_MAX__ 0x7fffffffffffffffL
__INT64_C(c) c ## L
__INT_LEAST64_WIDTH__ 64
__UINT_LEAST8_MAX__ 0xff
__UINT8_C(c) c
__UINT_LEAST16_MAX__ 0xffff
__UINT16_C(c) c
__UINT_LEAST32_MAX__ 0xffffffffU
__UINT32_C(c) c ## U
__UINT_LEAST64_MAX__ 0xffffffffffffffffUL
__UINT64_C(c) c ## UL
__INT_FAST8_MAX__ 0x7f
__INT_FAST8_WIDTH__ 8
__INT_FAST16_MAX__ 0x7fffffffffffffffL
__INT_FAST16_WIDTH__ 64
__INT_FAST32_MAX__ 0x7fffffffffffffffL
__INT_FAST32_WIDTH__ 64
__INT_FAST64_MAX__ 0x7fffffffffffffffL
__INT_FAST64_WIDTH__ 64
__UINT_FAST8_MAX__ 0xff
__UINT_FAST16_MAX__ 0xffffffffffffffffUL
__UINT_FAST32_MAX__ 0xffffffffffffffffUL
__UINT_FAST64_MAX__ 0xffffffffffffffffUL
__INTPTR_MAX__ 0x7fffffffffffffffL
__INTPTR_WIDTH__ 64
__UINTPTR_MAX__ 0xffffffffffffffffUL
__GCC_IEC_559 2
__GCC_IEC_559_COMPLEX 2
__FLT_EVAL_METHOD__ 0
__FLT_EVAL_METHOD_TS_18661_3__ 0
__DEC_EVAL_METHOD__ 2
__FLT_RADIX__ 2
__FLT_MANT_DIG__ 24
__FLT_DIG__ 6
__FLT_MIN_EXP__ (-125)
__FLT_MIN_10_EXP__ (-37)
__FLT_MAX_EXP__ 128
__FLT_MAX_10_EXP__ 38
__FLT_DECIMAL_DIG__ 9
__FLT_MAX__ 3.40282346638528859811704183484516925e+38F
__FLT_NORM_MAX__ 3.40282346638528859811704183484516925e+38F
__FLT_MIN__ 1.17549435082228750796873653722224568e-38F
__FLT_EPSILON__ 1.19209289550781250000000000000000000e-7F
__FLT_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F
__FLT_HAS_DENORM__ 1
__FLT_HAS_INFINITY__ 1
__FLT_HAS_QUIET_NAN__ 1
__FLT_IS_IEC_60559__ 1
__DBL_MANT_DIG__ 53
__DBL_DIG__ 15
__DBL_MIN_EXP__ (-1021)
__DBL_MIN_10_EXP__ (-307)
__DBL_MAX_EXP__ 1024
__DBL_MAX_10_EXP__ 308
__DBL_DECIMAL_DIG__ 17
__DBL_MAX__ double(1.79769313486231570814527423731704357e+308L)
__DBL_NORM_MAX__ double(1.79769313486231570814527423731704357e+308L)
__DBL_MIN__ double(2.22507385850720138309023271733240406e-308L)
__DBL_EPSILON__ double(2.22044604925031308084726333618164062e-16L)
__DBL_DENORM_MIN__ double(4.94065645841246544176568792868221372e-324L)
__DBL_HAS_DENORM__ 1
__DBL_HAS_INFINITY__ 1
__DBL_HAS_QUIET_NAN__ 1
__DBL_IS_IEC_60559__ 1
__LDBL_MANT_DIG__ 64
__LDBL_DIG__ 18
__LDBL_MIN_EXP__ (-16381)
__LDBL_MIN_10_EXP__ (-4931)
__LDBL_MAX_EXP__ 16384
__LDBL_MAX_10_EXP__ 4932
__DECIMAL_DIG__ 21
__LDBL_DECIMAL_DIG__ 21
__LDBL_MAX__ 1.18973149535723176502126385303097021e+4932L
__LDBL_NORM_MAX__ 1.18973149535723176502126385303097021e+4932L
__LDBL_MIN__ 3.36210314311209350626267781732175260e-4932L
__LDBL_EPSILON__ 1.08420217248550443400745280086994171e-19L
__LDBL_DENORM_MIN__ 3.64519953188247460252840593361941982e-4951L
__LDBL_HAS_DENORM__ 1
__LDBL_HAS_INFINITY__ 1
__LDBL_HAS_QUIET_NAN__ 1
__LDBL_IS_IEC_60559__ 1
__FLT16_MANT_DIG__ 11
__FLT16_DIG__ 3
__FLT16_MIN_EXP__ (-13)
__FLT16_MIN_10_EXP__ (-4)
__FLT16_MAX_EXP__ 16
__FLT16_MAX_10_EXP__ 4
__FLT16_DECIMAL_DIG__ 5
__FLT16_MAX__ 6.55040000000000000000000000000000000e+4F16
__FLT16_NORM_MAX__ 6.55040000000000000000000000000000000e+4F16
__FLT16_MIN__ 6.10351562500000000000000000000000000e-5F16
__FLT16_EPSILON__ 9.76562500000000000000000000000000000e-4F16
__FLT16_DENORM_MIN__ 5.96046447753906250000000000000000000e-8F16
__FLT16_HAS_DENORM__ 1
__FLT16_HAS_INFINITY__ 1
__FLT16_HAS_QUIET_NAN__ 1
__FLT16_IS_IEC_60559__ 1
__FLT32_MANT_DIG__ 24
__FLT32_DIG__ 6
__FLT32_MIN_EXP__ (-125)
__FLT32_MIN_10_EXP__ (-37)
__FLT32_MAX_EXP__ 128
__FLT32_MAX_10_EXP__ 38
__FLT32_DECIMAL_DIG__ 9
__FLT32_MAX__ 3.40282346638528859811704183484516925e+38F32
__FLT32_NORM_MAX__ 3.40282346638528859811704183484516925e+38F32
__FLT32_MIN__ 1.17549435082228750796873653722224568e-38F32
__FLT32_EPSILON__ 1.19209289550781250000000000000000000e-7F32
__FLT32_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F32
__FLT32_HAS_DENORM__ 1
__FLT32_HAS_INFINITY__ 1
__FLT32_HAS_QUIET_NAN__ 1
__FLT32_IS_IEC_60559__ 1
__FLT64_MANT_DIG__ 53
__FLT64_DIG__ 15
__FLT64_MIN_EXP__ (-1021)
__FLT64_MIN_10_EXP__ (-307)
__FLT64_MAX_EXP__ 1024
__FLT64_MAX_10_EXP__ 308
__FLT64_DECIMAL_DIG__ 17
__FLT64_MAX__ 1.79769313486231570814527423731704357e+308F64
__FLT64_NORM_MAX__ 1.79769313486231570814527423731704357e+308F64
__FLT64_MIN__ 2.22507385850720138309023271733240406e-308F64
__FLT64_EPSILON__ 2.22044604925031308084726333618164062e-16F64
__FLT64_DENORM_MIN__ 4.94065645841246544176568792868221372e-324F64
__FLT64_HAS_DENORM__ 1
__FLT64_HAS_INFINITY__ 1
__FLT64_HAS_QUIET_NAN__ 1
__FLT64_IS_IEC_60559__ 1
__FLT128_MANT_DIG__ 113
__FLT128_DIG__ 33
__FLT128_MIN_EXP__ (-16381)
__FLT128_MIN_10_EXP__ (-4931)
__FLT128_MAX_EXP__ 16384
__FLT128_MAX_10_EXP__ 4932
__FLT128_DECIMAL_DIG__ 36
__FLT128_MAX__ 1.18973149535723176508575932662800702e+4932F128
__FLT128_NORM_MAX__ 1.18973149535723176508575932662800702e+4932F128
__FLT128_MIN__ 3.36210314311209350626267781732175260e-4932F128
__FLT128_EPSILON__ 1.92592994438723585305597794258492732e-34F128
__FLT128_DENORM_MIN__ 6.47517511943802511092443895822764655e-4966F128
__FLT128_HAS_DENORM__ 1
__FLT128_HAS_INFINITY__ 1
__FLT128_HAS_QUIET_NAN__ 1
__FLT128_IS_IEC_60559__ 1
__FLT32X_MANT_DIG__ 53
__FLT32X_DIG__ 15
__FLT32X_MIN_EXP__ (-1021)
__FLT32X_MIN_10_EXP__ (-307)
__FLT32X_MAX_EXP__ 1024
__FLT32X_MAX_10_EXP__ 308
__FLT32X_DECIMAL_DIG__ 17
__FLT32X_MAX__ 1.79769313486231570814527423731704357e+308F32x
__FLT32X_NORM_MAX__ 1.79769313486231570814527423731704357e+308F32x
__FLT32X_MIN__ 2.22507385850720138309023271733240406e-308F32x
__FLT32X_EPSILON__ 2.22044604925031308084726333618164062e-16F32x
__FLT32X_DENORM_MIN__ 4.94065645841246544176568792868221372e-324F32x
__FLT32X_HAS_DENORM__ 1
__FLT32X_HAS_INFINITY__ 1
__FLT32X_HAS_QUIET_NAN__ 1
__FLT32X_IS_IEC_60559__ 1
__FLT64X_MANT_DIG__ 64
__FLT64X_DIG__ 18
__FLT64X_MIN_EXP__ (-16381)
__FLT64X_MIN_10_EXP__ (-4931)
__FLT64X_MAX_EXP__ 16384
__FLT64X_MAX_10_EXP__ 4932
__FLT64X_DECIMAL_DIG__ 21
__FLT64X_MAX__ 1.18973149535723176502126385303097021e+4932F64x
__FLT64X_NORM_MAX__ 1.18973149535723176502126385303097021e+4932F64x
__FLT64X_MIN__ 3.36210314311209350626267781732175260e-4932F64x
__FLT64X_EPSILON__ 1.08420217248550443400745280086994171e-19F64x
__FLT64X_DENORM_MIN__ 3.64519953188247460252840593361941982e-4951F64x
__FLT64X_HAS_DENORM__ 1
__FLT64X_HAS_INFINITY__ 1
__FLT64X_HAS_QUIET_NAN__ 1
__FLT64X_IS_IEC_60559__ 1
__BFLT16_MANT_DIG__ 8
__BFLT16_DIG__ 2
__BFLT16_MIN_EXP__ (-125)
__BFLT16_MIN_10_EXP__ (-37)
__BFLT16_MAX_EXP__ 128
__BFLT16_MAX_10_EXP__ 38
__BFLT16_DECIMAL_DIG__ 4
__BFLT16_MAX__ 3.38953138925153547590470800371487867e+38BF16
__BFLT16_NORM_MAX__ 3.38953138925153547590470800371487867e+38BF16
__BFLT16_MIN__ 1.17549435082228750796873653722224568e-38BF16
__BFLT16_EPSILON__ 7.81250000000000000000000000000000000e-3BF16
__BFLT16_DENORM_MIN__ 9.18354961579912115600575419704879436e-41BF16
__BFLT16_HAS_DENORM__ 1
__BFLT16_HAS_INFINITY__ 1
__BFLT16_HAS_QUIET_NAN__ 1
__BFLT16_IS_IEC_60559__ 0
__DEC32_MANT_DIG__ 7
__DEC32_MIN_EXP__ (-94)
__DEC32_MAX_EXP__ 97
__DEC32_MIN__ 1E-95DF
__DEC32_MAX__ 9.999999E96DF
__DEC32_EPSILON__ 1E-6DF
__DEC32_SUBNORMAL_MIN__ 0.000001E-95DF
__DEC64_MANT_DIG__ 16
__DEC64_MIN_EXP__ (-382)
__DEC64_MAX_EXP__ 385
__DEC64_MIN__ 1E-383DD
__DEC64_MAX__ 9.999999999999999E384DD
__DEC64_EPSILON__ 1E-15DD
__DEC64_SUBNORMAL_MIN__ 0.000000000000001E-383DD
__DEC128_MANT_DIG__ 34
__DEC128_MIN_EXP__ (-6142)
__DEC128_MAX_EXP__ 6145
__DEC128_MIN__ 1E-6143DL
__DEC128_MAX__ 9.999999999999999999999999999999999E6144DL
__DEC128_EPSILON__ 1E-33DL
__DEC128_SUBNORMAL_MIN__ 0.000000000000000000000000000000001E-6143DL
__REGISTER_PREFIX__ 
__USER_LABEL_PREFIX__ 
__GNUC_STDC_INLINE__ 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1
__GCC_ATOMIC_BOOL_LOCK_FREE 2
__GCC_ATOMIC_CHAR_LOCK_FREE 2
__GCC_ATOMIC_CHAR16_T_LOCK_FREE 2
__GCC_ATOMIC_CHAR32_T_LOCK_FREE 2
__GCC_ATOMIC_WCHAR_T_LOCK_FREE 2
__GCC_ATOMIC_SHORT_LOCK_FREE 2
__GCC_ATOMIC_INT_LOCK_FREE 2
__GCC_ATOMIC_LONG_LOCK_FREE 2
__GCC_ATOMIC_LLONG_LOCK_FREE 2
__GCC_ATOMIC_TEST_AND_SET_TRUEVAL 1
__GCC_DESTRUCTIVE_SIZE 64
__GCC_CONSTRUCTIVE_SIZE 64
__GCC_ATOMIC_POINTER_LOCK_FREE 2
__HAVE_SPECULATION_SAFE_VALUE 1
__GCC_HAVE_DWARF2_CFI_ASM 1
__PRAGMA_REDEFINE_EXTNAME 1
__SIZEOF_INT128__ 16
__SIZEOF_WCHAR_T__ 4
__SIZEOF_WINT_T__ 4
__SIZEOF_PTRDIFF_T__ 8
__amd64 1
__amd64__ 1
__x86_64 1
__x86_64__ 1
__SIZEOF_FLOAT80__ 16
__SIZEOF_FLOAT128__ 16
__ATOMIC_HLE_ACQUIRE 65536
__ATOMIC_HLE_RELEASE 131072
__GCC_ASM_FLAG_OUTPUTS__ 1
__k8 1
__k8__ 1
__code_model_small__ 1
__MMX__ 1
__SSE__ 1
__SSE2__ 1
__FXSR__ 1
__SSE_MATH__ 1
__SSE2_MATH__ 1
__MMX_WITH_SSE__ 1
__SEG_FS 1
__SEG_GS 1
__gnu_linux__ 1
__linux 1
__linux__ 1
linux 1
__unix 1
__unix__ 1
unix 1
__ELF__ 1
__DECIMAL_BID_FORMAT__ 1
_GNU_SOURCE 1
file /home/osandov/src/elfutils/tests/main.cc
 file /usr/include/stdc-predef.h
  _STDC_PREDEF_H 1
  __STDC_IEC_559__ 1
  __STDC_IEC_60559_BFP__ 201404L
  __STDC_IEC_559_COMPLEX__ 1
  __STDC_IEC_60559_COMPLEX__ 201404L
  __STDC_ISO_10646__ 201706L
 /file
 file /home/osandov/src/elfutils/tests/foobar.h
  FROB(x) ((x) ^ 0x2a2a2a2a)
  FRY(x) ((x) * 0x100000001b3)
 /file
 MAIN_ARGS int argc, char **argv
/file
EOF
done

exit 0
