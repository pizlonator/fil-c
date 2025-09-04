/* Test that raise sends signal to current thread even if blocked.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <signal.h>
#include <support/check.h>
#include <support/xsignal.h>
#include <support/xthread.h>
#include <pthread.h>
#include <unistd.h>

/* Used to create a dummy thread ID distinct from all other thread
   IDs.  */
static void *
noop (void *ignored)
{
  return NULL;
}

static volatile pthread_t signal_thread;

static void
signal_handler (int signo)
{
  signal_thread = pthread_self ();
}

/* Used to ensure that waiting_thread has launched and can accept
   signals.  */
static pthread_barrier_t barrier;

static void *
waiting_thread (void *ignored)
{
  xpthread_barrier_wait (&barrier);
  pause ();
  return NULL;
}

static int
do_test (void)
{
  xsignal (SIGUSR1, signal_handler);
  xpthread_barrier_init (&barrier, NULL, 2);

  /* Distinct thread ID value to */
  pthread_t dummy = xpthread_create (NULL, noop, NULL);
  signal_thread = dummy;

  pthread_t helper = xpthread_create (NULL, waiting_thread, NULL);

  /* Make sure that the thread is running.  */
  xpthread_barrier_wait (&barrier);

  /* Block signals on this thread.  */
  sigset_t set;
  sigfillset (&set);
  xpthread_sigmask (SIG_BLOCK, &set, NULL);

  /* Send the signal to this thread.  It must not be delivered.  */
  raise (SIGUSR1);
  TEST_VERIFY (signal_thread == dummy);

  /* Wait a bit to give a chance for signal delivery (increases
     chances of failure with bug 28407).  */
  usleep (50 * 1000);

  /* Unblocking should cause synchronous delivery of the signal.  */
  xpthread_sigmask (SIG_UNBLOCK, &set, NULL);
  TEST_VERIFY (signal_thread == pthread_self ());

  xpthread_cancel (helper);
  xpthread_join (helper);
  xpthread_join (dummy);
  return 0;
}

#include <support/test-driver.c>
