/* Check if cancellation state and type are correctly set on thread exit.
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

#include <array_length.h>
#include <stdio.h>
#include <support/check.h>
#include <support/xthread.h>
#include <support/xunistd.h>

static int pipefds[2];
static pthread_barrier_t b;

static void
clh (void *arg)
{
  /* Although POSIX states that setting either the cancellation state or type
     is undefined during cleanup handler execution, both calls should be safe,
     since neither has any side effects (they should not change the current
     state nor trigger a pending cancellation).  */

  int state;
  TEST_VERIFY (pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &state) == 0);
  TEST_COMPARE (state, PTHREAD_CANCEL_DISABLE);

  int type;
  TEST_VERIFY (pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, &type) == 0);
  TEST_COMPARE (type, PTHREAD_CANCEL_DEFERRED);
}

/* Check if a thread with different cancellation types on pthread_cleanup_pop
   sets the correct state and type as pthread_exit.  */
static void *
tf_cancel (void *arg)
{
  int *cancel_type = (int *) (arg);

  pthread_cleanup_push (clh, NULL);

  TEST_VERIFY (pthread_setcanceltype (*cancel_type, NULL) == 0);

  xpthread_barrier_wait (&b);

  xread (pipefds[0], &(char){0}, 1);

  support_record_failure ();

  pthread_cleanup_pop (1);

  return NULL;
}

/* Check if a thread with different cancellation types on blocked read()
   sets the correct state and type as pthread_exit.  */
static void *
tf_testcancel (void *arg)
{
  int *cancel_type = (int *) (arg);

  pthread_cleanup_push (clh, NULL);

  xpthread_barrier_wait (&b);

  /* For PTHREAD_CANCEL_ASYNCHRONOUS, the cancellation might act on
     the pthread_setcanceltype.   */
  TEST_VERIFY (pthread_setcanceltype (*cancel_type, NULL) == 0);

  pthread_testcancel ();

  support_record_failure ();

  pthread_cleanup_pop (1);

  return NULL;
}

#define EXIT_EXPECTED_VALUE ((void *) 42)

/* Check if a thread with different cancellation types on pthread_exit() sets
   the correct state and type.  */
static void *
tf_exit (void *arg)
{
  int *cancel_type = (int *) (arg);

  TEST_VERIFY (pthread_setcanceltype (*cancel_type, NULL) == 0);

  pthread_cleanup_push (clh, NULL);

  xpthread_barrier_wait (&b);

  pthread_exit (EXIT_EXPECTED_VALUE);

  support_record_failure ();

  pthread_cleanup_pop (1);

  return NULL;
}

static int
do_test (void)
{
  xpipe (pipefds);

  xpthread_barrier_init (&b, NULL, 2);

  static const struct
  {
    const char *n;
    int t;
  } cts[] =
  {
    { "PTHREAD_CANCEL_DEFERRED", PTHREAD_CANCEL_DEFERRED },
    { "PTHREAD_CANCEL_ASYNCHRONOUS", PTHREAD_CANCEL_ASYNCHRONOUS },
  };

  for (int i = 0; i < array_length (cts); i++)
    {
      {
	printf ("info: checking %s\n", cts[i].n);
	pthread_t th = xpthread_create (NULL, tf_cancel, &(int){cts[i].t});
	xpthread_barrier_wait (&b);
	xpthread_cancel (th);
	void *r = xpthread_join (th);
	TEST_VERIFY (r == PTHREAD_CANCELED);
      }

      {
	printf ("info: checking %s with pthread_testcancel\n", cts[i].n);
	pthread_t th = xpthread_create (NULL, tf_testcancel, &(int){cts[i].t});
	xpthread_cancel (th);
	xpthread_barrier_wait (&b);
	void *r = xpthread_join (th);
	TEST_VERIFY (r == PTHREAD_CANCELED);
      }

      {
	printf ("info: checking %s with pthread_exit\n", cts[i].n);
	pthread_t th = xpthread_create (NULL, tf_exit, &(int){cts[i].t});
	xpthread_barrier_wait (&b);
	void *r = xpthread_join (th);
	TEST_VERIFY (r == EXIT_EXPECTED_VALUE);
      }
    }

  return 0;
}

#include <support/test-driver.c>
