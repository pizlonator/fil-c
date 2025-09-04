/* Test that pthread_kill succeeds for an exited thread.
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

/* This test verifies that pthread_kill returns 0 (and not ESRCH) for
   a thread that has exited on the kernel side.  */

#include <stddef.h>
#include <support/support.h>
#include <support/xthread.h>

static void *
noop_thread (void *closure)
{
  return NULL;
}

static int
do_test (void)
{
  pthread_t thr = xpthread_create (NULL, noop_thread, NULL);

  support_wait_for_thread_exit ();

  xpthread_cancel (thr);
  xpthread_join (thr);

  return 0;
}

#include <support/test-driver.c>
