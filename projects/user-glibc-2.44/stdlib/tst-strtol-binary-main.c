/* Test strtol functions with C23 binary integers.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <support/check.h>
#if TEST_LOCALE
# include <support/support.h>
#endif

#define CONCAT_(X, Y) X ## Y
#define CONCAT(X, Y) CONCAT_ (X, Y)
#define FNX(FN) CONCAT (FNPFX, FN)

#if WIDE
# define STRCHR wcschr
#else
# define STRCHR strchr
#endif

#define CHECK_RES(ARG, RES, EP, EXPECTED, EXPECTED_EP)			\
  do									\
    {									\
      if (TEST_C23)							\
	{								\
	  TEST_COMPARE ((RES), EXPECTED);				\
	  TEST_VERIFY ((EP) == EXPECTED_EP);				\
	}								\
      else								\
	{								\
	  TEST_COMPARE ((RES), 0);					\
	  TEST_VERIFY ((EP) == ((ARG)[0] == L_('-')			\
				? (ARG) + 2				\
				: (ARG) + 1));				\
	}								\
    }									\
  while (0)

static void
one_check (const CHAR *s, const CHAR *expected_p, long int expected_l,
	   unsigned long int expected_ul, long long int expected_ll,
	   unsigned long long int expected_ull)
{
  expected_p = expected_p == NULL ? STRCHR (s, '\0') : expected_p;

  CHAR *ep;
  long int ret_l;
  unsigned long int ret_ul;
  long long int ret_ll;
  unsigned long long int ret_ull;
  ret_l = FNX (l) (s, &ep, 0);
  CHECK_RES (s, ret_l, ep, expected_l, expected_p);
  ret_l = FNX (l) (s, &ep, 2);
  CHECK_RES (s, ret_l, ep, expected_l, expected_p);
  ret_ul = FNX (ul) (s, &ep, 0);
  CHECK_RES (s, ret_ul, ep, expected_ul, expected_p);
  ret_ul = FNX (ul) (s, &ep, 2);
  CHECK_RES (s, ret_ul, ep, expected_ul, expected_p);
  ret_ll = FNX (ll) (s, &ep, 0);
  CHECK_RES (s, ret_ll, ep, expected_ll, expected_p);
  ret_ll = FNX (ll) (s, &ep, 2);
  CHECK_RES (s, ret_ll, ep, expected_ll, expected_p);
  ret_ull = FNX (ull) (s, &ep, 0);
  CHECK_RES (s, ret_ull, ep, expected_ull, expected_p);
  ret_ull = FNX (ull) (s, &ep, 2);
  CHECK_RES (s, ret_ull, ep, expected_ull, expected_p);
  ret_ll = FNX (imax) (s, &ep, 0);
  CHECK_RES (s, ret_ll, ep, expected_ll, expected_p);
  ret_ll = FNX (imax) (s, &ep, 2);
  CHECK_RES (s, ret_ll, ep, expected_ll, expected_p);
  ret_ull = FNX (umax) (s, &ep, 0);
  CHECK_RES (s, ret_ull, ep, expected_ull, expected_p);
  ret_ull = FNX (umax) (s, &ep, 2);
  CHECK_RES (s, ret_ull, ep, expected_ull, expected_p);
#if TEST_Q
  ret_ll = FNX (q) (s, &ep, 0);
  CHECK_RES (s, ret_ll, ep, expected_ll, expected_p);
  ret_ll = FNX (q) (s, &ep, 2);
  CHECK_RES (s, ret_ll, ep, expected_ll, expected_p);
  ret_ull = FNX (uq) (s, &ep, 0);
  CHECK_RES (s, ret_ull, ep, expected_ull, expected_p);
  ret_ull = FNX (uq) (s, &ep, 2);
  CHECK_RES (s, ret_ull, ep, expected_ull, expected_p);
#endif
#if TEST_LOCALE
  locale_t loc = xnewlocale (LC_NUMERIC_MASK, "C", (locale_t) 0);
  ret_l = FNX (l_l) (s, &ep, 0, loc);
  CHECK_RES (s, ret_l, ep, expected_l, expected_p);
  ret_l = FNX (l_l) (s, &ep, 2, loc);
  CHECK_RES (s, ret_l, ep, expected_l, expected_p);
  ret_ul = FNX (ul_l) (s, &ep, 0, loc);
  CHECK_RES (s, ret_ul, ep, expected_ul, expected_p);
  ret_ul = FNX (ul_l) (s, &ep, 2, loc);
  CHECK_RES (s, ret_ul, ep, expected_ul, expected_p);
  ret_ll = FNX (ll_l) (s, &ep, 0, loc);
  CHECK_RES (s, ret_ll, ep, expected_ll, expected_p);
  ret_ll = FNX (ll_l) (s, &ep, 2, loc);
  CHECK_RES (s, ret_ll, ep, expected_ll, expected_p);
  ret_ull = FNX (ull_l) (s, &ep, 0, loc);
  CHECK_RES (s, ret_ull, ep, expected_ull, expected_p);
  ret_ull = FNX (ull_l) (s, &ep, 2, loc);
  CHECK_RES (s, ret_ull, ep, expected_ull, expected_p);
#endif
}

static int
do_test (void)
{
  {
    const CHAR *input = L_("0b");
    one_check (input, input + 1, 0L, 0UL, 0LL, 0ULL);
  }
  one_check (L_("0b101"), NULL, 5, 5, 5, 5);
  one_check (L_("0B101"), NULL, 5, 5, 5, 5);
  one_check (L_("-0b11111"), NULL, -31, -31, -31, -31);
  one_check (L_("-0B11111"), NULL, -31, -31, -31, -31);
  one_check (L_("0b111111111111111111111111111111111"), NULL,
	     LONG_MAX >= 0x1ffffffffLL ? (long int) 0x1ffffffffLL : LONG_MAX,
	     (ULONG_MAX >= 0x1ffffffffULL
	      ? (unsigned long int) 0x1ffffffffULL
	      : ULONG_MAX),
	     0x1ffffffffLL, 0x1ffffffffULL);
  one_check (L_("0B111111111111111111111111111111111"), NULL,
	     LONG_MAX >= 0x1ffffffffLL ? (long int) 0x1ffffffffLL : LONG_MAX,
	     (ULONG_MAX >= 0x1ffffffffULL
	      ? (unsigned long int) 0x1ffffffffULL
	      : ULONG_MAX),
	     0x1ffffffffLL, 0x1ffffffffULL);
  one_check (L_("-0b111111111111111111111111111111111"), NULL,
	     LONG_MIN <= -0x1ffffffffLL ? (long int) -0x1ffffffffLL : LONG_MIN,
	     (ULONG_MAX >= 0x1ffffffffULL
	      ? (unsigned long int) -0x1ffffffffULL
	      : ULONG_MAX),
	     -0x1ffffffffLL, -0x1ffffffffULL);
  one_check (L_("-0B111111111111111111111111111111111"), NULL,
	     LONG_MIN <= -0x1ffffffffLL ? (long int) -0x1ffffffffLL : LONG_MIN,
	     (ULONG_MAX >= 0x1ffffffffULL
	      ? (unsigned long int) -0x1ffffffffULL
	      : ULONG_MAX),
	     -0x1ffffffffLL, -0x1ffffffffULL);
  return 0;
}

#include <support/test-driver.c>
