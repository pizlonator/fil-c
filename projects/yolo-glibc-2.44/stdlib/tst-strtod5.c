/* Tests of strtod in a locale using decimal comma.
   Copyright (C) 2007-2026 Free Software Foundation, Inc.
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

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "tst-strtod.h"

#define NBSP "\xc2\xa0"

#define TEST_STRTOD(FSUF, FTYPE, FTOSTR, LSUF, CSUF)			\
static const struct							\
{									\
  const char *in;							\
  FTYPE expected;							\
} tests_strto ## FSUF[] =						\
  {									\
    { "0", 0.0 ## LSUF },						\
    { "000", 0.0 ## LSUF },						\
    { "-0", -0.0 ## LSUF },						\
    { "-000", -0.0 ## LSUF },						\
    { "0,", 0.0 ## LSUF },						\
    { "-0,", -0.0 ## LSUF },						\
    { "0,0", 0.0 ## LSUF },						\
    { "-0,0", -0.0 ## LSUF },						\
    { "0e-10", 0.0 ## LSUF },						\
    { "-0e-10", -0.0 ## LSUF },						\
    { "0,e-10", 0.0 ## LSUF },						\
    { "-0,e-10", -0.0 ## LSUF },					\
    { "0,0e-10", 0.0 ## LSUF },						\
    { "-0,0e-10", -0.0 ## LSUF },					\
    { "0e-1000000", 0.0 ## LSUF },					\
    { "-0e-1000000", -0.0 ## LSUF },					\
    { "0,0e-1000000", 0.0 ## LSUF },					\
    { "-0,0e-1000000", -0.0 ## LSUF },					\
  };									\
									\
									\
static int								\
test_strto ## FSUF (void)						\
{									\
  int status = 0;							\
									\
  for (int i = 0;							\
       i < sizeof (tests_strto ## FSUF) / sizeof (tests_strto ## FSUF[0]); \
       ++i)								\
    {									\
      char *ep;								\
      FTYPE r = strto ## FSUF (tests_strto ## FSUF[i].in, &ep);		\
									\
      if (*ep != '\0')							\
	{								\
	  printf ("%d: got rest string \"%s\", expected \"\"\n", i, ep); \
	  status = 1;							\
	}								\
									\
      if (r != tests_strto ## FSUF[i].expected				\
	  || (copysign ## CSUF (10.0 ## LSUF, r)			\
	      != copysign ## CSUF (10.0 ## LSUF,			\
				   tests_strto ## FSUF[i].expected)))	\
	{								\
	  char buf1[FSTRLENMAX], buf2[FSTRLENMAX];			\
	  FTOSTR (buf1, sizeof (buf1), "%g", r);			\
	  FTOSTR (buf2, sizeof (buf2), "%g",				\
		  tests_strto ## FSUF[i].expected);			\
	  printf ("%d: got wrong results %s, expected %s\n",		\
		  i, buf1, buf2);					\
	  status = 1;							\
	}								\
    }									\
									\
  return status;							\
}

GEN_TEST_STRTOD_FOREACH (TEST_STRTOD)

static int
do_test (void)
{
  if (setlocale (LC_ALL, "cs_CZ.UTF-8") == NULL)
    {
      puts ("could not set locale");
      return 1;
    }

  return STRTOD_TEST_FOREACH (test_strto);
}

#include <support/test-driver.c>
