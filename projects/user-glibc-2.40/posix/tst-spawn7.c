/* Tests for posix_spawn signal handling.
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
   <http://www.gnu.org/licenses/>.  */

#include <assert.h>
#include <getopt.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/xsignal.h>
#include <support/xunistd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <tst-spawn.h>

/* Nonzero if the program gets called via `exec'.  */
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },
static int restart;

/* Hold the four initial argument used to respawn the process, plus the extra
   '--direct', '--restart', the check type ('SIG_IGN' or 'SIG_DFL'), and a
   final NULL.  */
static char *spargs[8];
static int check_type_argc;

/* Called on process re-execution.  */
_Noreturn static void
handle_restart (int argc, char *argv[])
{
  assert (argc == 1);

  if (strcmp (argv[0], "SIG_DFL") == 0)
    {
      for (int i = 1; i < NSIG; i++)
	{
	  struct sigaction sa;
	  int r = sigaction (i, NULL, &sa);
	  /* Skip internal signals (such as SIGCANCEL).  */
	  if (r == -1)
	    continue;
	  TEST_VERIFY_EXIT (sa.sa_handler == SIG_DFL);
	}
      exit (EXIT_SUCCESS);
    }
  else if (strcmp (argv[0], "SIG_IGN") == 0)
    {
      for (int i = 1; i < NSIG; i++)
	{
	  struct sigaction sa;
	  int r = sigaction (i, NULL, &sa);
	  /* Skip internal signals (such as SIGCANCEL).  */
	  if (r == -1)
	    continue;
	  if (i == SIGUSR1 || i == SIGUSR2)
	    TEST_VERIFY_EXIT (sa.sa_handler == SIG_IGN);
	  else
	    TEST_VERIFY_EXIT (sa.sa_handler == SIG_DFL);
	}
      exit (EXIT_SUCCESS);
    }

  exit (EXIT_FAILURE);
}

static void
spawn_signal_test (const char *type, const posix_spawnattr_t *attr)
{
  spargs[check_type_argc] = (char*) type;

  PID_T_TYPE pid;
  siginfo_t sinfo;

  TEST_COMPARE (POSIX_SPAWN (&pid, spargs[0], NULL, attr, spargs, environ), 0);
  TEST_COMPARE (WAITID (P_ALL, 0, &sinfo, WEXITED), 0);
  TEST_COMPARE (sinfo.si_code, CLD_EXITED);
  TEST_COMPARE (sinfo.si_status, 0);
}

static void
dummy_sa_handler (int signal)
{
}

static void
do_test_signals (void)
{
  /* Ensure the initial signal disposition, ignore EINVAL for internal
     signal such as SIGCANCEL.  */
  for (int sig = 1; sig < _NSIG; ++sig)
    sigaction (sig, &(struct sigaction) { .sa_handler = SIG_DFL,
					  .sa_flags = 0 }, NULL);

  {
    /* Check if all signals handler are set to SIG_DFL on spawned process.  */
    spawn_signal_test ("SIG_DFL", NULL);
  }

  {
    /* Same as before, but set SIGUSR1 and SIGUSR2 to a handler different than
       SIG_IGN or SIG_DFL.  */
    struct sigaction sa = { 0 };
    sa.sa_handler = dummy_sa_handler;
    xsigaction (SIGUSR1, &sa, NULL);
    xsigaction (SIGUSR2, &sa, NULL);
    spawn_signal_test ("SIG_DFL", NULL);
  }

  {
    /* Check if SIG_IGN is keep as is.  */
    struct sigaction sa = { 0 };
    sa.sa_handler = SIG_IGN;
    xsigaction (SIGUSR1, &sa, NULL);
    xsigaction (SIGUSR2, &sa, NULL);
    spawn_signal_test ("SIG_IGN", NULL);
  }

  {
    /* Check if SIG_IGN handlers are set to SIG_DFL.  */
    posix_spawnattr_t attr;
    posix_spawnattr_init (&attr);
    sigset_t mask;
    sigemptyset (&mask);
    sigaddset (&mask, SIGUSR1);
    sigaddset (&mask, SIGUSR2);
    posix_spawnattr_setsigdefault (&attr, &mask);
    posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETSIGDEF);
    spawn_signal_test ("SIG_DFL", &attr);
    posix_spawnattr_destroy (&attr);
  }
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
       + argv[1]: the application name
       + argv[2]: check SIG_IGN/SIG_DFL.

     * When built with --enable-hardcoded-path-in-tests or issued without
       using the loader directly.  */

  if (restart)
    handle_restart (argc - 1, &argv[1]);

  TEST_VERIFY_EXIT (argc == 2 || argc == 5);

  int i;
  for (i = 0; i < argc - 1; i++)
    spargs[i] = argv[i + 1];
  spargs[i++] = (char *) "--direct";
  spargs[i++] = (char *) "--restart";
  check_type_argc = i++;
  spargs[i] = NULL;


  do_test_signals ();

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
