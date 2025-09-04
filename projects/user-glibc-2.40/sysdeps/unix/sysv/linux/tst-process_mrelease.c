/* Basic tests for Linux process_mrelease.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xsocket.h>
#include <support/xunistd.h>
#include <sys/mman.h>
#include <sys/pidfd.h>
#include <sys/wait.h>

static void
exit_subprocess (int dummy)
{
  exit (EXIT_FAILURE);
}

static void
subprocess (void)
{
  /* In case something goes wrong with parent before pidfd_send_signal.  */
  support_create_timer (5, 0, false, exit_subprocess);

  pause ();
  _exit (0);
}

static int
do_test (void)
{
  {
    int r = process_mrelease (-1, 0);
    TEST_COMPARE (r, -1);
    if (errno == ENOSYS)
      FAIL_UNSUPPORTED ("kernel does not support process_mrelease, "
		        "skipping test");
    TEST_COMPARE (errno, EBADF);
  }

  pid_t pid = xfork ();
  if (pid == 0)
    subprocess ();

  int pidfd = pidfd_open (pid, 0);
  TEST_VERIFY (pidfd != -1);

  /* The syscall only succeeds if the target process is exiting and there
     is no guarantee that calling if after pidfd_send_signal will not error
     (ince the process might have already been reaped by the OS).  So just
     check if it does fail when the process is stll running.  */
  TEST_COMPARE (process_mrelease (pidfd, 0), -1);
  TEST_COMPARE (errno, EINVAL);

  TEST_COMPARE (pidfd_send_signal (pidfd, SIGKILL, NULL, 0), 0);
  {
    siginfo_t info;
    int r = waitid (P_PIDFD, pidfd, &info, WEXITED);
    TEST_COMPARE (r, 0);
    TEST_COMPARE (info.si_status, SIGKILL);
    TEST_COMPARE (info.si_code, CLD_KILLED);
  }

  TEST_COMPARE (pidfd_send_signal (pidfd, SIGKILL, NULL, 0), -1);
  TEST_COMPARE (errno, ESRCH);

  return 0;
}

#include <support/test-driver.c>
