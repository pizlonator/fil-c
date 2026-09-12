/* Measure pthread trylock throughput under high cache contention.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
#include "bench-util.h"
#include "json-lib.h"

#define ITERS 10000000
#define RUN_COUNT 10

pthread_mutex_t mutex = PTHREAD_MUTEX_TYPE_INITIALIZER;

/* Shared counter only incremented by successful threads.  */
int counter = 0;

static void *
worker (void *v)
{
  for (int i = 0; i < ITERS; i++)
    if (pthread_mutex_trylock(&mutex) == 0)
      {
	counter++;
	pthread_mutex_unlock(&mutex);
      }
  return NULL;
}

static void
do_bench_one (const char *name, int num_threads, int nprocs, json_ctx_t *js)
{
  struct timeval ts, te;
  double tsd, ted, td, mean, stdev;
  int i, j;
  pthread_t *threads = malloc (num_threads * sizeof (pthread_t));

  /* Array to hold throughput(#updates/second) result of each run.  We do
     RUN_COUNT + 2 runs so that we can discard the highest and lowest as
     outliers.  */
  double throughput_count[RUN_COUNT + 2];

  for (i = 0; i < RUN_COUNT + 2; i++)
    {
      /* Initialize counter before each run.  */
      counter = 0;
      gettimeofday (&ts, NULL);

      for (j = 0; j < num_threads; j++)
	pthread_create (&threads[j], NULL, worker, NULL);

      for (j = 0; j < num_threads; j++)
	pthread_join (threads[j], NULL);
      gettimeofday (&te, NULL);
      tsd = ts.tv_sec + ts.tv_usec / 1000000.0;
      ted = te.tv_sec + te.tv_usec / 1000000.0;
      td = ted - tsd;

      /* Number of successful updates per second.  */
      throughput_count[i] = counter / td;
    }

  free (threads);

  /* Sort the results so we can discard the largest and smallest
     throughput as outliers.  */
  for (i = 0; i < RUN_COUNT + 1; i++)
    for (j = i + 1; j < RUN_COUNT + 2; j++)
      if (throughput_count[i] > throughput_count[j])
	{
	  double temp = throughput_count[i];
	  throughput_count[i] = throughput_count[j];
	  throughput_count[j] = temp;
	}

  /* Calculate mean and standard deviation.  */
  mean = 0;
  for (i = 1; i < RUN_COUNT + 1; i++)
    mean += throughput_count[i];
  mean /= RUN_COUNT;

  stdev = 0.0;
  for (i = 1; i < RUN_COUNT + 1; i++)
    {
      double s = throughput_count[i] - mean;
      stdev += s * s;
    }
  stdev = sqrt (stdev / (RUN_COUNT - 1));

  char buf[256];
  snprintf (buf, sizeof buf, "%s,threads=%d,HW threads=%d", name, num_threads,
	    nprocs);

  json_attr_object_begin (js, buf);

  json_attr_uint (js, "mean", mean);
  json_attr_uint (js, "stdev", stdev);
  json_attr_uint (js, "min-outlier", throughput_count[0]);
  json_attr_uint (js, "min", throughput_count[1]);
  json_attr_uint (js, "max", throughput_count[RUN_COUNT]);
  json_attr_uint (js, "max-outlier", throughput_count[RUN_COUNT + 1]);

  json_attr_object_end (js);
}

int
do_bench (void)
{
  int rv = 0;
  json_ctx_t json_ctx;
  int th_num, th_conf, nprocs;
  char name[128];

  json_init (&json_ctx, 2, stdout);
  json_attr_object_begin (&json_ctx, TEST_NAME);

  /* The thread config begins from 2, and increases by 2x until nprocs.
     We also want to test nprocs and over-saturation case 2*nprocs and
     4*nprocs.  */
  nprocs = get_nprocs ();

  /* Allocate [nprocs + 2] thread config array.  */
  int *threads = malloc ((nprocs + 2) * sizeof (int));

  th_num = 2;
  for (th_conf = 0; th_num < nprocs; th_conf++)
    {
      threads[th_conf] = th_num;
      th_num <<= 1;
    }
  threads[th_conf++] = nprocs;
  threads[th_conf++] = nprocs * 2;
  threads[th_conf++] = nprocs * 4;

  snprintf (name, sizeof name, "type=throughput(#updates/second)");

  for (int i = 0; i < th_conf; i++)
    {
      th_num = threads[i];
      do_bench_one (name, th_num, nprocs, &json_ctx);
    }

  free (threads);

  json_attr_object_end (&json_ctx);

  return rv;
}

#define TEST_FUNCTION do_bench

#include <support/test-driver.c>
