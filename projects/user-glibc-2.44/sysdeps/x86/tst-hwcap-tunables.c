/* Tests for x86 GLIBC_TUNABLES=glibc.cpu.hwcaps filter.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
#include <intprops.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xunistd.h>
#include <support/capture_subprocess.h>

/* Nonzero if the program gets called via `exec'.  */
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },
static int restart;

/* Disable everything.  */
static const char *test_1[] =
{
  "__memcpy_avx512_no_vzeroupper",
  "__memcpy_avx512_unaligned",
  "__memcpy_avx512_unaligned_erms",
  "__memcpy_evex_unaligned",
  "__memcpy_evex_unaligned_erms",
  "__memcpy_avx_unaligned",
  "__memcpy_avx_unaligned_erms",
  "__memcpy_avx_unaligned_rtm",
  "__memcpy_avx_unaligned_erms_rtm",
  "__memcpy_ssse3",
};

static const struct test_t
{
  const char *env;
  const char *const *funcs;
  size_t nfuncs;
} tests[] =
{
  {
    /* Disable everything.  */
    "-Prefer_ERMS,-Prefer_FSRM,-AVX,-AVX2,-AVX512F,-AVX512VL,"
    "-SSE4_1,-SSE4_2,-SSSE3,-Fast_Unaligned_Load,-ERMS,"
    "-AVX_Fast_Unaligned_Load,-Avoid_Non_Temporal_Memset,"
    "-Avoid_STOSB",
    test_1,
    array_length (test_1)
  },
  {
    /* Same as before, but with some empty suboptions.  */
    ",-,-Prefer_ERMS,-Prefer_FSRM,-AVX,-AVX2,-AVX512F,-AVX512VL,"
    "-SSE4_1,-SSE4_2,-SSSE3,-Fast_Unaligned_Load,,-,"
    "-ERMS,-AVX_Fast_Unaligned_Load,-Avoid_Non_Temporal_Memset,"
    "-Avoid_STOSB,-,",
    test_1,
    array_length (test_1)
  }
};

/* Called on process re-execution.  */
_Noreturn static void
handle_restart (int ntest)
{
  struct libc_ifunc_impl impls[32];
  int cnt = __libc_ifunc_impl_list ("memcpy", impls, array_length (impls));
  if (cnt == 0)
    _exit (EXIT_SUCCESS);
  TEST_VERIFY_EXIT (cnt >= 1);
  for (int i = 0; i < cnt; i++)
    {
      for (int f = 0; f < tests[ntest].nfuncs; f++)
	{
	  if (strcmp (impls[i].name, tests[ntest].funcs[f]) == 0)
	    TEST_COMPARE (impls[i].usable, false);
	}
    }

  _exit (EXIT_SUCCESS);
}

static int
do_test (int argc, char *argv[])
{
  /* We must have either:
     - One our fource parameters left if called initially:
       + path to ld.so         optional
       + "--library-path"      optional
       + the library path      optional
       + the application name
       + the test to check  */

  TEST_VERIFY_EXIT (argc == 2 || argc == 5);

  if (restart)
    handle_restart (atoi (argv[1]));

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

  for (int i = 0; i < array_length (tests); i++)
    {
      snprintf (nteststr, sizeof nteststr, "%d", i);

      printf ("[%d] Spawned test for %s\n", i, tests[i].env);
      char *tunable = xasprintf ("glibc.cpu.hwcaps=%s", tests[i].env);
      setenv ("GLIBC_TUNABLES", tunable, 1);

      struct support_capture_subprocess result
	= support_capture_subprogram (spargv[0], spargv, NULL);
      support_capture_subprocess_check (&result, "tst-tunables", 0,
					sc_allow_stderr);
      support_capture_subprocess_free (&result);

      free (tunable);
    }

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
