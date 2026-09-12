/* Test getenv from a signal handler interrupting environment updates.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xthread.h>
#include <support/xsignal.h>

/* Set to false by the main thread after doing all the setenv
   calls.  */
static bool running = true;

/* Used to synchronize the start of signal sending.  */
static pthread_barrier_t barrier;

/* Identity of the main thread.  */
static pthread_t main_thread;

/* Send SIGUSR1 signals to main_thread.  */
static void *
signal_thread (void *ignored)
{
  xpthread_barrier_wait (&barrier);
  while (__atomic_load_n (&running, __ATOMIC_RELAXED))
    xpthread_kill (main_thread, SIGUSR1);
  return NULL;
}

/* Call getenv from a signal handler.  */
static void
signal_handler (int signo)
{
  TEST_COMPARE_STRING (getenv ("unset_variable"), NULL);
  char *value = getenv ("set_variable");
  TEST_VERIFY (strncmp (value, "value", strlen ("value")) == 0);
}

static int
do_test (void)
{
  /* Added to the environment using putenv.  */
  char *variables[30];
  for (int i = 0; i < array_length (variables); ++i)
    variables[i] = xasprintf ("v%d=%d", i, i);

  xsignal (SIGUSR1, signal_handler);
  TEST_COMPARE (setenv ("set_variable", "value", 1), 0);
  xraise (SIGUSR1);
  main_thread = pthread_self ();
  xpthread_barrier_init (&barrier, NULL, 2);
  pthread_t thr = xpthread_create (NULL, signal_thread, NULL);
  xpthread_barrier_wait (&barrier);

  for (int i = 0; i < array_length (variables); ++i)
    {
      char buf[30];
      TEST_COMPARE (setenv ("temporary_variable", "1", 1), 0);
      snprintf (buf, sizeof (buf), "V%d", i);
      TEST_COMPARE (setenv (buf, buf + 1, 1), 0);
      TEST_COMPARE (putenv (variables[i]), 0);
      snprintf (buf, sizeof (buf), "value%d", i);
      TEST_COMPARE (setenv ("set_variable", buf, 1), 0);
      TEST_COMPARE (unsetenv ("temporary_variable"), 0);
    }

  __atomic_store_n (&running, false, __ATOMIC_RELAXED);
  xpthread_join (thr);
  xpthread_barrier_destroy (&barrier);

  for (int i = 0; i < array_length (variables); ++i)
    free (variables[i]);
  return 0;
}

#include <support/test-driver.c>
