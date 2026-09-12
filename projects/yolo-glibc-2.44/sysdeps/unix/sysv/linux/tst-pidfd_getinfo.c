/* Basic tests for Linux PID_GET_INFO interfaces.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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
#include <stdint.h>
#include <sys/pidfd.h>
#include <support/check.h>
#include <support/xunistd.h>

static int
do_test (void)
{
  {
    /* The pidfd_getfd syscall was the last in the set of pidfd related
       syscalls added to the kernel.  Use pidfd_getfd to decide if this
       kernel has pidfd support that we can test.  */
    int r = pidfd_getfd (0, 0, 1);
    TEST_VERIFY_EXIT (r == -1);
    if (errno == ENOSYS)
      FAIL_UNSUPPORTED ("kernel does not support pidfd_getfd, skipping test");
  }

  int pidfd = pidfd_open (getpid(), 0);
  TEST_VERIFY (pidfd >= 0);

  int pid = pidfd_getpid (pidfd);
  TEST_VERIFY (pid >= 0);

  struct pidfd_info info = {
    .mask = PIDFD_INFO_CGROUPID,
  };
  if (ioctl (pidfd, PIDFD_GET_INFO, &info) != 0)
    {
      if (errno == ENOTTY || errno == EINVAL)
	FAIL_UNSUPPORTED ("kernel does not support PIDFD_GET_INFO");
      else
	FAIL_EXIT1 ("ioctl (PIDFD_GET_INFO) failed: %m");
    }

  TEST_COMPARE (info.pid, pid);
  TEST_COMPARE (info.ppid, getppid ());
  TEST_COMPARE (info.ruid, getuid ());
  TEST_COMPARE (info.rgid, getgid ());
  TEST_COMPARE (info.euid, geteuid ());
  TEST_COMPARE (info.egid, getegid ());
  TEST_COMPARE (info.suid, geteuid ());
  TEST_COMPARE (info.sgid, getegid ());
  if (info.mask & PIDFD_INFO_CGROUPID)
    TEST_VERIFY (info.cgroupid != 0);

  xclose (pidfd);

  return 0;
}

#include <support/test-driver.c>
