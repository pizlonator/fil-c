/* Test fwrite against bug 29459.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

/* This test is based on the code attached to bug 29459.
   It depends on stdout being redirected to a specific process via a script
   with the same name.  Because of this, we cannot use the features from
   test_driver.c.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <support/check.h>
#include <support/xsignal.h>

/* Usually this test reproduces in a few iterations.  However, keep a high
   number of iterations in order to avoid return false-positives due to an
   overwhelmed/slow system.  */
#define ITERATIONS 500000

/* The goal of this test is to use fwrite () on a redirected and closed
   stdout.  A script will guarantee that stdout is redirected to another
   process that closes it during the execution.  The process reading from
   the pipe must read at least the first line in order to guarantee that
   flag _IO_CURRENTLY_PUTTING is set in the write end of the pipe, triggering
   important parts of the code that flushes lines from fwrite's internal
   buffer.  The underlying write () returns EPIPE, which fwrite () must
   propagate.  */

int
main (void)
{
  int i;
  size_t rc;
  /* Ensure the string we send has a new line because we're dealing
     with a lined-buffered stream.  */
  const char *s = "hello world\n";
  const size_t len = strlen(s);

  /* Ensure that fwrite buffers the output before writing to stdout.  */
  setlinebuf(stdout);
  /* Ignore SIGPIPE in order to catch the EPIPE returned by the
     underlying call to write().  */
  xsignal(SIGPIPE, SIG_IGN);

  for (i = 1; i <= ITERATIONS; i++)
    {
      /* Keep writing to stdout.  The test succeeds if fwrite () returns an
         error.  */
      if ((rc = fwrite(s, 1, len, stdout)) < len)
	{
	  /* An error happened.  Check if ferror () does return an error
	     and that it is indeed EPIPE.  */
	  TEST_COMPARE (ferror (stdout), 1);
	  TEST_COMPARE (errno, EPIPE);
	  fprintf(stderr, "Success: i=%d. fwrite returned %zu < %zu "
			  "and errno=EPIPE\n",
		  i, rc, len);
	  /* The test succeeded!  */
	  return 0;
	}
      else
	{
	  /* fwrite () was able to write all the contents.  Check if no errors
	     have been reported and try again.  */
	  TEST_COMPARE (ferror (stdout), 0);
	  TEST_COMPARE (errno, 0);
	}
    }

  fprintf(stderr, "Error: fwrite did not return an error\n");
  return 1;
}
