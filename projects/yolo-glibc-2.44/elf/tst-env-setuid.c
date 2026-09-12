/* Copyright (C) 2012-2026 Free Software Foundation, Inc.
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

/* Verify that correctly filter out unsafe environment variables defined
   in unsecvars.h.  */

#include <array_length.h>
#include <gnu/lib-names.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <support/capture_subprocess.h>

static char SETGID_CHILD[] = "setgid-child";

#define FILTERED_VALUE   "some-filtered-value"
#define UNFILTERED_VALUE "some-unfiltered-value"
/* It assumes no other programs is being profile with a library with same
   SONAME using the default folder.  */
#ifndef PROFILE_LIB
# define PROFILE_LIB      "tst-sonamemove-runmod2.so"
#endif

/* Computed path for LD_DEBUG_OUTPUT.  */
static char *debugoutputpath;

/* Expected file name for erroneous LD_PROFILE output.  */
static char *profilepath;

struct envvar_t
{
  const char *env;
  const char *value;
};

/* That is not an extensible list of all filtered out environment
   variables.  */
static const struct envvar_t filtered_envvars[] =
{
  { "GLIBC_TUNABLES",          FILTERED_VALUE },
  { "LD_AUDIT",                FILTERED_VALUE },
  { "LD_LIBRARY_PATH",         FILTERED_VALUE },
  { "LD_PRELOAD",              FILTERED_VALUE },
  { "LD_PROFILE",              PROFILE_LIB },
  { "LD_PROFILE_OUTPUT",       "/var/tmp" },      /* Not actually used.  */
  { "MALLOC_ARENA_MAX",        FILTERED_VALUE },
  { "MALLOC_PERTURB_",         FILTERED_VALUE },
  { "MALLOC_TRACE",            FILTERED_VALUE },
  { "MALLOC_TRIM_THRESHOLD_",  FILTERED_VALUE },
  { "RES_OPTIONS",             FILTERED_VALUE },
  { "LD_DEBUG",                "all" },
  { "LD_DEBUG_OUTPUT",         "overwritten" },   /* Not actually used.  */
  { "LD_WARN",                 FILTERED_VALUE },
  { "LD_VERBOSE",              FILTERED_VALUE },
  { "LD_BIND_NOW",             "0" },
  { "LD_BIND_NOT",             "1" },
};

static const struct envvar_t unfiltered_envvars[] =
{
  /* Non longer supported option.  */
  { "LD_ASSUME_KERNEL",        UNFILTERED_VALUE },
};

static void
unlink_ld_debug_output (pid_t pid)
{
  char *output = xasprintf ("%s.%d", debugoutputpath, pid);
  unlink (output);
  free (output);
}

static int
test_child (void)
{
  int ret = 0;

  for (const struct envvar_t *e = filtered_envvars;
       e != array_end (filtered_envvars);
       e++)
    {
      const char *env = getenv (e->env);
      if (env != NULL)
	{
	  printf ("FAIL: filtered environment variable is not NULL: %s=%s\n",
		  e->env, env);
	  ret = 1;
	}
    }

  for (const struct envvar_t *e = unfiltered_envvars;
       e != array_end (unfiltered_envvars);
       e++)
    {
      const char *env = getenv (e->env);
      if (!(env != NULL && strcmp (env, e->value) == 0))
	{
	  if (env == NULL)
	    printf ("FAIL: unfiltered environment variable %s is NULL\n",
		    e->env);
	  else
	    printf ("FAIL: unfiltered environment variable %s=%s != %s\n",
		    e->env, env, e->value);

	  ret = 1;
	}
    }

  /* Also check if no profile file was created.
     Note: LD_PROFILE is not supported for static binaries.  */
  if (!access (profilepath, R_OK))
    {
      printf ("FAIL: LD_PROFILE file at %s was created!\n", profilepath);
      ret = 1;
    }

  return ret;
}

static int
do_test (int argc, char **argv)
{
  /* For dynamic loader, the test requires --enable-hardcoded-path-in-tests so
     the kernel sets the AT_SECURE on process initialization.  */
  if (argc >= 2 && strstr (argv[1], LD_SO) != 0)
    FAIL_UNSUPPORTED ("dynamic test requires --enable-hardcoded-path-in-tests");

  profilepath = xasprintf ("%s/%s.profile",
			   support_objdir_root, PROFILE_LIB);
  debugoutputpath = xasprintf ("%s/tst-env-setuid-file",
			       support_objdir_root);

  /* Setgid child process.  */
  if (argc == 2 && strcmp (argv[1], SETGID_CHILD) == 0)
    {
      pid_t ppid = getppid ();

      if (getgid () == getegid ())
	{
	  /* This can happen if the file system is mounted nosuid.  */
	  unlink_ld_debug_output (ppid);

	  FAIL_UNSUPPORTED ("SGID failed: GID and EGID match (%jd)\n",
			    (intmax_t) getgid ());
	}

      int ret = test_child ();

      unlink_ld_debug_output (ppid);

      if (ret != 0)
	exit (1);
    }
  else
    {
      for (const struct envvar_t *e = filtered_envvars;
	   e != array_end (filtered_envvars);
	   e++)
	setenv (e->env, e->value, 1);

      for (const struct envvar_t *e = unfiltered_envvars;
	   e != array_end (unfiltered_envvars);
	   e++)
	setenv (e->env, e->value, 1);

      /* Dynamically computed values.  */
      setenv ("LD_DEBUG_OUTPUT", debugoutputpath, 1);
      setenv ("LD_PROFILE_OUTPUT", support_objdir_root, 1);

      /* Ensure that the profile output does not exist from a previous run
	 (e.g. if test_dir, which defaults to /tmp, is mounted nosuid.)
	 Note: support_capture_subprogram_self_sgid creates the SGID binary
	 in test_dir.  */
      unlink (profilepath);

      support_capture_subprogram_self_sgid (SETGID_CHILD);

      /* And clean up afterwards if necessary.  */
      unlink (profilepath);
    }

  free (profilepath);
  free (debugoutputpath);
  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
