/* Measure lock functions for different threads and critical sections.
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
#define TIMEOUT (20 * 60)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include "bench-timing.h"
#include "json-lib.h"

static bench_lock_t lock;
static bench_lock_attr_t attr;
static pthread_barrier_t barrier;

#define START_ITERS 1000

#pragma GCC push_options
#pragma GCC optimize(1)

static int __attribute__ ((noinline)) fibonacci (int i)
{
  asm("");
  if (i > 2)
    return fibonacci (i - 1) + fibonacci (i - 2);
  return 10 + i;
}

static void
do_filler (void)
{
  char buf1[512], buf2[512];
  int f = fibonacci (4);
  memcpy (buf1, buf2, f);
}

static void
do_filler_shared (void)
{
  static char buf1[512], buf2[512];
  int f = fibonacci (4);
  memcpy (buf1, buf2, f);
}

#pragma GCC pop_options

#define UNIT_WORK_CRT do_filler_shared ()
#define UNIT_WORK_NON_CRT do_filler ()

static inline void
critical_section (int length)
{
  for (int i = length; i >= 0; i--)
    UNIT_WORK_CRT;
}

static inline void
non_critical_section (int length)
{
  for (int i = length; i >= 0; i--)
    UNIT_WORK_NON_CRT;
}

typedef struct Worker_Params
{
  long iters;
  int crt_len;
  int non_crt_len;
  timing_t duration;
} Worker_Params;

static void *
worker (void *v)
{
  timing_t start, stop;
  Worker_Params *p = (Worker_Params *) v;
  long iters = p->iters;
  int crt_len = p->crt_len;
  int non_crt_len = p->non_crt_len;

  pthread_barrier_wait (&barrier);
  TIMING_NOW (start);
  while (iters--)
    {
      LOCK (&lock);
      critical_section (crt_len);
      UNLOCK (&lock);
      non_critical_section (non_crt_len);
    }
  TIMING_NOW (stop);

  TIMING_DIFF (p->duration, start, stop);
  return NULL;
}

static double
do_one_test (int num_threads, int crt_len, int non_crt_len, long iters)
{
  int i;
  timing_t mean;
  Worker_Params *p, params[num_threads];
  pthread_t threads[num_threads];

  LOCK_INIT (&lock, &attr);
  pthread_barrier_init (&barrier, NULL, num_threads);

  for (i = 0; i < num_threads; i++)
    {
      p = &params[i];
      p->iters = iters;
      p->crt_len = crt_len;
      p->non_crt_len = non_crt_len;
      pthread_create (&threads[i], NULL, worker, (void *) p);
    }
  for (i = 0; i < num_threads; i++)
    pthread_join (threads[i], NULL);

  LOCK_DESTROY (&lock);
  pthread_barrier_destroy (&barrier);

  mean = 0;
  for (i = 0; i < num_threads; i++)
    mean += params[i].duration;
  mean /= num_threads;
  return mean;
}

#define RUN_COUNT 10
#define MIN_TEST_SEC 0.01

static void
do_bench_one (const char *name, int num_threads, int crt_len, int non_crt_len,
	      json_ctx_t *js)
{
  timing_t cur;
  struct timeval ts, te;
  double tsd, ted, td;
  long iters, iters_limit, total_iters;
  timing_t curs[RUN_COUNT + 2];
  int i, j;
  double mean, stdev;

  iters = START_ITERS;
  iters_limit = LONG_MAX / 100;

  while (1)
    {
      gettimeofday (&ts, NULL);
      cur = do_one_test (num_threads, crt_len, non_crt_len, iters);
      gettimeofday (&te, NULL);
      /* Make sure the test to run at least MIN_TEST_SEC.  */
      tsd = ts.tv_sec + ts.tv_usec / 1000000.0;
      ted = te.tv_sec + te.tv_usec / 1000000.0;
      td = ted - tsd;
      if (td >= MIN_TEST_SEC || iters >= iters_limit)
	break;

      iters *= 10;
    }

  curs[0] = cur;
  for (i = 1; i < RUN_COUNT + 2; i++)
    curs[i] = do_one_test (num_threads, crt_len, non_crt_len, iters);

  /* Sort the results so we can discard the fastest and slowest
     times as outliers.  */
  for (i = 0; i < RUN_COUNT + 1; i++)
    for (j = i + 1; j < RUN_COUNT + 2; j++)
      if (curs[i] > curs[j])
	{
	  timing_t temp = curs[i];
	  curs[i] = curs[j];
	  curs[j] = temp;
	}

  /* Calculate mean and standard deviation.  */
  mean = 0.0;
  total_iters = iters * num_threads;
  for (i = 1; i < RUN_COUNT + 1; i++)
    mean += (double) curs[i] / (double) total_iters;
  mean /= RUN_COUNT;

  stdev = 0.0;
  for (i = 1; i < RUN_COUNT + 1; i++)
    {
      double s = (double) curs[i] / (double) total_iters - mean;
      stdev += s * s;
    }
  stdev = sqrt (stdev / (RUN_COUNT - 1));

  char buf[256];
  snprintf (buf, sizeof buf, "%s,non_crt_len=%d,crt_len=%d,threads=%d", name,
	    non_crt_len, crt_len, num_threads);

  json_attr_object_begin (js, buf);

  json_attr_double (js, "duration", (double) cur);
  json_attr_double (js, "iterations", (double) total_iters);
  json_attr_double (js, "mean", mean);
  json_attr_double (js, "stdev", stdev);
  json_attr_double (js, "min-outlier",
		    (double) curs[0] / (double) total_iters);
  json_attr_double (js, "min", (double) curs[1] / (double) total_iters);
  json_attr_double (js, "max",
		    (double) curs[RUN_COUNT] / (double) total_iters);
  json_attr_double (js, "max-outlier",
		    (double) curs[RUN_COUNT + 1] / (double) total_iters);

  json_attr_object_end (js);
}

#define TH_CONF_MAX 10

int
do_bench (void)
{
  int rv = 0;
  json_ctx_t json_ctx;
  int i, j, k;
  int th_num, th_conf, nprocs;
  int threads[TH_CONF_MAX];
  int crt_lens[] = { 0, 1, 2, 4, 8, 16, 32, 64, 128 };
  int non_crt_lens[] = { 1, 32, 128 };
  char name[128];

  json_init (&json_ctx, 2, stdout);
  json_attr_object_begin (&json_ctx, TEST_NAME);

  /* The thread config begins from 1, and increases by 2x until nprocs.
     We also wants to test over-saturation case (1.25*nprocs).  */
  nprocs = get_nprocs ();
  th_num = 1;
  for (th_conf = 0; th_conf < (TH_CONF_MAX - 2) && th_num < nprocs; th_conf++)
    {
      threads[th_conf] = th_num;
      th_num <<= 1;
    }
  threads[th_conf++] = nprocs;
  threads[th_conf++] = nprocs + nprocs / 4;

  LOCK_ATTR_INIT (&attr);
  snprintf (name, sizeof name, "type=adaptive");

  for (k = 0; k < (sizeof (non_crt_lens) / sizeof (int)); k++)
    {
      int non_crt_len = non_crt_lens[k];
      for (j = 0; j < (sizeof (crt_lens) / sizeof (int)); j++)
	{
	  int crt_len = crt_lens[j];
	  for (i = 0; i < th_conf; i++)
	    {
	      th_num = threads[i];
	      do_bench_one (name, th_num, crt_len, non_crt_len, &json_ctx);
	    }
	}
    }

  json_attr_object_end (&json_ctx);

  return rv;
}

#define TEST_FUNCTION do_bench ()

#include "../test-skeleton.c"
