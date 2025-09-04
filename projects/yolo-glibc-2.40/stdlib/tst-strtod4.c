#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define NNBSP "\xe2\x80\xaf"

#define TEST_STRTOD(FSUF, FTYPE, FTOSTR, LSUF, CSUF)			\
static const struct							\
{									\
  const char *in;							\
  const char *out;							\
  FTYPE expected;							\
} tests_strto ## FSUF[] =						\
  {									\
    { "000"NNBSP"000"NNBSP"000", "", 0.0 ## LSUF },			\
    { "1"NNBSP"000"NNBSP"000,5x", "x", 1000000.5 ## LSUF },		\
    /* Bug 30964 */							\
    { "10"NNBSP NNBSP"200", NNBSP NNBSP"200", 10.0 ## LSUF }		\
  };									\
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
      FTYPE r = __strto ## FSUF ## _internal (tests_strto ## FSUF[i].in, \
					      &ep, 1);			\
									\
      if (strcmp (ep, tests_strto ## FSUF[i].out) != 0)			\
	{								\
	  printf ("%d: got rest string \"%s\", expected \"%s\"\n",	\
		  i, ep, tests_strto ## FSUF[i].out);			\
	  status = 1;							\
	}								\
									\
      if (r != tests_strto ## FSUF[i].expected)				\
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

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
