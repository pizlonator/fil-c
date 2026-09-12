/* Check if pthread_create does not act as cancellation entrypoint (BZ 33715)
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

#include <errno.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <support/xthread.h>
#include <stdlib.h>

static pthread_barrier_t b;

static void *
thr_func2 (void *arg)
{
  abort ();
  return NULL;
}

static void *
thr_func1 (void *closure)
{
  int max_cpu = xsysconf (_SC_NPROCESSORS_CONF) + 1;
  /* Set a affinity mask with an invalid CPU.  */
  cpu_set_t *cpuset = CPU_ALLOC (max_cpu);
  TEST_VERIFY_EXIT (cpuset != NULL);
  size_t cpusetsize = CPU_ALLOC_SIZE (max_cpu);
  CPU_ZERO_S (cpusetsize, cpuset);
  CPU_SET_S (max_cpu, cpusetsize, cpuset);

  /* Check if the affinity mask does trigger an error.  */
  TEST_COMPARE (sched_setaffinity (0, cpusetsize, cpuset), -1);
  TEST_COMPARE (errno, EINVAL);

  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  xpthread_attr_setaffinity_np (&attr, cpusetsize, cpuset);

  xpthread_barrier_wait (&b);

  pthread_t thr;
  TEST_COMPARE (pthread_create (&thr, &attr, thr_func2, NULL), EINVAL);
  xpthread_attr_destroy (&attr);

  return NULL;
}

static int
do_test (void)
{
  xpthread_barrier_init (&b, NULL, 2);

  pthread_t thr = xpthread_create (NULL, thr_func1, NULL);

  xpthread_cancel (thr);

  xpthread_barrier_wait (&b);

  void *r = xpthread_join (thr);
  TEST_VERIFY_EXIT (r == NULL);

  return 0;
}

#include <support/test-driver.c>
