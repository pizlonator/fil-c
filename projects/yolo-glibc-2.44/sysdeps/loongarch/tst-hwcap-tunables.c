/* Tests for LoongArch GLIBC_TUNABLES=glibc.cpu.hwcaps filter.
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
  { "restart", no_argument, &restart, 1 }, \
  { "enable", no_argument, &enable, 1 },
static int restart;
static int enable;

/* Hold the four initial argument used to respawn the process, plus the extra
   '--direct', '--restart', and the function to check.  */
static char *spargs[9];
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
  for (int i = 0; i < cnt; i++)
  {
    if (strcmp (impls[i].name, funcname) == 0)
      {
	if (enable && impls[i].usable != true)
	  FAIL_EXIT1 ("FAIL: %s ifunc selection is not enabled.\n", funcname);
	else if (!enable && impls[i].usable != false)
	  FAIL_EXIT1 ("FAIL: %s ifunc selection is not disabled.\n", funcname);
	break;
      }
  }

  _exit (EXIT_SUCCESS);
}

static void
run_test (const char *filter, const char *funcname, int disable)
{
  if (disable)
    printf ("info: checking filter %s (expect %s ifunc "
	    "selection to be disabled)\n", filter, funcname);
  else
    {
      printf ("info: checking filter %s (expect %s ifunc "
	      "selection to be enabled)\n", filter, funcname);
      spargs[fc++] = (char *) "--enable";
    }

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

  if (!disable)
    fc--;
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

  if (hwcap & HWCAP_LOONGARCH_LASX)
    run_test ("-LASX", "__memcpy_lasx", 1);
  if (hwcap & HWCAP_LOONGARCH_LSX)
    run_test ("-LSX", "__memcpy_lsx", 1);
  if (hwcap & HWCAP_LOONGARCH_UAL)
    run_test ("-UAL", "__memcpy_unaligned", 1);

  /* __memcpy_aligned is the default ifunc selection and will be
   * always enabled.  */
  run_test ("-LASX,-LSX,-UAL", "__memcpy_aligned", 0);
  run_test ("-LASX,-LSX", "__memcpy_aligned", 0);
  run_test ("-LASX", "__memcpy_aligned", 0);

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
