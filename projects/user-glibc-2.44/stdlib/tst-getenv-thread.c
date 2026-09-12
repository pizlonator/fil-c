/* Test getenv with concurrent setenv.
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/xthread.h>

/* Set to false by the main thread after doing all the setenv
   calls.  */
static bool running = true;

/* Used to synchronize the start of the getenv thread.  */
static pthread_barrier_t barrier;

/* Invoke getenv for a nonexisting environment variable in a loop.
   This checks that concurrent setenv does not invalidate the
   environment array while getenv reads it.  */
static void *
getenv_thread (void *ignored)
{
  xpthread_barrier_wait (&barrier);
  while (__atomic_load_n (&running, __ATOMIC_RELAXED))
    TEST_VERIFY (getenv ("unset_variable") == NULL);
  return NULL;
}

static int
do_test (void)
{
  xpthread_barrier_init (&barrier, NULL, 2);
  pthread_t thr = xpthread_create (NULL, getenv_thread, NULL);
  xpthread_barrier_wait (&barrier);
  for (int i = 0; i < 1000; ++i)
    {
      char buf[30];
      snprintf (buf, sizeof (buf), "V%d", i);
      TEST_COMPARE (setenv (buf, buf + 1, 1), 0);
    }
  __atomic_store_n (&running, false, __ATOMIC_RELAXED);
  xpthread_join (thr);
  xpthread_barrier_destroy (&barrier);
  return 0;
}

#include <support/test-driver.c>
