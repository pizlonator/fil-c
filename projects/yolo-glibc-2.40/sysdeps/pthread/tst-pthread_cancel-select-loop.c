/* Test that pthread_cancel succeeds during thread exit.
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

/* This test tries to trigger an internal race condition in
   pthread_cancel, where the cancellation signal is sent after the
   thread has begun the cancellation process.  This can result in a
   spurious ESRCH error.  For the original bug 12889, the window is
   quite small, so the bug was not reproduced in every run.  */

#include <stdbool.h>
#include <stddef.h>
#include <support/check.h>
#include <support/xthread.h>
#include <support/xunistd.h>
#include <sys/select.h>
#include <unistd.h>

/* Set to true by timeout_thread_function when the test should
   terminate.  */
static bool timeout;

static void *
timeout_thread_function (void *unused)
{
  usleep (5 * 1000 * 1000);
  __atomic_store_n (&timeout, true, __ATOMIC_RELAXED);
  return NULL;
}

/* Used for blocking the select function below.  */
static int pipe_fds[2];

static void *
canceled_thread_function (void *unused)
{
  while (true)
    {
      fd_set rfs;
      fd_set wfs;
      fd_set efs;
      FD_ZERO (&rfs);
      FD_ZERO (&wfs);
      FD_ZERO (&efs);
      FD_SET (pipe_fds[0], &rfs);

      /* If the cancellation request is recognized early, the thread
         begins exiting while the cancellation signal arrives.  */
      select (FD_SETSIZE, &rfs, &wfs, &efs, NULL);
    }
  return NULL;
}

static int
do_test (void)
{
  xpipe (pipe_fds);
  pthread_t thr_timeout = xpthread_create (NULL, timeout_thread_function, NULL);

  while (!__atomic_load_n (&timeout, __ATOMIC_RELAXED))
    {
      pthread_t thr = xpthread_create (NULL, canceled_thread_function, NULL);
      xpthread_cancel (thr);
      TEST_VERIFY (xpthread_join (thr) == PTHREAD_CANCELED);
    }

  xpthread_join (thr_timeout);
  xclose (pipe_fds[0]);
  xclose (pipe_fds[1]);
  return 0;
}

#include <support/test-driver.c>
