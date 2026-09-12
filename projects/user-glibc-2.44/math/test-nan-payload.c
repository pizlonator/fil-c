/* Test nan functions payload handling (bug 16961).
   Copyright (C) 2015-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#define _LIBC_TEST 1
#define __STDC_WANT_IEC_60559_TYPES_EXT__
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Avoid built-in functions.  */
#define WRAP_NAN(FUNC, STR) \
  ({ const char *volatile wns = (STR); FUNC (wns); })
#define WRAP_STRTO(FUNC, STR) \
  ({ const char *volatile wss = (STR); FUNC (wss, NULL); })

#define CHECK_IS_NAN(TYPE, A)			\
  do						\
    {						\
      if (isnan (A) && !issignaling (A))	\
	puts ("PASS: " #TYPE " " #A);		\
      else					\
	{					\
	  puts ("FAIL: " #TYPE " " #A);		\
	  result = 1;				\
	}					\
    }						\
  while (0)

#define CHECK_PAYLOAD(TYPE, FUNC, A, P)		\
  do						\
    {						\
      if (FUNC (&(A)) == (P))			\
	puts ("PASS: " #TYPE " payload " #A);	\
      else					\
	{					\
	  puts ("FAIL: " #TYPE " payload " #A);	\
	  result = 1;				\
	}					\
    }						\
  while (0)

#define CHECK_SAME_NAN(TYPE, A, B)			\
  do							\
    {							\
      if (memcmp (&(A), &(B), sizeof (A)) == 0)		\
	puts ("PASS: " #TYPE " " #A " = " #B);		\
      else						\
	{						\
	  puts ("FAIL: " #TYPE " " #A " = " #B);	\
	  result = 1;					\
	}						\
    }							\
  while (0)

#define CHECK_DIFF_NAN(TYPE, A, B)			\
  do							\
    {							\
      if (memcmp (&(A), &(B), sizeof (A)) != 0)		\
	puts ("PASS: " #TYPE " " #A " != " #B);		\
      else						\
	{						\
	  puts ("FAIL: " #TYPE " " #A " != " #B);	\
	  result = 1;					\
	}						\
    }							\
  while (0)

#define CLEAR_ERRNO				\
  do						\
    {						\
      errno = 12345;				\
    }						\
  while (0)

#define CHECK_ERRNO(TYPE, A)				\
  do							\
    {							\
      if (errno == 12345)				\
	puts ("PASS: " #TYPE " " #A " errno");		\
      else						\
	{						\
	  puts ("FAIL: " #TYPE " " #A " errno");	\
	  result = 1;					\
	}						\
    }							\
  while (0)

/* Cannot test payloads by memcmp for formats where NaNs have padding
   bits.  */
#define CAN_TEST_EQ(MANT_DIG) ((MANT_DIG) != 64 && (MANT_DIG) != 106)

#define RUN_TESTS(TYPE, SFUNC, FUNC, PLFUNC, MANT_DIG)	\
  do							\
    {							\
     CLEAR_ERRNO;					\
     TYPE n123 = WRAP_NAN (FUNC, "123");		\
     CHECK_ERRNO (TYPE, n123);				\
     CHECK_IS_NAN (TYPE, n123);				\
     CLEAR_ERRNO;					\
     TYPE s123 = WRAP_STRTO (SFUNC, "NAN(123)");	\
     CHECK_ERRNO (TYPE, s123);				\
     CHECK_IS_NAN (TYPE, s123);				\
     CLEAR_ERRNO;					\
     TYPE n456 = WRAP_NAN (FUNC, "456");		\
     CHECK_ERRNO (TYPE, n456);				\
     CHECK_IS_NAN (TYPE, n456);				\
     CLEAR_ERRNO;					\
     TYPE s456 = WRAP_STRTO (SFUNC, "NAN(456)");	\
     CHECK_ERRNO (TYPE, s456);				\
     CHECK_IS_NAN (TYPE, s456);				\
     CLEAR_ERRNO;					\
     TYPE nh123 = WRAP_NAN (FUNC, "0x123");		\
     CHECK_ERRNO (TYPE, nh123);				\
     CHECK_IS_NAN (TYPE, nh123);			\
     CLEAR_ERRNO;					\
     TYPE sh123 = WRAP_STRTO (SFUNC, "NAN(0x123)");	\
     CHECK_ERRNO (TYPE, sh123);				\
     CHECK_IS_NAN (TYPE, sh123);			\
     CLEAR_ERRNO;					\
     TYPE n123x = WRAP_NAN (FUNC, "123)");		\
     CHECK_ERRNO (TYPE, n123x);				\
     CHECK_IS_NAN (TYPE, n123x);			\
     CLEAR_ERRNO;					\
     TYPE nemp = WRAP_NAN (FUNC, "");			\
     CHECK_ERRNO (TYPE, nemp);				\
     CHECK_IS_NAN (TYPE, nemp);				\
     CLEAR_ERRNO;					\
     TYPE semp = WRAP_STRTO (SFUNC, "NAN()");		\
     CHECK_ERRNO (TYPE, semp);				\
     CHECK_IS_NAN (TYPE, semp);				\
     CLEAR_ERRNO;					\
     TYPE sx = WRAP_STRTO (SFUNC, "NAN");		\
     CHECK_ERRNO (TYPE, sx);				\
     CHECK_IS_NAN (TYPE, sx);				\
     CLEAR_ERRNO;					\
     TYPE novf = WRAP_NAN (FUNC, "9999999999"		\
			   "99999999999999999999"	\
			   "9999999999");		\
     CHECK_ERRNO (TYPE, novf);				\
     CHECK_IS_NAN (TYPE, novf);				\
     CLEAR_ERRNO;					\
     TYPE sovf = WRAP_STRTO (SFUNC, "NAN(9999999999"	\
			     "99999999999999999999"	\
			     "9999999999)");		\
     CHECK_ERRNO (TYPE, sovf);				\
     CHECK_IS_NAN (TYPE, sovf);				\
     if (CAN_TEST_EQ (MANT_DIG))			\
       CHECK_SAME_NAN (TYPE, n123, s123);		\
     CHECK_PAYLOAD (TYPE, PLFUNC, n123, 123);		\
     CHECK_PAYLOAD (TYPE, PLFUNC, s123, 123);		\
     if (CAN_TEST_EQ (MANT_DIG))			\
       CHECK_SAME_NAN (TYPE, n456, s456);		\
     CHECK_PAYLOAD (TYPE, PLFUNC, n456, 456);		\
     CHECK_PAYLOAD (TYPE, PLFUNC, s456, 456);		\
     if (CAN_TEST_EQ (MANT_DIG))			\
       CHECK_SAME_NAN (TYPE, nh123, sh123);		\
     CHECK_PAYLOAD (TYPE, PLFUNC, nh123, 0x123);	\
     CHECK_PAYLOAD (TYPE, PLFUNC, sh123, 0x123);	\
     if (CAN_TEST_EQ (MANT_DIG))			\
       CHECK_SAME_NAN (TYPE, nemp, semp);		\
     if (CAN_TEST_EQ (MANT_DIG))			\
       CHECK_SAME_NAN (TYPE, n123x, sx);		\
     CHECK_DIFF_NAN (TYPE, n123, n456);			\
     CHECK_DIFF_NAN (TYPE, n123, nemp);			\
     CHECK_DIFF_NAN (TYPE, n123, n123x);		\
     CHECK_DIFF_NAN (TYPE, n456, nemp);			\
     CHECK_DIFF_NAN (TYPE, n456, n123x);		\
    }							\
  while (0)

static int
do_test (void)
{
  int result = 0;
  RUN_TESTS (float, strtof, nanf, getpayloadf, FLT_MANT_DIG);
  RUN_TESTS (double, strtod, nan, getpayload, DBL_MANT_DIG);
  RUN_TESTS (long double, strtold, nanl, getpayloadl, LDBL_MANT_DIG);
#if __HAVE_FLOAT16
  RUN_TESTS (_Float16, strtof16, nanf16, getpayloadf16, FLT16_MANT_DIG);
#endif
#if __HAVE_FLOAT32
  RUN_TESTS (_Float32, strtof32, nanf32, getpayloadf32, FLT32_MANT_DIG);
#endif
#if __HAVE_FLOAT64
  RUN_TESTS (_Float64, strtof64, nanf64, getpayloadf64, FLT64_MANT_DIG);
#endif
#if __HAVE_FLOAT128
  RUN_TESTS (_Float128, strtof128, nanf128, getpayloadf128, FLT128_MANT_DIG);
#endif
#if __HAVE_FLOAT32X
  RUN_TESTS (_Float32x, strtof32x, nanf32x, getpayloadf32x, FLT32X_MANT_DIG);
#endif
#if __HAVE_FLOAT64X
  RUN_TESTS (_Float64x, strtof64x, nanf64x, getpayloadf64x, FLT64X_MANT_DIG);
#endif
#if __HAVE_FLOAT128X
  RUN_TESTS (_Float128x, strtof128x, nanf128x, getpayloadf128x,
	     FLT128X_MANT_DIG);
#endif
  return result;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
