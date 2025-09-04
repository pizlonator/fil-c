/* Basic tests for Linux process_madvise.
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

#include <array_length.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/process_state.h>
#include <support/support.h>
#include <support/xsocket.h>
#include <support/xunistd.h>
#include <sys/mman.h>
#include <sys/pidfd.h>
#include <sys/wait.h>

/* The pair of sockets used for coordination.  The subprocess uses
   sockets[1].  */
static int sockets[2];

static long int page_size;

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

  void *p1 = xmmap (NULL, page_size * 2, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1);

  void *p2 = xmmap (NULL, page_size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1);
  xmunmap(p2, page_size);

  xsendto (sockets[1], &(struct iovec) { p1, page_size * 2 },
	   sizeof (struct iovec), 0, NULL, 0);

  xsendto (sockets[1], &(struct iovec) { p2, page_size },
	   sizeof (struct iovec), 0, NULL, 0);

  pause ();

  _exit (0);
}

static int
do_test (void)
{
  page_size = sysconf (_SC_PAGE_SIZE);

  {
    int r = pidfd_open (-1, 0);
    TEST_COMPARE (r, -1);
    if (errno == ENOSYS)
      FAIL_UNSUPPORTED ("kernel does not support pidfd_open, skipping test");

    TEST_COMPARE (errno, EINVAL);
  }

  TEST_COMPARE (socketpair (AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  pid_t pid = xfork ();
  if (pid == 0)
    {
      xclose (sockets[0]);
      subprocess ();
    }
  xclose (sockets[1]);

  int pidfd = pidfd_open (pid, 0);
  TEST_VERIFY (pidfd != -1);

  /* The target process is going to send us two iovec's.  The first one points
     to a valid mapping, the other points to a previously valid mapping which
     has now been unmapped.  */
  {
    struct iovec iv;
    xrecvfrom (sockets[0], &iv, sizeof (iv), 0, NULL, 0);

    /* We expect this to succeed in the target process because the mapping
       is valid.  */
    ssize_t ret = process_madvise (pidfd, &iv, 1, MADV_COLD, 0);
    if (ret == -1 && errno == ENOSYS)
      FAIL_UNSUPPORTED ("kernel does not support process_madvise, skipping"
			"test");
    TEST_COMPARE (ret, 2 * page_size);
  }

  {
    struct iovec iv;
    xrecvfrom (sockets[0], &iv, sizeof (iv), 0, NULL, 0);

    /* We expect this to fail in the target process because the second iovec
       points to an unmapped region.  The target process arranges for this to
       be the case.  */
    TEST_COMPARE (process_madvise (pidfd, &iv, 1, MADV_COLD, 0), -1);
    TEST_COMPARE (errno, ENOMEM);
  }

  {
    struct iovec iv[IOV_MAX + 1];
    TEST_COMPARE (process_madvise (pidfd, iv, array_length (iv), MADV_COLD,
				   0), -1);
    TEST_COMPARE (errno, EINVAL);
  }

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
