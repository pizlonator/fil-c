/* Test for sched_setattr, sched_getattr involving multiple threads.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sched.h>

#include <stddef.h>
#include <stdio.h>
#include <support/check.h>
#include <support/xthread.h>
#include <unistd.h>

enum { initial_nice_value = 15 };

/* Check that thread TID has nice value EXPECTED.  */
static void
check_nice_value (int tid, int expected)
{
  struct sched_attr attr;
  if (sched_getattr (tid, &attr, sizeof (attr), 0) != 0)
    FAIL_EXIT1 ("sched_getattr (%d) failed: %m", tid);
  TEST_COMPARE (attr.sched_policy, SCHED_OTHER);
  int nice_value = attr.sched_nice;
  if (attr.sched_nice != expected)
    FAIL_EXIT1 ("thread %d: expected nice value %d, got %d"
                " (called from thread %d)",
                tid, expected, nice_value,  (int) gettid ());
  printf ("info: thread %d: nice value %d (called from thread %d)\n",
          tid, nice_value, (int) gettid ());
}

/* Set the nice value for TID to VALUE.  */
static void
set_nice_value (int tid, int value)
{
  struct sched_attr attr =
    {
      .size = sizeof (attr),
      .sched_policy = SCHED_OTHER,
      .sched_nice = value,
    };
  if (sched_setattr (tid, &attr, 0) != 0)
    FAIL_EXIT1 ("sched_setattr (%d) failed: %m", tid);
}

static pthread_barrier_t barrier;

static void *
thread_routine (void *nice_value_ptr)
{
  int nice_value = *(int *) nice_value_ptr;
  /* Check that the nice value was inherited.  */
  check_nice_value (gettid (), initial_nice_value);
  xpthread_barrier_wait (&barrier);
  /* Main thread sets nice value.  */
  xpthread_barrier_wait (&barrier);
  check_nice_value (gettid (), nice_value);
  set_nice_value (gettid (), nice_value + 2);
  xpthread_barrier_wait (&barrier);
  /* Main thread sets checks value.  */
  xpthread_barrier_wait (&barrier);
  return NULL;
}

static int
do_test (void)
{
  if (nice (initial_nice_value) != initial_nice_value)
    FAIL_UNSUPPORTED ("cannot set nice value to initial_nice_value: %m");

  xpthread_barrier_init (&barrier, NULL, 3);

  check_nice_value (0, initial_nice_value);
  check_nice_value (gettid (), initial_nice_value);

  int nice0 = initial_nice_value + 1;
  pthread_t thr0 = xpthread_create (NULL, thread_routine, &nice0);
  int nice1 = initial_nice_value + 2;
  pthread_t thr1 = xpthread_create (NULL, thread_routine, &nice1);
  check_nice_value (pthread_gettid_np (thr0), initial_nice_value);
  check_nice_value (pthread_gettid_np (thr1), initial_nice_value);
  xpthread_barrier_wait (&barrier);
  set_nice_value (pthread_gettid_np (thr0), nice0);
  check_nice_value (pthread_gettid_np (thr0), nice0);
  check_nice_value (pthread_gettid_np (thr1), initial_nice_value);
  set_nice_value (pthread_gettid_np (thr1), nice1);
  check_nice_value (pthread_gettid_np (thr0), nice0);
  check_nice_value (pthread_gettid_np (thr1), nice1);
  xpthread_barrier_wait (&barrier);
  /* Threads set nice value.  */
  xpthread_barrier_wait (&barrier);
  check_nice_value (pthread_gettid_np (thr0), nice0 + 2);
  check_nice_value (pthread_gettid_np (thr1), nice1 + 2);
  xpthread_barrier_wait (&barrier);

  TEST_VERIFY (xpthread_join (thr1) == NULL);
  TEST_VERIFY (xpthread_join (thr0) == NULL);

  return 0;
}

#include <support/test-driver.c>
