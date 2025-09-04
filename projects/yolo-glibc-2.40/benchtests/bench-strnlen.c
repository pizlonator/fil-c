/* Measure strlen functions.
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
# define TEST_NAME "strnlen"
#else
# define TEST_NAME "wcsnlen"
# define generic_strnlen generic_wcsnlen
#endif /* WIDE */
#include "bench-string.h"
#include "json-lib.h"

#define BIG_CHAR MAX_CHAR

#ifndef WIDE
# define MIDDLE_CHAR 127
#else
# define MIDDLE_CHAR 1121
#endif /* WIDE */

typedef size_t (*proto_t) (const CHAR *, size_t);
size_t generic_strnlen (const CHAR *, size_t);

IMPL (STRNLEN, 1)
IMPL (generic_strnlen, 0)

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, const CHAR *s, size_t maxlen,
	     size_t exp_len)
{
  size_t len = CALL (impl, s, maxlen), i, iters = INNER_LOOP_ITERS;
  timing_t start, stop, cur;

  if (len != exp_len)
    {
      error (0, 0, "Wrong result in function %s %zd %zd", impl->name, len,
	     exp_len);
      ret = 1;
      return;
    }
  /* Warmup.  */
  for (i = 0; i < iters / 16; ++i)
    {
      CALL (impl, s, maxlen);
    }

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      CALL (impl, s, maxlen);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double) cur / (double) iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t align, size_t len, size_t maxlen,
	 int max_char)
{
  size_t i;

  align &= getpagesize () - 1;
  if ((align + len) * sizeof (CHAR) >= page_size)
    return;

  CHAR *buf = (CHAR *) (buf1);

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "len", len);
  json_attr_uint (json_ctx, "maxlen", maxlen);
  json_attr_uint (json_ctx, "max_char", max_char);
  json_attr_uint (json_ctx, "align", align);
  json_array_begin (json_ctx, "timings");

  for (i = 0; i < len; ++i)
    buf[align + i] = 1 + 7 * i % max_char;
  buf[align + len] = 0;

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, (CHAR *) (buf + align), maxlen,
		 MIN (len, maxlen));

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

int
test_main (void)
{
  size_t i, j;
  json_ctx_t json_ctx;

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

  for (i = 0; i <= 1; ++i)
    {
      do_test (&json_ctx, i, 1, 128, MIDDLE_CHAR);
      do_test (&json_ctx, i, 128, 1, MIDDLE_CHAR);
      do_test (&json_ctx, i, 1, 2, MIDDLE_CHAR);
      do_test (&json_ctx, i, 2, 1, MIDDLE_CHAR);

      do_test (&json_ctx, 32 + i, 1, 128, MIDDLE_CHAR);
      do_test (&json_ctx, 32 + i, 128, 1, MIDDLE_CHAR);
      do_test (&json_ctx, 32 + i, 1, 2, MIDDLE_CHAR);
      do_test (&json_ctx, 32 + i, 2, 1, MIDDLE_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, i, i - 1, MIDDLE_CHAR);
      do_test (&json_ctx, 0, i, i, MIDDLE_CHAR);
      do_test (&json_ctx, 0, i, i + 1, MIDDLE_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, i, i, i - 1, MIDDLE_CHAR);
      do_test (&json_ctx, i, i, i, MIDDLE_CHAR);
      do_test (&json_ctx, i, i, i + 1, MIDDLE_CHAR);
    }

  for (i = 2; i <= 10; ++i)
    {
      do_test (&json_ctx, 0, 1 << i, 5000, MIDDLE_CHAR);
      do_test (&json_ctx, 1, 1 << i, 5000, MIDDLE_CHAR);
      do_test (&json_ctx, 0, 5000, 1 << i, MIDDLE_CHAR);
      do_test (&json_ctx, 1, 5000, 1 << i, MIDDLE_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, i, 5000, BIG_CHAR);
      do_test (&json_ctx, 0, 5000, i, BIG_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, i, i, 5000, BIG_CHAR);
      do_test (&json_ctx, i, 5000, i, BIG_CHAR);
    }

  for (i = 2; i <= 10; ++i)
    {
      do_test (&json_ctx, 0, 1 << i, 5000, BIG_CHAR);
      do_test (&json_ctx, 1, 1 << i, 5000, BIG_CHAR);
      do_test (&json_ctx, 0, 5000, 1 << i, BIG_CHAR);
      do_test (&json_ctx, 1, 5000, 1 << i, BIG_CHAR);
    }

  for (i = (16 / sizeof (CHAR)); i <= (8192 / sizeof (CHAR)); i += i)
    {
      for (j = 0; j <= (704 / sizeof (CHAR)); j += (32 / sizeof (CHAR)))
	{
	  do_test (&json_ctx, 0, i + j, i, BIG_CHAR);
	  do_test (&json_ctx, 64, i + j, i, BIG_CHAR);

	  do_test (&json_ctx, 0, i, i + j, BIG_CHAR);
	  do_test (&json_ctx, 64, i, i + j, BIG_CHAR);

	  if (j < i)
	    {
	      do_test (&json_ctx, 0, i - j, i, BIG_CHAR);
	      do_test (&json_ctx, 64, i - j, i, BIG_CHAR);

	      do_test (&json_ctx, 0, i, i - j, BIG_CHAR);
	      do_test (&json_ctx, 64, i, i - j, BIG_CHAR);
	    }
	}
    }

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>

#define libc_hidden_def(X)
#ifndef WIDE
# undef STRNLEN
# define STRNLEN generic_strnlen
# include <string/strnlen.c>
#else
# define WCSNLEN generic_strnlen
# include <wcsmbs/wcsnlen.c>
#endif
