/* Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

/* Verify that GLIBC_TUNABLES is kept unchanged but no tunable is actually
   enabled for AT_SECURE processes.  */

#include <dl-tunables.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <intprops.h>
#include <array_length.h>

#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <support/capture_subprocess.h>

static const char *teststrings[] =
{
  "glibc.malloc.check=2:glibc.malloc.mmap_threshold=4096",
  "glibc.malloc.check=2:glibc.malloc.check=2:glibc.malloc.mmap_threshold=4096",
  "glibc.malloc.check=2:glibc.malloc.mmap_threshold=4096:glibc.malloc.check=2",
  "glibc.malloc.perturb=0x800",
  "glibc.malloc.perturb=0x800:glibc.malloc.mmap_threshold=4096",
  "glibc.malloc.perturb=0x800:not_valid.malloc.check=2:glibc.malloc.mmap_threshold=4096",
  "glibc.not_valid.check=2:glibc.malloc.mmap_threshold=4096",
  "not_valid.malloc.check=2:glibc.malloc.mmap_threshold=4096",
  "glibc.malloc.mmap_threshold=glibc.malloc.mmap_threshold=4096",
  "glibc.malloc.check=2",
  "glibc.malloc.garbage=2:glibc.maoc.mmap_threshold=4096:glibc.malloc.check=2",
  "glibc.malloc.check=4:glibc.malloc.garbage=2:glibc.maoc.mmap_threshold=4096",
  ":glibc.malloc.garbage=2:glibc.malloc.check=1",
  "glibc.malloc.check=1:glibc.malloc.check=2",
  "not_valid.malloc.check=2",
  "glibc.not_valid.check=2",
};

static int
test_child (int off)
{
  const char *val = getenv ("GLIBC_TUNABLES");
  int ret = 1;

  printf ("    [%d] GLIBC_TUNABLES is %s\n", off, val);
  fflush (stdout);
  if (val != NULL)
    printf ("    [%d] Unexpected GLIBC_TUNABLES VALUE %s\n", off, val);
  else
    ret = 0;
  fflush (stdout);

  /* Also check if the set tunables are effectively unchanged.  */
  int32_t check = TUNABLE_GET_FULL (glibc, malloc, check, int32_t, NULL);
  size_t mmap_threshold = TUNABLE_GET_FULL (glibc, malloc, mmap_threshold,
					    size_t, NULL);
  int32_t perturb = TUNABLE_GET_FULL (glibc, malloc, perturb, int32_t, NULL);

  printf ("    [%d] glibc.malloc.check=%d\n", off, check);
  fflush (stdout);
  printf ("    [%d] glibc.malloc.mmap_threshold=%zu\n", off, mmap_threshold);
  fflush (stdout);
  printf ("    [%d] glibc.malloc.perturb=%d\n", off, perturb);
  fflush (stdout);

  ret |= check != 0;
  ret |= mmap_threshold != 0;
  ret |= perturb != 0;

  return ret;
}

static int
do_test (int argc, char **argv)
{
  /* Setgid child process.  */
  if (argc == 2)
    {
      if (getgid () == getegid ())
	/* This can happen if the file system is mounted nosuid.  */
	FAIL_UNSUPPORTED ("SGID failed: GID and EGID match (%jd)\n",
			  (intmax_t) getgid ());

      int ret = test_child (atoi (argv[1]));

      if (ret != 0)
	exit (1);

      /* Special return code to make sure that the child executed all the way
	 through.  */
      exit (42);
    }
  else
    {
      /* Spawn tests.  */
      for (int i = 0; i < array_length (teststrings); i++)
	{
	  char buf[INT_BUFSIZE_BOUND (int)];

	  printf ("[%d] Spawned test for %s\n", i, teststrings[i]);
	  snprintf (buf, sizeof (buf), "%d\n", i);
	  fflush (stdout);
	  if (setenv ("GLIBC_TUNABLES", teststrings[i], 1) != 0)
	    {
	      printf ("    [%d] Failed to set GLIBC_TUNABLES: %m", i);
	      support_record_failure ();
	      continue;
	    }

	  int status = support_capture_subprogram_self_sgid (buf);

	  /* Bail out early if unsupported.  */
	  if (WEXITSTATUS (status) == EXIT_UNSUPPORTED)
	    return EXIT_UNSUPPORTED;

	  if (WEXITSTATUS (status) != 42)
	    {
	      printf ("    [%d] child failed with status %d\n", i,
		      WEXITSTATUS (status));
	      support_record_failure ();
	    }
	}
      return 0;
    }
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
