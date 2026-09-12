/* Test getenv with concurrent unsetenv.
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
#include <support/check.h>
#include <support/support.h>
#include <support/xthread.h>

/* Used to synchronize the start of each test iteration.  */
static pthread_barrier_t barrier;

/* Number of iterations.  */
enum { iterations = 10000 };

/* Check that even with concurrent unsetenv, a variable that is known
   to be there is found.  */
static void *
getenv_thread (void *ignored)
{
  for (int i = 0; i < iterations; ++i)
    {
      xpthread_barrier_wait (&barrier);
      TEST_COMPARE_STRING (getenv ("variable"), "value");
      xpthread_barrier_wait (&barrier);
    }
  return NULL;
}

static int
do_test (void)
{
  xpthread_barrier_init (&barrier, NULL, 2);
  pthread_t thr = xpthread_create (NULL, getenv_thread, NULL);

  char *variables[50];
  for (int i = 0; i < array_length (variables); ++i)
    variables[i] = xasprintf ("V%d", i);

  for (int i = 0; i < iterations; ++i)
    {
      clearenv ();
      for (int j = 0; j < array_length (variables); ++j)
        TEST_COMPARE (setenv (variables[j], variables[j] + 1, 1), 0);
      TEST_COMPARE (setenv ("variable", "value", 1), 0);
      xpthread_barrier_wait (&barrier);
      /* Test runs.  */
      for (int j = 0; j < array_length (variables); ++j)
        TEST_COMPARE (unsetenv (variables[j]), 0);
      xpthread_barrier_wait (&barrier);
    }
  xpthread_join (thr);
  xpthread_barrier_destroy (&barrier);
  for (int i = 0; i < array_length (variables); ++i)
    free (variables[i]);
  return 0;
}

#include <support/test-driver.c>
