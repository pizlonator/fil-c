/* Test that pthread_kill succeeds during thread exit.
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

/* This test verifies that pthread_kill for a thread that is exiting
   succeeds (with or without actually delivering the signal).  */

#include <array_length.h>
#include <stdbool.h>
#include <stddef.h>
#include <support/xsignal.h>
#include <support/xthread.h>
#include <unistd.h>

/* Set to true by timeout_thread_function when the test should
   terminate.  */
static bool timeout;

static void *
timeout_thread_function (void *unused)
{
  usleep (1000 * 1000);
  __atomic_store_n (&timeout, true, __ATOMIC_RELAXED);
  return NULL;
}

/* Used to synchronize the sending threads with the target thread and
   main thread.  */
static pthread_barrier_t barrier_1;
static pthread_barrier_t barrier_2;

/* The target thread to which signals are to be sent.  */
static pthread_t target_thread;

/* Set by the main thread to true after timeout has been set to
   true.  */
static bool exiting;

static void *
sender_thread_function (void *unused)
{
  while (true)
    {
      /* Wait until target_thread has been initialized.  The target
         thread and main thread participate in this barrier.  */
      xpthread_barrier_wait (&barrier_1);

      if (exiting)
        break;

      xpthread_kill (target_thread, SIGUSR1);

      /* Communicate that the signal has been sent.  The main thread
         participates in this barrier.  */
      xpthread_barrier_wait (&barrier_2);
    }
  return NULL;
}

static void *
target_thread_function (void *unused)
{
  target_thread = pthread_self ();
  xpthread_barrier_wait (&barrier_1);
  return NULL;
}

static int
do_test (void)
{
  xsignal (SIGUSR1, SIG_IGN);

  pthread_t thr_timeout = xpthread_create (NULL, timeout_thread_function, NULL);

  pthread_t threads[4];
  xpthread_barrier_init (&barrier_1, NULL, array_length (threads) + 2);
  xpthread_barrier_init (&barrier_2, NULL, array_length (threads) + 1);

  for (int i = 0; i < array_length (threads); ++i)
    threads[i] = xpthread_create (NULL, sender_thread_function, NULL);

  while (!__atomic_load_n (&timeout, __ATOMIC_RELAXED))
    {
      xpthread_create (NULL, target_thread_function, NULL);

      /* Wait for the target thread to be set up and signal sending to
         start.  */
      xpthread_barrier_wait (&barrier_1);

      /* Wait for signal sending to complete.  */
      xpthread_barrier_wait (&barrier_2);

      xpthread_join (target_thread);
    }

  exiting = true;

  /* Signal the sending threads to exit.  */
  xpthread_create (NULL, target_thread_function, NULL);
  xpthread_barrier_wait (&barrier_1);

  for (int i = 0; i < array_length (threads); ++i)
    xpthread_join (threads[i]);
  xpthread_join (thr_timeout);

  return 0;
}

#include <support/test-driver.c>
