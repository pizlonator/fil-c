/* Test if a syscall is correctly restarted.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <support/xsignal.h>
#include <support/check.h>
#include <support/process_state.h>
#include <support/xunistd.h>
#include <support/xthread.h>
#include <sys/wait.h>

static int
check_pid (pid_t pid)
{
  /* Wait until the child has called pause and it blocking on kernel.  */
  support_process_state_wait (pid, support_process_state_sleeping);

  TEST_COMPARE (kill (pid, SIGSTOP), 0);

  /* Adding process_state_tracing_stop ('t') allows the test to work under
     trace programs such as ptrace.  */
  support_process_state_wait (pid, support_process_state_stopped
				   | support_process_state_tracing_stop);

  TEST_COMPARE (kill (pid, SIGCONT), 0);

  enum support_process_state state
    = support_process_state_wait (pid, support_process_state_sleeping
				       | support_process_state_zombie);

  TEST_COMPARE (state, support_process_state_sleeping);

  TEST_COMPARE (kill (pid, SIGTERM), 0);

  siginfo_t info;
  TEST_COMPARE (waitid (P_PID, pid, &info, WEXITED), 0);
  TEST_COMPARE (info.si_signo, SIGCHLD);
  TEST_COMPARE (info.si_code, CLD_KILLED);
  TEST_COMPARE (info.si_status, SIGTERM);
  TEST_COMPARE (info.si_pid, pid);

  return 0;
}

static void *
tf (void *closure)
{
  pause ();
  return NULL;
}

static void
child_mt (void)
{
  /* Let only the created thread to handle signals.  */
  sigset_t set;
  sigfillset (&set);
  xpthread_sigmask (SIG_BLOCK, &set, NULL);

  sigdelset (&set, SIGSTOP);
  sigdelset (&set, SIGCONT);
  sigdelset (&set, SIGTERM);

  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  TEST_COMPARE (pthread_attr_setsigmask_np (&attr, &set), 0);

  xpthread_join (xpthread_create (&attr, tf, NULL));
}

static void
do_test_syscall (bool multithread)
{
  pid_t pid = xfork ();
  if (pid == 0)
    {
      if (multithread)
	child_mt ();
      else
	pause ();
      _exit (127);
    }

  check_pid (pid);
}

static int
do_test (void)
{
  /* Check for both single and multi thread, since they use different syscall
     mechanisms.  */
  do_test_syscall (false);
  do_test_syscall (true);

  return 0;
}

#include <support/test-driver.c>
