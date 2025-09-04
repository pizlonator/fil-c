/* Test that pending signals are not delivered on thread exit (bug 28607).
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

/* Due to bug 28607, pthread_kill (or pthread_cancel) restored the
   signal mask during during thread exit, triggering the delivery of a
   blocked pending signal (SIGUSR1 in this test).  */

#include <support/xthread.h>
#include <support/xsignal.h>

static void *
threadfunc (void *closure)
{
  sigset_t sigmask;
  sigfillset (&sigmask);
  xpthread_sigmask (SIG_SETMASK, &sigmask, NULL);
  xpthread_kill (pthread_self (), SIGUSR1);
  pthread_exit (NULL);
  return NULL;
}

static int
do_test (void)
{
  pthread_t thr = xpthread_create (NULL, threadfunc, NULL);
  xpthread_join (thr);
  return 0;
}

#include <support/test-driver.c>
