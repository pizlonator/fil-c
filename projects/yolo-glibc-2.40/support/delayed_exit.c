/* Time-triggered process termination.
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

#include <support/xthread.h>
#include <support/xsignal.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <time.h>
#include <unistd.h>

struct delayed_exit_request
{
  void (*exitfunc) (int);
  int seconds;
};

static void *
delayed_exit_thread (void *closure)
{
  struct delayed_exit_request *request = closure;
  void (*exitfunc) (int) = request->exitfunc;
  struct timespec delay = { request->seconds, 0 };
  struct timespec remaining = { 0 };
  free (request);

  if (nanosleep (&delay, &remaining) != 0)
    FAIL_EXIT1 ("nanosleep: %m");
  /* Exit the process successfully.  */
  exitfunc (0);
  return NULL;
}

static void
delayed_exit_1 (int seconds, void (*exitfunc) (int))
{
  /* Create the new thread with all signals blocked.  */
  sigset_t all_blocked;
  sigfillset (&all_blocked);
  sigset_t old_set;
  xpthread_sigmask (SIG_SETMASK, &all_blocked, &old_set);
  struct delayed_exit_request *request = xmalloc (sizeof (*request));
  request->seconds = seconds;
  request->exitfunc = exitfunc;
  /* Create a detached thread. */
  pthread_t thr = xpthread_create (NULL, delayed_exit_thread, request);
  xpthread_detach (thr);
  /* Restore the original signal mask.  */
  xpthread_sigmask (SIG_SETMASK, &old_set, NULL);
}

void
delayed_exit (int seconds)
{
  delayed_exit_1 (seconds, exit);
}

void
delayed__exit (int seconds)
{
  delayed_exit_1 (seconds, _exit);
}
