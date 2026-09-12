#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "tst-strtod.h"

#define TEST_STRTOD(FSUF, FTYPE, FTOSTR, LSUF, CSUF)			\
struct test_strto ## FSUF						\
{									\
  const char *str;							\
  FTYPE result;								\
  size_t offset;							\
} tests_strto ## FSUF[] =						\
{									\
  { "0xy", 0.0 ## LSUF, 1 },						\
  { "0x.y", 0.0 ## LSUF, 1 },						\
  { "0x0.y", 0.0 ## LSUF, 4 },						\
  { "0x.0y", 0.0 ## LSUF, 4 },						\
  { ".y", 0.0 ## LSUF, 0 },						\
  { "0.y", 0.0 ## LSUF, 2 },						\
  { ".0y", 0.0 ## LSUF, 2 },						\
  { "1.0e", 1.0 ## LSUF, 3 },						\
  { "1.0e+", 1.0 ## LSUF, 3 },						\
  { "1.0e-", 1.0 ## LSUF, 3 },						\
  { "1.0ex", 1.0 ## LSUF, 3 },						\
  { "1.0e+x", 1.0 ## LSUF, 3 },						\
  { "1.0e-x", 1.0 ## LSUF, 3 },						\
  { "0x1p", 1.0 ## LSUF, 3 },						\
  { "0x1p+", 1.0 ## LSUF, 3 },						\
  { "0x1p-", 1.0 ## LSUF, 3 },						\
  { "0x1px", 1.0 ## LSUF, 3 },						\
  { "0x1p+x", 1.0 ## LSUF, 3 },						\
  { "0x1p-x", 1.0 ## LSUF, 3 },						\
  { "", 0.0 ## LSUF, 0 },						\
  { ".", 0.0 ## LSUF, 0 },						\
  { "-", 0.0 ## LSUF, 0 },						\
  { "-.", 0.0 ## LSUF, 0 },						\
  { ".e", 0.0 ## LSUF, 0 },						\
  { "-.e", 0.0 ## LSUF, 0 },						\
  { " \t", 0.0 ## LSUF, 0 },						\
  { " \t.", 0.0 ## LSUF, 0 },						\
  { " \t-", 0.0 ## LSUF, 0 },						\
  { " \t-.", 0.0 ## LSUF, 0 },						\
  { " \t.e", 0.0 ## LSUF, 0 },						\
  { " \t-.e", 0.0 ## LSUF, 0 },						\
  { " \t\f\r\n\v1", 1.0 ## LSUF, 7 },					\
  { " \t\f\r\n\v-1.5e2", -150.0 ## LSUF, 12 },				\
  { "INFx", INFINITY, 3 },						\
  { "infx", INFINITY, 3 },						\
  { "INFINITx", INFINITY, 3 },						\
  { "infinitx", INFINITY, 3 },						\
  { "INFINITYY", INFINITY, 8 },						\
  { "infinityy", INFINITY, 8 },						\
  { "NANx", NAN, 3 },							\
  { "nanx", NAN, 3 },							\
  { "NAN(", NAN, 3 },							\
  { "nan(", NAN, 3 },							\
  { "NAN(x", NAN, 3 },							\
  { "nan(x", NAN, 3 },							\
  { "NAN(x)y", NAN, 6 },						\
  { "nan(x)y", NAN, 6 },						\
  { "NAN(*)y", NAN, 3 },						\
  { "nan(*)y", NAN, 3 }							\
};									\
									\
static int								\
compare_strto ## FSUF (FTYPE x, FTYPE y)				\
{									\
  if (isnan (x) && isnan (y))						\
    return 1;								\
  return x == y;							\
}									\
									\
static int								\
test_strto ## FSUF (void)						\
{									\
  int status = 0;							\
  for (size_t i = 0;							\
       i < sizeof (tests_strto ## FSUF) / sizeof (tests_strto ## FSUF[0]); \
       ++i)								\
    {									\
      char *ep;								\
      FTYPE r = strto ## FSUF (tests_strto ## FSUF[i].str, &ep);	\
      if (!compare_strto ## FSUF (r, tests_strto ## FSUF[i].result))	\
	{								\
	  char buf1[FSTRLENMAX], buf2[FSTRLENMAX];			\
	  FTOSTR (buf1, sizeof (buf1), "%g", r);			\
	  FTOSTR (buf2, sizeof (buf2), "%g", tests_strto ## FSUF[i].result); \
	  printf ("test %zu r = %s, expect %s\n", i, buf1, buf2);	\
	  status = 1;							\
	}								\
      if (ep != tests_strto ## FSUF[i].str + tests_strto ## FSUF[i].offset) \
	{								\
	  printf ("test %zu strto" #FSUF				\
		  " parsed %tu characters, expected %zu\n",		\
		  i, ep - tests_strto ## FSUF[i].str,			\
		  tests_strto ## FSUF[i].offset);			\
	  status = 1;							\
	}								\
    }									\
  return status;							\
}

GEN_TEST_STRTOD_FOREACH (TEST_STRTOD)

static int
do_test (void)
{
  return STRTOD_TEST_FOREACH (test_strto);
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
