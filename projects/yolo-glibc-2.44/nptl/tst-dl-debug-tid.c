/* Test for thread ID logging in dynamic linker.
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

/* This test checks that the dynamic linker correctly logs thread creation
   and destruction. It creates a detached thread followed by a joinable
   thread to exercise different code paths. A barrier is used to ensure
   the detached thread has started before the joinable one is created,
   making the test more deterministic. The tst-dl-debug-tid.sh shell script
   wrapper then verifies the LD_DEBUG output.  */

#include <pthread.h>
#include <support/xthread.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <support/xdlfcn.h>
#include <support/check.h>

static void *
thread_function (void *arg)
{
  if (arg)
    pthread_barrier_wait ((pthread_barrier_t *) arg);

  /* Load a module with TLS to verify allocation/deallocation logs.  */
  void *h = xdlopen ("tst-tls-debug-mod.so", RTLD_NOW);

  /* Call a function that accesses TLS.  */
  int (*fp) (void) = (int (*) (void)) xdlsym (h, "in_dso");
  TEST_COMPARE (fp (), 0);

  xdlclose (h);

  return NULL;
}

static int
do_test (void)
{
  pthread_t thread1;
  pthread_attr_t attr;
  pthread_barrier_t barrier;

  pthread_barrier_init (&barrier, NULL, 2);

  /* A detached thread.
   * Deallocation is done by the thread itself upon exit.  */
  xpthread_attr_init (&attr);
  xpthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
  /* We don't need the thread handle for the detached thread.  */
  xpthread_create (&attr, thread_function, &barrier);
  xpthread_attr_destroy (&attr);

  /* Wait for the detached thread to be executed.  */
  pthread_barrier_wait (&barrier);
  pthread_barrier_destroy (&barrier);

  /* A joinable thread.
   * Deallocation is done by the main thread in pthread_join.  */
  thread1 = xpthread_create (NULL, thread_function, NULL);

  xpthread_join (thread1);

  return 0;
}

#include <support/test-driver.c>
