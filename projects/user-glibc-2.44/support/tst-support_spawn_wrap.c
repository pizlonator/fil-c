/* Tests for support_spawn_wrap.
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

#include <getopt.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/subprocess.h>
#include <support/support.h>
#include <support/xspawn.h>
#include <support/xunistd.h>
#include <sys/wait.h>
#include <unistd.h>

/* Return true if running via an explicit ld.so invocation.   */
static bool
running_via_ldso (void)
{
  char *self = realpath ("/proc/self/exe", NULL);
  if (self == NULL)
    FAIL_UNSUPPORTED ("/proc/self/exe not available");
  char *ldso = realpath (support_objdir_elf_ldso, NULL);
  TEST_VERIFY_EXIT (ldso != NULL);
  bool result = strcmp (self, ldso) == 0;
  free (ldso);
  free (self);
  return result;
}

static void
test_subprocess (int argc, char **argv)
{
  {
    char *argc_env = getenv ("argc");
    if (argc_env != NULL)
      {
        char *argc_arg = xasprintf ("%d", argc);
        TEST_COMPARE_STRING (argc_arg, argc_env);
        free (argc_arg);
      }
  }

  if (argc >= 2 && strcmp (argv[1], "alpha") == 0)
    {
      TEST_COMPARE (argc, 4);
      TEST_COMPARE_STRING (argv[0], "program");
      TEST_COMPARE_STRING (argv[2], "beta");
      TEST_COMPARE_STRING (argv[3], "gamma");
      TEST_COMPARE_STRING (argv[4], NULL);
    }
  else if (argc >= 2 && strcmp (argv[1], "check-env") == 0)
    printf ("%d %s\n", argc, getenv ("extra"));
  else if (argc >= 2 && strcmp (argv[1], "check-ld.so") == 0)
    TEST_VERIFY (running_via_ldso ());
}

/* The "recurse" environment variable and the --recurse option
   indicate recursive invocation.  */
static int flag_recurse;
#define CMDLINE_OPTIONS                         \
  { "recurse", no_argument, &flag_recurse, 1 }, \
  /* CMDLINE_OPTION */

static void
prepare (int argc, char **argv)
{
  if (getenv ("recurse") != NULL || flag_recurse)
    {
      test_subprocess (argc, argv);
      support_record_failure_barrier ();
      exit (0);
    }
}

#define PREPARE prepare

/* Test wrapping of a non-test program (iconv).  This uses posix_spawn
   directly, mainly for illustrative purposes.  */
void
test_iconv (void)
{
  char *iconv_prog = xasprintf ("%s/iconv/iconv_prog", support_objdir_root);
  char *argv[] = { (char *) "iconv", (char *) "-f", (char *) "UTF-8",
                   (char *) "-t", (char *) "ISO-8859-1", NULL };
  struct support_spawn_wrapped *w
    = support_spawn_wrap (iconv_prog, argv, NULL, support_spawn_wrap_force);

  /* Set up pipes for stdin, stdout, and stderr.  */
  int stdin_pipe[2];
  xpipe (stdin_pipe);
  int stdout_pipe[2];
  xpipe (stdout_pipe);
  int stderr_pipe[2];
  xpipe (stderr_pipe);

  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init (&fa);
  xposix_spawn_file_actions_adddup2 (&fa, stdin_pipe[0], STDIN_FILENO);
  xposix_spawn_file_actions_addclose (&fa, stdin_pipe[0]);
  xposix_spawn_file_actions_addclose (&fa, stdin_pipe[1]);
  xposix_spawn_file_actions_adddup2 (&fa, stdout_pipe[1], STDOUT_FILENO);
  xposix_spawn_file_actions_addclose (&fa, stdout_pipe[0]);
  xposix_spawn_file_actions_addclose (&fa, stdout_pipe[1]);
  xposix_spawn_file_actions_adddup2 (&fa, stderr_pipe[1], STDERR_FILENO);
  xposix_spawn_file_actions_addclose (&fa, stderr_pipe[0]);
  xposix_spawn_file_actions_addclose (&fa, stderr_pipe[1]);

  pid_t pid = xposix_spawn (w->path, &fa, NULL, w->argv, w->envp);
  posix_spawn_file_actions_destroy (&fa);

  xclose (stdin_pipe[0]);
  xclose (stdout_pipe[1]);
  xclose (stderr_pipe[1]);

  /* Write UTF-8 encoding of "äöü\n" to stdin.  */
  xwrite (stdin_pipe[1], "\xc3\xa4\xc3\xb6\xc3\xbc\n", 7);
  xclose (stdin_pipe[1]);

  /* Read the converted output from the pipe.  */
  char buf[16];
  ssize_t ret = read (stdout_pipe[0], buf, sizeof (buf));
  xclose (stdout_pipe[0]);

  /* ISO-8859-1 encoding of "äöü\n".  */
  TEST_COMPARE_BLOB (buf, ret, "\xe4\xf6\xfc\n", 4);

  int status;
  xwaitpid (pid, &status, 0);
  TEST_COMPARE (status, 0);

  /* Check that nothing has been written to stderr.  */
  ret = read (stderr_pipe[0], buf, sizeof (buf));
  TEST_COMPARE (ret, 0);
  xclose (stderr_pipe[0]);

  support_spawn_wrapped_free (w);
  free (iconv_prog);
}

static int
do_test (void)
{
  char *program = xasprintf ("%s/support/tst-support_spawn_wrap",
                             support_objdir_root);

  {
    char *env[] = { (char *) "recurse=", (char *) "argc=1", NULL };
    struct support_spawn_wrapped *w
      = support_spawn_wrap (program, NULL, env, 0);
    struct support_capture_subprocess proc
      = support_capture_subprogram (w->path, w->argv, w->envp);
    support_capture_subprocess_check (&proc, "no arguments", 0, sc_allow_none);
    support_capture_subprocess_free (&proc);
    support_spawn_wrapped_free (w);
  }

  {
    char *argv[] = { (char *) "program", (char *) "--recurse", NULL };
    struct support_spawn_wrapped *w
      = support_spawn_wrap (program, argv, NULL, 0);
    struct support_capture_subprocess proc
      = support_capture_subprogram (w->path, w->argv, w->envp);
    support_capture_subprocess_check (&proc, "default envvironment", 0,
                                      sc_allow_none);
    support_capture_subprocess_free (&proc);
    support_spawn_wrapped_free (w);
  }

  {
    char *argv[] = { (char *) "program", (char *) "alpha", (char *) "beta",
                     (char *) "gamma", NULL };
    char *env[] = { (char *) "recurse=", (char *) "argc=4", NULL };
    struct support_spawn_wrapped *w
      = support_spawn_wrap (program, argv, env, 0);
    struct support_capture_subprocess proc
      = support_capture_subprogram (w->path, w->argv, w->envp);
    support_capture_subprocess_check (&proc, "3 arguments", 0, sc_allow_none);
    support_capture_subprocess_free (&proc);
    support_spawn_wrapped_free (w);
  }

  {
    char *argv[] = { (char *) "program", (char *) "check-env", NULL };
    char *env[] = { (char *) "recurse=", (char *) "argc=2",
                    (char *) "extra=17", NULL };
    struct support_spawn_wrapped *w
      = support_spawn_wrap (program, argv, env, 0);
    struct support_capture_subprocess proc
      = support_capture_subprogram (w->path, w->argv, w->envp);
    TEST_COMPARE_STRING (proc.out.buffer, "2 17\n");
    support_capture_subprocess_check (&proc, "check-env", 0, sc_allow_stdout);
    support_capture_subprocess_free (&proc);
    support_spawn_wrapped_free (w);
  }

  test_iconv ();

  /* This may trigger EXIT_UNSUPPORTED, so run this before the tests
     that rely on running_via_ldso.  */
  TEST_COMPARE (!running_via_ldso (), support_hardcoded_paths_in_test);

  {
    char *argv[] = { (char *) "program", (char *) "check-ld.so", NULL };
    char *env[] = { (char *) "recurse=", NULL };
    struct support_spawn_wrapped *w
      = support_spawn_wrap (program, argv, env, support_spawn_wrap_force);
    struct support_capture_subprocess proc
      = support_capture_subprogram (w->path, w->argv, w->envp);
    support_capture_subprocess_check (&proc, "check-ld.so", 0, sc_allow_none);
    support_capture_subprocess_free (&proc);
    support_spawn_wrapped_free (w);
  }

  free (program);
  return 0;
}

#include <support/test-driver.c>
