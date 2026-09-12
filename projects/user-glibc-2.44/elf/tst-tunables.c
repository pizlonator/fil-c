/* Check GLIBC_TUNABLES parsing.
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
   <https://www.gnu.org/licenses/>.  */

#include <array_length.h>
/* The test uses the tunable_list size, which is only exported for
   ld.so.  This will result in a copy of tunable_list, which is ununsed by
   the test itself.  */
#define TUNABLES_INTERNAL 1
#include <dl-tunables.h>
#include <getopt.h>
#include <intprops.h>
#include <stdint.h>
#include <stdlib.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/support.h>

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

static struct test_t
{
  const char *name;
  const char *value;
  int32_t expected_malloc_check;
  size_t expected_mmap_threshold;
  int32_t expected_perturb;
} tests[] =
{
  /* Expected tunable format.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2",
    2,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2:glibc.malloc.mmap_threshold=4096",
    2,
    4096,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.mmap_threshold=-1",
    0,
    SIZE_MAX,
    0,
  },
  /* Empty tunable are ignored.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2::glibc.malloc.mmap_threshold=4096",
    2,
    4096,
    0,
  },
  /* As well empty values.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=:glibc.malloc.mmap_threshold=4096",
    0,
    4096,
    0,
  },
  /* Tunable are processed from left to right, so last one is the one set.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=1:glibc.malloc.check=2",
    2,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=1:glibc.malloc.check=2:glibc.malloc.mmap_threshold=4096",
    2,
    4096,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2:glibc.malloc.mmap_threshold=4096:glibc.malloc.check=1",
    1,
    4096,
    0,
  },
  /* 0x800 is larger than tunable maxval (0xff), so the tunable is unchanged.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.perturb=0x800",
    0,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.perturb=0x55",
    0,
    0,
    0x55,
  },
  /* Out of range values are just ignored.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.perturb=0x800:glibc.malloc.mmap_threshold=4096",
    0,
    4096,
    0,
  },
  /* Invalid keys are ignored.  */
  {
    "GLIBC_TUNABLES",
    ":glibc.malloc.garbage=2:glibc.malloc.check=1",
    1,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.perturb=0x800:not_valid.malloc.check=2:glibc.malloc.mmap_threshold=4096",
    0,
    4096,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.not_valid.check=2:glibc.malloc.mmap_threshold=4096",
    0,
    4096,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "not_valid.malloc.check=2:glibc.malloc.mmap_threshold=4096",
    0,
    4096,
    0,
  },
  /* Invalid subkeys are ignored.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.garbage=2:glibc.maoc.mmap_threshold=4096:glibc.malloc.check=2",
    2,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=4:glibc.malloc.garbage=2:glibc.maoc.mmap_threshold=4096",
    0,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "not_valid.malloc.check=2",
    0,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.not_valid.check=2",
    0,
    0,
    0,
  },
  /* An ill-formatted tunable in the for key=key=value will considere the
     value as 'key=value' (which can not be parsed as an integer).  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.mmap_threshold=glibc.malloc.mmap_threshold=4096",
    0,
    0,
    0,
  },
  /* Ill-formatted tunables string is not parsed.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.mmap_threshold=glibc.malloc.mmap_threshold=4096:glibc.malloc.check=2",
    0,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2=2",
    0,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2=2:glibc.malloc.mmap_threshold=4096",
    0,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2=2:glibc.malloc.check=2",
    0,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2:glibc.malloc.mmap_threshold=4096=4096",
    0,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2:glibc.malloc.mmap_threshold=4096=4096",
    0,
    0,
    0,
  },
  /* Invalid numbers are ignored.  */
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=abc:glibc.malloc.mmap_threshold=4096",
    0,
    4096,
    0,
  },
  {
    "GLIBC_TUNABLES",
    "glibc.malloc.check=2:glibc.malloc.mmap_threshold=abc",
    2,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    /* SIZE_MAX + 1 */
    "glibc.malloc.mmap_threshold=18446744073709551616",
    0,
    0,
    0,
  },
  /* Also check some tunable aliases.  */
  {
    "MALLOC_CHECK_",
    "2",
    2,
    0,
    0,
  },
  {
    "MALLOC_MMAP_THRESHOLD_",
    "4096",
    0,
    4096,
    0,
  },
  {
    "MALLOC_PERTURB_",
    "0x55",
    0,
    0,
    0x55,
  },
  /* 0x800 is larger than tunable maxval (0xff), so the tunable is unchanged.  */
  {
    "MALLOC_PERTURB_",
    "0x800",
    0,
    0,
    0,
  },
  /* Also check for repeated tunables with a count larger than the total number
     of tunables.  */
  {
    "GLIBC_TUNABLES",
    NULL,
    2,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    NULL,
    1,
    0,
    0,
  },
  {
    "GLIBC_TUNABLES",
    NULL,
    0,
    0,
    0,
  },
};

static int
handle_restart (int i)
{
  TEST_COMPARE (tests[i].expected_malloc_check,
		TUNABLE_GET_FULL (glibc, malloc, check, int32_t, NULL));
  TEST_COMPARE (tests[i].expected_mmap_threshold,
		TUNABLE_GET_FULL (glibc, malloc, mmap_threshold, size_t, NULL));
  TEST_COMPARE (tests[i].expected_perturb,
		TUNABLE_GET_FULL (glibc, malloc, perturb, int32_t, NULL));
  return 0;
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

  /* Create a tunable line with the duplicate values with a total number
     larger than the different number of tunables.  */
  {
    enum { tunables_list_size = array_length (tunable_list) };
    const char *value = "";
    for (int i = 0; i < tunables_list_size; i++)
      value = xasprintf ("%sglibc.malloc.check=2%c",
			 value,
			 i == (tunables_list_size - 1) ? '\0' : ':');
    tests[33].value = value;
  }
  /* Same as before, but the last tunable values is differen than the
     rest.  */
  {
    enum { tunables_list_size = array_length (tunable_list) };
    const char *value = "";
    for (int i = 0; i < tunables_list_size - 1; i++)
      value = xasprintf ("%sglibc.malloc.check=2:", value);
    value = xasprintf ("%sglibc.malloc.check=1", value);
    tests[34].value = value;
  }
  /* Same as before, but with an invalid last entry.  */
  {
    enum { tunables_list_size = array_length (tunable_list) };
    const char *value = "";
    for (int i = 0; i < tunables_list_size - 1; i++)
      value = xasprintf ("%sglibc.malloc.check=2:", value);
    value = xasprintf ("%sglibc.malloc.check=1=1", value);
    tests[35].value = value;
  }

  for (int i = 0; i < array_length (tests); i++)
    {
      snprintf (nteststr, sizeof nteststr, "%d", i);

      printf ("[%d] Spawned test for %s=%s\n",
	      i,
	      tests[i].name,
	      tests[i].value);
      setenv (tests[i].name, tests[i].value, 1);
      struct support_capture_subprocess result
	= support_capture_subprogram (spargv[0], spargv, NULL);
      support_capture_subprocess_check (&result, "tst-tunables", 0,
					sc_allow_stderr);
      support_capture_subprocess_free (&result);
      unsetenv (tests[i].name);
    }

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
