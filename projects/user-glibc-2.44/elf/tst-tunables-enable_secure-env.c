/* Check enable_secure tunable handles removed ENV variables without
   assertions.
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
#include <errno.h>
#include <getopt.h>
#include <intprops.h>
#include <stdlib.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#ifdef __linux__
# define HAVE_AUXV 1
# include <sys/auxv.h>
#else
# define HAVE_AUXV 0
#endif

/* Nonzero if the program gets called via `exec'.  */
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },
static int restart;

/* Hold the four initial argument used to respawn the process, plus the extra
   '--direct', '--restart', auxiliary vector values, and final NULL.  */
static char *spargs[11];

#if HAVE_AUXV
static void
check_auxv (unsigned long type, char *argv)
{
  char *endptr;
  errno = 0;
  unsigned long int varg = strtoul (argv, &endptr, 10);
  TEST_VERIFY_EXIT (errno == 0);
  TEST_VERIFY_EXIT (*endptr == '\0');
  errno = 0;
  unsigned long int v = getauxval (type);
  TEST_COMPARE (errno, 0);
  TEST_COMPARE (varg, v);
}
#endif

/* Called on process re-execution.  */
_Noreturn static void
handle_restart (int argc, char *argv[])
{
  TEST_VERIFY (getenv ("GLIBC_TUNABLES") == NULL);
  TEST_VERIFY (getenv ("LD_BIND_NOW") == NULL);

#if HAVE_AUXV
  TEST_VERIFY_EXIT (argc == 4);
  check_auxv (AT_PHENT, argv[0]);
  check_auxv (AT_PHNUM, argv[1]);
  check_auxv (AT_PAGESZ, argv[2]);
  check_auxv (AT_HWCAP, argv[3]);
#endif

  exit (EXIT_SUCCESS);
}

static int
do_test (int argc, char *argv[])
{
  /* We must have either:

     - four parameter if called initially:
       + path for ld.so            [optional]
       + "--library-path"          [optional]
       + the library path          [optional]
       + the application name

     - either parameters left if called through re-execution.
       + auxiliary vector value 1
       + auxiliary vector value 2
       + auxiliary vector value 3
       + auxiliary vector value 4
  */
  if (restart)
    handle_restart (argc - 1, &argv[1]);

  TEST_VERIFY_EXIT (argc == 2 || argc == 5);

#if HAVE_AUXV
  struct
  {
    unsigned long int type;
    char str[INT_BUFSIZE_BOUND (unsigned long)];
  } auxvals[] =
  {
    /* Check some auxiliary values that should be constant over process
       re-execution.  */
    { AT_PHENT },
    { AT_PHNUM },
    { AT_PAGESZ },
    { AT_HWCAP },
  };
  for (int i = 0; i < array_length (auxvals); i++)
  {
    unsigned long int v = getauxval (auxvals[i].type);
    snprintf (auxvals[i].str, sizeof auxvals[i].str, "%lu", v);
  }
#endif

  {
    int i;
    for (i = 0; i < argc - 1; i++)
      spargs[i] = argv[i + 1];
    spargs[i++] = (char *) "--direct";
    spargs[i++] = (char *) "--restart";
#if HAVE_AUXV
    for (int j = 0; j < array_length (auxvals); j++)
      spargs[i++] = auxvals[j].str;
#endif
    spargs[i] = NULL;
  }

  {
    char *envs[] =
    {
      /* Add some environment variable that should be filtered out.  */
      (char *) "GLIBC_TUNABLES=glibc.rtld.enable_secure=1",
      (char* ) "LD_BIND_NOW=0",
      NULL,
    };
    struct support_capture_subprocess result
      = support_capture_subprogram (spargs[0], spargs, envs);
    support_capture_subprocess_check (&result,
				      "tst-tunables-enable_secure-env",
				      0,
				      sc_allow_none);
    support_capture_subprocess_free (&result);
  }

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
