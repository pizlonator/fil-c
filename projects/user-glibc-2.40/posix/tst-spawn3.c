/* Check posix_spawn add file actions.
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <spawn.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <paths.h>
#include <intprops.h>

#include <support/check.h>
#include <support/temp_file.h>
#include <support/xunistd.h>
#include <tst-spawn.h>

static int
do_test (void)
{
  /* The test checks if posix_spawn open file action close the file descriptor
     before opening a new one in case the input file descriptor is already
     opened.  It does by exhausting all file descriptors on the process before
     issue posix_spawn.  It then issues a posix_spawn for '/bin/sh echo $$'
     and add two rules:

     1. Redirect stdout to a temporary filepath
     2. Redirect stderr to stdout

     If the implementation does not close the file 1. will fail with
     EMFILE.  */

  struct rlimit rl;
  int max_fd = 24;

  /* Set maximum number of file descriptor to a low value to avoid open
     too many files in environments where RLIMIT_NOFILE is large and to
     limit the array size to track the opened file descriptors.  */

  if (getrlimit (RLIMIT_NOFILE, &rl) == -1)
    FAIL_EXIT1 ("getrlimit (RLIMIT_NOFILE): %m");

  max_fd = (rl.rlim_cur < max_fd ? rl.rlim_cur : max_fd);
  rl.rlim_cur = max_fd;

  if (setrlimit (RLIMIT_NOFILE, &rl) == 1)
    FAIL_EXIT1 ("setrlimit (RLIMIT_NOFILE): %m");

  /* Exhauste the file descriptor limit with temporary files.  */
  int files[max_fd];
  int nfiles = 0;
  for (; nfiles < max_fd; nfiles++)
    {
      int fd = create_temp_file ("tst-spawn3.", NULL);
      if (fd == -1)
	{
	  if (errno != EMFILE)
	    FAIL_EXIT1 ("create_temp_file: %m");
	  break;
	}
      files[nfiles] = fd;
    }
  TEST_VERIFY_EXIT (nfiles != 0);

  posix_spawn_file_actions_t a;
  TEST_COMPARE (posix_spawn_file_actions_init (&a), 0);

  /* Executes a /bin/sh echo $$ 2>&1 > ${objpfx}tst-spawn3.pid .  */
  const char pidfile[] = OBJPFX "tst-spawn3.pid";
  TEST_COMPARE (posix_spawn_file_actions_addopen (&a, STDOUT_FILENO, pidfile,
						  O_WRONLY| O_CREAT | O_TRUNC,
						  0644),
		0);

  TEST_COMPARE (posix_spawn_file_actions_adddup2 (&a, STDOUT_FILENO,
						  STDERR_FILENO),
		0);

  /* Since execve (called by posix_spawn) might require to open files to
     actually execute the shell script, setup to close the temporary file
     descriptors.  */
  int maxnfiles =
#ifdef TST_SPAWN_PIDFD
    /* The sparing file descriptor will be returned as the pid descriptor,
       otherwise clone fail with EMFILE.  */
    nfiles - 1;
#else
    nfiles;
#endif

  for (int i=0; i<maxnfiles; i++)
    TEST_COMPARE (posix_spawn_file_actions_addclose (&a, files[i]), 0);

  char *spawn_argv[] = { (char *) _PATH_BSHELL, (char *) "-c",
			 (char *) "echo $$", NULL };
  PID_T_TYPE pid;

  {
    int r = POSIX_SPAWN (&pid, _PATH_BSHELL, &a, NULL, spawn_argv, NULL);
    if (r == ENOSYS)
      FAIL_UNSUPPORTED ("kernel does not support CLONE_PIDFD clone flag");
#ifdef TST_SPAWN_PIDFD
    TEST_COMPARE (r, EMFILE);

    /* Free up one file descriptor, so posix_spawn_pidfd_ex can return it.  */
    xclose (files[nfiles-1]);
    nfiles--;
    r = POSIX_SPAWN (&pid, _PATH_BSHELL, &a, NULL, spawn_argv, NULL);
#endif
    TEST_COMPARE (r, 0);
  }

  siginfo_t sinfo;
  TEST_COMPARE (WAITID (P_PID, pid, &sinfo, WEXITED), 0);
  TEST_COMPARE (sinfo.si_code, CLD_EXITED);
  TEST_COMPARE (sinfo.si_status, 0);

  /* Close the temporary files descriptor so it can check posix_spawn
     output.  */
  for (int i=0; i<nfiles; i++)
    xclose (files[i]);

  int pidfd = xopen (pidfile, O_RDONLY, 0);

  char buf[INT_BUFSIZE_BOUND (pid_t)];
  ssize_t n = read (pidfd, buf, sizeof (buf));
  TEST_VERIFY (n < sizeof buf && n >= 0);

  xunlink (pidfile);

  /* We only expect to read the PID.  */
  char *endp;
  long int rpid = strtol (buf, &endp, 10);
  TEST_VERIFY (*endp == '\n' && endp != buf);

  TEST_COMPARE (rpid, sinfo.si_pid);

  return 0;
}

#include <support/test-driver.c>
