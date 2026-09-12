/* Test for strtod handling of arguments that may cause floating-point
   underflow.
   Copyright (C) 2012-2026 Free Software Foundation, Inc.
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

/* Defining _LIBC_TEST ensures long double math functions are
   declared in the headers.  */
#define _LIBC_TEST 1
#define __STDC_WANT_IEC_60559_TYPES_EXT__
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <tininess.h>

#include "tst-strtod.h"

/* Logic for selecting between tests for different formats is as in
   tst-strtod-skeleton.c, but here it is selecting string inputs with
   different underflow properties, rather than generated test
   data.  */

#define _CONCAT(a, b) a ## b
#define CONCAT(a, b) _CONCAT (a, b)

#define MEMBER(FSUF, FTYPE, FTOSTR, LSUF, CSUF)	\
  const char *s_ ## FSUF;

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
# define CHOOSE_ld(f,d,...) d
#elif LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384 && LDBL_MIN_EXP == -16381
# define CHOOSE_ld(f,d,ld64i,...) ld64i
#elif LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384 && LDBL_MIN_EXP == -16382
# define CHOOSE_ld(f,d,ld64i,ld64m,...) ld64m
#elif LDBL_MANT_DIG == 106 && LDBL_MAX_EXP == 1024
# define CHOOSE_ld(f,d,ld64i,ld64m,ld106,...) ld106
#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384
# define CHOOSE_ld(f,d,ld64i,ld64m,ld106,ld113,...) ld113
#else
# error "unknown long double format"
#endif

#define CHOOSE_f(f,...) f
#define CHOOSE_f32(f,...) f
#define CHOOSE_d(f,d,...) d
#define CHOOSE_f64(f,d,...) d
#define CHOOSE_f32x(f,d,...) d
#define CHOOSE_f128(f,d,ld64i,ld64m,ld106,ld113,...) ld113

#if __HAVE_FLOAT64X
# if FLT64X_MANT_DIG == 113 && FLT64X_MAX_EXP == 16384
#  define CHOOSE_f64x(f,d,ld64i,ld64m,ld106,ld113,...) ld113
# elif (FLT64X_MANT_DIG == 64			\
	&& FLT64X_MAX_EXP == 16384		\
	&& FLT64X_MIN_EXP == -16381)
#  define CHOOSE_f64x(f,d,ld64i,...) ld64i
# else
#  error "unknown _Float64x format"
# endif
#endif

#define _XNTRY(FSUF, FTYPE, FTOSTR, LSUF, CSUF, ...)	\
  CHOOSE_ ## FSUF (__VA_ARGS__),
#define XNTRY(...) \
  GEN_TEST_STRTOD_FOREACH (_XNTRY, __VA_ARGS__)

#define TEST(f, d, ld64i, ld64m, ld106, ld113, u) \
  { XNTRY(f, d, ld64i, ld64m, ld106, ld113) u }

enum underflow_case
  {
    /* Result is exact or outside the subnormal range.  */
    UNDERFLOW_NONE,
    /* Result has magnitude at most half way between the largest
       subnormal value and the smallest positive normal value, and is
       not exact, so underflows in all rounding modes and independent
       of how tininess is detected.  */
    UNDERFLOW_ALWAYS,
    /* Result is positive, with magnitude larger than half way between
       the largest subnormal value and the least positive normal
       value, but would underflow when rounded to nearest to normal
       precision, so underflows after rounding in all modes except
       rounding upward.  */
    UNDERFLOW_EXCEPT_UPWARD,
    /* Likewise, for a negative result, underflowing after rounding
       except when rounding downward.  */
    UNDERFLOW_EXCEPT_DOWNWARD,
    /* Result is positive, with magnitude at least three quarters of
       the way from the largest subnormal value to the smallest
       positive normal value, so underflows after rounding only when
       rounding downward or toward zero.  */
    UNDERFLOW_ONLY_DOWNWARD_ZERO,
    /* Likewise, for a negative result, underflowing after rounding
       only when rounding upward or toward zero.  */
    UNDERFLOW_ONLY_UPWARD_ZERO,
  };

struct test
{
  GEN_TEST_STRTOD_FOREACH (MEMBER)
  enum underflow_case c;
};

static const struct test tests[] =
  {
    TEST ("0x1p-126",
	  "0x1p-1022",
	  "0x1p-16382",
	  "0x1p-16383",
	  "0x1p-969",
	  "0x1p-16382",
	  UNDERFLOW_NONE),
    TEST ("-0x1p-126",
	  "-0x1p-1022",
	  "-0x1p-16382",
	  "-0x1p-16383",
	  "-0x1p-969",
	  "-0x1p-16382",
	  UNDERFLOW_NONE),
    TEST ("0x0p-10000000000000000000000000",
	  "0x0p-10000000000000000000000000",
	  "0x0p-10000000000000000000000000",
	  "0x0p-10000000000000000000000000",
	  "0x0p-10000000000000000000000000",
	  "0x0p-10000000000000000000000000",
	  UNDERFLOW_NONE),
    TEST ("-0x0p-10000000000000000000000000",
	  "-0x0p-10000000000000000000000000",
	  "-0x0p-10000000000000000000000000",
	  "-0x0p-10000000000000000000000000",
	  "-0x0p-10000000000000000000000000",
	  "-0x0p-10000000000000000000000000",
	  UNDERFLOW_NONE),
    TEST ("0x1p-10000000000000000000000000",
	  "0x1p-10000000000000000000000000",
	  "0x1p-10000000000000000000000000",
	  "0x1p-10000000000000000000000000",
	  "0x1p-10000000000000000000000000",
	  "0x1p-10000000000000000000000000",
	  UNDERFLOW_ALWAYS),
    TEST ("-0x1p-10000000000000000000000000",
	  "-0x1p-10000000000000000000000000",
	  "-0x1p-10000000000000000000000000",
	  "-0x1p-10000000000000000000000000",
	  "-0x1p-10000000000000000000000000",
	  "-0x1p-10000000000000000000000000",
	  UNDERFLOW_ALWAYS),
    TEST ("0x1.000000000000000000001p-126",
	  "0x1.000000000000000000001p-1022",
	  "0x1.000000000000000000001p-16382",
	  "0x1.000000000000000000001p-16383",
	  "0x1.000000000000000000001p-969",
	  "0x1.00000000000000000000000000000000000000001p-16382",
	  UNDERFLOW_NONE),
    TEST ("-0x1.000000000000000000001p-126",
	  "-0x1.000000000000000000001p-1022",
	  "-0x1.000000000000000000001p-16382",
	  "-0x1.000000000000000000001p-16383",
	  "-0x1.000000000000000000001p-969",
	  "-0x1.00000000000000000000000000000000000000001p-16382",
	  UNDERFLOW_NONE),
    TEST ("0x1p-150",
	  "0x1p-1075",
	  "0x1p-16446",
	  "0x1p-16447",
	  "0x1p-1075",
	  "0x1p-16495",
	  UNDERFLOW_ALWAYS),
    TEST ("-0x1p-150",
	  "-0x1p-1075",
	  "-0x1p-16446",
	  "-0x1p-16447",
	  "-0x1p-1075",
	  "-0x1p-16495",
	  UNDERFLOW_ALWAYS),
    TEST ("0x1p-127",
	  "0x1p-1023",
	  "0x1p-16383",
	  "0x1p-16384",
	  "0x1p-970",
	  "0x1p-16383",
	  UNDERFLOW_NONE),
    TEST ("-0x1p-127",
	  "-0x1p-1023",
	  "-0x1p-16383",
	  "-0x1p-16384",
	  "-0x1p-970",
	  "-0x1p-16383",
	  UNDERFLOW_NONE),
    TEST ("0x1p-149",
	  "0x1p-1074",
	  "0x1p-16445",
	  "0x1p-16446",
	  "0x1p-1074",
	  "0x1p-16494",
	  UNDERFLOW_NONE),
    TEST ("-0x1p-149",
	  "-0x1p-1074",
	  "-0x1p-16445",
	  "-0x1p-16446",
	  "-0x1p-1074",
	  "-0x1p-16494",
	  UNDERFLOW_NONE),
    TEST ("0x1.fffffcp-127",
	  "0x1.ffffffffffffep-1023",
	  "0x1.fffffffffffffffcp-16383",
	  "0x1.fffffffffffffffcp-16384",
	  "0x1.ffffffffffffffffffffffffffp-970",
	  "0x1.fffffffffffffffffffffffffffep-16383",
	  UNDERFLOW_NONE),
    TEST ("-0x1.fffffcp-127",
	  "-0x1.ffffffffffffep-1023",
	  "-0x1.fffffffffffffffcp-16383",
	  "-0x1.fffffffffffffffcp-16384",
	  "-0x1.ffffffffffffffffffffffffffp-970",
	  "-0x1.fffffffffffffffffffffffffffep-16383",
	  UNDERFLOW_NONE),
    TEST ("0x1.fffffep-127",
	  "0x1.fffffffffffffp-1023",
	  "0x1.fffffffffffffffep-16383",
	  "0x1.fffffffffffffffep-16384",
	  "0x1.ffffffffffffffffffffffffff8p-970",
	  "0x1.ffffffffffffffffffffffffffffp-16383",
	  UNDERFLOW_ALWAYS),
    TEST ("-0x1.fffffep-127",
	  "-0x1.fffffffffffffp-1023",
	  "-0x1.fffffffffffffffep-16383",
	  "-0x1.fffffffffffffffep-16384",
	  "-0x1.ffffffffffffffffffffffffff8p-970",
	  "-0x1.ffffffffffffffffffffffffffffp-16383",
	  UNDERFLOW_ALWAYS),
    TEST ("0x1.fffffe0001p-127",
	  "0x1.fffffffffffff0001p-1023",
	  "0x1.fffffffffffffffe0001p-16383",
	  "0x1.fffffffffffffffe0001p-16384",
	  "0x1.ffffffffffffffffffffffffff80001p-970",
	  "0x1.ffffffffffffffffffffffffffff0001p-16383",
	  UNDERFLOW_EXCEPT_UPWARD),
    TEST ("-0x1.fffffe0001p-127",
	  "-0x1.fffffffffffff0001p-1023",
	  "-0x1.fffffffffffffffe0001p-16383",
	  "-0x1.fffffffffffffffe0001p-16384",
	  "-0x1.ffffffffffffffffffffffffff80001p-970",
	  "-0x1.ffffffffffffffffffffffffffff0001p-16383",
	  UNDERFLOW_EXCEPT_DOWNWARD),
    TEST ("0x1.fffffeffffp-127",
	  "0x1.fffffffffffff7fffp-1023",
	  "0x1.fffffffffffffffeffffp-16383",
	  "0x1.fffffffffffffffeffffp-16384",
	  "0x1.ffffffffffffffffffffffffffbffffp-970",
	  "0x1.ffffffffffffffffffffffffffff7fffp-16383",
	  UNDERFLOW_EXCEPT_UPWARD),
    TEST ("-0x1.fffffeffffp-127",
	  "-0x1.fffffffffffff7fffp-1023",
	  "-0x1.fffffffffffffffeffffp-16383",
	  "-0x1.fffffffffffffffeffffp-16384",
	  "-0x1.ffffffffffffffffffffffffffbffffp-970",
	  "-0x1.ffffffffffffffffffffffffffff7fffp-16383",
	  UNDERFLOW_EXCEPT_DOWNWARD),
    TEST ("0x1.ffffffp-127",
	  "0x1.fffffffffffff8p-1023",
	  "0x1.ffffffffffffffffp-16383",
	  "0x1.ffffffffffffffffp-16384",
	  "0x1.ffffffffffffffffffffffffffcp-970",
	  "0x1.ffffffffffffffffffffffffffff8p-16383",
	  UNDERFLOW_ONLY_DOWNWARD_ZERO),
    TEST ("-0x1.ffffffp-127",
	  "-0x1.fffffffffffff8p-1023",
	  "-0x1.ffffffffffffffffp-16383",
	  "-0x1.ffffffffffffffffp-16384",
	  "-0x1.ffffffffffffffffffffffffffcp-970",
	  "-0x1.ffffffffffffffffffffffffffff8p-16383",
	  UNDERFLOW_ONLY_UPWARD_ZERO),
    TEST ("0x1.ffffffffffp-127",
	  "0x1.fffffffffffffffffp-1023",
	  "0x1.ffffffffffffffffffffp-16383",
	  "0x1.ffffffffffffffffffffp-16384",
	  "0x1.ffffffffffffffffffffffffffffffp-970",
	  "0x1.ffffffffffffffffffffffffffffffffp-16383",
	  UNDERFLOW_ONLY_DOWNWARD_ZERO),
    TEST ("-0x1.ffffffffffp-127",
	  "-0x1.fffffffffffffffffp-1023",
	  "-0x1.ffffffffffffffffffffp-16383",
	  "-0x1.ffffffffffffffffffffp-16384",
	  "-0x1.ffffffffffffffffffffffffffffffp-970",
	  "-0x1.ffffffffffffffffffffffffffffffffp-16383",
	  UNDERFLOW_ONLY_UPWARD_ZERO),
  };

/* Return whether to expect underflow from a particular testcase, in a
   given rounding mode.  */

static bool
expect_underflow (enum underflow_case c, int rm)
{
  if (c == UNDERFLOW_NONE)
    return false;
  if (c == UNDERFLOW_ALWAYS)
    return true;
  if (TININESS_AFTER_ROUNDING)
    {
      switch (rm)
	{
#ifdef FE_DOWNWARD
	case FE_DOWNWARD:
	  return (c == UNDERFLOW_EXCEPT_UPWARD
		  || c == UNDERFLOW_ONLY_DOWNWARD_ZERO);
#endif

#ifdef FE_TOWARDZERO
	case FE_TOWARDZERO:
	  return true;
#endif

#ifdef FE_UPWARD
	case FE_UPWARD:
	  return (c == UNDERFLOW_EXCEPT_DOWNWARD
		  || c == UNDERFLOW_ONLY_UPWARD_ZERO);
#endif

	default:
	  return (c == UNDERFLOW_EXCEPT_UPWARD
		  || c == UNDERFLOW_EXCEPT_DOWNWARD);
	}
    }
  else
    return true;
}

static bool support_underflow_exception = false;
volatile double d = DBL_MIN;
volatile double dd;

static bool
test_got_fe_underflow (void)
{
#ifdef FE_UNDERFLOW
  return fetestexcept (FE_UNDERFLOW) != 0;
#else
  return false;
#endif
}

#define TEST_STRTOD(FSUF, FTYPE, FTOSTR, LSUF, CSUF)			\
static int								\
test_strto ## FSUF (int i, int rm, const char *mode_name)		\
{									\
  const char *s = tests[i].s_ ## FSUF;					\
  enum underflow_case c = tests[i].c;					\
  int result = 0;							\
  feclearexcept (FE_ALL_EXCEPT);					\
  errno = 0;								\
  FTYPE d = strto ## FSUF (s, NULL);					\
  int got_errno = errno;						\
  bool got_fe_underflow = test_got_fe_underflow ();			\
  char buf[FSTRLENMAX];							\
  FTOSTR (buf, sizeof (buf), "%a", d);					\
  printf ("strto" #FSUF							\
	  " (%s) (%s) returned %s, errno = %d, "			\
	  "%sunderflow exception\n",					\
	  s, mode_name, buf, got_errno,					\
	  got_fe_underflow ? "" : "no ");				\
  bool this_expect_underflow = expect_underflow (c, rm);		\
  if (got_errno != 0 && got_errno != ERANGE)				\
    {									\
      puts ("FAIL: errno neither 0 nor ERANGE");			\
      result = 1;							\
    }									\
  else if (this_expect_underflow != (errno == ERANGE))			\
    {									\
      puts ("FAIL: underflow from errno differs from expectations");	\
      result = 1;							\
    }									\
  if (support_underflow_exception					\
      && got_fe_underflow != this_expect_underflow)			\
    {									\
      puts ("FAIL: underflow from exceptions "				\
	    "differs from expectations");				\
      result = 1;							\
    }									\
  return result;							\
}

GEN_TEST_STRTOD_FOREACH (TEST_STRTOD)

static int
test_in_one_mode (size_t i, int rm, const char *mode_name)
{
  return STRTOD_TEST_FOREACH (test_strto, i, rm, mode_name);
}

static int
do_test (void)
{
  int save_round_mode __attribute__ ((unused)) = fegetround ();
  int result = 0;
#ifdef FE_TONEAREST
  const int fe_tonearest = FE_TONEAREST;
#else
  const int fe_tonearest = 0;
# if defined FE_DOWNWARD || defined FE_TOWARDZERO || defined FE_UPWARD
#  error "FE_TONEAREST not defined, but another rounding mode is"
# endif
#endif
#ifdef FE_UNDERFLOW
  feclearexcept (FE_ALL_EXCEPT);
  dd = d * d;
  if (fetestexcept (FE_UNDERFLOW))
    support_underflow_exception = true;
  else
    puts ("underflow exception not supported at runtime, only testing errno");
#endif
  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
    {
      result |= test_in_one_mode (i, fe_tonearest,
				  "default rounding mode");
#ifdef FE_DOWNWARD
      if (!fesetround (FE_DOWNWARD))
	{
	  result |= test_in_one_mode (i, FE_DOWNWARD,
				      "FE_DOWNWARD");
	  fesetround (save_round_mode);
	}
#endif
#ifdef FE_TOWARDZERO
      if (!fesetround (FE_TOWARDZERO))
	{
	  result |= test_in_one_mode (i, FE_TOWARDZERO,
				      "FE_TOWARDZERO");
	  fesetround (save_round_mode);
	}
#endif
#ifdef FE_UPWARD
      if (!fesetround (FE_UPWARD))
	{
	  result |= test_in_one_mode (i, FE_UPWARD,
				      "FE_UPWARD");
	  fesetround (save_round_mode);
	}
#endif
    }
  return result;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
