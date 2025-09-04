/* Measure STRCHR functions.
   Copyright (C) 2013-2024 Free Software Foundation, Inc.
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

#define TEST_MAIN
#ifndef WIDE
# ifdef USE_FOR_STRCHRNUL
#  define TEST_NAME "strchrnul"
# else
#  define TEST_NAME "strchr"
# endif /* !USE_FOR_STRCHRNUL */
#else
# ifdef USE_FOR_STRCHRNUL
#  define TEST_NAME "wcschrnul"
# else
#  define TEST_NAME "wcschr"
# endif /* !USE_FOR_STRCHRNUL */
#endif /* WIDE */
#include "bench-string.h"

#include "json-lib.h"
#define BIG_CHAR MAX_CHAR

#ifndef WIDE
# ifdef USE_FOR_STRCHRNUL
#  undef STRCHR
#  define STRCHR strchrnul
# endif /* !USE_FOR_STRCHRNUL */
# define MIDDLE_CHAR 127
# define SMALL_CHAR 23
#else
# ifdef USE_FOR_STRCHRNUL
#  undef STRCHR
#  define STRCHR wcschrnul
# endif /* !USE_FOR_STRCHRNUL */
# define MIDDLE_CHAR 1121
# define SMALL_CHAR 851
#endif /* WIDE */

#ifdef USE_FOR_STRCHRNUL
# define DO_RAND_TEST(...)
#else
# define DO_RAND_TEST(...) do_rand_test(__VA_ARGS__)
#endif
#ifdef USE_FOR_STRCHRNUL
# define NULLRET(endptr) endptr
#else
# define NULLRET(endptr) NULL
#endif /* !USE_FOR_STRCHRNUL */


typedef CHAR *(*proto_t) (const CHAR *, int);

IMPL (STRCHR, 1)

#ifndef WIDE
char *generic_strchr (const char *, int);
char *generic_strchrnul (const char *, int);

# ifndef USE_FOR_STRCHRNUL
IMPL (generic_strchr, 0)
# else
IMPL (generic_strchrnul, 0)
# endif
#endif

#ifndef USE_FOR_STRCHRNUL
/* Random benchmarks for strchr (if return is CHAR or NULL).  The
   rational for the benchmark is returning null/char can be done with
   predicate execution (i.e cmovcc on x86) or a branch. */


/* Large enough that full history can't be stored in BHT. */
#define NUM_SEARCH_CHARS 2048

/* Expectation is usecases of strchr check the return. Otherwise
   strchrnul would almost always be better. Since there is another
   branch coming we want to test the case where a potential branch in
   strchr can be used to skip a later mispredict because of the
   relationship between the two branches. */
static void __attribute__ ((noinline, noclone))
do_one_rand_plus_branch_test (json_ctx_t *json_ctx, impl_t *impl,
                              const CHAR *s, const CHAR *c)
{
  size_t i, iters = INNER_LOOP_ITERS8;
  int must_execute = 0;
  timing_t start, stop, cur;
  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      if (CALL (impl, s, c[i % NUM_SEARCH_CHARS]))
        {
          /* We just need something that will force compiler to emit
             a branch instead of conditional execution. */
          ++must_execute;
          asm volatile("" : : :);
        }
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double)cur / (double)iters);
}

static void __attribute__ ((noinline, noclone))
do_one_rand_test (json_ctx_t *json_ctx, impl_t *impl, const CHAR *s,
                  const CHAR *c)
{
  size_t i, iters = INNER_LOOP_ITERS8;
  timing_t start, stop, cur;
  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      CALL (impl, s, c[i % NUM_SEARCH_CHARS]);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double)cur / (double)iters);
}

static void
do_rand_test (json_ctx_t *json_ctx, size_t align, size_t pos, size_t len,
              float perc_zero)
{
  size_t i;
  int perc_zero_int;
  CHAR *buf = (CHAR *)buf1;
  CHAR *c = (CHAR *)buf2;
  align &= 127;
  if ((align + len) * sizeof (CHAR) >= page_size)
    return;

  /* Test is only interesting if we can hit both cases. */
  if (pos >= len)
    return;

  /* Segfault if we run the test. */
  if (NUM_SEARCH_CHARS * sizeof (CHAR) > page_size)
    return;

  for (i = 0; i < len; ++i)
    {
      buf[align + i] = 2;
    }
  buf[align + len] = 0;
  buf[align + pos] = 1;

  perc_zero_int = perc_zero * RAND_MAX;
  for (i = 0; i < NUM_SEARCH_CHARS; ++i)
    {
      if (rand () > perc_zero_int)
        c[i] = 0;
      else
        c[i] = 1;
    }
  {
    json_element_object_begin (json_ctx);
    json_attr_uint (json_ctx, "rand", 1);
    json_attr_uint (json_ctx, "branch", 1);
    json_attr_double (json_ctx, "perc-zero", perc_zero);
    json_attr_uint (json_ctx, "length", len);
    json_attr_uint (json_ctx, "pos", pos);
    json_attr_uint (json_ctx, "alignment", align);
    json_array_begin (json_ctx, "timings");

    FOR_EACH_IMPL (impl, 0)
      do_one_rand_plus_branch_test (json_ctx, impl, buf + align, c);

    json_array_end (json_ctx);
    json_element_object_end (json_ctx);
  }
  {
    json_element_object_begin (json_ctx);
    json_attr_uint (json_ctx, "rand", 1);
    json_attr_uint (json_ctx, "branch", 0);
    json_attr_double (json_ctx, "perc-zero", perc_zero);
    json_attr_uint (json_ctx, "length", len);
    json_attr_uint (json_ctx, "pos", pos);
    json_attr_uint (json_ctx, "alignment", align);
    json_array_begin (json_ctx, "timings");

    FOR_EACH_IMPL (impl, 0)
      do_one_rand_test (json_ctx, impl, buf + align, c);

    json_array_end (json_ctx);
    json_element_object_end (json_ctx);
  }
}
#endif

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, const CHAR *s, int c,
             const CHAR *exp_res)
{
  size_t i, iters = INNER_LOOP_ITERS8;
  timing_t start, stop, cur;
  const CHAR *res = CALL (impl, s, c);
  if (res != exp_res)
    {
      error (0, 0, "Wrong result in function %s %p != %p", impl->name, res,
             exp_res);
      ret = 1;
      return;
    }

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      CALL (impl, s, c);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double)cur / (double)iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t align, size_t pos, size_t len,
         int seek_char, int max_char)
/* For wcschr: align here means align not in bytes,
   but in wchar_ts, in bytes it will equal to align * (sizeof (wchar_t))
   len for wcschr here isn't in bytes but it's number of wchar_t symbols.  */
{
  size_t i;
  CHAR *result;
  CHAR *buf = (CHAR *) buf1;
  align &= 127;
  if ((align + len) * sizeof (CHAR) >= page_size)
    return;

  for (i = 0; i < len; ++i)
    {
      buf[align + i] = 32 + 23 * i % max_char;
      if (buf[align + i] == seek_char)
	buf[align + i] = seek_char + 1;
      else if (buf[align + i] == 0)
	buf[align + i] = 1;
    }
  buf[align + len] = 0;

  if (pos < len)
    {
      buf[align + pos] = seek_char;
      result = buf + align + pos;
    }
  else if (seek_char == 0)
    result = buf + align + len;
  else
    result = NULLRET (buf + align + len);

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "rand", 0);
  json_attr_uint (json_ctx, "length", len);
  json_attr_uint (json_ctx, "pos", pos);
  json_attr_uint (json_ctx, "seek_char", seek_char);
  json_attr_uint (json_ctx, "max_char", max_char);
  json_attr_uint (json_ctx, "alignment", align);
  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, buf + align, seek_char, result);

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

int
test_main (void)
{
  json_ctx_t json_ctx;

  size_t i, j;
  test_init ();

  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);
  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);

  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, TEST_NAME);
  json_attr_string (&json_ctx, "bench-variant", "");

  json_array_begin (&json_ctx, "ifuncs");
  FOR_EACH_IMPL (impl, 0)
    json_element_string (&json_ctx, impl->name);
  json_array_end (&json_ctx);

  json_array_begin (&json_ctx, "results");

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, 16 << i, 2048, SMALL_CHAR, MIDDLE_CHAR);
      do_test (&json_ctx, i, 16 << i, 2048, SMALL_CHAR, MIDDLE_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, 16 << i, 4096, SMALL_CHAR, MIDDLE_CHAR);
      do_test (&json_ctx, i, 16 << i, 4096, SMALL_CHAR, MIDDLE_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, i, 64, 256, SMALL_CHAR, MIDDLE_CHAR);
      do_test (&json_ctx, i, 64, 256, SMALL_CHAR, BIG_CHAR);
    }

  for (i = 0; i < 8; ++i)
    {
      do_test (&json_ctx, 16 * i, 256, 512, SMALL_CHAR, MIDDLE_CHAR);
      do_test (&json_ctx, 16 * i, 256, 512, SMALL_CHAR, BIG_CHAR);
    }

  for (i = 0; i < 32; ++i)
    {
      do_test (&json_ctx, 0, i, i + 1, SMALL_CHAR, MIDDLE_CHAR);
      do_test (&json_ctx, 0, i, i + 1, SMALL_CHAR, BIG_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, 16 << i, 2048, 0, MIDDLE_CHAR);
      do_test (&json_ctx, i, 16 << i, 2048, 0, MIDDLE_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, 16 << i, 4096, 0, MIDDLE_CHAR);
      do_test (&json_ctx, i, 16 << i, 4096, 0, MIDDLE_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, i, 64, 256, 0, MIDDLE_CHAR);
      do_test (&json_ctx, i, 64, 256, 0, BIG_CHAR);
    }

  for (i = 0; i < 8; ++i)
    {
      do_test (&json_ctx, 16 * i, 256, 512, 0, MIDDLE_CHAR);
      do_test (&json_ctx, 16 * i, 256, 512, 0, BIG_CHAR);
    }

  for (i = 0; i < 32; ++i)
    {
      do_test (&json_ctx, 0, i, i + 1, 0, MIDDLE_CHAR);
      do_test (&json_ctx, 0, i, i + 1, 0, BIG_CHAR);
    }

  for (i = 16 / sizeof (CHAR); i <= 8192 / sizeof (CHAR); i += i)
    {
      for (j = 32 / sizeof (CHAR); j <= 320 / sizeof (CHAR);
	   j += 32 / sizeof (CHAR))
	{
	  do_test (&json_ctx, 0, i, i + j, 0, MIDDLE_CHAR);
	  do_test (&json_ctx, 0, i + j, i, 0, MIDDLE_CHAR);
	  if (i > j)
	    {
	      do_test (&json_ctx, 0, i, i - j, 0, MIDDLE_CHAR);
	      do_test (&json_ctx, 0, i - j, i, 0, MIDDLE_CHAR);
	    }
	}
    }

  DO_RAND_TEST (&json_ctx, 0, 15, 16, 0.0);
  DO_RAND_TEST (&json_ctx, 0, 15, 16, 0.1);
  DO_RAND_TEST (&json_ctx, 0, 15, 16, 0.25);
  DO_RAND_TEST (&json_ctx, 0, 15, 16, 0.33);
  DO_RAND_TEST (&json_ctx, 0, 15, 16, 0.5);
  DO_RAND_TEST (&json_ctx, 0, 15, 16, 0.66);
  DO_RAND_TEST (&json_ctx, 0, 15, 16, 0.75);
  DO_RAND_TEST (&json_ctx, 0, 15, 16, 0.9);
  DO_RAND_TEST (&json_ctx, 0, 15, 16, 1.0);

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>

#ifndef WIDE
# undef STRCHRNUL
# define STRCHRNUL generic_strchrnul
# undef STRCHR
# define STRCHR generic_strchr
# include <string/strchrnul.c>
# include <string/strchr.c>
#endif
