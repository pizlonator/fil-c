/* Test posix_spawn setsid attribute.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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
#include <getopt.h>
#include <intprops.h>
#include <paths.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <support/check.h>
#include <support/xunistd.h>
#include <support/temp_file.h>
#include <tst-spawn.h>

/* Nonzero if the program gets called via `exec'.  */
static int restart;

/* Hold the four initial argument used to respawn the process, plus
   the extra '--direct' and '--restart', and a final NULL.  */
static char *initial_argv[7];
static int initial_argv_count;

#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

static char *pidfile;

static pid_t
read_child_sid (void)
{
  int pidfd = xopen (pidfile, O_RDONLY, 0);

  char buf[INT_STRLEN_BOUND (pid_t)];
  ssize_t n = read (pidfd, buf, sizeof (buf));
  TEST_VERIFY (n < sizeof buf && n >= 0);
  buf[n] = '\0';

  /* We only expect to read the PID.  */
  char *endp;
  long int rpid = strtol (buf, &endp, 10);
  TEST_VERIFY (endp != buf);

  xclose (pidfd);

  return rpid;
}

/* Called on process re-execution, write down the session id on PIDFILE.  */
static void
handle_restart (const char *pidfile)
{
  int pidfd = xopen (pidfile, O_WRONLY, 0);

  char buf[INT_STRLEN_BOUND (pid_t)];
  int s = snprintf (buf, sizeof buf, "%d", getsid (0));
  size_t n = write (pidfd, buf, s);
  TEST_VERIFY (n == s);

  xclose (pidfd);
}

static void
do_test_setsid (bool test_setsid)
{
  /* Current session ID.  */
  pid_t sid = getsid (0);
  TEST_VERIFY (sid != (pid_t) -1);

  posix_spawnattr_t attrp;
  TEST_COMPARE (posix_spawnattr_init (&attrp), 0);
  if (test_setsid)
    TEST_COMPARE (posix_spawnattr_setflags (&attrp, POSIX_SPAWN_SETSID), 0);

  /* 1 or 4 elements from initial_argv:
       + path to ld.so          optional
       + --library-path         optional
       + the library path       optional
       + application name
       + --direct
       + --restart
       + pidfile  */
  int argv_size = initial_argv_count + 2;
  char *args[argv_size];
  int argc = 0;

  for (char **arg = initial_argv; *arg != NULL; arg++)
    args[argc++] = *arg;
  args[argc++] = pidfile;
  args[argc] = NULL;
  TEST_VERIFY (argc < argv_size);

  PID_T_TYPE pid;
  TEST_COMPARE (POSIX_SPAWN (&pid, args[0], NULL, &attrp, args, environ), 0);
  TEST_COMPARE (posix_spawnattr_destroy (&attrp), 0);

  siginfo_t sinfo;
  TEST_COMPARE (WAITID (P_PID, pid, &sinfo, WEXITED), 0);
  TEST_COMPARE (sinfo.si_code, CLD_EXITED);
  TEST_COMPARE (sinfo.si_status, 0);

  pid_t child_sid = read_child_sid ();

  /* Child should have a different session ID than parent.  */
  TEST_VERIFY (child_sid != (pid_t) -1);

  if (test_setsid)
    TEST_VERIFY (child_sid != sid);
  else
    TEST_VERIFY (child_sid == sid);
}

static int
do_test (int argc, char *argv[])
{
  /* We must have either:

     - one or four parameters if called initially:
       + argv[1]: path for ld.so        optional
       + argv[2]: "--library-path"      optional
       + argv[3]: the library path      optional
       + argv[4]: the application name

     - six parameters left if called through re-execution:
       + argv[5/1]: the application name
       + argv[6/2]: the pidfile

     * When built with --enable-hardcoded-path-in-tests or issued without
       using the loader directly.  */

  if (restart)
    {
      handle_restart (argv[1]);
      return 0;
    }

  TEST_VERIFY_EXIT (argc == 2 || argc == 5);

  int i;
  for (i = 0; i < argc - 1; i++)
    initial_argv[i] = argv[i + 1];
  initial_argv[i++] = (char *) "--direct";
  initial_argv[i++] = (char *) "--restart";
  initial_argv_count = i;

  create_temp_file ("tst-posix_spawn-setsid-", &pidfile);

  do_test_setsid (false);
  do_test_setsid (true);

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
