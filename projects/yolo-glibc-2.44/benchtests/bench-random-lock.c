/* Benchmark internal libc locking functions used in random.
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
#define TEST_NAME "random-lock"
#define TEST_FUNCTION test_main
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "bench-timing.h"
#include "json-lib.h"

/* Modern cores run 20M iterations in about 1 second.  */
#define NUM_ITERS 50000000


/* Measure the overhead of __libc_lock_lock and __libc_lock_unlock by
   calling random ().  */
static void
bench_random_lock (json_ctx_t *json_ctx, size_t iters)
{
  timing_t start, stop, total;

  srandom (0);

  /* Warmup to reduce variations due to frequency scaling.  */
  for (int i = 0; i < iters / 4; i++)
    (void) random ();

  TIMING_NOW (start);

  for (int i = 0; i < iters; i++)
    (void) random ();

  TIMING_NOW (stop);

  TIMING_DIFF (total, start, stop);

  json_element_double (json_ctx, (double) total / (double) iters);
}

static void *
thread_start (void *p)
{
  return p;
}

int
test_main (void)
{
  json_ctx_t json_ctx;

  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);

  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);
  json_attr_object_begin (&json_ctx, "functions");
  json_attr_object_begin (&json_ctx, "random");
  json_attr_string (&json_ctx, "bench-variant", "single-threaded");
  json_array_begin (&json_ctx, "results");

  /* Run benchmark single threaded.  */
  bench_random_lock (&json_ctx, NUM_ITERS);

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);

  json_attr_object_begin (&json_ctx, "random");
  json_attr_string (&json_ctx, "bench-variant", "multi-threaded");
  json_array_begin (&json_ctx, "results");

  /* Start a short thread to force SINGLE_THREAD_P == false.  This relies on
     the runtime disabling single-threaded optimizations when multiple
     threads are used, even after they finish.  */

  pthread_t t;
  pthread_create (&t, NULL, thread_start, NULL);
  pthread_join (t, NULL);

  /* Repeat benchmark with single-threaded optimizations disabled.  */
  bench_random_lock (&json_ctx, NUM_ITERS);

  json_array_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_attr_object_end (&json_ctx);
  json_document_end (&json_ctx);
  return 0;
}

#include "support/test-driver.c"
