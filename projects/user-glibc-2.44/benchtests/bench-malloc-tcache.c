/* Benchmark tcache hotpath allocations.
   Copyright (C) 2013-2026 Free Software Foundation, Inc.
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

#ifndef TEST_FUNC
# define TEST_FUNC(size) malloc(size)
# define TEST_NAME "malloc"
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "bench-util.h"
#include "bench-util.c"
#include "bench-timing.h"
#include "json-lib.h"

/* Benchmark duration in seconds.  */
#define BENCHMARK_DURATION	3

static volatile bool timeout;
size_t alloc_size;

static void
alarm_handler (int signum)
{
  timeout = true;
}

struct bench_result {
  size_t iters;
  timing_t elapsed;
};

static __always_inline size_t
malloc_benchmark_loop (void **elems, size_t nr_items)
{
  size_t iters = nr_items;

  while (!timeout)
    {
      elems[iters % nr_items] = TEST_FUNC (alloc_size);
      iters++;
      free (elems[iters % nr_items]);
    }

  return iters - nr_items;
}

static void
do_benchmark (struct bench_result *res)
{
  timing_t start, stop;
  void *elems[10];
  memset (elems, 0, sizeof(elems));

  alarm (BENCHMARK_DURATION);
  /* Ramp up cpu before measuring.  */
  bench_start ();
  TIMING_NOW (start);
  res[0].iters = malloc_benchmark_loop (elems, 1);
  TIMING_NOW (stop);
  TIMING_DIFF (res[0].elapsed, start, stop);

  timeout = false;
  alarm (BENCHMARK_DURATION);
  bench_start ();
  TIMING_NOW (start);
  res[1].iters = malloc_benchmark_loop (elems, 4);
  TIMING_NOW (stop);
  TIMING_DIFF (res[1].elapsed, start, stop);
}

static void usage (const char *name)
{
  fprintf (stderr, "%s: <alloc_size>\n", name);
  exit (1);
}

int
main (int argc, char **argv)
{
  json_ctx_t json_ctx;
  double d_total_s, d_total_i;
  struct sigaction act;

  if (argc == 1)
    alloc_size = 1024;
  else if (argc == 2)
    {
      long ret;

      errno = 0;
      ret = strtol (argv[1], NULL, 10);

      if (errno || ret == 0)
	usage (argv[0]);

      alloc_size = ret;
    }
  else
    usage (argv[0]);

  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);

  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);

  json_attr_object_begin (&json_ctx, "functions");

  json_attr_object_begin (&json_ctx, TEST_NAME);

  memset (&act, 0, sizeof (act));
  act.sa_handler = &alarm_handler;

  sigaction (SIGALRM, &act, NULL);

  struct bench_result res[2];
  memset (res, 0, sizeof (struct bench_result) * 2);

  do_benchmark (res);

  d_total_s = res[0].elapsed;
  d_total_i = res[0].iters;

  json_attr_object_begin (&json_ctx, "simple");

  json_attr_double (&json_ctx, "alloc_size", alloc_size);
  json_attr_double (&json_ctx, "duration", d_total_s);
  json_attr_double (&json_ctx, "iterations", d_total_i);
  json_attr_double (&json_ctx, "time_per_iteration", d_total_s / d_total_i);

  json_attr_object_end (&json_ctx);

  d_total_s = res[1].elapsed;
  d_total_i = res[1].iters;

  json_attr_object_begin (&json_ctx, "optimized");

  json_attr_double (&json_ctx, "alloc_size", alloc_size);
  json_attr_double (&json_ctx, "duration", d_total_s);
  json_attr_double (&json_ctx, "iterations", d_total_i);
  json_attr_double (&json_ctx, "time_per_iteration", d_total_s / d_total_i);

  json_attr_object_end (&json_ctx);

  json_attr_object_end (&json_ctx);

  json_attr_object_end (&json_ctx);

  json_document_end (&json_ctx);

  return 0;
}
