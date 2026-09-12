/* AArch64 test for GCS for creating child process using fork
   with ptrace to check locked GCS operations.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include "tst-gcs-helper.h"

#include <support/xunistd.h>
#include <support/xsignal.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* Uapi struct for PTRACE_GETREGSET with NT_ARM_GCS.  */
struct user_gcs
{
  uint64_t enabled;
  uint64_t locked;
  uint64_t gcspr_el0;
};

static int
target (void)
{
  /* This signal is raised after the process has started
     and has been initialised so we can ptrace it at this
     point and obtain GCS locked features.  */
  xraise (SIGUSR1);
  return 0;
}

static void
fork_target (char *args[], uint64_t aarch64_gcs)
{
  /* Currently kernel returns only lower 32 bits of locked
     features so we only compare them.  */
  bool lock_gcs = aarch64_gcs != 0 && aarch64_gcs != 2;
  uint64_t expected_locked = lock_gcs ? 0xfffffffful : 0ul;
  pid_t pid = xfork ();
  if (pid == 0)
    {
      char tunables[90];
      snprintf (tunables, sizeof (tunables), "GLIBC_TUNABLES="
		"glibc.cpu.aarch64_gcs=0x%016lx", aarch64_gcs);
      char *envp[] = { tunables, NULL };
      /* We need to ptrace child process to use PTRACE_GETREGSET
	 with NT_ARM_GCS after it has started.  */
      int res = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      if (res != 0)
	FAIL_EXIT1 ("ptrace: %m");
      execve (args[0], args, envp);
      FAIL_EXIT1 ("execve: %m");
    }
  bool checked = false;
  while (true)
    {
      int status;
      xwaitpid (pid, &status, 0);
      if (WIFSTOPPED (status))
	{
	  /* Child stopped by signal.  */
	  int sig = WSTOPSIG (status);
	  if (sig == SIGUSR1)
	    {
	      struct user_gcs ugcs = {};
	      struct iovec io;
	      io.iov_base = &ugcs;
	      io.iov_len = sizeof (struct user_gcs);
	      if (ptrace (PTRACE_GETREGSET, pid, NT_ARM_GCS, &io))
		FAIL_EXIT1 ("ptrace (PTRACE_GETREGSET): %m");
	      printf ("expected vs locked: %016lx %016lx\n",
		      expected_locked, ugcs.locked);
	      if (lock_gcs)
		TEST_VERIFY_EXIT (ugcs.enabled);
	      TEST_VERIFY_EXIT (ugcs.locked == expected_locked);
	      if (aarch64_gcs != 0)
		TEST_VERIFY_EXIT ((void *) ugcs.gcspr_el0 != NULL);
	      checked = true;
	    }
        }
      else if (WIFSIGNALED (status))
	{
	  /* Child terminated by signal.  */
	  break;
	}
      else if (WIFEXITED (status))
	{
          /* Child terminated by normally.  */
	  break;
	}
      ptrace (PTRACE_CONT, pid, 0, 0);
    }
  /* If child process hasn't run correctly, this will remain false.  */
  TEST_VERIFY_EXIT (checked);
}

int main(int argc, char *argv[])
{
  /* Check if GCS could possible by enabled.  */
  if (!(getauxval (AT_HWCAP) & HWCAP_GCS))
    FAIL_UNSUPPORTED ("kernel or CPU does not support GCS");

  /* GCS should be enabled for this test.  */
  TEST_VERIFY (__check_gcs_status ());

  /* If last argument is 'target', we just run target code.  */
  if (strcmp (argv[argc - 1], "target") == 0)
    return target ();

  /* In parent, we should at least have 3 arguments.  */
  if (argc < 3)
    FAIL_EXIT1 ("wrong number of arguments: %d", argc);

  char *child_args[] = { NULL, NULL, NULL, NULL, NULL , NULL };

  /* Check command line arguments to construct child command.  */
  if (strcmp (argv[0], argv[2]) == 0)
    {
      /* Command looks like
	 /path/to/test -- /path/to/test  */
      /* /path/to/test  */
      child_args[0] = argv[0];
      /* Extra argument for the child process.  */
      child_args[1] = (char *)"target";
    }
  else
    {
      /* Command looks like
	 /path/to/test -- /path/to/ld.so ...  */
      TEST_VERIFY_EXIT (argc > 5);
      TEST_COMPARE_STRING (argv[3], "--library-path");
      /* /path/to/ld-linux-aarch64.so.1  */
      child_args[0] = argv[2];
      /* --library-path  */
      child_args[1] = argv[3];
      /* Library path...  */
      child_args[2] = argv[4];
      /* /path/to/test  */
      child_args[3] = argv[5];
      /* Extra argument for the child process.  */
      child_args[4] = (char *)"target";
    }

  /* Check all 4 values for the aarch64_gcs tunable.  */
  for (uint64_t aarch64_gcs = 0; aarch64_gcs < 4; aarch64_gcs++)
    fork_target (child_args, aarch64_gcs);
  return 0;
}
