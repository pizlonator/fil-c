/* Basic tests for Linux epoll_* wrappers.
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
#include <intprops.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xsignal.h>
#include <support/xunistd.h>
#include <support/process_state.h>
#include <stdlib.h>
#include <sys/epoll.h>

/* The test focus on checking if the timeout argument is correctly handled
   by glibc wrappers.  */

static void
handler (int sig)
{
}

typedef int (*epoll_wait_check_t) (int, struct epoll_event *, int,
				   int, const sigset_t *);

static void
test_epoll_basic (epoll_wait_check_t epoll_wait_check)
{
  {
    sigset_t ss_usr1;
    sigemptyset (&ss_usr1);
    sigaddset (&ss_usr1, SIGUSR1);
    TEST_COMPARE (sigprocmask (SIG_BLOCK, &ss_usr1, NULL), 0);
  }

  int fds[2][2];
  xpipe (fds[0]);
  xpipe (fds[1]);

  sigset_t ss;
  TEST_COMPARE (sigprocmask (SIG_SETMASK, NULL, &ss), 0);
  sigdelset (&ss, SIGUSR1);

  int efd = epoll_create1 (0);
  TEST_VERIFY_EXIT (efd != -1);

  struct epoll_event event;

  event.data.fd = fds[1][0];
  event.events = EPOLLIN | EPOLLET;
  TEST_COMPARE (epoll_ctl (efd, EPOLL_CTL_ADD, fds[1][0], &event), 0);

  pid_t parent = getpid ();
  pid_t p = xfork ();
  if (p == 0)
    {
      xclose (fds[0][1]);
      xclose (fds[1][0]);

      event.data.fd = fds[0][0];
      event.events = EPOLLIN | EPOLLET;
      TEST_COMPARE (epoll_ctl (efd, EPOLL_CTL_ADD, fds[0][0], &event), 0);

      int e;
      do
	{
	  if (getppid () != parent)
	    FAIL_EXIT1 ("getppid()=%d != parent=%d", getppid(), parent);

	  errno = 0;
	  e = epoll_wait_check (efd, &event, 1, 500, &ss);
	}
      while (e == 0);

      TEST_COMPARE (e, -1);
      TEST_COMPARE (errno, EINTR);

      TEMP_FAILURE_RETRY (write (fds[1][1], "foo", 3));

      exit (0);
    }

  xclose (fds[0][0]);
  xclose (fds[1][1]);

  /* Wait until child is blocked on epoll_wait.  */
  support_process_state_wait (p, support_process_state_sleeping);
  TEST_COMPARE (kill (p, SIGUSR1), 0);

  int e = epoll_wait_check (efd, &event, 1, 500000000, &ss);
  TEST_COMPARE (e, 1);
  TEST_VERIFY (event.events & EPOLLIN);

  xclose (fds[0][1]);
  xclose (fds[1][0]);
  xclose (efd);
}


static void
test_epoll_large_timeout (epoll_wait_check_t epoll_wait_check)
{
  timer_t t = support_create_timer (0, 100000000, true, NULL);

  int fds[2];
  xpipe (fds);

  fd_set rfds;
  FD_ZERO (&rfds);
  FD_SET (fds[0], &rfds);

  sigset_t ss;
  TEST_COMPARE (sigprocmask (SIG_SETMASK, NULL, &ss), 0);
  sigdelset (&ss, SIGALRM);

  int efd = epoll_create1 (0);
  TEST_VERIFY_EXIT (efd != -1);

  struct epoll_event event;
  event.data.fd = fds[0];
  event.events = EPOLLIN | EPOLLET;
  TEST_COMPARE (epoll_ctl (efd, EPOLL_CTL_ADD, fds[0], &event), 0);

  int tmo = TYPE_MAXIMUM (int);
  TEST_COMPARE (epoll_wait_check (efd, &event, 1, tmo, &ss), -1);
  TEST_VERIFY (errno == EINTR || errno == EOVERFLOW);

  TEST_COMPARE (epoll_wait_check (efd, &event, 1, -1, &ss), -1);
  TEST_VERIFY (errno == EINTR || errno == EOVERFLOW);

  support_delete_timer (t);

  xclose (fds[0]);
  xclose (fds[1]);
  xclose (efd);
}


static int
epoll_wait_check (int epfd, struct epoll_event *ev, int maxev, int tmo,
		  const sigset_t *ss)
{
  sigset_t orig;
  TEST_COMPARE (sigprocmask (SIG_SETMASK, ss, &orig), 0);
  int r = epoll_wait (epfd, ev, maxev, tmo);
  TEST_COMPARE (sigprocmask (SIG_SETMASK, &orig, NULL), 0);
  return r;
}

static int
epoll_pwait_check (int epfd, struct epoll_event *ev, int maxev, int tmo,
		   const sigset_t *ss)
{
  return epoll_pwait (epfd, ev, maxev, tmo, ss);
}

static int
epoll_pwait2_check (int epfd, struct epoll_event *ev, int maxev, int tmo,
		    const sigset_t *ss)
{
  time_t s = tmo == -1 ? TYPE_MAXIMUM (time_t) : tmo / 1000;
  long int ns = tmo == -1 ? 0 : (tmo % 1000) * 1000000;
  return epoll_pwait2 (epfd, ev, maxev, &(struct timespec) { s, ns }, ss);
}

static int
do_test (void)
{
  struct epoll_event ev;

  {
    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);
    xsigaction (SIGUSR1, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    xsigaction (SIGCHLD, &sa, NULL);
  }

  int r = epoll_pwait2 (-1, &ev, 0, NULL, NULL);
  TEST_COMPARE (r, -1);
  bool pwait2_supported = errno != ENOSYS;

  test_epoll_basic (epoll_wait_check);
  test_epoll_basic (epoll_pwait_check);
  if (pwait2_supported)
    test_epoll_basic (epoll_pwait2_check);

  test_epoll_large_timeout (epoll_wait_check);
  test_epoll_large_timeout (epoll_pwait_check);
  if (pwait2_supported)
    test_epoll_large_timeout (epoll_pwait2_check);

  return 0;
}

#include <support/test-driver.c>
