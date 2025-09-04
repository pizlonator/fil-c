/* Measure strncpy functions.
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

#define BIG_CHAR MAX_CHAR

#ifdef WIDE
# define SMALL_CHAR 1273
#else
# define SMALL_CHAR 127
#endif /* !WIDE */

#include "json-lib.h"

#ifndef STRNCPY_RESULT
# define STRNCPY_RESULT(dst, len, n) dst
# define TEST_MAIN
# ifndef WIDE
#  define TEST_NAME "strncpy"
# else
#  define TEST_NAME "wcsncpy"
#  define generic_strncpy generic_wcsncpy
# endif /* WIDE */
# include "bench-string.h"

CHAR *
generic_strncpy (CHAR *dst, const CHAR *src, size_t n)
{
  size_t nc = STRNLEN (src, n);
  if (nc != n)
    MEMSET (dst + nc, 0, n - nc);
  return MEMCPY (dst, src, nc);
}

IMPL (STRNCPY, 1)
IMPL (generic_strncpy, 0)

#endif /* !STRNCPY_RESULT */

typedef CHAR *(*proto_t) (CHAR *, const CHAR *, size_t);

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, CHAR *dst, const CHAR *src,
	     size_t len, size_t n)
{
  size_t i, iters = INNER_LOOP_ITERS_LARGE / CHARBYTES;
  timing_t start, stop, cur;

  if (CALL (impl, dst, src, n) != STRNCPY_RESULT (dst, len, n))
    {
      error (0, 0, "Wrong result in function %s %p %p", impl->name,
	     CALL (impl, dst, src, n), dst);
      ret = 1;
      return;
    }

  if (memcmp (dst, src, (len > n ? n : len) * sizeof (CHAR)) != 0)
    {
      error (0, 0, "Wrong result in function %s", impl->name);
      ret = 1;
      return;
    }

  if (n > len)
    {
      size_t i;

      for (i = len; i < n; ++i)
	if (dst[i] != '\0')
	  {
	    error (0, 0, "Wrong result in function %s", impl->name);
	    ret = 1;
	    return;
	  }
    }

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      CALL (impl, dst, src, n);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double) cur / (double) iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t align1, size_t align2, size_t len,
	 size_t n, int max_char)
{
  size_t i;
  CHAR *s1, *s2;

  /* For wcsncpy: align1 and align2 here mean alignment not in bytes,
     but in wchar_ts, in bytes it will equal to align * (sizeof (wchar_t)).  */
  align1 &= 7;
  if ((align1 + len) * sizeof (CHAR) >= page_size)
    return;

  align2 &= 7;
  if ((align2 + len) * sizeof (CHAR) >= page_size)
    return;

  s1 = (CHAR *) (buf1) + align1;
  s2 = (CHAR *) (buf2) + align2;

  for (i = 0; i < len; ++i)
    s1[i] = 32 + 23 * i % (max_char - 32);
  s1[len] = 0;
  for (i = len + 1; (i + align1) * sizeof (CHAR) < page_size && i < len + 64;
       ++i)
    s1[i] = 32 + 32 * i % (max_char - 32);

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "align1", align1);
  json_attr_uint (json_ctx, "align2", align2);
  json_attr_uint (json_ctx, "len", len);
  json_attr_uint (json_ctx, "n", n);
  json_attr_uint (json_ctx, "max_char", max_char);

  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, s2, s1, len, n);

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

static int
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
      do_test (&json_ctx, i, i, 16, 16, SMALL_CHAR);
      do_test (&json_ctx, i, i, 16, 16, BIG_CHAR);
      do_test (&json_ctx, i, 2 * i, 16, 16, SMALL_CHAR);
      do_test (&json_ctx, 2 * i, i, 16, 16, BIG_CHAR);
      do_test (&json_ctx, 8 - i, 2 * i, 1 << i, 2 << i, SMALL_CHAR);
      do_test (&json_ctx, 2 * i, 8 - i, 2 << i, 1 << i, SMALL_CHAR);
      do_test (&json_ctx, 8 - i, 2 * i, 1 << i, 2 << i, BIG_CHAR);
      do_test (&json_ctx, 2 * i, 8 - i, 2 << i, 1 << i, BIG_CHAR);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, 0, 4 << i, 8 << i, SMALL_CHAR);
      do_test (&json_ctx, 0, 0, 16 << i, 8 << i, SMALL_CHAR);
      do_test (&json_ctx, 8 - i, 2 * i, 4 << i, 8 << i, SMALL_CHAR);
      do_test (&json_ctx, 8 - i, 2 * i, 16 << i, 8 << i, SMALL_CHAR);
    }

  for (i = 128; i < 2048; i += i)
    {
      for (j = i - 64; j <= i + 64; j += 32)
	{
	  do_test (&json_ctx, 1, 0, i, j, SMALL_CHAR);
	  do_test (&json_ctx, 0, i, i, j, SMALL_CHAR);
	  do_test (&json_ctx, 0, 0, i, j, SMALL_CHAR);
	  do_test (&json_ctx, i, i, i, j, SMALL_CHAR);
	  do_test (&json_ctx, 1, 0, j, i, SMALL_CHAR);
	  do_test (&json_ctx, 0, i, j, i, SMALL_CHAR);
	  do_test (&json_ctx, 0, 0, j, i, SMALL_CHAR);
	  do_test (&json_ctx, i, i, j, i, SMALL_CHAR);
	}
    }

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>
