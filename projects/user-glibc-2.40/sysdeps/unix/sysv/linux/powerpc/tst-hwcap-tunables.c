/* Tests for powerpc GLIBC_TUNABLES=glibc.cpu.hwcaps filter.
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

#include <array_length.h>
#include <getopt.h>
#include <ifunc-impl-list.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xunistd.h>
#include <sys/auxv.h>
#include <sys/wait.h>

/* Nonzero if the program gets called via `exec'.  */
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },
static int restart;

/* Hold the four initial argument used to respawn the process, plus the extra
   '--direct', '--restart', and the function to check  */
static char *spargs[8];
static int fc;

/* Called on process re-execution.  */
_Noreturn static void
handle_restart (int argc, char *argv[])
{
  TEST_VERIFY_EXIT (argc == 1);
  const char *funcname = argv[0];

  struct libc_ifunc_impl impls[32];
  int cnt = __libc_ifunc_impl_list ("memcpy", impls, array_length (impls));
  if (cnt == 0)
    _exit (EXIT_SUCCESS);
  TEST_VERIFY_EXIT (cnt >= 1);
  for (int i = 0; i < cnt; i++) {
    if (strcmp (impls[i].name, funcname) == 0)
      {
	TEST_COMPARE (impls[i].usable, false);
	break;
      }
  }

  _exit (EXIT_SUCCESS);
}

static void
run_test (const char *filter, const char *funcname)
{
  printf ("info: checking filter %s (expect %s ifunc selection to be removed)\n",
	  filter, funcname);
  char *tunable = xasprintf ("GLIBC_TUNABLES=glibc.cpu.hwcaps=%s", filter);
  char *const newenvs[] = { (char*) tunable, NULL };
  spargs[fc] = (char *) funcname;

  pid_t pid;
  TEST_COMPARE (posix_spawn (&pid, spargs[0], NULL, NULL, spargs, newenvs), 0);
  int status;
  TEST_COMPARE (xwaitpid (pid, &status, 0), pid);
  TEST_VERIFY (WIFEXITED (status));
  TEST_VERIFY (!WIFSIGNALED (status));
  TEST_COMPARE (WEXITSTATUS (status), 0);

  free (tunable);
}

static int
do_test (int argc, char *argv[])
{
  if (restart)
    handle_restart (argc - 1, &argv[1]);

  TEST_VERIFY_EXIT (argc == 2 || argc == 5);

  int i;
  for (i = 0; i < argc - 1; i++)
    spargs[i] = argv[i + 1];
  spargs[i++] = (char *) "--direct";
  spargs[i++] = (char *) "--restart";
  fc = i++;
  spargs[i] = NULL;

  unsigned long int hwcap = getauxval (AT_HWCAP);
  unsigned long int hwcap2 = getauxval (AT_HWCAP2);
  if (__WORDSIZE == 64)
    {
      if (hwcap2 & PPC_FEATURE2_ARCH_3_1)
	run_test ("-arch_3_1", "__memcpy_power10");
      if (hwcap2 & PPC_FEATURE2_ARCH_2_07)
	run_test ("-arch_2_07", "__memcpy_power8_cached");
      if (hwcap & PPC_FEATURE_ARCH_2_06)
	run_test ("-arch_2_06", "__memcpy_power7");
      if (hwcap & PPC_FEATURE_ARCH_2_05)
	run_test ("-arch_2_06,-arch_2_05","__memcpy_power6");
      run_test ("-arch_2_06,-arch_2_05,-power5+,-power5,-power4",
		"__memcpy_power4");
      /* Also run with valid, but empty settings.  */
      run_test (",-,-arch_2_06,-arch_2_05,-power5+,-power5,,-power4,-",
		"__memcpy_power4");
    }
  else
    {
      if (hwcap & PPC_FEATURE_HAS_VSX)
	run_test ("-vsx", "__memcpy_power7");
      if (hwcap & PPC_FEATURE_ARCH_2_06)
	run_test ("-arch_2_06", "__memcpy_a2");
      if (hwcap & PPC_FEATURE_ARCH_2_05)
	run_test ("-arch_2_05", "__memcpy_power6");
    }
  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
