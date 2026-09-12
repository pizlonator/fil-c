/* Test concurrent popen and fork.
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

#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>

#include <support/check.h>
#include <support/xthread.h>
#include <support/xunistd.h>

static void
popen_and_pclose (void)
{
  FILE *f = popen ("true", "r");
  TEST_VERIFY_EXIT (f != NULL);
  pclose (f);
  return;
}

static atomic_bool done = ATOMIC_VAR_INIT (0);

static void *
popen_and_pclose_forever (__attribute__ ((unused))
                          void *arg)
{
  while (!atomic_load_explicit (&done, memory_order_acquire))
    popen_and_pclose ();
  return NULL;
}

static int
do_test (void)
{

  /* Repeatedly call popen in a loop during the entire test.  */
  pthread_t t = xpthread_create (NULL, popen_and_pclose_forever, NULL);

  /* Repeatedly fork off and reap child processes one-by-one.
     Each child calls popen once, then exits, leading to the possibility
     that a child forks *during* our own popen call, thus inheriting any
     intermediate popen state, possibly including lock state(s).  */
  for (int i = 0; i < 100; i++)
    {
      int cpid = xfork ();

      if (cpid == 0)
        {
          popen_and_pclose ();
          _exit (0);
        }
      else
        xwaitpid (cpid, NULL, 0);
    }

  /* Stop calling popen.  */
  atomic_store_explicit (&done, 1, memory_order_release);
  xpthread_join (t);

  return 0;
}

#include <support/test-driver.c>
