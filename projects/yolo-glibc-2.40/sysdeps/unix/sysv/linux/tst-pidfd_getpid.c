/* Specific tests for Linux pidfd_getpid.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
#include <sched.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <support/test-driver.h>
#include <sys/pidfd.h>
#include <sys/wait.h>
#include <sys/mount.h>

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
    if (errno == EPERM)
      FAIL_UNSUPPORTED ("kernel does not allow pidfd_getfd, skipping test");
  }

  /* Check if pidfd_getpid returns EREMOTE for process not in current
     namespace.  */
  {
    pid_t child0 = xfork ();
    TEST_VERIFY_EXIT (child0 >= 0);
    if (child0 == 0)
      {
	/* Create another unrelated descriptor, so child2 will inherit the
	   file descriptor.  */
	pid_t child1 = xfork ();
	TEST_VERIFY_EXIT (child1 >= 0);
        if (child1 == 0)
	  _exit (0);
	int child1_pidfd = pidfd_open (child1, 0);
	TEST_VERIFY_EXIT (child1_pidfd != -1);

	if (unshare (CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWPID) < 0)
	  {
	    /* Older kernels may not support all the options, or security
	       policy may block this call.  */
	    if (errno == EINVAL || errno == EPERM
	        || errno == ENOSPC || errno == EACCES)
	      exit (EXIT_UNSUPPORTED);
	    FAIL_EXIT1 ("unshare user/fs/pid failed: %m");
	  }

	if (mount (NULL, "/", NULL, MS_REC | MS_PRIVATE, 0) != 0)
	  {
	    /* This happens if we're trying to create a nested container,
	       like if the build is running under podman, and we lack
	       priviledges.  */
	    if (errno  == EPERM)
	      _exit (EXIT_UNSUPPORTED);
	    else
	      _exit (EXIT_FAILURE);
	  }

	pid_t child2 = xfork ();
	if (child2 > 0)
	  {
	    int status;
	    xwaitpid (child2, &status, 0);
	    TEST_VERIFY (WIFEXITED (status));
	    xwaitpid (child1, &status, 0);
	    TEST_VERIFY (WIFEXITED (status));

	    _exit (WEXITSTATUS (status));
	  }

	/* Now that we're pid 1 (effectively "root") we can mount /proc  */
	if (mount ("proc", "/proc", "proc", 0, NULL) != 0)
	  {
	    if (errno == EPERM)
	      _exit (EXIT_UNSUPPORTED);
	    else
	      _exit (EXIT_FAILURE);
	  }

	TEST_COMPARE (pidfd_getpid (child1_pidfd), -1);
	TEST_COMPARE (errno, EREMOTE);

	_exit (EXIT_SUCCESS);
      }
      int child0_pidfd = pidfd_open (child0, 0);
      TEST_VERIFY_EXIT (child0_pidfd != -1);

      pid_t child0pid = pidfd_getpid (child0_pidfd);

      siginfo_t info;
      TEST_COMPARE (waitid (P_PIDFD, child0_pidfd, &info, WEXITED), 0);
      if (info.si_status == EXIT_UNSUPPORTED)
	FAIL_UNSUPPORTED ("unable to unshare user/fs/pid");
      TEST_COMPARE (info.si_status, 0);
      TEST_COMPARE (info.si_code, CLD_EXITED);
      TEST_COMPARE (info.si_pid, child0pid);
  }

  return 0;
}

#include <support/test-driver.c>
