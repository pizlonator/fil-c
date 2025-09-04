/* Measure strspn functions.
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
# define TEST_NAME "strspn"
#else
# define TEST_NAME "wcsspn"
#endif /* WIDE */
#include "bench-string.h"
#include "json-lib.h"

#define BIG_CHAR MAX_CHAR

#ifndef WIDE
# define SMALL_CHAR 127
#else
# define SMALL_CHAR 1273
#endif /* WIDE */

typedef size_t (*proto_t) (const CHAR *, const CHAR *);

IMPL (STRSPN, 1)

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl, const CHAR *s,
             const CHAR *acc, size_t exp_res)
{
  size_t res = CALL (impl, s, acc), i, iters = INNER_LOOP_ITERS8 / CHARBYTES;
  timing_t start, stop, cur;

  if (res != exp_res)
    {
      error (0, 0, "Wrong result in function %s %p %p", impl->name,
	     (void *) res, (void *) exp_res);
      ret = 1;
      return;
    }

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      CALL (impl, s, acc);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double)cur / (double)iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t align1, size_t align2, size_t pos,
         size_t len)
{
  size_t i;
  CHAR *acc, *s;

  align1 &= 7;
  if ((align1 + pos + 10) * sizeof (CHAR) >= page_size || len > 240 || !len)
    return;
  if ((align2 + len) * sizeof (CHAR) >= page_size)
    return;

  acc = (CHAR *) (buf2) + align2;
  s = (CHAR *) (buf1) + align1;

  for (i = 0; i < len; ++i)
    {
      acc[i] = random () & BIG_CHAR;
      if (!acc[i])
	acc[i] = random () & BIG_CHAR;
      if (!acc[i])
	acc[i] = 1 + (random () & SMALL_CHAR);
    }
  acc[len] = '\0';

  for (i = 0; i < pos; ++i)
    s[i] = acc[random () % len];
  s[pos] = random () & BIG_CHAR;
  if (STRCHR (acc, s[pos]))
    s[pos] = '\0';
  else
    {
      for (i = pos + 1; i < pos + 10; ++i)
	s[i] = random () & BIG_CHAR;
      s[i] = '\0';
    }

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "len", len);
  json_attr_uint (json_ctx, "pos", pos);
  json_attr_uint (json_ctx, "align1", align1);
  json_attr_uint (json_ctx, "align2", align2);
  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, s, acc, pos);

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

  for (i = 0; i < 32; ++i)
    {
      do_test (&json_ctx, 0, 0, 512, i);
      do_test (&json_ctx, i, 0, 512, i);
      do_test (&json_ctx, 0, i, 512, i);
      do_test (&json_ctx, i, i, 512, i);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, 0, 0, 16 << i, 4);
      do_test (&json_ctx, i, 0, 16 << i, 4);
      do_test (&json_ctx, 0, i, 16 << i, 4);
      do_test (&json_ctx, i, i, 16 << i, 4);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (&json_ctx, i, 0, 64, 10);
      do_test (&json_ctx, i, i, 64, 10);
    }

  for (i = 0; i < 64; ++i)
    {
      do_test (&json_ctx, 0, 0, i, 6);
      do_test (&json_ctx, 0, i, i, 6);
    }

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>
