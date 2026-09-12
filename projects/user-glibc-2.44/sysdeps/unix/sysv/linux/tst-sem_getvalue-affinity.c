/* Test sem_getvalue across CPUs.  Based on tst-skeleton-thread-affinity.c.
   Copyright (C) 2015-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <support/xthread.h>
#include <sys/time.h>

struct conf;
static bool early_test (struct conf *);

static int
setaffinity (size_t size, const cpu_set_t *set)
{
  int ret = pthread_setaffinity_np (pthread_self (), size, set);
  if (ret != 0)
    {
      errno = ret;
      return -1;
    }
  return 0;
}

static int
getaffinity (size_t size, cpu_set_t *set)
{
  int ret = pthread_getaffinity_np (pthread_self (), size, set);
  if (ret != 0)
    {
      errno = ret;
      return -1;
    }
  return 0;
}

#include "tst-skeleton-affinity.c"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem;

static void *
tf (void *arg)
{
  void *ret = NULL;
  xpthread_mutex_lock (&lock);
  int semval;
  if (sem_getvalue (&sem, &semval) != 0)
    {
      printf ("sem_getvalue failed: %m\n");
      ret = (void *) 1;
    }
  else if (semval != 12345)
    {
      printf ("sem_getvalue returned %d not 12345\n", semval);
      ret = (void *) 1;
    }
  xpthread_mutex_unlock (&lock);
  return ret;
}

static int
stop_and_join_threads (struct conf *conf, cpu_set_t *set,
		       pthread_t *pinned_first, pthread_t *pinned_last)
{
  int failed = 0;
  for (pthread_t *p = pinned_first; p < pinned_last; ++p)
    {
      int cpu = p - pinned_first;
      if (!CPU_ISSET_S (cpu, CPU_ALLOC_SIZE (conf->set_size), set))
	continue;

      void *retval = (void *) 1;
      int ret = pthread_join (*p, &retval);
      if (ret != 0)
	{
	  printf ("error: Failed to join thread %d: %s\n", cpu, strerror (ret));
	  fflush (stdout);
	  /* Cannot shut down cleanly with threads still running.  */
	  abort ();
	}
      if (retval != NULL)
	failed = 1;
    }
  return failed;
}

static bool
early_test (struct conf *conf)
{
  int ret;
  ret = sem_init (&sem, 0, 12345);
  if (ret != 0)
    {
      printf ("error: sem_init failed: %m\n");
      return false;
    }
  xpthread_mutex_lock (&lock);
  pthread_t *pinned_threads
    = calloc (conf->last_cpu + 1, sizeof (*pinned_threads));
  cpu_set_t *initial_set = CPU_ALLOC (conf->set_size);
  cpu_set_t *scratch_set = CPU_ALLOC (conf->set_size);

  if (pinned_threads == NULL || initial_set == NULL || scratch_set == NULL)
    {
      puts ("error: Memory allocation failure");
      return false;
    }
  if (getaffinity (CPU_ALLOC_SIZE (conf->set_size), initial_set) < 0)
    {
      printf ("error: pthread_getaffinity_np failed: %m\n");
      return false;
    }

  pthread_attr_t attr;
  ret = pthread_attr_init (&attr);
  if (ret != 0)
    {
      printf ("error: pthread_attr_init failed: %s\n", strerror (ret));
      return false;
    }
  support_set_small_thread_stack_size (&attr, false);

  /* Spawn a thread pinned to each available CPU.  */
  for (int cpu = 0; cpu <= conf->last_cpu; ++cpu)
    {
      if (!CPU_ISSET_S (cpu, CPU_ALLOC_SIZE (conf->set_size), initial_set))
	continue;
      CPU_ZERO_S (CPU_ALLOC_SIZE (conf->set_size), scratch_set);
      CPU_SET_S (cpu, CPU_ALLOC_SIZE (conf->set_size), scratch_set);
      ret = pthread_attr_setaffinity_np
	(&attr, CPU_ALLOC_SIZE (conf->set_size), scratch_set);
      if (ret != 0)
	{
	  printf ("error: pthread_attr_setaffinity_np for CPU %d failed: %s\n",
		  cpu, strerror (ret));
	  stop_and_join_threads (conf, initial_set,
				 pinned_threads, pinned_threads + cpu);
	  return false;
	}
      ret = pthread_create (pinned_threads + cpu, &attr,
			    tf, (void *) (uintptr_t) cpu);
      if (ret != 0)
	{
	  printf ("error: pthread_create for CPU %d failed: %s\n",
		  cpu, strerror (ret));
	  stop_and_join_threads (conf, initial_set,
				 pinned_threads, pinned_threads + cpu);
	  return false;
	}
    }

  /* Main thread.  */
  xpthread_mutex_unlock (&lock);
  int failed = stop_and_join_threads (conf, initial_set,
				      pinned_threads,
				      pinned_threads + conf->last_cpu + 1);

  printf ("info: Main thread ran on %d CPU(s) of %d available CPU(s)\n",
	  CPU_COUNT_S (CPU_ALLOC_SIZE (conf->set_size), scratch_set),
	  CPU_COUNT_S (CPU_ALLOC_SIZE (conf->set_size), initial_set));

  pthread_attr_destroy (&attr);
  CPU_FREE (scratch_set);
  CPU_FREE (initial_set);
  free (pinned_threads);
  return failed == 0;
}
