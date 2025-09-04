/* multi-threaded memory allocation/deallocation test.
   Copyright (C) 2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xthread.h>
#include <support/test-driver.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#ifndef ITERATIONS
#  define ITERATIONS 16
#endif

#ifndef NUM_THREADS
#  define NUM_THREADS 8
#endif

#ifndef NUM_ALLOCATIONS
#  define NUM_ALLOCATIONS 2048
#endif

static pthread_barrier_t barrier;

__thread unsigned int seed;

typedef struct
{
  int id;
  pthread_t thread;
} thread;

thread threads[NUM_THREADS];

void *allocations[NUM_THREADS][NUM_ALLOCATIONS];

void
run_thread_dealloc (int id)
{
  for (int i = 0; i < NUM_ALLOCATIONS; i++)
    {
      free (allocations[id][i]);
      allocations[id][i] = NULL;
    }
}

void
run_thread_alloc (int id)
{
  size_t msb, size;
  for (int i = 0; i < NUM_ALLOCATIONS; i++)
    {
      msb = 1 << rand_r (&seed) % 16;
      size = msb + rand_r (&seed) % msb;
      allocations[id][i] = malloc (size);
      TEST_VERIFY_EXIT (allocations[id][i] != NULL);
    }
}

void *
run_allocations (void *arg)
{
  int id = *((int *) arg);
  seed = time (NULL) + id;

  /* Stage 1: First half o the threads allocating memory and the second
   * half waiting for them to finish
   */
  if (id < NUM_THREADS / 2)
    run_thread_alloc (id);

  xpthread_barrier_wait (&barrier);

  /* Stage 2: Half of the threads allocationg memory and the other
   * half deallocating:
   * - In the non cross-thread dealloc scenario the first half will be
   *   deallocating the memory allocated by themselves in stage 1 and the
   *   second half will be allocating memory.
   * - In the cross-thread dealloc scenario the first half will continue
   *   to allocate memory and the second half will deallocate the memory
   *   allocated by the first half in stage 1.
   */
  if (id < NUM_THREADS / 2)
#ifndef CROSS_THREAD_DEALLOC
    run_thread_dealloc (id);
#else
    run_thread_alloc (id + NUM_THREADS / 2);
#endif
  else
#ifndef CROSS_THREAD_DEALLOC
    run_thread_alloc (id);
#else
    run_thread_dealloc (id - NUM_THREADS / 2);
#endif

  xpthread_barrier_wait (&barrier);

  // Stage 3: Second half of the threads deallocating and the first half
  // waiting for them to finish.
  if (id >= NUM_THREADS / 2)
    run_thread_dealloc (id);

  return NULL;
}

static int
do_test (void)
{
  xpthread_barrier_init (&barrier, NULL, NUM_THREADS);

  for (int i = 0; i < ITERATIONS; i++)
    {
      for (int t = 0; t < NUM_THREADS; t++)
	{
	  threads[t].id = t;
	  threads[t].thread
	      = xpthread_create (NULL, run_allocations, &threads[t].id);
	}

      for (int t = 0; t < NUM_THREADS; t++)
	xpthread_join (threads[t].thread);
    }

  return 0;
}

#include <support/test-driver.c>
