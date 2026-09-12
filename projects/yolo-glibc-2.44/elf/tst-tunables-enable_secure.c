/* Check GLIBC_TUNABLES parsing for enable_secure.
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

#include <array_length.h>
/* The test uses the tunable_env_alias_list size, which is only exported for
   ld.so.  This will result in a copy of tunable_list and
   tunable_env_alias_list, which is ununsed by the test itself.  */
#define TUNABLES_INTERNAL 1
#include <dl-tunables.h>
#include <getopt.h>
#include <intprops.h>
#include <stdint.h>
#include <stdlib.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <sys/auxv.h>
#include <unistd.h>

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

static const struct test_t
{
  const char *env;
  const char *extraenv;
  bool check_multiple;
  int32_t expected_malloc_check;
  int32_t expected_enable_secure;
} tests[] =
{
  /* Expected tunable format.  */
  /* Tunables should be ignored if enable_secure is set. */
  {
    "GLIBC_TUNABLES=glibc.malloc.check=2:glibc.rtld.enable_secure=1",
    NULL,
    false,
    0,
    1,
  },
  /* Tunables should be ignored if enable_secure is set. */
  {
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=1:glibc.malloc.check=2",
    NULL,
    false,
    0,
    1,
  },
  /* Tunables should be set if enable_secure is unset. */
  {
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=0:glibc.malloc.check=2",
    NULL,
    false,
    2,
    0,
  },
  /* Tunables should be ignored if enable_secure is set. */
  {
    "GLIBC_TUNABLES=glibc.malloc.check=2:glibc.rtld.enable_secure=1",
    "MALLOC_CHECK_=2",
    false,
    0,
    1,
  },
  /* Same as before, but with enviroment alias prior GLIBC_TUNABLES.  */
  {
    "MALLOC_CHECK_=2",
    "GLIBC_TUNABLES=glibc.malloc.check=2:glibc.rtld.enable_secure=1",
    false,
    0,
    1,
  },
  /* Tunables should be ignored if enable_secure is set. */
  {
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=1:glibc.malloc.check=2",
    "MALLOC_CHECK_=2",
    false,
    0,
    1,
  },
  {
    "MALLOC_CHECK_=2",
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=1:glibc.malloc.check=2",
    false,
    0,
    1,
  },
  /* Tunables should be set if enable_secure is unset. */
  {
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=0:glibc.malloc.check=2",
    /* Tunable have precedence over the environment variable.  */
    "MALLOC_CHECK_=1",
    false,
    2,
    0,
  },
  {
    "MALLOC_CHECK_=1",
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=0:glibc.malloc.check=2",
    /* Tunable have precedence over the environment variable.  */
    false,
    2,
    0,
  },
  /* Tunables should be set if enable_secure is unset. */
  {
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=0",
    /* Tunable have precedence over the environment variable.  */
    "MALLOC_CHECK_=1",
    false,
    1,
    0,
  },
  /* Tunables should be set if enable_secure is unset. */
  {
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=0",
    /* Tunable have precedence over the environment variable.  */
    "MALLOC_CHECK_=1",
    false,
    1,
    0,
  },
  /* Check with tunables environment variable alias set multiple times.  */
  {
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=1:glibc.malloc.check=2",
    "MALLOC_CHECK_=2",
    true,
    0,
    1,
  },
  /* Tunables should be set if enable_secure is unset. */
  {
    "GLIBC_TUNABLES=glibc.rtld.enable_secure=0",
    /* Tunable have precedence over the environment variable.  */
    "MALLOC_CHECK_=1",
    true,
    1,
    0,
  },
};

static int
handle_restart (int i)
{
  if (tests[i].expected_enable_secure == 1)
    TEST_COMPARE (1, __libc_enable_secure);
  else
    TEST_COMPARE (tests[i].expected_enable_secure,
		  TUNABLE_GET_FULL (glibc, rtld, enable_secure, int32_t,
				     NULL));
  TEST_COMPARE (tests[i].expected_malloc_check,
		TUNABLE_GET_FULL (glibc, malloc, check, int32_t, NULL));
  return 0;
}

static int
do_test (int argc, char *argv[])
{
  /* We must have either:
     - One or four parameters left if called initially:
       + path to ld.so         optional
       + "--library-path"      optional
       + the library path      optional
       + the application name
       + the test to check  */

  TEST_VERIFY_EXIT (argc == 2 || argc == 5);

  if (restart)
    return handle_restart (atoi (argv[1]));

  char nteststr[INT_BUFSIZE_BOUND (int)];

  char *spargv[10];
  {
    int i = 0;
    for (; i < argc - 1; i++)
      spargv[i] = argv[i + 1];
    spargv[i++] = (char *) "--direct";
    spargv[i++] = (char *) "--restart";
    spargv[i++] = nteststr;
    spargv[i] = NULL;
  }

  enum { tunable_num_env_alias = array_length (tunable_env_alias_list) };

  for (int i = 0; i < array_length (tests); i++)
    {
      snprintf (nteststr, sizeof nteststr, "%d", i);

      printf ("[%d] Spawned test for %s\n", i, tests[i].env);
      setenv ("GLIBC_TUNABLES", tests[i].env, 1);

      char *envp[2 + tunable_num_env_alias + 1] =
      {
	(char *) tests[i].env,
	(char *) tests[i].extraenv,
	NULL,
      };
      if (tests[i].check_multiple)
	{
	  int j;
	  for (j=0; j < tunable_num_env_alias; j++)
	    envp[j + 2] = (char *) tests[i].extraenv;
	  envp[j + 2] = NULL;
	}

      struct support_capture_subprocess result
	= support_capture_subprogram (spargv[0], spargv, envp);
      support_capture_subprocess_check (&result, "tst-tunables-enable_secure",
		                        0, sc_allow_stderr);
      support_capture_subprocess_free (&result);
    }

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
