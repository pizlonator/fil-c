#include <array_length.h>
#include <errno.h>
#include <stdio.h>
#include <support/check.h>
#include <sys/types.h>
#include <wchar.h>


static wchar_t buf[100];
static const struct
{
  size_t n;
  const char *str;
  ssize_t exp;
} tests[] =
  {
    { array_length (buf), "hello world", 11 },
    { 0, "hello world", -1 },
    { 1, "hello world", -1 },
    { 2, "hello world", -1 },
    { 11, "hello world", -1 },
    { 12, "hello world", 11 },
    { 0, "", -1 },
    { array_length (buf), "", 0 }
  };

static int
do_test (void)
{
  size_t n;

  TEST_COMPARE (swprintf (buf, array_length (buf), L"Hello %s", "world"), 11);
  TEST_COMPARE_STRING_WIDE (buf, L"Hello world");

  TEST_COMPARE (swprintf (buf, array_length (buf), L"Is this >%g< 3.1?", 3.1),
		18);
  TEST_COMPARE_STRING_WIDE (buf, L"Is this >3.1< 3.1?");

  for (n = 0; n < array_length (tests); ++n)
    {
      wmemset (buf, 0xabcd, array_length (buf));
      errno = 0;
      ssize_t res = swprintf (buf, tests[n].n, L"%s", tests[n].str);

      if (tests[n].exp < 0 && res >= 0)
	{
	  support_record_failure ();
	  printf ("swprintf (buf, %zu, L\"%%s\", \"%s\") expected to fail\n",
		  tests[n].n, tests[n].str);
	}
      else if (tests[n].exp >= 0 && tests[n].exp != res)
	{
	  support_record_failure ();
	  printf ("\
swprintf (buf, %zu, L\"%%s\", \"%s\") expected to return %zd, but got %zd\n",
		  tests[n].n, tests[n].str, tests[n].exp, res);
	}
      else if (res < 0
	       && tests[n].n > 0
	       && wcsnlen (buf, array_length (buf)) == array_length (buf))
	{
	  support_record_failure ();
	  printf ("\
error: swprintf (buf, %zu, L\"%%s\", \"%s\") missing null terminator\n",
		  tests[n].n, tests[n].str);
	}
      else if (res < 0
	       && tests[n].n > 0
	       && wcsnlen (buf, array_length (buf)) < array_length (buf)
	       && buf[wcsnlen (buf, array_length (buf)) + 1] != 0xabcd)
	{
	  support_record_failure ();
	  printf ("\
error: swprintf (buf, %zu, L\"%%s\", \"%s\") out of bounds write\n",
		  tests[n].n, tests[n].str);
	}

      if (res < 0 && tests[n].n < 0)
	TEST_COMPARE (errno, E2BIG);

      printf ("swprintf (buf, %zu, L\"%%s\", \"%s\") OK\n",
	      tests[n].n, tests[n].str);
    }

  TEST_COMPARE (swprintf (buf, array_length (buf), L"%.0s", "foo"), 0);
  TEST_COMPARE_STRING_WIDE (buf, L"");

  TEST_COMPARE (swprintf (buf, array_length (buf), L"%.0ls", L"foo"), 0);
  TEST_COMPARE_STRING_WIDE (buf, L"");

  return 0;
}

#include <support/test-driver.c>
