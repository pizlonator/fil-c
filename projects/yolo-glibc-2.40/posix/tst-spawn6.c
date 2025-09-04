/* Check posix_spawn set controlling terminal extension.
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
#include <fcntl.h>
#include <getopt.h>
#include <intprops.h>
#include <paths.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <termios.h>
#include <tst-spawn.h>

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif
static char ptmxpath[PATH_MAX];

static int
handle_restart (const char *argv1, const char *argv2)
{
  /* If process group is not changed (POSIX_SPAWN_SETPGROUP), then check
     the creating process one, otherwise check against the process group
     itself.  */
  pid_t pgrp;
  if (strcmp (argv1, "setgrpr") != 0)
    TEST_COMPARE (sscanf (argv1, "%d", &pgrp), 1);
  else
    {
      pgrp = getpgrp ();
      /* Check if a new process group was actually created.  */
      pid_t ppid = getppid ();
      pid_t pgid = getpgid (ppid);
      TEST_VERIFY (pgid != pgrp);
    }

  char *endptr;
  long int tcfd = strtol (argv2, &endptr, 10);
  if (*endptr != '\0' || tcfd > INT_MAX)
    FAIL_EXIT1 ("invalid file descriptor name: %s", argv2);
  if (tcfd != -1)
    {
      TEST_COMPARE (fcntl (tcfd, F_GETFD), -1);
      TEST_COMPARE (errno, EBADF);
    }

  int fd = xopen (_PATH_TTY, O_RDONLY, 0600);
  TEST_COMPARE (tcgetpgrp (fd), pgrp);
  xclose (fd);

  return 0;
}

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

static void
run_subprogram (int argc, char *argv[], const posix_spawnattr_t *attr,
		const posix_spawn_file_actions_t *actions, int tcfd,
		int exp_err)
{
  short int flags;
  TEST_COMPARE (posix_spawnattr_getflags (attr, &flags), 0);
  bool setpgrp = flags & POSIX_SPAWN_SETPGROUP;

  char *spargv[9];
  TEST_VERIFY_EXIT (((argc - 1) + 4) < array_length (spargv));
  char pgrp[INT_STRLEN_BOUND (pid_t)];
  char tcfdstr[INT_STRLEN_BOUND (int)];

  int i = 0;
  for (; i < argc - 1; i++)
    spargv[i] = argv[i + 1];
  spargv[i++] = (char *) "--direct";
  spargv[i++] = (char *) "--restart";
  if (setpgrp)
    spargv[i++] = (char *) "setgrpr";
  else
    {
      snprintf (pgrp, sizeof pgrp, "%d", getpgrp ());
      spargv[i++] = pgrp;
    }
  snprintf (tcfdstr, sizeof tcfdstr, "%d", tcfd);
  spargv[i++] = tcfdstr;
  spargv[i] = NULL;

  pid_t pid;
  TEST_COMPARE (POSIX_SPAWN (&pid, argv[1], actions, attr, spargv, environ),
		exp_err);
  if (exp_err != 0)
    return;

  siginfo_t sinfo;
  TEST_COMPARE (WAITID (P_ALL, 0, &sinfo, WEXITED), 0);
  TEST_COMPARE (sinfo.si_code, CLD_EXITED);
  TEST_COMPARE (sinfo.si_status, 0);
}

static int
run_test (int argc, char *argv[])
{
  /* We must have either:
     - four parameters left if called initially:
       + path to ld.so         optional
       + "--library-path"      optional
       + the library path      optional
       + the application name
     - six parameters left if called through re-execution:
       + --setgrpr             optional
   */

  int tcfd = xopen (ptmxpath, O_RDONLY, 0600);

  /* Check setting the controlling terminal without changing the group.  */
  {
    posix_spawnattr_t attr;
    TEST_COMPARE (posix_spawnattr_init (&attr), 0);
    posix_spawn_file_actions_t actions;
    TEST_COMPARE (posix_spawn_file_actions_init (&actions), 0);
    TEST_COMPARE (posix_spawn_file_actions_addtcsetpgrp_np (&actions, tcfd),
		  0);

    run_subprogram (argc, argv, &attr, &actions, -1, 0);
  }

  /* Check setting both the controlling terminal and the create a new process
     group.  */
  {
    posix_spawnattr_t attr;
    TEST_COMPARE (posix_spawnattr_init (&attr), 0);
    TEST_COMPARE (posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETPGROUP), 0);
    posix_spawn_file_actions_t actions;
    TEST_COMPARE (posix_spawn_file_actions_init (&actions), 0);
    TEST_COMPARE (posix_spawn_file_actions_addtcsetpgrp_np (&actions, tcfd),
		  0);

    run_subprogram (argc, argv, &attr, &actions, -1, 0);
  }

  /* Same as before, but check if the addclose file actions closes the terminal
     file descriptor.  */
  {
    posix_spawnattr_t attr;
    TEST_COMPARE (posix_spawnattr_init (&attr), 0);
    TEST_COMPARE (posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETPGROUP), 0);
    posix_spawn_file_actions_t actions;
    TEST_COMPARE (posix_spawn_file_actions_init (&actions), 0);
    TEST_COMPARE (posix_spawn_file_actions_addtcsetpgrp_np (&actions, tcfd),
		  0);
    TEST_COMPARE (posix_spawn_file_actions_addclose (&actions, tcfd), 0);

    run_subprogram (argc, argv, &attr, &actions, tcfd, 0);
  }

  /* Trying to set the controlling terminal after a setsid incurs in a ENOTTY
     from tcsetpgrp.  */
  {
    posix_spawnattr_t attr;
    TEST_COMPARE (posix_spawnattr_init (&attr), 0);
    TEST_COMPARE (posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETSID), 0);
    posix_spawn_file_actions_t actions;
    TEST_COMPARE (posix_spawn_file_actions_init (&actions), 0);
    TEST_COMPARE (posix_spawn_file_actions_addtcsetpgrp_np (&actions, tcfd),
		  0);

    run_subprogram (argc, argv, &attr, &actions, -1, ENOTTY);
  }

  xclose (tcfd);

  return 0;
}

static int
do_test (int argc, char *argv[])
{
  if (restart)
    return handle_restart (argv[1], argv[2]);

  pid_t pid = xfork ();
  if (pid == 0)
    {
      /* Create a pseudo-terminal to avoid interfering with the one using by
	 test itself, creates a new session (so there is no controlling
	 terminal), and set the pseudo-terminal as the controlling one.  */
      int ptmx = posix_openpt (0);
      if (ptmx == -1)
	{
	  if (errno == ENXIO)
	    FAIL_UNSUPPORTED ("terminal not available, skipping test");
	  FAIL_EXIT1 ("posix_openpt (0): %m");
	}
      TEST_VERIFY_EXIT (grantpt (ptmx) == 0);
      TEST_VERIFY_EXIT (unlockpt (ptmx) == 0);

      TEST_VERIFY_EXIT (setsid () != -1);
      TEST_VERIFY_EXIT (ioctl (ptmx, TIOCSCTTY, NULL) == 0);
      while (dup2 (ptmx, STDIN_FILENO) == -1 && errno == EBUSY)
	;
      while (dup2 (ptmx, STDOUT_FILENO) == -1 && errno == EBUSY)
	;
      while (dup2 (ptmx, STDERR_FILENO) == -1 && errno == EBUSY)
	;
      TEST_VERIFY_EXIT (ptsname_r (ptmx, ptmxpath, sizeof ptmxpath) == 0);
      xclose (ptmx);

      run_test (argc, argv);
      _exit (0);
    }
  int status;
  xwaitpid (pid, &status, 0);
  TEST_VERIFY (WIFEXITED (status));
  exit (0);
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
