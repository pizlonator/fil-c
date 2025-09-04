/* Copyright (C) 2002-2024 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <paths.h>

#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/support.h>
#include <support/xthread.h>
#include <support/xunistd.h>

static char *tmpdir;
static long int namemax;

static void
do_prepare (int argc, char *argv[])
{
  tmpdir = support_create_temp_directory ("tst-system-");
  /* Include the last '/0'.  */
  namemax = pathconf (tmpdir, _PC_NAME_MAX) + 1;
  TEST_VERIFY_EXIT (namemax != -1);
}
#define PREPARE do_prepare

struct args
{
  const char *command;
  int exit_status;
  int term_sig;
  const char *path;
};

static void
call_system (void *closure)
{
  struct args *args = (struct args *) closure;
  int ret;

  if (args->path != NULL)
    TEST_COMPARE (setenv ("PATH", args->path, 1), 0);
  ret = system (args->command);
  if (args->term_sig == 0)
    {
      /* Expect regular termination.  */
      TEST_VERIFY (WIFEXITED (ret) != 0);
      TEST_COMPARE (WEXITSTATUS (ret), args->exit_status);
    }
  else
    {
      /* status_or_signal < 0.  Expect termination by signal.  */
      TEST_VERIFY (WIFSIGNALED (ret) != 0);
      TEST_COMPARE (WTERMSIG (ret), args->term_sig);
    }
}

static void *
sleep_and_check_sigchld (void *closure)
{
  double *seconds = (double *) closure;
  char cmd[namemax];
  sprintf (cmd, "sleep %lf" , *seconds);
  TEST_COMPARE (system (cmd), 0);

  sigset_t blocked = {0};
  TEST_COMPARE (sigprocmask (SIG_BLOCK, NULL, &blocked), 0);
  TEST_COMPARE (sigismember (&blocked, SIGCHLD), 0);
  return NULL;
}

static int
do_test (void)
{
  TEST_VERIFY (system (NULL) != 0);

  {
    char cmd[namemax];
    memset (cmd, 'a', sizeof(cmd));
    cmd[sizeof(cmd) - 1] = '\0';

    struct support_capture_subprocess result;
    result = support_capture_subprocess (call_system,
					 &(struct args) {
					   cmd, 127, 0, tmpdir
					 });
    support_capture_subprocess_check (&result, "system", 0, sc_allow_stderr);

    char *returnerr = xasprintf ("%s: execing %s failed: "
				 "No such file or directory",
				 basename(_PATH_BSHELL), cmd);
    TEST_COMPARE_STRING (result.err.buffer, returnerr);
    free (returnerr);
  }

  {
    char cmd[namemax + 1];
    memset (cmd, 'a', sizeof(cmd));
    cmd[sizeof(cmd) - 1] = '\0';

    struct support_capture_subprocess result;
    result = support_capture_subprocess (call_system,
					 &(struct args) {
					   cmd, 127, 0, tmpdir
					 });
    support_capture_subprocess_check (&result, "system", 0, sc_allow_stderr);

    char *returnerr = xasprintf ("%s: execing %s failed: "
				 "File name too long",
				 basename(_PATH_BSHELL), cmd);
    TEST_COMPARE_STRING (result.err.buffer, returnerr);
    free (returnerr);
  }

  {
    struct support_capture_subprocess result;
    result = support_capture_subprocess (call_system,
					 &(struct args) {
					   "kill $$", 0, SIGTERM
					 });
    support_capture_subprocess_check (&result, "system", 0, sc_allow_none);
  }

  {
    struct support_capture_subprocess result;
    result = support_capture_subprocess (call_system,
					 &(struct args) { "echo ...", 0 });
    support_capture_subprocess_check (&result, "system", 0, sc_allow_stdout);
    TEST_COMPARE_STRING (result.out.buffer, "...\n");
  }

  {
    struct support_capture_subprocess result;
    const char *cmd = "-echo";
    result = support_capture_subprocess (call_system,
					 &(struct args) { cmd, 127 });
    support_capture_subprocess_check (&result, "system", 0, sc_allow_stderr |
			sc_allow_stdout);
    char *returnerr = xasprintf ("%s: execing -echo failed: "
				 "No such file or directory",
				 basename(_PATH_BSHELL));
    TEST_COMPARE_STRING (result.err.buffer, returnerr);
    free (returnerr);
  }

  {
    struct support_capture_subprocess result;
    result = support_capture_subprocess (call_system,
					 &(struct args) { "exit 1", 1 });
    support_capture_subprocess_check (&result, "system", 0, sc_allow_none);
  }

  {
    struct stat64 st;
    xstat (_PATH_BSHELL, &st);
    mode_t mode = st.st_mode;
    xchmod (_PATH_BSHELL, mode & ~(S_IXUSR | S_IXGRP | S_IXOTH));

    struct support_capture_subprocess result;
    result = support_capture_subprocess (call_system,
					 &(struct args) {
					   "exit 1", 127, 0
					 });
    support_capture_subprocess_check (&result, "system", 0, sc_allow_none);

    xchmod (_PATH_BSHELL, st.st_mode);
  }

  {
    pthread_t long_sleep_thread = xpthread_create (NULL,
                                                   sleep_and_check_sigchld,
                                                   &(double) { 0.2 });
    pthread_t short_sleep_thread = xpthread_create (NULL,
                                                    sleep_and_check_sigchld,
                                                    &(double) { 0.1 });
    xpthread_join (short_sleep_thread);
    xpthread_join (long_sleep_thread);
  }

  TEST_COMPARE (system (""), 0);

  return 0;
}

#include <support/test-driver.c>
