/* Test for race during FILE initialization in _IO_new_file_init_internal.
   Copyright The GNU Toolchain Authors.
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
#include <time.h>

#include <support/check.h>
#include <support/xthread.h>

static atomic_bool stop = ATOMIC_VAR_INIT (0);

static void *
opener_thread (__attribute__ ((unused)) void *arg)
{
  while (!atomic_load_explicit (&stop, memory_order_acquire))
    {
      FILE *fp = fopen ("/dev/null", "r");
      if (fp == NULL)
        FAIL_EXIT1 ("fopen: %m");
      if (fclose (fp) != 0)
        FAIL_EXIT1 ("fclose: %m");
    }
  return NULL;
}

static void *
flusher_thread (__attribute__ ((unused)) void *arg)
{
  while (!atomic_load_explicit (&stop, memory_order_acquire))
    fflush (NULL);
  return NULL;
}

static int
do_test (void)
{
  pthread_t t1 = xpthread_create (NULL, opener_thread, NULL);
  pthread_t t2 = xpthread_create (NULL, flusher_thread, NULL);

  struct timespec ts = { .tv_sec = 3, .tv_nsec = 0 };
  nanosleep (&ts, NULL);

  atomic_store_explicit (&stop, 1, memory_order_release);

  xpthread_join (t1);
  xpthread_join (t2);

  return 0;
}

#define TIMEOUT 30
#include <support/test-driver.c>
