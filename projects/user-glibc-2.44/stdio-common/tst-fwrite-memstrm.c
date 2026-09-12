/* Test fwrite on a memory stream.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

void
test_ro (void)
{
  FILE *f;
  char *out;

  /* Try to allocate a small buffer for this test. */
  out = malloc (2);
  TEST_VERIFY_EXIT (out != NULL);

  /* Try to open the allocated buffer as a read-only stream.  */
  f = fmemopen (out, 2, "r");
  TEST_VERIFY_EXIT (f != NULL);

  /* Try to write to the temporary file with nmemb = 0, then check that
     fwrite returns 0.  No errors are expected from this.  */
  TEST_COMPARE (fwrite ("a", 1, 0, f), 0);
  TEST_COMPARE (ferror (f), 0);

  /* Try to write to the temporary file with size = 0, then check that
     fwrite returns 0.  No errors are expected from this.  */
  TEST_COMPARE (fwrite ("a", 0, 1, f), 0);
  TEST_COMPARE (ferror (f), 0);

  /* Try to write a single byte to the temporary file, then check that
     fwrite returns 0.  Check if an error was reported.  */
  TEST_COMPARE (fwrite ("a", 1, 1, f), 0);
  TEST_COMPARE (ferror (f), 1);

  clearerr (f);
  xfclose (f);
  free (out);
}

/* Length of the output buffer in bytes. */
#define RWBUF_SIZE 16 * 1024
/* Maximum number of bytes to be written in output buffer. The rest will be
   used to check against overflow.  */
#define RWBUF_SIZE_WRITABLE RWBUF_SIZE-2048

/* Use the following byte to identify areas that should have not been
   modified.  */
#define KNOWN_BYTE 0xaa

void
test_one_rw (const char *in, size_t size, size_t nmemb,
             size_t expected_ret)
{
  FILE *f;
  char *out, *expected_out;
  /* Total number of bytes expected to be written.  */
  size_t expected_bytes = size * nmemb;

  printf ("Testing with size = %zd, nmemb = %zd\n", size, nmemb);

  TEST_VERIFY_EXIT (expected_ret <= RWBUF_SIZE_WRITABLE);
  TEST_VERIFY_EXIT (expected_bytes <= RWBUF_SIZE_WRITABLE);

  /* Try to allocate a buffer for this test and initialize it with
     known contents.  */
  out = malloc (RWBUF_SIZE);
  TEST_VERIFY_EXIT (out != NULL);
  memset (out, KNOWN_BYTE, RWBUF_SIZE);

  /* Try to allocate a buffer and fill it with the contents that are expected
     to be in memory after flushing/closing the memory stream.  */
  expected_out = malloc (RWBUF_SIZE);
  TEST_VERIFY_EXIT (expected_out != NULL);
  if (expected_bytes > 0)
    {
      memcpy (expected_out, in, expected_bytes);
      expected_out[expected_bytes] = 0;
      memset (expected_out + expected_bytes + 1, KNOWN_BYTE,
              RWBUF_SIZE - expected_bytes - 1);
    }
  else
    {
      /* No changes to the output are expected.  */
      memset (expected_out, KNOWN_BYTE, RWBUF_SIZE);
    }

  /* Try to open the allocated buffer as a read-write stream.  */
  f = fmemopen (out, RWBUF_SIZE, "w");
  TEST_VERIFY_EXIT (f != NULL);

  /* Try to write to the memory stream.  Check if fwrite() returns the
     expected value. No errors are expected.  */
  TEST_COMPARE (fwrite (in, size, nmemb, f), expected_ret);
  TEST_COMPARE (ferror (f), 0);

  xfclose (f);

  /* Ensure the output has the expected contents. */
  TEST_COMPARE (memcmp (out, expected_out, expected_bytes), 0);

  free (expected_out);
  free (out);
}

void
test_rw (void)
{
  char * in;
  int i, j;
  size_t size[] = {1, 8, 11, 16, 17, 0};
  size_t nmemb[] = {32, 83, 278, 709, 4097, RWBUF_SIZE / 2,
                    RWBUF_SIZE_WRITABLE, 0};
  size_t n;

  /* Try to write to the temporary file with nmemb = 0, then check that
     fwrite returns 0;  */
  test_one_rw ("a", 1, 0, 0);

  /* Try to write to the temporary file with size = 0, then check that
     fwrite returns 0;  */
  test_one_rw ("a", 0, 1, 0);

  /* Try to write a single byte to the temporary file, then check that
     fwrite returns 1;  */
  test_one_rw ("a", 1, 2, 2);

  in = malloc (RWBUF_SIZE);
  TEST_VERIFY_EXIT (in != NULL);
  for (i = 0; i < RWBUF_SIZE / 2; i++)
    in[i] = i % 0xff;

  /* Test with all posibilities of size[] x nmemb[].  */
  for (i = 0; nmemb[i] != 0; i++)
    {
      for (j = 0; size[j] != 0; j++)
        {
          n = nmemb[i] / size[j];
          test_one_rw (in, size[j], n, n);
        }
      /* Run the test with a single item of maximum size.  */
      test_one_rw (in, nmemb[i], 1, 1);
    }

  free (in);
}

static int
do_test (void)
{
  test_ro ();
  test_rw ();

  return 0;
}

#include <support/test-driver.c>
