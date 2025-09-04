/* Test that setuid, pthread_create, thread exit do not deadlock (bug 28361).
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

#include <support/check.h>
#include <support/xthread.h>
#include <unistd.h>

/* How many threads to launch during each iteration.  */
enum { threads = 4 };

/* How many iterations to perform.  This value seems to reproduce
   bug 28361 in a bout one in three runs.  */
enum { iterations = 5000 };

/* Cache of the real user ID used by setuid_thread.  */
static uid_t uid;

/* Start routine for the threads.  */
static void *
setuid_thread (void *closure)
{
  TEST_COMPARE (setuid (uid), 0);
  return NULL;
}

static int
do_test (void)
{
  /* The setxid machinery is still invoked even if the UID is
     unchanged.  (The kernel might reset other credentials as part of
     the system call.)  */
  uid = getuid ();

  for (int i = 0; i < iterations; ++i)
    {
      pthread_t thread_ids[threads];
      for (int j = 0; j < threads; ++j)
        thread_ids[j] = xpthread_create (NULL, setuid_thread, NULL);
      for (int j = 0; j < threads; ++j)
        xpthread_join (thread_ids[j]);
    }

  return 0;
}

#include <support/test-driver.c>
