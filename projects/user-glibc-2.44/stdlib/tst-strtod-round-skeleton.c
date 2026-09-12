/* Test for correct rounding of results of strtod and related
   functions.
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
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math-tests.h>
#include <tininess.h>

#include "tst-strtod.h"

/* Non-standard macros expected to be externally defined:

   L_(str): Pastes the appropriate modifier to a string literal str.

   FNPFX: Expands to the correct prefix for the strtod equivalent
          of type CHAR. (e.g str or wcs).

   CHAR: Expands to the string type being tested (e.g wchar_t or char).

   STRM: Expands to a string literal suitable for printing CHAR* via
         printf (e.g "%s" or "%ls"). */

#define _CONCAT(a, b) a ## b
#define CONCAT(a, b) _CONCAT (a, b)

#define STRTO(x) CONCAT (CONCAT (FNPFX, to), x)

#if LDBL_MANT_DIG == 106 && LDBL_MAX_EXP == 1024
/* This is a stupid hack for IBM long double.  This test ignores
   inexact values for long double due to the limitations of the
   format.  This ensures rounding tests are ignored.  */
# undef ROUNDING_TESTS_long_double
# define ROUNDING_TESTS_long_double(x) 0
#endif

/* Generator to create an FTYPE member variabled named FSUF
   used to populate struct member variables.  */
#define FTYPE_MEMBER(FSUF, FTYPE, FTOSTR, LSUF, CSUF)  \
       FTYPE FSUF;

/* Likewise, but each member is of type bool.  */
#define BOOL_MEMBER(FSUF, FTYPE, FTOSTR, LSUF, CSUF)  \
       bool FSUF;

#define STRUCT_FOREACH_FLOAT_FTYPE GEN_TEST_STRTOD_FOREACH (FTYPE_MEMBER)
#define STRUCT_FOREACH_FLOAT_BOOL GEN_TEST_STRTOD_FOREACH (BOOL_MEMBER)

/* Define the long double choose (CHOOSE_ld) macro
   to select the appropriate generated long double
   value from the generated test data.  */
#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024
/* This is for the long double == double format.  */
# define CHOOSE_ld(f,d,...) d
#elif LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384 && LDBL_MIN_EXP == -16381
/* This is for the Intel extended float format.  */
# define CHOOSE_ld(f,d,ld64i,...) ld64i
#elif LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384 && LDBL_MIN_EXP == -16382
/* This is for the Motorola extended float format.  */
# define CHOOSE_ld(f,d,ld64i,ld64m,...) ld64m
#elif LDBL_MANT_DIG == 106 && LDBL_MAX_EXP == 1024
/* This is for the IBM extended double format.  */
# define CHOOSE_ld(f,d,ld64i,ld64m,ld106,...) ld106
#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384
/* This is for the IEEE binary128 format.  */
# define CHOOSE_ld(f,d,ld64i,ld64m,ld106,ld113,...) ld113
#else
# error "unknown long double format"
#endif

/* Add type specific choosing macros below.  */
#define CHOOSE_f(f,...) f
#define CHOOSE_f32(f,...) f
#define CHOOSE_d(f,d,...) d
#define CHOOSE_f64(f,d,...) d
#define CHOOSE_f32x(f,d,...) d
#define CHOOSE_f128(f,d,ld64i,ld64m,ld106,ld113,...) ld113
/* long double is special, and handled above.  _Float16 would require
   updates to the generator to generate appropriate expectations, and
   updates to the test inputs to cover difficult rounding cases for
   _Float16.  */

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

/* Selector for expected result field of a given type.  */
#define _ENTRY(FSUF, FTYPE, FTOSTR, LSUF, CSUF, ...)  \
  CONCAT (CHOOSE_ ## FSUF (__VA_ARGS__), LSUF),
#define ENTRY(...) \
  GEN_TEST_STRTOD_FOREACH (_ENTRY, __VA_ARGS__)

/* Selector for boolean exact tag of expected results and that for
   overflow.  */
#define _XNTRY(FSUF, FTYPE, FTOSTR, LSUF, CSUF, ...)  \
  CHOOSE_ ## FSUF (__VA_ARGS__),
#define XNTRY(...) \
  GEN_TEST_STRTOD_FOREACH (_XNTRY, __VA_ARGS__)

/* This is hacky way around the seemingly unavoidable macro
   expansion of the INFINITY or HUGE_VAL like macros in the
   above.  It is assumed the compiler will implicitly convert
   the infinity correctly.  */
#define INF INFINITY + 0.0

/* This macro is used in conjunction with the output from the
   gen-tst-strtod-round utility to select the appropriately
   rounded long double value for a given format.  */
#define TEST(s,							\
	     fx, fd, fdo, fdu, fn, fno, fnu,			\
	     fz, fzo, fzu, fu, fuo, fuu,			\
	     dx, dd, ddo, ddu, dn, dno, dnu,			\
	     dz, dzo, dzu, du, duo, duu,			\
	     ld64ix, ld64id, ld64ido, ld64idu,			\
	     ld64in, ld64ino, ld64inu,				\
	     ld64iz, ld64izo, ld64izu,				\
	     ld64iu, ld64iuo, ld64iuu,				\
	     ld64mx, ld64md, ld64mdo, ld64mdu,			\
	     ld64mn, ld64mno, ld64mnu,				\
	     ld64mz, ld64mzo, ld64mzu,				\
	     ld64mu, ld64muo, ld64muu,				\
	     ld106x, ld106d, ld106do, ld106du,			\
	     ld106n, ld106no, ld106nu,				\
	     ld106z, ld106zo, ld106zu,				\
	     ld106u, ld106uo, ld106uu,				\
	     ld113x, ld113d, ld113do, ld113du,			\
	     ld113n, ld113no, ld113nu,				\
	     ld113z, ld113zo, ld113zu,				\
	     ld113u, ld113uo, ld113uu)				\
  {								\
    L_ (s),							\
    { XNTRY (fx, dx, ld64ix, ld64mx, ld106x, ld113x) },		\
    {								\
    { ENTRY (fn, dn, ld64in, ld64mn, ld106n, ld113n) },		\
    { ENTRY (fd, dd, ld64id, ld64md, ld106d, ld113d) },		\
    { ENTRY (fz, dz, ld64iz, ld64mz, ld106z, ld113z) },		\
    { ENTRY (fu, du, ld64iu, ld64mu, ld106u, ld113u) }		\
    },								\
    {								\
    { XNTRY (fno, dno, ld64ino, ld64mno, ld106no, ld113no) },	\
    { XNTRY (fdo, ddo, ld64ido, ld64mdo, ld106do, ld113do) },	\
    { XNTRY (fzo, dzo, ld64izo, ld64mzo, ld106zo, ld113zo) },	\
    { XNTRY (fuo, duo, ld64iuo, ld64muo, ld106uo, ld113uo) }	\
    },								\
    {								\
    { XNTRY (fnu, dnu, ld64inu, ld64mnu, ld106nu, ld113nu) },	\
    { XNTRY (fdu, ddu, ld64idu, ld64mdu, ld106du, ld113du) },	\
    { XNTRY (fzu, dzu, ld64izu, ld64mzu, ld106zu, ld113zu) },	\
    { XNTRY (fuu, duu, ld64iuu, ld64muu, ld106uu, ld113uu) }	\
    }								\
  }

struct test_exactness
  {
  STRUCT_FOREACH_FLOAT_BOOL
  };

struct test_results
  {
  STRUCT_FOREACH_FLOAT_FTYPE
  };

struct test_overflow
  {
  STRUCT_FOREACH_FLOAT_BOOL
  };

struct test_underflow
  {
  STRUCT_FOREACH_FLOAT_BOOL
  };

struct test {
  const CHAR *s;
  struct test_exactness exact;
  struct test_results r[4];
  struct test_overflow o[4];
  struct test_underflow u[4];
};

/* Include the generated test data.  */
#include "tst-strtod-round-data.h"

#define STRX(x) #x
#define STR(x) STRX (x)
#define FNPFXS STR (FNPFX)

#ifndef FE_INEXACT
# define FE_INEXACT 0
#endif

#ifndef FE_OVERFLOW
# define FE_OVERFLOW 0
#endif

#ifndef FE_UNDERFLOW
# define FE_UNDERFLOW 0
#endif

#define GEN_ONE_TEST(FSUF, FTYPE, FTOSTR, LSUF, CSUF)		\
{								\
  feclearexcept (FE_ALL_EXCEPT);				\
  errno = 12345;						\
  FTYPE f = STRTO (FSUF) (s, NULL);				\
  int new_errno = errno;					\
  if (f != expected->FSUF					\
      || (copysign ## CSUF) (1.0 ## LSUF, f)			\
	 != (copysign ## CSUF) (1.0 ## LSUF, expected->FSUF))	\
    {								\
      char efstr[FSTRLENMAX];					\
      char fstr[FSTRLENMAX];					\
      FTOSTR (efstr, FSTRLENMAX, "%a", expected->FSUF);		\
      FTOSTR (fstr, FSTRLENMAX, "%a", f);			\
      printf (FNPFXS "to" #FSUF  " (" STRM ") returned %s not "	\
	      "%s (%s)\n", s, fstr, efstr, mode_name);		\
      if (ROUNDING_TESTS (FTYPE, rnd_mode) || exact->FSUF)	\
	result = 1;						\
      else							\
	printf ("ignoring this inexact result\n");		\
    }								\
  else								\
    {								\
      if (FE_INEXACT != 0)					\
	{							\
	  bool inexact_raised = fetestexcept (FE_INEXACT) != 0;	\
	  if (inexact_raised != !exact->FSUF)			\
	    {							\
	      printf (FNPFXS "to" #FSUF				\
		      " (" STRM ") inexact %d "			\
		      "not %d\n", s, inexact_raised,		\
		      !exact->FSUF);				\
	      if (EXCEPTION_TESTS (FTYPE))			\
		result = 1;					\
	      else						\
		printf ("ignoring this exception error\n");	\
	    }							\
	}							\
      if (FE_OVERFLOW != 0)					\
	{							\
	  bool overflow_raised					\
	    = fetestexcept (FE_OVERFLOW) != 0;			\
	  if (overflow_raised != overflow->FSUF)		\
	    {							\
	      printf (FNPFXS "to" #FSUF				\
		      " (" STRM ") overflow %d "		\
		      "not %d\n", s, overflow_raised,		\
		      overflow->FSUF);				\
	      if (EXCEPTION_TESTS (FTYPE))			\
		result = 1;					\
	      else						\
		printf ("ignoring this exception error\n");	\
	    }							\
	}							\
      if (overflow->FSUF && new_errno != ERANGE)		\
	{							\
	  printf (FNPFXS "to" #FSUF				\
		  " (" STRM ") left errno == %d,"		\
		  " not %d (ERANGE)\n",				\
		  s, new_errno, ERANGE);			\
	  result = 1;						\
	}							\
      if (FE_UNDERFLOW != 0)					\
	{							\
	  bool underflow_raised					\
	    = fetestexcept (FE_UNDERFLOW) != 0;			\
	  if (underflow_raised != underflow->FSUF)		\
	    {							\
	      printf (FNPFXS "to" #FSUF				\
		      " (" STRM ") underflow %d "		\
		      "not %d\n", s, underflow_raised,		\
		      underflow->FSUF);				\
	      if (EXCEPTION_TESTS (FTYPE))			\
		result = 1;					\
	      else						\
		printf ("ignoring this exception error\n");	\
	    }							\
	}							\
      if (underflow->FSUF && new_errno != ERANGE)		\
	{							\
	  printf (FNPFXS "to" #FSUF				\
		  " (" STRM ") left errno == %d,"		\
		  " not %d (ERANGE)\n",				\
		  s, new_errno, ERANGE);			\
	  result = 1;						\
	}							\
      if (!overflow->FSUF					\
	  && !underflow->FSUF					\
	  && new_errno != 12345)				\
	{							\
	  printf (FNPFXS "to" #FSUF				\
		  " (" STRM ") set errno == %d,"		\
		  " should be unchanged\n",			\
		  s, new_errno);				\
	  result = 1;						\
	}							\
    }								\
}

static int
test_in_one_mode (const CHAR *s, const struct test_results *expected,
		    const struct test_exactness *exact,
		    const struct test_overflow *overflow,
		    const struct test_underflow *underflow,
		    const char *mode_name, int rnd_mode)
{
  int result = 0;
  GEN_TEST_STRTOD_FOREACH (GEN_ONE_TEST)
  return result;
}

static const struct fetestmodes
  {
  const char *mode_name;
  int rnd_mode;
  int rnd_i; /* Corresponding index into r array of struct test.  */
  } modes[] = {
    { "default rounding mode", FE_TONEAREST, 0 },
#ifdef FE_DOWNWARD
    { "FE_DOWNWARD", FE_DOWNWARD, 1 },
#endif
#ifdef FE_TOWARDZERO
    { "FE_TOWARDZERO", FE_TOWARDZERO, 2 },
#endif
#ifdef FE_UPWARD
    { "FE_UPWARD", FE_UPWARD, 3 },
#endif
    {}
};

static int
do_test (void)
{
  int save_round_mode __attribute__ ((unused)) = fegetround ();
  int result = 0;
  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
    {
      result |= test_in_one_mode (tests[i].s, &tests[i].r[modes[0].rnd_i],
				  &tests[i].exact, &tests[i].o[modes[0].rnd_i],
				  &tests[i].u[modes[0].rnd_i],
				  modes[0].mode_name, modes[0].rnd_mode);
      for (const struct fetestmodes *m = &modes[1]; m->mode_name != NULL; m++)
	{
	  if (!fesetround (m->rnd_mode))
	    {
	      result |= test_in_one_mode (tests[i].s, &tests[i].r[m->rnd_i],
					  &tests[i].exact,
					  &tests[i].o[m->rnd_i],
					  &tests[i].u[m->rnd_i],
					  m->mode_name,
					  m->rnd_mode);
	      fesetround (save_round_mode);
	    }
	}
    }
  return result;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
