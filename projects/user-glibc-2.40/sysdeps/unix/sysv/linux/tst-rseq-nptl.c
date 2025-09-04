/* Restartable Sequences NPTL test.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.

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

/* These tests validate that rseq is registered from various execution
   contexts (main thread, destructor, other threads, other threads created
   from destructor, forked process (without exec), pthread_atfork handlers,
   pthread setspecific destructors, signal handlers, atexit handlers).

   See the Linux kernel selftests for extensive rseq stress-tests.  */

#include <stdio.h>
#include <support/check.h>
#include <support/xthread.h>
#include <sys/rseq.h>
#include <unistd.h>

#ifdef RSEQ_SIG
# include <array_length.h>
# include <errno.h>
# include <error.h>
# include <pthread.h>
# include <signal.h>
# include <stdlib.h>
# include <string.h>
# include <support/namespace.h>
# include <support/xsignal.h>
# include <syscall.h>
# include <sys/types.h>
# include <sys/wait.h>
# include "tst-rseq.h"

static pthread_key_t rseq_test_key;

static void
atfork_prepare (void)
{
  if (!rseq_thread_registered ())
    {
      printf ("error: rseq not registered in pthread atfork prepare\n");
      support_record_failure ();
    }
}

static void
atfork_parent (void)
{
  if (!rseq_thread_registered ())
    {
      printf ("error: rseq not registered in pthread atfork parent\n");
      support_record_failure ();
    }
}

static void
atfork_child (void)
{
  if (!rseq_thread_registered ())
    {
      printf ("error: rseq not registered in pthread atfork child\n");
      support_record_failure ();
    }
}

static void
rseq_key_destructor (void *arg)
{
  /* Cannot use deferred failure reporting after main returns.  */
  if (!rseq_thread_registered ())
    FAIL_EXIT1 ("rseq not registered in pthread key destructor");
}

static void
atexit_handler (void)
{
  /* Cannot use deferred failure reporting after main returns.  */
  if (!rseq_thread_registered ())
    FAIL_EXIT1 ("rseq not registered in atexit handler");
}

/* Used to avoid -Werror=stringop-overread warning with
   pthread_setspecific and GCC 11.  */
static char one = 1;

static void
do_rseq_main_test (void)
{
  TEST_COMPARE (atexit (atexit_handler), 0);
  rseq_test_key = xpthread_key_create (rseq_key_destructor);
  TEST_COMPARE (pthread_atfork (atfork_prepare, atfork_parent, atfork_child), 0);
  xraise (SIGUSR1);
  TEST_COMPARE (pthread_setspecific (rseq_test_key, &one), 0);
  TEST_VERIFY_EXIT (rseq_thread_registered ());
}

static void
cancel_routine (void *arg)
{
  if (!rseq_thread_registered ())
    {
      printf ("error: rseq not registered in cancel routine\n");
      support_record_failure ();
    }
}

static pthread_barrier_t cancel_thread_barrier;
static pthread_cond_t cancel_thread_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t cancel_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

static void
test_cancel_thread (void)
{
  pthread_cleanup_push (cancel_routine, NULL);
  (void) xpthread_barrier_wait (&cancel_thread_barrier);
  /* Wait forever until cancellation.  */
  xpthread_cond_wait (&cancel_thread_cond, &cancel_thread_mutex);
  pthread_cleanup_pop (0);
}

static void *
thread_function (void * arg)
{
  int i = (int) (intptr_t) arg;

  xraise (SIGUSR1);
  if (i == 0)
    test_cancel_thread ();
  TEST_COMPARE (pthread_setspecific (rseq_test_key, &one), 0);
  return rseq_thread_registered () ? NULL : (void *) 1l;
}

static void
sighandler (int sig)
{
  if (!rseq_thread_registered ())
    {
      printf ("error: rseq not registered in signal handler\n");
      support_record_failure ();
    }
}

static void
setup_signals (void)
{
  struct sigaction sa;

  sigemptyset (&sa.sa_mask);
  sigaddset (&sa.sa_mask, SIGUSR1);
  sa.sa_flags = 0;
  sa.sa_handler = sighandler;
  xsigaction (SIGUSR1, &sa, NULL);
}

static int
do_rseq_threads_test (int nr_threads)
{
  pthread_t th[nr_threads];
  int i;
  int result = 0;

  xpthread_barrier_init (&cancel_thread_barrier, NULL, 2);

  for (i = 0; i < nr_threads; ++i)
    th[i] = xpthread_create (NULL, thread_function,
                             (void *) (intptr_t) i);

  (void) xpthread_barrier_wait (&cancel_thread_barrier);

  xpthread_cancel (th[0]);

  for (i = 0; i < nr_threads; ++i)
    {
      void *v;

      v = xpthread_join (th[i]);
      if (i != 0 && v != NULL)
        {
          printf ("error: join %d successful, but child failed\n", i);
          result = 1;
        }
      else if (i == 0 && v == NULL)
        {
          printf ("error: join %d successful, child did not fail as expected\n", i);
          result = 1;
        }
    }

  xpthread_barrier_destroy (&cancel_thread_barrier);

  return result;
}

static void
subprocess_callback (void *closure)
{
  do_rseq_main_test ();
}

static void
do_rseq_fork_test (void)
{
  support_isolate_in_subprocess (subprocess_callback, NULL);
}

static int
do_rseq_test (void)
{
  int t[] = { 1, 2, 6, 5, 4, 3, 50 };
  int i, result = 0;

  if (!rseq_available ())
    FAIL_UNSUPPORTED ("kernel does not support rseq, skipping test");
  setup_signals ();
  xraise (SIGUSR1);
  do_rseq_main_test ();
  for (i = 0; i < array_length (t); i++)
    if (do_rseq_threads_test (t[i]))
      result = 1;
  do_rseq_fork_test ();
  return result;
}

static void __attribute__ ((destructor))
do_rseq_destructor_test (void)
{
  /* Cannot use deferred failure reporting after main returns.  */
  if (do_rseq_test ())
    FAIL_EXIT1 ("rseq not registered within destructor");
  xpthread_key_delete (rseq_test_key);
}

#else /* RSEQ_SIG */
static int
do_rseq_test (void)
{
  FAIL_UNSUPPORTED ("glibc does not define RSEQ_SIG, skipping test");
  return 0;
}
#endif /* RSEQ_SIG */

static int
do_test (void)
{
  return do_rseq_test ();
}

#include <support/test-driver.c>
