/* Basic tests for __strtod_internal.
   Copyright (C) 1991-2026 Free Software Foundation, Inc.
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

#include <ctype.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "tst-strtod.h"

/* This tests internal interfaces, which are only defined for types
   with distinct ABIs, so disable testing for types without distinct
   ABIs.  */
#undef IF_FLOAT32
#define IF_FLOAT32(x)
#undef IF_FLOAT64
#define IF_FLOAT64(x)
#undef IF_FLOAT32X
#define IF_FLOAT32X(x)
#undef IF_FLOAT64X
#define IF_FLOAT64X(x)
#if !__HAVE_DISTINCT_FLOAT128
# undef IF_FLOAT128
# define IF_FLOAT128(x)
#endif

#define ntests (sizeof (tests) / sizeof (tests[0]))

/* Perform a few tests in a locale with thousands separators.  */
#define TEST_STRTOD(FSUF, FTYPE, FTOSTR, LSUF, CSUF)			\
static int								\
test_strto ## FSUF (void)						\
{									\
  static const struct							\
  {									\
    const char *loc;							\
    const char *str;							\
    FTYPE exp;								\
    ptrdiff_t nread;							\
  } tests[] =								\
    {									\
      { "de_DE.UTF-8", "1,5", 1.5 ## LSUF, 3 },				\
      { "de_DE.UTF-8", "1.5", 1.0 ## LSUF, 1 },				\
      { "de_DE.UTF-8", "1.500", 1500.0 ## LSUF, 5 },			\
      { "de_DE.UTF-8", "36.893.488.147.419.103.232", 0x1.0p65 ## LSUF, 26 } \
    };									\
  size_t n;								\
  int result = 0;							\
									\
  puts ("\nLocale tests");						\
									\
  for (n = 0; n < ntests; ++n)						\
    {									\
      FTYPE d;								\
      char *endp;							\
									\
      if (setlocale (LC_ALL, tests[n].loc) == NULL)			\
	{								\
	  printf ("cannot set locale %s\n", tests[n].loc);		\
	  result = 1;							\
	  continue;							\
	}								\
									\
      d = __strto ## FSUF ## _internal (tests[n].str, &endp, 1);	\
      if (d != tests[n].exp)						\
	{								\
	  char buf1[FSTRLENMAX], buf2[FSTRLENMAX];			\
	  FTOSTR (buf1, sizeof (buf1), "%g", d);			\
	  FTOSTR (buf2, sizeof (buf2), "%g", tests[n].exp);		\
	  printf ("strto" # FSUF "(\"%s\") returns %s and not %s\n",	\
		  tests[n].str, buf1, buf2);				\
	  result = 1;							\
	}								\
      else if (endp - tests[n].str != tests[n].nread)			\
	{								\
	  printf ("strto" # FSUF "(\"%s\") read %td bytes and not %td\n", \
		  tests[n].str, endp - tests[n].str, tests[n].nread);	\
	  result = 1;							\
	}								\
    }									\
									\
  if (result == 0)							\
    puts ("all OK");							\
									\
  return result ? EXIT_FAILURE : EXIT_SUCCESS;				\
}

GEN_TEST_STRTOD_FOREACH (TEST_STRTOD)

static int
do_test (void)
{
  return STRTOD_TEST_FOREACH (test_strto);
}

#include <support/test-driver.c>
