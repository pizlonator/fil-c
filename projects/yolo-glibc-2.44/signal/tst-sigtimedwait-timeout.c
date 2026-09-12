/* Test that sigtimedwait time out cleans up correctly for further signaling
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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
#include <stdio.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <time.h>
#include <unistd.h>

static int signaled;

/* Handler for SIGUSR1.  */
static void
sigusr1_handler (int signo)
{
  TEST_VERIFY (signo == SIGUSR1);
  signaled++;
}

/* Spawn a subprocess to send two SIGUSR1 signals.
   Return the PID of the process.  */
static pid_t
signal_sender (void)
{
  pid_t pid = xfork ();
  if (pid == 0)
    {
      static const struct timespec delay = { .tv_sec = 1 };
      if (nanosleep (&delay, NULL) != 0)
        FAIL_EXIT1 ("nanosleep: %m");
      if (kill (getppid (), SIGUSR1) != 0)
        FAIL_EXIT1 ("kill (SIGUSR1): %m");
      if (nanosleep (&delay, NULL) != 0)
        FAIL_EXIT1 ("nanosleep: %m");
      if (kill (getppid (), SIGUSR1) != 0)
        FAIL_EXIT1 ("kill (SIGUSR1): %m");
      _exit (0);
    }
  return pid;
}

static int
do_test (void)
{
  if (signal (SIGUSR1, sigusr1_handler) == SIG_ERR)
    FAIL_EXIT1 ("signal (SIGUSR1): %m\n");

  sigset_t sigs;
  sigemptyset (&sigs);
  sigaddset (&sigs, SIGUSR1);
  if (sigprocmask (SIG_BLOCK, &sigs, NULL) != 0)
    FAIL_EXIT1 ("sigprocmask (SIGBLOCK, SIGUSR1): %m");
  pid_t pid = signal_sender ();

  siginfo_t info;
  struct timespec ts = { .tv_nsec = 500000000 };
  int ret = sigtimedwait (&sigs, &info, &ts);
  TEST_VERIFY (ret == -1);
  TEST_VERIFY (errno == EAGAIN);

  static const struct timespec delay = { .tv_sec = 1 };
  if (nanosleep (&delay, NULL) != 0)
    FAIL_EXIT1 ("nanosleep: %m");
  TEST_VERIFY (signaled == 0);

  if (sigprocmask (SIG_UNBLOCK, &sigs, NULL) != 0)
    FAIL_EXIT1 ("sigprocmask (SIGBLOCK, SIGUSR1): %m");
  TEST_VERIFY (signaled == 1);

  if (nanosleep (&delay, NULL) != -1)
    FAIL_EXIT1 ("nanosleep: %m");
  TEST_VERIFY (errno == EINTR);
  TEST_VERIFY (signaled == 2);

  int status;
  xwaitpid (pid, &status, 0);
  TEST_VERIFY (status == 0);

  return 0;
}

#include <support/test-driver.c>
