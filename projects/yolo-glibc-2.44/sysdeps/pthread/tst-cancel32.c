/* Check if pthread_setcanceltype disables asynchronous cancellation
   once cancellation happens (BZ 32782)

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

/* The pthread_setcanceltype is a cancellation entrypoint, and if
   asynchronous is enabled and the cancellation starts (on the second
   pthread_setcanceltype call), the asynchronous should not restart
   the process.  */

#include <support/xthread.h>

#define NITER     1000
#define NTHREADS     8

static void
tf_cleanup (void *arg)
{
}

static void *
tf (void *closure)
{
  pthread_cleanup_push (tf_cleanup, NULL);
  for (;;)
    {
      /* The only possible failure for pthread_setcanceltype is an
	 invalid state type.  */
      pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
      pthread_setcanceltype (PTHREAD_CANCEL_DEFERRED, NULL);
    }
  pthread_cleanup_pop (1);

  return NULL;
}

static void
poll_threads (int nthreads)
{
  pthread_t thr[nthreads];
  for (int i = 0; i < nthreads; i++)
    thr[i] = xpthread_create (NULL, tf, NULL);
  for (int i = 0; i < nthreads; i++)
    xpthread_cancel (thr[i]);
  for (int i = 0; i < nthreads; i++)
    xpthread_join (thr[i]);
}

static int
do_test (void)
{
  for (int k = 0; k < NITER; k++)
    poll_threads (NTHREADS);

  return 0;
}

#include <support/test-driver.c>
