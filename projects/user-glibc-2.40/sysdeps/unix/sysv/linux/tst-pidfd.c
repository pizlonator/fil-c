/* Basic tests for Linux pidfd interfaces.
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
#include <fcntl.h>
#include <limits.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/process_state.h>
#include <support/support.h>
#include <support/xsignal.h>
#include <support/xunistd.h>
#include <support/xsocket.h>
#include <sys/pidfd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>

#define REMOTE_PATH "/dev/null"

/* The pair of sockets used for coordination.  The subprocess uses
   sockets[1].  */
static int sockets[2];

static pid_t ppid;
static uid_t puid;

static void
sighandler (int sig)
{
}

static void
subprocess (void)
{
  xsignal (SIGUSR1, sighandler);
  xsignal (SIGUSR2, sighandler);

  /* Check first pidfd_send_signal with default NULL siginfo_t argument.  */
  {
    sigset_t set;
    sigemptyset (&set);
    sigaddset (&set, SIGUSR1);
    siginfo_t info;
    TEST_COMPARE (sigtimedwait (&set, &info, NULL), SIGUSR1);
    TEST_COMPARE (info.si_signo, SIGUSR1);
    TEST_COMPARE (info.si_errno, 0);
    TEST_COMPARE (info.si_code, SI_USER);
    TEST_COMPARE (info.si_pid, ppid);
    TEST_COMPARE (info.si_uid, puid);
  }

  /* Check second pidfd_send_signal with crafted siginfo_t argument.  */
  {
    sigset_t set;
    sigemptyset (&set);
    sigaddset (&set, SIGUSR2);
    siginfo_t info;
    TEST_COMPARE (sigtimedwait (&set, &info, NULL), SIGUSR2);
    TEST_COMPARE (info.si_signo, SIGUSR2);
    TEST_COMPARE (info.si_errno, EAGAIN);
    TEST_COMPARE (info.si_code, -10);
    TEST_COMPARE (info.si_pid, ppid);
    TEST_COMPARE (info.si_uid, puid);
  }

  /* Send a local file descriptor value to check pidfd_getfd.  */
  int remote_fd = xopen (REMOTE_PATH, O_WRONLY | O_CLOEXEC, 0);
  xsendto (sockets[1], &remote_fd, sizeof (remote_fd), 0, NULL, 0);

  /* Wait for final pidfd_send_signal.  */
  pause ();

  _exit (0);
}

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

  ppid = getpid ();
  puid = getuid ();

  /* Sanity check for invalid inputs.  */
  TEST_COMPARE (pidfd_getpid (-1), -1);
  TEST_COMPARE (errno, EBADF);

  {
    pid_t pid = pidfd_getpid (STDOUT_FILENO);
    TEST_COMPARE (pid, -1);
    TEST_COMPARE (errno, EBADF);
  }

  /* Check if pidfd_getpid returns ESRCH for exited subprocess.  */
  {
    pid_t pidfork = xfork ();
    if (pidfork == 0)
      _exit (EXIT_SUCCESS);
    int pidfork_pidfd = pidfd_open (pidfork, 0);

    /* The process might be still running or already in zombie state, in
       either case the PID is still allocated to the process.  */
    pid_t pid = pidfd_getpid (pidfork_pidfd);
    TEST_COMPARE (pidfork, pid);
    if (pid > 0)
      support_process_state_wait (pid, support_process_state_zombie);

    siginfo_t info;
    TEST_COMPARE (waitid (P_PIDFD, pidfork_pidfd, &info, WEXITED), 0);
    TEST_COMPARE (info.si_pid, pidfork);
    TEST_COMPARE (info.si_status, 0);
    TEST_COMPARE (info.si_code, CLD_EXITED);

    /* Once the process is reaped the associated PID is not available.  */
    pid = pidfd_getpid (pidfork_pidfd);
    TEST_COMPARE (pid, -1);
    TEST_COMPARE (errno, ESRCH);

    xclose (pidfork_pidfd);
  }

  TEST_COMPARE (socketpair (AF_UNIX, SOCK_STREAM, 0, sockets), 0);

  pid_t pid = xfork ();
  if (pid == 0)
    {
      xclose (sockets[0]);
      subprocess ();
    }
  xclose (sockets[1]);

  TEST_COMPARE (pidfd_open (-1, 0), -1);
  TEST_COMPARE (errno, EINVAL);

  int pidfd = pidfd_open (pid, 0);
  TEST_VERIFY (pidfd != -1);

  TEST_COMPARE (pidfd_getpid (INT_MAX), -1);
  {
    pid_t querypid = pidfd_getpid (pidfd);
    TEST_COMPARE (querypid, pid);
  }

  /* Wait for first sigtimedwait.  */
  support_process_state_wait (pid, support_process_state_sleeping);
  TEST_COMPARE (pidfd_send_signal (pidfd, SIGUSR1, NULL, 0), 0);

  /* Wait for second sigtimedwait.  */
  support_process_state_wait (pid, support_process_state_sleeping);
  {
    siginfo_t info =
      {
	.si_signo = SIGUSR2,
	.si_errno = EAGAIN,
	.si_code = -10,
	.si_pid = ppid,
	.si_uid = puid
      };
    TEST_COMPARE (pidfd_send_signal (pidfd, SIGUSR2, &info, 0), 0);
  }

  /* Get remote file descriptor to check for pidfd_getfd.  */
  {
    int remote_fd;
    xrecvfrom (sockets[0], &remote_fd, sizeof (remote_fd), 0, NULL, 0);

    int fd = pidfd_getfd (pidfd, remote_fd, 0);
    /* pidfd_getfd may fail with EPERM if the process does not have
       PTRACE_MODE_ATTACH_REALCREDS permissions. This means the call
       may be denied if the process doesn't have CAP_SYS_PTRACE or
       if a LSM security_ptrace_access_check denies access.  */
    if (fd == -1 && errno == EPERM)
      {
	TEST_COMPARE (pidfd_send_signal (pidfd, SIGKILL, NULL, 0), 0);
	FAIL_UNSUPPORTED ("don't have permission to use pidfd_getfd on pidfd, "
			  "skipping test");
      }
    TEST_VERIFY (fd > 0);

    char *path = xasprintf ("/proc/%d/fd/%d", pid, remote_fd);
    char *resolved = xreadlink (path);
    TEST_COMPARE_STRING (resolved, REMOTE_PATH);

    int remote_fd_mode = fcntl (fd, F_GETFL);
    TEST_VERIFY (remote_fd_mode != -1);
    TEST_VERIFY (remote_fd_mode & O_WRONLY);

    int remote_fd_flags = fcntl (fd, F_GETFD);
    TEST_VERIFY (remote_fd_flags != -1);
    TEST_VERIFY (remote_fd_flags & FD_CLOEXEC);
  }

  TEST_COMPARE (pidfd_send_signal (pidfd, SIGKILL, NULL, 0), 0);
  {
    siginfo_t info;
    int r = waitid (P_PIDFD, pidfd, &info, WEXITED);
    TEST_COMPARE (r, 0);
    TEST_COMPARE (info.si_status, SIGKILL);
    TEST_COMPARE (info.si_code, CLD_KILLED);
  }

  return 0;
}

#include <support/test-driver.c>
