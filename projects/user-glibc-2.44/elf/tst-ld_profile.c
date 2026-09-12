/* Test LD_PROFILE/LD_PROFILE_OUTPUT interaction (bug 33797).
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

#include <gnu/lib-names.h>
#include <stdlib.h>
#include <string.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/subprocess.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <unistd.h>

/* Expected profile file path (based on LD_PROFILE_OUTPUT and LIBC_SO).  */
static char *profile_file_path;

/* Path to this test program for recursive invocation.  */
static char *program;

/* LD_PROFILE_OUTPUT environment variable setting.  */
static char *ld_profile_output_env;

/* Run the test program with the specified environment array.  */
static struct support_capture_subprocess
run_test_program (char *env[])
{
  /* Make sure the the potential output file does not exist.  */
  unlink (profile_file_path);

/* Command line arguments for recursive invocation.  This turns the
   test program in a no-op (with LD_PROFILE output the only side effect).  */
  static char *recurse_argv[] =
    {
      (char *) "tst-ld_profile", (char *) "recurse", NULL
    };
  struct support_spawn_wrapped *w
    = support_spawn_wrap (program, recurse_argv, env, 0);
  struct support_capture_subprocess proc
    = support_capture_subprogram (w->path, w->argv, w->envp);
  support_spawn_wrapped_free (w);

  return proc;
}

static void
test_profile_without_output (void)
{
  char *env[] = { (char *) "LD_PROFILE=" LIBC_SO, NULL };
  struct support_capture_subprocess proc = run_test_program (env);
  const char *expected_warning =
    "warning: LD_PROFILE ignored because LD_PROFILE_OUTPUT not specified\n";
  TEST_COMPARE_STRING (proc.err.buffer, expected_warning);
  support_capture_subprocess_check (&proc,
                                    "LD_PROFILE without LD_PROFILE_OUTPUT",
                                    0, sc_allow_stderr);
  support_capture_subprocess_free (&proc);

  TEST_VERIFY (access (profile_file_path, F_OK) != 0);
  /* Also check the old /var/tmp path.  */
  TEST_VERIFY (access ("/var/tmp/" LIBC_SO ".profile", F_OK) != 0);
}

static void
test_profile_with_output (void)
{
  char *env[] = { (char *) "LD_PROFILE=" LIBC_SO, ld_profile_output_env, NULL };
  struct support_capture_subprocess proc = run_test_program (env);
  support_capture_subprocess_check (&proc, "LD_PROFILE with LD_PROFILE_OUTPUT",
                                    0, sc_allow_none);
  support_capture_subprocess_free (&proc);

  /* This asserts that the file was created.  */
  TEST_COMPARE (unlink (profile_file_path), 0);
}

static void
test_output_without_profile (void)
{
  char *env[] = { ld_profile_output_env, NULL };
  struct support_capture_subprocess proc = run_test_program (env);
  support_capture_subprocess_check (&proc,
                                    "LD_PROFILE_OUTPUT without LD_PROFILE",
                                    0, sc_allow_none);
  support_capture_subprocess_free (&proc);

  TEST_VERIFY (access (profile_file_path, F_OK) != 0);
}

static void
prepare (int argc, char **argv)
{
  /* Do nothing on recursive invocation.  */
  if (argc >= 2 && strcmp (argv[1], "recurse") == 0)
    exit (0);
}

#define PREPARE prepare

static int
do_test (void)
{
  if (access ("/var/tmp/" LIBC_SO ".profile", F_OK) == 0)
    FAIL_UNSUPPORTED ("/var/tmp/" LIBC_SO ".profile exists");

  /* Temporary directory for the profile output.  */
  char *profile_output_dir = support_create_temp_directory ("tst-ld_profile");
  profile_file_path = xasprintf ("%s/%s.profile", profile_output_dir, LIBC_SO);
  ld_profile_output_env = xasprintf ("LD_PROFILE_OUTPUT=%s",
                                     profile_output_dir);
  program = xasprintf ("%s/elf/tst-ld_profile", support_objdir_root);

  test_profile_without_output ();
  test_profile_with_output ();
  test_output_without_profile ();

  free (program);
  free (ld_profile_output_env);
  free (profile_file_path);
  free (profile_output_dir);

  return 0;
}

#include <support/test-driver.c>
