/* Test pthread interface which should return ESRCH when issued
   with a terminated pthread_t.

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

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <grp.h>
#include <pwd.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xthread.h>
#include <unistd.h>

static void *
noop_thread (void *closure)
{
  return NULL;
}

enum { nthreads = 8 };

static int
do_test_default (void)
{
  pthread_t thrs[nthreads];
  for (int i = 0; i < nthreads; i++)
    thrs[i] = xpthread_create (NULL, noop_thread, NULL);

  support_wait_for_thread_exit ();

  for (int i = 0; i < nthreads; i++)
    {
      clockid_t clk;
      TEST_COMPARE (pthread_getcpuclockid (thrs[i], &clk), ESRCH);

      struct sched_param sch = { 0 };
      int policy;
      TEST_COMPARE (pthread_getschedparam (thrs[i], &policy, &sch), ESRCH);

      TEST_COMPARE (pthread_setschedparam (thrs[i], SCHED_FIFO, &sch), ESRCH);

      TEST_COMPARE (pthread_setschedprio (thrs[i], 0), ESRCH);
    }

  for (int i = 0; i < nthreads; i++)
    xpthread_join (thrs[i]);

  return 0;
}


static void *
detached_pause_thread (void *closure)
{
  pthread_detach (pthread_self ());
  pause ();
  return NULL;
}

static void
do_test_detached (void)
{
  pthread_t thrs[nthreads];
  for (int i = 0; i < nthreads; i++)
    thrs[i] = xpthread_create (NULL, detached_pause_thread, NULL);

  for (int i = 0; i < nthreads; i++)
    {
      clockid_t clk;
      TEST_COMPARE (pthread_getcpuclockid (thrs[i], &clk), 0);

      struct sched_param sch = { 0 };
      int policy;
      TEST_COMPARE (pthread_getschedparam (thrs[i], &policy, &sch), 0);

      sch.sched_priority = 8;
      TEST_COMPARE (pthread_setschedparam (thrs[i], SCHED_FIFO, &sch), EPERM);

      TEST_COMPARE (pthread_setschedprio (thrs[i], 0), 0);
    }
}

static int
do_test (void)
{
  /* The test relies on pthread setup failures that succeed as root.  */
  if (geteuid () == 0)
    {
      struct passwd *pwd = getpwnam ("nobody");
      if (pwd == NULL)
	FAIL_UNSUPPORTED ("iuser nobody doesn't exist");

      TEST_VERIFY_EXIT (setresuid (pwd->pw_uid, pwd->pw_uid, -1) == 0);
    }

  do_test_default ();
  do_test_detached ();

  return 0;
}

#include <support/test-driver.c>
