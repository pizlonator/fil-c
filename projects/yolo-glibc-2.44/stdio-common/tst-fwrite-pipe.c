/* Test if fwrite returns EPIPE.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <support/check.h>
#include <support/xstdio.h>
#include <support/xsignal.h>
#include <support/xunistd.h>

/* Usually this test reproduces in a few iterations.  However, keep a high
   number of iterations in order to avoid return false-positives due to an
   overwhelmed/slow system.  */
#define ITERATIONS 500000

#define BUFFERSIZE 20

/* When the underlying write () fails with EPIPE, fwrite () is expected to
   return an error by returning < nmemb and keeping errno=EPIPE.  */

static int
do_test (void)
{
  int fd[2];
  pid_t p;
  FILE *f;
  size_t written;
  int ret = 1;  /* Return failure by default.  */

  /* Try to create a pipe.  */
  xpipe (fd);

  p = xfork ();
  if (p == 0)
    {
      char b[BUFFERSIZE];
      size_t bytes;

      /* Read at least the first line from the pipe before closing it.
	 This is important because it guarantees the file stream will have
	 flag _IO_CURRENTLY_PUTTING set, which triggers important parts of
	 the code that flushes lines from fwrite's internal buffer.  */
      do {
	bytes = read (fd[0], b, BUFFERSIZE);
      } while(bytes > 0 && memrchr (b, '\n', bytes) == NULL);

      /* Child closes both ends of the pipe in order to trigger an EPIPE
	 error on the parent.  */
      xclose (fd[0]);
      xclose (fd[1]);

      return 0;
    }
  else
    {
      /* Ensure the string we send has a new line because we're dealing
         with a lined-buffered stream.  */
      const char *s = "hello world\n";
      size_t len = strlen (s);
      int i;

      /* Parent only writes to pipe.
	 Close the unused read end of the pipe.  */
      xclose (fd[0]);

      /* Ignore SIGPIPE in order to catch the EPIPE returned by the
	 underlying call to write().  */
      xsignal(SIGPIPE, SIG_IGN);

      /* Create a file stream associated with the write end of the pipe.  */
      f = fdopen (fd[1], "w");
      TEST_VERIFY_EXIT (f != NULL);
      /* Ensure that fwrite buffers the output before writing to the pipe.  */
      setlinebuf (f);

      /* Ensure errno is not set before starting.  */
      errno = 0;
      for (i = 1; i <= ITERATIONS; i++)
	{
	  /* Try to write to the pipe.  The first calls are expected to
	     suceeded until the child process closes the read end.
	     After that, fwrite () is expected to fail and errno should be
	     set to EPIPE.  */
	  written = fwrite (s, 1, len, f);

	  if (written == len)
	    {
	      TEST_VERIFY_EXIT (ferror (f) == 0);
	      TEST_VERIFY_EXIT (errno == 0);
	    }
	  else
	    {
	      /* An error happened.  Check if ferror () does return an error
		 and that it is indeed EPIPE.  */
	      TEST_COMPARE (ferror (f), 1);
	      TEST_COMPARE (errno, EPIPE);
	      /* The test succeeded!  Clear the error from the file stream and
		 return success.  */
	      clearerr (f);
	      ret = 0;
              break;
	    }
	}

      xfclose (f);
    }

  if (ret)
    FAIL_RET ("fwrite should have returned an error, but it didn't.\n");

  return ret;
}

#include <support/test-driver.c>
