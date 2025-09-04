/* Measure strcat functions.
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
# define TEST_NAME "strcat"
#else
# define TEST_NAME "wcscat"
# define generic_strcat generic_wcscat
#endif /* WIDE */
#include "bench-string.h"

#define BIG_CHAR MAX_CHAR

#ifndef WIDE
# define sfmt "s"
# define SMALL_CHAR 127
#else
# define sfmt "ls"
# define SMALL_CHAR 1273
#endif /* WIDE */

#include "json-lib.h"

typedef CHAR *(*proto_t) (CHAR *, const CHAR *);

CHAR *
generic_strcat (CHAR *dst, const CHAR *src)
{
  STRCPY (dst + STRLEN (dst), src);
  return dst;
}

IMPL (STRCAT, 1)
IMPL (generic_strcat, 0)

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, CHAR *dst, const CHAR *src)
{
  size_t k = STRLEN (dst), i, iters = INNER_LOOP_ITERS_LARGE / CHARBYTES;
  timing_t start, stop, cur;

  if (CALL (impl, dst, src) != dst)
    {
      error (0, 0, "Wrong result in function %s %p %p", impl->name,
	     CALL (impl, dst, src), dst);
      ret = 1;
      return;
    }

  if (STRCMP (dst + k, src) != 0)
    {
      error (0, 0,
	     "Wrong result in function %s dst \"%" sfmt "\" src \"%" sfmt "\"",
	     impl->name, dst, src);
      ret = 1;
      return;
    }

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      dst[k] = '\0';
      CALL (impl, dst, src);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double) cur / (double) iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t align1, size_t align2, size_t len1,
	 size_t len2, int max_char)
{
  size_t i;
  CHAR *s1, *s2;

  align1 &= 7;
  if ((align1 + len1) * sizeof (CHAR) >= page_size)
    return;

  align2 &= 7;
  if ((align2 + len1 + len2) * sizeof (CHAR) >= page_size)
    return;

  s1 = (CHAR *) (buf1) + align1;
  s2 = (CHAR *) (buf2) + align2;

  for (i = 0; i < len1; ++i)
    s1[i] = 32 + 23 * i % (max_char - 32);
  s1[len1] = '\0';

  for (i = 0; i < len2; i++)
    s2[i] = 32 + 23 * i % (max_char - 32);

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "align1", align1);
  json_attr_uint (json_ctx, "align2", align2);
  json_attr_uint (json_ctx, "len1", len1);
  json_attr_uint (json_ctx, "len2", len2);
  json_attr_uint (json_ctx, "max_char", max_char);

  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    {
      s2[len2] = '\0';
      do_one_test (json_ctx, impl, s2, s1);
    }

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

int
test_main (void)
{
  json_ctx_t json_ctx;
  size_t i;

  test_init ();

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

  for (i = 0; i < 16; ++i)
    {
      do_test (&json_ctx, 0, 0, i, i, SMALL_CHAR);
      do_test (&json_ctx, 0, 0, i, i, BIG_CHAR);
      do_test (&json_ctx, 0, i, i, i, SMALL_CHAR);
      do_test (&json_ctx, i, 0, i, i, BIG_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, 0, 8 << i, 8 << i, SMALL_CHAR);
      do_test (&json_ctx, 8 - i, 2 * i, 8 << i, 8 << i, SMALL_CHAR);
      do_test (&json_ctx, 0, 0, 8 << i, 2 << i, SMALL_CHAR);
      do_test (&json_ctx, 8 - i, 2 * i, 8 << i, 2 << i, SMALL_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, i, 2 * i, 8 << i, 1, SMALL_CHAR);
      do_test (&json_ctx, 2 * i, i, 8 << i, 1, BIG_CHAR);
      do_test (&json_ctx, i, i, 8 << i, 10, SMALL_CHAR);
      do_test (&json_ctx, i, i, 8 << i, 10, BIG_CHAR);
    }

  for (i = 32; i < 256; i += 32)
    {
      do_test (&json_ctx, 1, 0, i, 31, SMALL_CHAR);
      do_test (&json_ctx, 0, i, i, 31, SMALL_CHAR);
      do_test (&json_ctx, 0, 0, i, 31, SMALL_CHAR);
      do_test (&json_ctx, i, i, i, 31, SMALL_CHAR);
    }

  for (; i < 512; i += 64)
    {
      do_test (&json_ctx, 1, 0, i, 31, SMALL_CHAR);
      do_test (&json_ctx, 0, i, i, 31, SMALL_CHAR);
      do_test (&json_ctx, 0, 0, i, 31, SMALL_CHAR);
      do_test (&json_ctx, i, i, i, 31, SMALL_CHAR);
    }

  for (; i < 1024; i += 128)
    {
      do_test (&json_ctx, 1, 0, i, 31, SMALL_CHAR);
      do_test (&json_ctx, 0, i, i, 31, SMALL_CHAR);
      do_test (&json_ctx, 0, 0, i, 31, SMALL_CHAR);
      do_test (&json_ctx, i, i, i, 31, SMALL_CHAR);
    }

  for (; i < 2048; i += 256)
    {
      do_test (&json_ctx, 1, 0, i, 31, SMALL_CHAR);
      do_test (&json_ctx, 0, i, i, 31, SMALL_CHAR);
      do_test (&json_ctx, 0, 0, i, 31, SMALL_CHAR);
      do_test (&json_ctx, i, i, i, 31, SMALL_CHAR);
    }

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>
