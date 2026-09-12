/* Measure bsearch functions.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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
#define TEST_NAME "bsearch"

#define ARRAY_SIZE 100000
#define LOOP_ITERS 100000000

/* Directly including <stdlib.h> leads to the use of an inline version
   of bsearch(), which may cause our test cases to be optimized away by
   the compiler due to predictability. To address this, we should
   include <bits/stdlib-bsearch.h> directly and replace __extern_inline
   with __attribute__((noinline)) to ensure the compiler does not
   inline the function. Additionally, we need to add some macros
   required for compilation. */
#include <stddef.h>
#define __extern_inline __attribute__((noinline))
#define __GNUC_PREREQ(x, y) 0
typedef int (*__compar_fn_t) (const void *, const void *);
#include <bits/stdlib-bsearch.h>
#undef __extern_inline
#undef __GNUC_PREREQ

#include "json-lib.h"
#include "bench-timing.h"

int arr[ARRAY_SIZE];

static int
comp_asc (const void *p1, const void *p2)
{
  int x1 = *(int *) p1;
  int x2 = *(int *) p2;

  if (x1 < x2)
    return -1;
  if (x1 > x2)
    return 1;
  return 0;
}

static int
comp_desc (const void *p1, const void *p2)
{
  int x1 = *(int *) p1;
  int x2 = *(int *) p2;

  if (x1 > x2)
    return -1;
  if (x1 < x2)
    return 1;
  return 0;
}

static void
do_bench (json_ctx_t *json_ctx, bool ascending, bool contained)
{
  size_t i, iters = LOOP_ITERS;
  timing_t start, stop, cur;
  int key;
  volatile __attribute__((__unused__)) void *res;

  for (i = 0; i < ARRAY_SIZE; ++i)
    {
      arr[i] = (ascending ? i : ARRAY_SIZE - 1 - i) << 1;
    }

  json_element_object_begin (json_ctx);
  json_attr_uint (json_ctx, "array-size", ARRAY_SIZE);
  json_attr_uint (json_ctx, "element-size", sizeof(arr[0]));
  json_attr_string (json_ctx, "key-pattern", ascending ? "ascending" : "descending");
  json_attr_string (json_ctx, "contained", contained ? "yes" : "no");
  json_attr_string (json_ctx, "simple", "yes");

  TIMING_NOW (start);

  for (i = 0; i < iters; ++i)
    {
      key = (i % ARRAY_SIZE << 1) + !contained;
      res = bsearch(&key, arr, ARRAY_SIZE, sizeof(arr[0]), ascending ? comp_asc : comp_desc);
    }

  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  json_attr_double (json_ctx, "timing", (double) cur / (double) iters);
  json_element_object_end (json_ctx);
}

int
do_test (void)
{
  json_ctx_t json_ctx;

  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);
  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);
  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, TEST_NAME);
  json_attr_string (&json_ctx, "bench-variant", "default");
  json_array_begin (&json_ctx, "results");

  for (int ascending = 0; ascending < 2; ++ascending)
    {
      for (int contained = 0; contained < 2; ++contained)
        {
          do_bench(&json_ctx, ascending, contained);
        }
    }

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return 0;
}

#include <support/test-driver.c>
