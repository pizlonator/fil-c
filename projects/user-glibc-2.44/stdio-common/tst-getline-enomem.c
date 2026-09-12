/* Test getline: ENOMEM on allocation failure.
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

#include <errno.h>
#include <mcheck.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

#include <support/check.h>
#include <support/test-driver.h>

/* Produce a stream of test data based on data in COOKIE (ignored),
   storing up to SIZE bytes in BUF.  */

static ssize_t
io_read (void *cookie, char *buf, size_t size)
{
  memset (buf, 'x', size);
  return size;
}

/* Set up a test stream with fopencookie.  */

static FILE *
open_test_stream (void)
{
  static cookie_io_functions_t io_funcs = { .read = io_read };
  static int cookie;
  FILE *fp = fopencookie (&cookie, "r", io_funcs);
  TEST_VERIFY_EXIT (fp != NULL);
  return fp;
}

int
do_test (void)
{
  FILE *fp;
  char *lineptr = NULL;
  size_t size = 0;
  ssize_t ret;
  mtrace ();
  /* Test ENOMEM (and error indicator for stream set) for memory
     allocation failure.  */
  verbose_printf ("Testing memory allocation failure\n");
  fp = open_test_stream ();
  struct rlimit limit;
  TEST_VERIFY_EXIT (getrlimit (RLIMIT_AS, &limit) == 0);
  limit.rlim_cur = 32 * 1024 * 1024;
  TEST_VERIFY_EXIT (setrlimit (RLIMIT_AS, &limit) == 0);
  errno = 0;
  ret = getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, ENOMEM);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  free (lineptr);
  fclose (fp);
  return 0;
}

#include <support/test-driver.c>
