/* Test for pthread_gettid_np.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <support/check.h>
#include <support/xthread.h>
#include <unistd.h>

static pthread_barrier_t barrier;

static pid_t thread_tid;

static void *
thread_func (void *ignored)
{
  thread_tid = gettid ();
  TEST_VERIFY (thread_tid != getpid ());
  TEST_COMPARE (thread_tid, pthread_gettid_np (pthread_self ()));
  xpthread_barrier_wait (&barrier);
  /* The main thread calls pthread_gettid_np here.  */
  xpthread_barrier_wait (&barrier);
  return NULL;
}

static int
do_test (void)
{
  TEST_COMPARE (pthread_gettid_np (pthread_self ()), getpid ());
  TEST_COMPARE (pthread_gettid_np (pthread_self ()), gettid ());

  xpthread_barrier_init (&barrier, NULL, 2);

  pthread_t thr = xpthread_create (NULL, thread_func, NULL);
  xpthread_barrier_wait (&barrier);
  TEST_COMPARE (thread_tid, pthread_gettid_np (thr));
  xpthread_barrier_wait (&barrier);

  while (true)
    {
      /* Check if the kernel thread is still running.  */
      if (tgkill (getpid (), thread_tid, 0))
        {
          TEST_COMPARE (errno, ESRCH);
          break;
        }

      pid_t tid = pthread_gettid_np (thr);
      if (tid != thread_tid)
        {
          TEST_COMPARE (tid, -1);
          break;
        }
      TEST_COMPARE (sched_yield (), 0);
    }

  TEST_VERIFY (xpthread_join (thr) == NULL);

  return 0;
}

#include <support/test-driver.c>
