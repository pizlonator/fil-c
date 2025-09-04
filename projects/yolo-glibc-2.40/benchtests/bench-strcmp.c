/* Measure strcmp and wcscmp functions.
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
#ifdef WIDE
# define TEST_NAME "wcscmp"
#else
# define TEST_NAME "strcmp"
#endif
#include "bench-string.h"
#include "json-lib.h"

#ifdef WIDE
# define CHARBYTESLOG 2
# define MIDCHAR 0x7fffffff
# define LARGECHAR 0xfffffffe
#else
# define CHARBYTESLOG 0
# define MIDCHAR 0x7f
# define LARGECHAR 0xfe

int
generic_strcmp (const char *s1, const char *s2);

IMPL (generic_strcmp, 0)

#endif

typedef int (*proto_t) (const CHAR *, const CHAR *);

IMPL (STRCMP, 1)

static void
do_one_test (json_ctx_t *json_ctx, impl_t *impl,
	     const CHAR *s1, const CHAR *s2,
	     int exp_result)
{
  size_t i, iters = INNER_LOOP_ITERS8 / 2;
  timing_t start, stop, cur;

  TIMING_NOW (start);
  for (i = 0; i < iters; ++i)
    {
      CALL (impl, s1, s2);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_element_double (json_ctx, (double) cur / (double) iters);
}

static void
do_test (json_ctx_t *json_ctx, size_t align1, size_t align2, size_t len,
         int max_char, int exp_result, int at_end)
{
  size_t i;

  CHAR *s1, *s2;

  if (len == 0)
    return;

  align1 &= ~(CHARBYTES - 1);
  align2 &= ~(CHARBYTES - 1);

  align1 &= (getpagesize () - 1);
  if (align1 + (len + 1) * CHARBYTES >= page_size)
    return;

  align2 &= (getpagesize () - 1);
  if (align2 + (len + 1) * CHARBYTES >= page_size)
    return;

  /* Put them close to the end of page.  */
  if (at_end)
    {
      i = align1 + CHARBYTES * (len + 2);
      align1 = ((page_size - i) / 16 * 16) + align1;
      i = align2 + CHARBYTES * (len + 2);
      align2 = ((page_size - i) / 16 * 16) + align2;
    }

  s1 = (CHAR *)(buf1 + align1);
  s2 = (CHAR *)(buf2 + align2);

  for (i = 0; i < len; i++)
    s1[i] = s2[i] = 1 + (23 << ((CHARBYTES - 1) * 8)) * i % max_char;

  s1[len] = s2[len] = 0;
  s1[len + 1] = 23;
  s2[len + 1] = 24 + exp_result;
  s2[len - 1] -= exp_result;

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "length", (double)len);
  json_attr_uint (json_ctx, "align1", (double)align1);
  json_attr_uint (json_ctx, "align2", (double)align2);
  json_array_begin (json_ctx, "timings");

  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, s1, s2, exp_result);

  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

static void
do_one_test_page_boundary (json_ctx_t *json_ctx, CHAR *s1, CHAR *s2,
			   size_t align1, size_t align2, size_t len,
			   int exp_result)
{
  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "length", (double) len);
  json_attr_uint (json_ctx, "align1", (double) align1);
  json_attr_uint (json_ctx, "align2", (double) align2);
  json_array_begin (json_ctx, "timings");
  FOR_EACH_IMPL (impl, 0)
    do_one_test (json_ctx, impl, s1, s2, exp_result);
  json_array_end (json_ctx);
  json_element_object_end (json_ctx);
}

static void
do_test_page_boundary (json_ctx_t *json_ctx)
{
  /* To trigger bug 25933, we need a size that is equal to the vector
     length times 4. In the case of AVX2 for Intel, we need 32 * 4.  We
     make this test generic and run it for all architectures as additional
     boundary testing for such related algorithms.  */
  size_t size = 32 * 4;
  size_t len;
  CHAR *s1 = (CHAR *) (buf1 + (BUF1PAGES - 1) * page_size);
  CHAR *s2 = (CHAR *) (buf2 + (BUF1PAGES - 1) * page_size);
  int exp_result;

  memset (s1, 'a', page_size);
  memset (s2, 'a', page_size);

  s1[(page_size / CHARBYTES) - 1] = (CHAR) 0;
  s2[(page_size / CHARBYTES) - 1] = (CHAR) 0;

  /* Iterate over a size that is just below where we expect the bug to
     trigger up to the size we expect will trigger the bug e.g. [99-128].
     Likewise iterate the start of two strings between 30 and 31 bytes
     away from the boundary to simulate alignment changes.  */
  for (size_t s = 99; s <= size; s++)
    for (size_t s1a = 30; s1a < 32; s1a++)
      for (size_t s2a = 30; s2a < 32; s2a++)
	{
	  size_t align1 = (page_size / CHARBYTES - s) - s1a;
	  size_t align2 = (page_size / CHARBYTES - s) - s2a;
	  CHAR *s1p = s1 + align1;
	  CHAR *s2p = s2 + align2;
	  len = (page_size / CHARBYTES) - 1 - align1;
	  exp_result = STRCMP (s1p, s2p);
	  do_one_test_page_boundary (json_ctx, s1p, s2p, align1, align2,
				     len, exp_result);
	}
}

int
test_main (void)
{
  json_ctx_t json_ctx;
  size_t i, j, k;
  size_t pg_sz = getpagesize ();

  test_init ();

  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);
  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);

  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, TEST_NAME);
  json_attr_string (&json_ctx, "bench-variant", "default");

  json_array_begin (&json_ctx, "ifuncs");
  FOR_EACH_IMPL (impl, 0)
    json_element_string (&json_ctx, impl->name);
  json_array_end (&json_ctx);

  json_array_begin (&json_ctx, "results");
  for (k = 0; k < 2; ++k)
    {
      for (i = 1; i < 32; ++i)
        {
          do_test (&json_ctx, CHARBYTES * i, CHARBYTES * i, i, MIDCHAR, 0, k);
          do_test (&json_ctx, CHARBYTES * i, CHARBYTES * i, i, MIDCHAR, 1, k);
          do_test (&json_ctx, CHARBYTES * i, CHARBYTES * i, i, MIDCHAR, -1, k);
        }

      for (i = 1; i <= 8192;)
        {
          /* No page crosses.  */
          do_test (&json_ctx, 0, 0, i, MIDCHAR, 0, k);
          do_test (&json_ctx, i * CHARBYTES, 0, i, MIDCHAR, 0, k);
          do_test (&json_ctx, 0, i * CHARBYTES, i, MIDCHAR, 0, k);

          /* False page crosses.  */
          do_test (&json_ctx, pg_sz / 2, pg_sz / 2 - CHARBYTES, i, MIDCHAR, 0,
                   k);
          do_test (&json_ctx, pg_sz / 2 - CHARBYTES, pg_sz / 2, i, MIDCHAR, 0,
                   k);

          do_test (&json_ctx, pg_sz - (i * CHARBYTES), 0, i, MIDCHAR, 0, k);
          do_test (&json_ctx, 0, pg_sz - (i * CHARBYTES), i, MIDCHAR, 0, k);

          /* Real page cross.  */
          for (j = 16; j < 128; j += 16)
            {
              do_test (&json_ctx, pg_sz - j, 0, i, MIDCHAR, 0, k);
              do_test (&json_ctx, 0, pg_sz - j, i, MIDCHAR, 0, k);

              do_test (&json_ctx, pg_sz - j, pg_sz - j / 2, i, MIDCHAR, 0, k);
              do_test (&json_ctx, pg_sz - j / 2, pg_sz - j, i, MIDCHAR, 0, k);
            }

          if (i < 32)
            {
              ++i;
            }
          else if (i < 160)
            {
              i += 8;
            }
          else if (i < 512)
            {
              i += 32;
            }
          else
            {
              i *= 2;
            }
        }

      for (i = 1; i < 10 + CHARBYTESLOG; ++i)
        {
          do_test (&json_ctx, 0, 0, 2 << i, MIDCHAR, 0, k);
          do_test (&json_ctx, 0, 0, 2 << i, LARGECHAR, 0, k);
          do_test (&json_ctx, 0, 0, 2 << i, MIDCHAR, 1, k);
          do_test (&json_ctx, 0, 0, 2 << i, LARGECHAR, 1, k);
          do_test (&json_ctx, 0, 0, 2 << i, MIDCHAR, -1, k);
          do_test (&json_ctx, 0, 0, 2 << i, LARGECHAR, -1, k);
          do_test (&json_ctx, 0, CHARBYTES * i, 2 << i, MIDCHAR, 1, k);
          do_test (&json_ctx, CHARBYTES * i, CHARBYTES * (i + 1), 2 << i,
                   LARGECHAR, 1, k);
        }

      for (i = 1; i < 8; ++i)
        {
          do_test (&json_ctx, CHARBYTES * i, 2 * CHARBYTES * i, 8 << i,
                   MIDCHAR, 0, k);
          do_test (&json_ctx, 2 * CHARBYTES * i, CHARBYTES * i, 8 << i,
                   LARGECHAR, 0, k);
          do_test (&json_ctx, CHARBYTES * i, 2 * CHARBYTES * i, 8 << i,
                   MIDCHAR, 1, k);
          do_test (&json_ctx, 2 * CHARBYTES * i, CHARBYTES * i, 8 << i,
                   LARGECHAR, 1, k);
          do_test (&json_ctx, CHARBYTES * i, 2 * CHARBYTES * i, 8 << i,
                   MIDCHAR, -1, k);
          do_test (&json_ctx, 2 * CHARBYTES * i, CHARBYTES * i, 8 << i,
                   LARGECHAR, -1, k);
        }
    }
  do_test_page_boundary (&json_ctx);

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return ret;
}

#include <support/test-driver.c>

#ifndef WIDE
# undef STRCMP
# define STRCMP generic_strcmp
# include <string/strcmp.c>
#endif
