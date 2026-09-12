/* Test the overflow of fwrite's internal buffer.
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

/* stdio.h provides BUFSIZ, which is the size of fwrite's internal buffer.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/support.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

/* Length of the buffers in bytes.  */
#define RWBUF_SIZE (2 * BUFSIZ)

void
test_one_rw (const char *in, size_t size, size_t nmemb, size_t blocks)
{
  int fd;
  FILE *f;
  char *out;
  size_t written, to_write;
  const size_t requested = size * nmemb;

  printf ("Testing with size = %zd, nmemb = %zd, blocks = %zd\n",
	  size, nmemb, blocks);

  TEST_VERIFY_EXIT (requested <= RWBUF_SIZE);
  /* Ensure fwrite's internal buffer will overflow.  */
  TEST_VERIFY_EXIT (requested > BUFSIZ);

  /* Create a temporary file and open it for reading and writing.  */
  fd = create_temp_file ("tst-fwrite-overflow", NULL);
  TEST_VERIFY_EXIT (fd != -1);
  f = fdopen (fd, "w+");
  TEST_VERIFY_EXIT (f != NULL);

  /* Call fwrite() as many times as needed, until all data is written,
     limiting the amount of data written per call to block items.  */
  for (written = 0; written < nmemb; written += to_write)
    {
      if (written + blocks <= nmemb)
	to_write = blocks;
      else
	to_write = nmemb - written;
      /* Check if fwrite() returns the expected value.  No errors are
	 expected.  */
      TEST_COMPARE (fwrite (in + size * written, size, to_write, f),
		    to_write);
      TEST_COMPARE (ferror (f), 0);
    }
  TEST_VERIFY_EXIT (written == nmemb);

  /* Ensure all the data is flushed to file.  */
  TEST_COMPARE (fflush (f), 0);

  /* We have to check if the contents in the file are correct.  Go back to
     the beginning of the file.  */
  rewind (f);
  /* Try to allocate a buffer and save the contents of the generated file to
     it.  */
  out = xmalloc (RWBUF_SIZE);
  TEST_COMPARE (fread (out, size, nmemb, f), nmemb);

  /* Ensure the output has the expected contents.  */
  TEST_COMPARE (memcmp (out, in, requested), 0);

  xfclose (f);
  free (out);
}

static int
do_test (void)
{
  char * in;
  int i, j;
  size_t nmemb[] = {BUFSIZ + 1, RWBUF_SIZE, 0};
  /* Maximum number of items written for each fwrite call.  */
  size_t block[] = {100, 1024, 2047, 0};
  /* The largest block must fit entirely in fwrite's buffer.  */
  _Static_assert (2047 < BUFSIZ,
		  "a block must fit in fwrite's internal buffer");

  in = xmalloc (RWBUF_SIZE);
  for (i = 0; i < RWBUF_SIZE; i++)
    in[i] = i % 0xff;

  for (i = 0; nmemb[i] != 0; i++)
    for (j = 0; block[j] != 0; j++)
      {
	/* Run a test with an array of nmemb bytes.  Write at most block
	   items per fwrite call.  */
	test_one_rw (in, 1, nmemb[i], block[j]);
	/* Run a test that overflows fwrite's internal buffer in a single call
	   by writting a single item of nmemb bytes.
	   This call should not use the buffer and should be written directly
	   to the file.  */
	test_one_rw (in, nmemb[i], 1, nmemb[i]);
      }

  for (j = 0; block[j] != 0; j++)
    {
      /* Run a test with size=2 and the minimum nmemb value that still
	 overflows the buffer.  Write at most block items per fwrite call.  */
      test_one_rw (in, 2, BUFSIZ / 2 + 1, block[j]);
      /* Likewise, but size=3.  */
      test_one_rw (in, 3, BUFSIZ / 3 + 1, block[j]);
    }

  free (in);
  return 0;
}

#include <support/test-driver.c>
