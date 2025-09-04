/* Measure strncasecmp functions.
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

#include <ctype.h>
#define TEST_MAIN
#define TEST_NAME "strncasecmp"
#include "bench-string.h"
#include "json-lib.h"

typedef int (*proto_t) (const char *, const char *, size_t);

IMPL (strncasecmp, 1)

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, const char *s1,
             const char *s2, size_t n, int exp_result)
{
  size_t i, iters = INNER_LOOP_ITERS8;
  timing_t start, stop, cur;

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      CALL (impl, s1, s2, n);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double) cur / (double) iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t align1, size_t align2, size_t n,
         size_t len, int max_char, int exp_result)
{
  size_t i;
  char *s1, *s2;

  if (len == 0)
    return;

  align1 &= 7;
  if (align1 + len + 1 >= page_size)
    return;

  align2 &= 7;
  if (align2 + len + 1 >= page_size)
    return;

  s1 = (char *) (buf1 + align1);
  s2 = (char *) (buf2 + align2);

  for (i = 0; i < len; i++)
    {
      s1[i] = toupper (1 + 23 * i % max_char);
      s2[i] = tolower (s1[i]);
    }

  s1[len] = s2[len] = 0;
  s1[len + 1] = 23;
  s2[len + 1] = 24 + exp_result;
  if ((s2[len - 1] == 'z' && exp_result == -1)
      || (s2[len - 1] == 'a' && exp_result == 1))
    s1[len - 1] += exp_result;
  else
    s2[len - 1] -= exp_result;

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "length", len);
  json_attr_uint (json_ctx, "n", n);
  json_attr_uint (json_ctx, "align1", align1);
  json_attr_uint (json_ctx, "align2", align2);
  json_attr_uint (json_ctx, "max_char", max_char);
  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, s1, s2, n, exp_result);

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

int
test_main (void)
{
  json_ctx_t json_ctx;
  size_t i;

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

  for (i = 1; i < 16; ++i)
    {
      do_test (&json_ctx, i, i, i - 1, i, 127, 0);

      do_test (&json_ctx, i, i, i, i, 127, 0);
      do_test (&json_ctx, i, i, i, i, 127, 1);
      do_test (&json_ctx, i, i, i, i, 127, -1);

      do_test (&json_ctx, i, i, i + 1, i, 127, 0);
      do_test (&json_ctx, i, i, i + 1, i, 127, 1);
      do_test (&json_ctx, i, i, i + 1, i, 127, -1);
    }

  for (i = 1; i < 10; ++i)
    {
      do_test (&json_ctx, 0, 0, (2 << i) - 1, 2 << i, 127, 0);
      do_test (&json_ctx, 0, 0, 2 << i, 2 << i, 254, 0);
      do_test (&json_ctx, 0, 0, (2 << i) + 1, 2 << i, 127, 0);

      do_test (&json_ctx, 0, 0, (2 << i) + 1, 2 << i, 254, 0);

      do_test (&json_ctx, 0, 0, 2 << i, 2 << i, 127, 1);
      do_test (&json_ctx, 0, 0, (2 << i) + 10, 2 << i, 127, 1);

      do_test (&json_ctx, 0, 0, 2 << i, 2 << i, 254, 1);
      do_test (&json_ctx, 0, 0, (2 << i) + 10, 2 << i, 254, 1);

      do_test (&json_ctx, 0, 0, 2 << i, 2 << i, 127, -1);
      do_test (&json_ctx, 0, 0, (2 << i) + 10, 2 << i, 127, -1);

      do_test (&json_ctx, 0, 0, 2 << i, 2 << i, 254, -1);
      do_test (&json_ctx, 0, 0, (2 << i) + 10, 2 << i, 254, -1);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, i, 2 * i, (8 << i) - 1, 8 << i, 127, 0);
      do_test (&json_ctx, i, 2 * i, 8 << i, 8 << i, 127, 0);
      do_test (&json_ctx, i, 2 * i, (8 << i) + 100, 8 << i, 127, 0);

      do_test (&json_ctx, 2 * i, i, (8 << i) - 1, 8 << i, 254, 0);
      do_test (&json_ctx, 2 * i, i, 8 << i, 8 << i, 254, 0);
      do_test (&json_ctx, 2 * i, i, (8 << i) + 100, 8 << i, 254, 0);

      do_test (&json_ctx, i, 2 * i, 8 << i, 8 << i, 127, 1);
      do_test (&json_ctx, i, 2 * i, (8 << i) + 100, 8 << i, 127, 1);

      do_test (&json_ctx, 2 * i, i, 8 << i, 8 << i, 254, 1);
      do_test (&json_ctx, 2 * i, i, (8 << i) + 100, 8 << i, 254, 1);

      do_test (&json_ctx, i, 2 * i, 8 << i, 8 << i, 127, -1);
      do_test (&json_ctx, i, 2 * i, (8 << i) + 100, 8 << i, 127, -1);

      do_test (&json_ctx, 2 * i, i, 8 << i, 8 << i, 254, -1);
      do_test (&json_ctx, 2 * i, i, (8 << i) + 100, 8 << i, 254, -1);
    }

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>
