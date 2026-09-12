/* Check if pthread_join acts a cancellation entrypoiny (BZ 33717)
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

#include <support/check.h>
#include <support/xunistd.h>
#include <support/xthread.h>
#include <support/process_state.h>

static pthread_barrier_t b1;
static pthread_barrier_t b2;

static void *
thr_func2 (void *arg)
{
  xpthread_barrier_wait (&b2);
  return NULL;
}

static void *
thr_func1 (void *arg)
{
  int (*join_func)(pthread_t, void **) = arg;

  xpthread_barrier_init (&b2, NULL, 2);

  pthread_t thr = xpthread_create (NULL, thr_func2, NULL);

  pid_t tid = pthread_gettid_np (thr);
  /* Synchronize the inner thread so the pthread_gettid_np returns a valid
     TID.  */
  xpthread_barrier_wait (&b2);

  support_thread_state_wait (tid, support_process_state_dead);

  /* Synchronize with parent thread so the cancellation is issues after
     the inner thread has finished.  */
  xpthread_barrier_wait (&b1);

  /* Wait for the cancellation.  */
  xpthread_barrier_wait (&b1);

  join_func (thr, NULL);

  support_record_failure ();

  return NULL;
}

static void
do_test_common (int (*join_func)(pthread_t, void **))
{
  xpthread_barrier_init (&b1, NULL, 2);

  pthread_t thr = xpthread_create (NULL, thr_func1, join_func);

  /* Wait until the inner thread is created and terminated
     (support_process_state_dead) before issuing the pthread_cancel.  */
  xpthread_barrier_wait (&b1);

  xpthread_cancel (thr);

  /* The deferred cancellation is pending on the thread, issue the
     routine to check.  */
  xpthread_barrier_wait (&b1);

  TEST_VERIFY (xpthread_join (thr) == PTHREAD_CANCELED);
}

static int
pthread_timedjoin_np_wrapper (pthread_t thr, void **retval)
{
  return pthread_timedjoin_np (thr, retval, NULL);
}

static int
pthread_clockjoin_np_wrapper (pthread_t thr, void **retval)
{
  return pthread_clockjoin_np (thr, retval, CLOCK_REALTIME, NULL);
}

static int
do_test (void)
{
  do_test_common (pthread_join);
  do_test_common (pthread_timedjoin_np_wrapper);
  do_test_common (pthread_clockjoin_np_wrapper);

  return 0;
}

#define TIMEOUT 3
#include <support/test-driver.c>
