/* Measure hash functions runtime.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#ifndef TEST_FUNC
# error "No TEST_FUNC provided!"
#endif
#ifndef SIMPLE_TEST_FUNC
# error "No SIMPLE_TEST_FUNC provided!"
#endif

#ifndef TEST_NAME
# define STRINGIFY_PRIMITIVE(x) #  x
# define STRINGIFY(x) STRINGIFY_PRIMITIVE (x)

# define TEST_NAME STRINGIFY (TEST_FUNC)
#endif

#include "json-lib.h"
#include "bench-timing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
  NFIXED_ITERS = 1048576,
  NRAND_BUFS = 16384,
  NRAND_ITERS = 256,
  RAND_BENCH_MAX_LEN = 128
};

#include "bench-hash-funcs-kernel.h"
#define SIMPLE
#include "bench-hash-funcs-kernel.h"

static void
do_one_test (json_ctx_t *json_ctx, size_t len)
{
  char buf[len + 1];
  memset (buf, -1, len);
  buf[len] = '\0';

  json_element_object_begin (json_ctx);

  json_attr_string (json_ctx, "type", "fixed");
  json_attr_uint (json_ctx, "length", len);
  json_attr_double (json_ctx, "time_simple", do_one_test_kernel_simple (buf, len));
  json_attr_double (json_ctx, "time_optimized", do_one_test_kernel_optimized (buf, len));

  json_element_object_end (json_ctx);
}

static void __attribute__ ((noinline, noclone))
do_rand_test (json_ctx_t *json_ctx)
{
  size_t i, sz, offset;
  char *bufs;
  unsigned int *sizes;

  bufs = (char *) calloc (NRAND_BUFS, RAND_BENCH_MAX_LEN);
  sizes = (unsigned int *) calloc (NRAND_BUFS, sizeof (unsigned int));
  if (bufs == NULL || sizes == NULL)
    {
      fprintf (stderr, "Failed to allocate bufs for random test\n");
      goto done;
    }

  for (sz = 2; sz <= RAND_BENCH_MAX_LEN; sz += sz)
    {
      json_element_object_begin (json_ctx);
      json_attr_string (json_ctx, "type", "random");
      json_attr_uint (json_ctx, "length", sz);

      for (i = 0, offset = 0; i < NRAND_BUFS;
	   ++i, offset += RAND_BENCH_MAX_LEN)
	{
	  sizes[i] = random () % sz;
	  memset (bufs + offset, -1, sizes[i]);
	  bufs[offset + sizes[i]] = '\0';
	}

      json_attr_double (json_ctx, "time_simple",
			do_rand_test_kernel_simple (bufs, sizes));
      json_attr_double (json_ctx, "time_optimized",
			do_rand_test_kernel_optimized (bufs, sizes));
      json_element_object_end (json_ctx);
    }

done:
  if (bufs)
    free (bufs);

  if (sizes)
    free (sizes);
}

static int
do_test (void)
{
  int i;
  json_ctx_t json_ctx;

  json_init (&json_ctx, 0, stdout);
  json_document_begin (&json_ctx);
  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);
  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, TEST_NAME);
  json_array_begin (&json_ctx, "results");

  for (i = 0; i < 16; ++i)
    do_one_test (&json_ctx, i);

  for (i = 16; i <= 256; i += i)
    do_one_test (&json_ctx, i);

  do_rand_test (&json_ctx);

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);

  return 0;
}

#include <support/test-driver.c>
