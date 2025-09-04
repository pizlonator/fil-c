/* arc4random benchmarks.
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

#include "bench-timing.h"
#include "bench-util.h"
#include "json-lib.h"
#include <array_length.h>
#include <intprops.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/support.h>
#include <support/timespec.h>
#include <support/xthread.h>

static volatile sig_atomic_t timer_finished;

static void timer_callback (int unused)
{
  timer_finished = 1;
}

static timer_t timer;

/* Run for approximately DURATION seconds, and it does not matter who
   receive the signal (so not need to mask it on main thread).  */
static void
timer_start (void)
{
  timer_finished = 0;
  timer = support_create_timer (DURATION, 0, false, timer_callback);
}
static void
timer_stop (void)
{
  support_delete_timer (timer);
}

static const uint32_t sizes[] = { 0, 16, 32, 48, 64, 80, 96, 112, 128 };

static double
bench_throughput (void)
{
  uint64_t n = 0;

  struct timespec start, end;
  clock_gettime (CLOCK_MONOTONIC, &start);
  while (1)
    {
      DO_NOT_OPTIMIZE_OUT (arc4random ());
      n++;

      if (timer_finished == 1)
	break;
    }
  clock_gettime (CLOCK_MONOTONIC, &end);
  struct timespec diff = timespec_sub (end, start);

  double total = (double) n * sizeof (uint32_t);
  double duration = (double) diff.tv_sec
    + (double) diff.tv_nsec / TIMESPEC_HZ;

  return total / duration;
}

static double
bench_latency (void)
{
  timing_t start, stop, cur;
  const size_t iters = 1024;

  TIMING_NOW (start);
  for (size_t i = 0; i < iters; i++)
    DO_NOT_OPTIMIZE_OUT (arc4random ());
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  return (double) (cur) / (double) iters;
}

static double
bench_buf_throughput (size_t len)
{
  uint8_t buf[len];
  uint64_t n = 0;

  struct timespec start, end;
  clock_gettime (CLOCK_MONOTONIC, &start);
  while (1)
    {
      arc4random_buf (buf, len);
      n++;

      if (timer_finished == 1)
	break;
    }
  clock_gettime (CLOCK_MONOTONIC, &end);
  struct timespec diff = timespec_sub (end, start);

  double total = (double) n * len;
  double duration = (double) diff.tv_sec
    + (double) diff.tv_nsec / TIMESPEC_HZ;

  return total / duration;
}

static double
bench_buf_latency (size_t len)
{
  timing_t start, stop, cur;
  const size_t iters = 1024;

  uint8_t buf[len];

  TIMING_NOW (start);
  for (size_t i = 0; i < iters; i++)
    arc4random_buf (buf, len);
  TIMING_NOW (stop);

  TIMING_DIFF (cur, start, stop);

  return (double) (cur) / (double) iters;
}

static void
bench_singlethread (json_ctx_t *json_ctx)
{
  json_element_object_begin (json_ctx);

  json_array_begin (json_ctx, "throughput");
  for (int i = 0; i < array_length (sizes); i++)
    {
      timer_start ();
      double r = sizes[i] == 0
	? bench_throughput () : bench_buf_throughput (sizes[i]);
      timer_stop ();

      json_element_double (json_ctx, r);
    }
  json_array_end (json_ctx);

  json_array_begin (json_ctx, "latency");
  for (int i = 0; i < array_length (sizes); i++)
    {
      timer_start ();
      double r = sizes[i] == 0
	? bench_latency () : bench_buf_latency (sizes[i]);
      timer_stop ();

      json_element_double (json_ctx, r);
    }
  json_array_end (json_ctx);

  json_element_object_end (json_ctx);
}

static void
run_bench (json_ctx_t *json_ctx, const char *name,
	   char *const*fnames, size_t fnameslen,
	   void (*bench) (json_ctx_t *ctx))
{
  json_attr_object_begin (json_ctx, name);
  json_array_begin (json_ctx, "functions");
  for (int i = 0; i < fnameslen; i++)
    json_element_string (json_ctx, fnames[i]);
  json_array_end (json_ctx);

  json_array_begin (json_ctx, "results");
  bench (json_ctx);
  json_array_end (json_ctx);
  json_attr_object_end (json_ctx);
}

static int
do_test (void)
{
  char *fnames[array_length (sizes)];
  for (int i = 0; i < array_length (sizes); i++)
    if (sizes[i] == 0)
      fnames[i] = xasprintf ("arc4random");
    else
      fnames[i] = xasprintf ("arc4random_buf(%u)", sizes[i]);

  json_ctx_t json_ctx;
  json_init (&json_ctx, 0, stdout);

  json_document_begin (&json_ctx);
  json_attr_string (&json_ctx, "timing_type", TIMING_TYPE);

  run_bench (&json_ctx, "single-thread", fnames, array_length (fnames),
	     bench_singlethread);

  json_document_end (&json_ctx);

  for (int i = 0; i < array_length (sizes); i++)
    free (fnames[i]);

  return 0;
}

#include <support/test-driver.c>
