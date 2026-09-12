/* Test getline.
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
#include <fcntl.h>
#include <malloc.h>
#include <mcheck.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <support/check.h>
#include <support/test-driver.h>
#include <support/support.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

static struct test_data
{
  /* Input test data for fopencookie stream.  */
  const char *in_data;

  /* The amount of test data left.  */
  size_t in_data_left;

  /* Error number for forcing an error on next read.  */
  int in_error;

  /* Error number for forcing an error (rather than EOF) after all
     bytes read.  */
  int in_error_after;
} the_cookie;

/* Produce a stream of test data based on data in COOKIE, storing up
   to SIZE bytes in BUF.  */

static ssize_t
io_read (void *cookie, char *buf, size_t size)
{
  struct test_data *p = cookie;
  if (p->in_error)
    {
      errno = p->in_error;
      return -1;
    }
  if (size > p->in_data_left)
    size = p->in_data_left;
  memcpy (buf, p->in_data, size);
  p->in_data += size;
  p->in_data_left -= size;
  if (p->in_data_left == 0)
    p->in_error = p->in_error_after;
  return size;
}

/* Set up a test stream with fopencookie.  */

static FILE *
open_test_stream (const char *in_data, size_t size)
{
  static cookie_io_functions_t io_funcs = { .read = io_read };
  the_cookie.in_data = in_data;
  the_cookie.in_data_left = size;
  the_cookie.in_error = 0;
  the_cookie.in_error_after = 0;
  FILE *fp = fopencookie (&the_cookie, "r", io_funcs);
  TEST_VERIFY_EXIT (fp != NULL);
  return fp;
}

/* Set up a test stream with fopencookie, using data from a string
   literal.  */
#define OPEN_TEST_STREAM(IN_DATA) open_test_stream (IN_DATA, sizeof (IN_DATA))

/* Wrap getline to verify that (as per the glibc manual), *LINEPTR is
   returned as non-null and with at least *N bytes (even on error or
   EOF).  Also clear errno for the benefit of tests that check the
   value of errno after the call.  */

ssize_t
wrap_getline (char **lineptr, size_t *n, FILE *stream)
{
  errno = 0;
  ssize_t ret = getline (lineptr, n, stream);
  if (lineptr != NULL && n != NULL)
    {
      TEST_VERIFY (*lineptr != NULL);
      TEST_VERIFY (malloc_usable_size (*lineptr) >= *n);
    }
  return ret;
}

int
do_test (void)
{
  FILE *fp;
  char *lineptr = NULL;
  size_t size = 0;
  ssize_t ret;
  mtrace ();
  /* Test failure with EINVAL (and error indicator for stream set) if
     lineptr is a null pointer.  */
  verbose_printf ("Testing lineptr == NULL\n");
  fp = OPEN_TEST_STREAM ("test");
  ret = wrap_getline (NULL, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, EINVAL);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  fclose (fp);
  /* Test failure with EINVAL (and error indicator for stream set) if
     n is a null pointer.  */
  verbose_printf ("Testing n == NULL\n");
  fp = OPEN_TEST_STREAM ("test");
  ret = wrap_getline (&lineptr, NULL, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, EINVAL);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  fclose (fp);
  /* Test failure with EINVAL (and error indicator for stream set) if
     both lineptr and n are null pointers.  */
  verbose_printf ("Testing lineptr == NULL and n == NULL\n");
  fp = OPEN_TEST_STREAM ("test");
  ret = wrap_getline (NULL, NULL, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, EINVAL);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  fclose (fp);
  /* Test normal line, fitting in available space (including case with
     null bytes).  */
  verbose_printf ("Testing normal nonempty input\n");
  lineptr = xmalloc (10);
  size = 10;
  fp = OPEN_TEST_STREAM ("foo\nbar\0\n\0baz\nte\0st\n");
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "foo\n", 5);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "bar\0\n", 6);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "\0baz\n", 6);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 6);
  TEST_COMPARE_BLOB (lineptr, 7, "te\0st\n", 7);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 1);
  TEST_COMPARE_BLOB (lineptr, 1, "", 1);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (!!feof (fp), 1);
  fclose (fp);
  /* Test normal line, with reallocation (including case with null bytes).  */
  verbose_printf ("Testing normal nonempty input with reallocation\n");
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = OPEN_TEST_STREAM ("foo\nbar\0\n\0baz\nte\0st\n"
			 "foo\nbar\0\n\0baz\nte\0st\n");
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "foo\n", 5);
  free (lineptr);
  lineptr = NULL;
  size = 0;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "bar\0\n", 6);
  free (lineptr);
  lineptr = NULL;
  size = 0;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "\0baz\n", 6);
  free (lineptr);
  lineptr = NULL;
  size = 0;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 6);
  TEST_COMPARE_BLOB (lineptr, 7, "te\0st\n", 7);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "foo\n", 5);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "bar\0\n", 6);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "\0baz\n", 6);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 6);
  TEST_COMPARE_BLOB (lineptr, 7, "te\0st\n", 7);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 1);
  TEST_COMPARE_BLOB (lineptr, 1, "", 1);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (!!feof (fp), 1);
  fclose (fp);
  /* Test EOF before delimiter but after some bytes read, fitting in
     available space (including case with null bytes).  */
  verbose_printf ("Testing EOF before delimiter\n");
  free (lineptr);
  lineptr = xmalloc (10);
  size = 10;
  fp = open_test_stream ("foo", 3);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 3);
  TEST_COMPARE_BLOB (lineptr, 4, "foo", 4);
  fclose (fp);
  free (lineptr);
  lineptr = xmalloc (10);
  size = 10;
  fp = open_test_stream ("bar\0", 4);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "bar\0", 5);
  fclose (fp);
  free (lineptr);
  lineptr = xmalloc (10);
  size = 10;
  fp = open_test_stream ("\0baz", 4);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "\0baz", 5);
  fclose (fp);
  free (lineptr);
  lineptr = xmalloc (10);
  size = 10;
  fp = open_test_stream ("te\0st", 5);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "te\0st", 6);
  fclose (fp);
  /* Test EOF before delimiter but after some bytes read, with
     reallocation (including case with null bytes).  */
  verbose_printf ("Testing EOF before delimiter with reallocation\n");
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = open_test_stream ("foo", 3);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 3);
  TEST_COMPARE_BLOB (lineptr, 4, "foo", 4);
  fclose (fp);
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = open_test_stream ("bar\0", 4);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "bar\0", 5);
  fclose (fp);
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = open_test_stream ("\0baz", 4);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "\0baz", 5);
  fclose (fp);
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = open_test_stream ("te\0st", 5);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "te\0st", 6);
  fclose (fp);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  fp = open_test_stream ("foo", 3);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 3);
  TEST_COMPARE_BLOB (lineptr, 4, "foo", 4);
  fclose (fp);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  fp = open_test_stream ("bar\0", 4);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "bar\0", 5);
  fclose (fp);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  fp = open_test_stream ("\0baz", 4);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 4);
  TEST_COMPARE_BLOB (lineptr, 5, "\0baz", 5);
  fclose (fp);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  fp = open_test_stream ("te\0st", 5);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE_BLOB (lineptr, 6, "te\0st", 6);
  fclose (fp);
  /* Test EOF with no bytes read (nothing is specified about anything
     written to the buffer), including EOF again when already at end
     of file.  */
  verbose_printf ("Testing EOF with no bytes read\n");
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = open_test_stream ("", 0);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (!!feof (fp), 1);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (!!feof (fp), 1);
  fclose (fp);
  free (lineptr);
  lineptr = xmalloc (1);
  size = 1;
  fp = open_test_stream ("", 0);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (!!feof (fp), 1);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (!!feof (fp), 1);
  fclose (fp);
  /* Test error occurring with no bytes read, including calling
     wrap_getline again while the file is in error state.  */
  verbose_printf ("Testing error with no bytes read\n");
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = open_test_stream ("", 0);
  the_cookie.in_error = EINVAL;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, EINVAL);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  /* Make sure error state is sticky.  */
  the_cookie.in_error = 0;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  fclose (fp);
  /* Test error occurring after some bytes read.  Specifications are
     ambiguous here; at least in the fopencookie case used for
     testing, glibc returns the partial line (but with the error
     indicator on the stream set).  */
  verbose_printf ("Testing error after some bytes read\n");
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = open_test_stream ("foo", 3);
  the_cookie.in_error_after = EINVAL;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, 3);
  TEST_COMPARE_BLOB (lineptr, 4, "foo", 4);
  TEST_COMPARE (errno, EINVAL);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  /* Make sure error state is sticky.  */
  the_cookie.in_error = 0;
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  fclose (fp);
  /* Test EBADF error as a representative example of an fgetc error
     resulting in an error from wrap_getline.  We don't try to cover all
     error cases for fgetc here.  */
  verbose_printf ("Testing EBADF error\n");
  free (lineptr);
  lineptr = NULL;
  size = 0;
  fp = xfopen ("/dev/null", "r");
  xclose (fileno (fp));
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  fclose (fp);
  /* Test EAGAIN error as an example of an fgetc error on a valid file
     descriptor.  */
  verbose_printf ("Testing EAGAIN error\n");
  free (lineptr);
  lineptr = NULL;
  size = 0;
  int pipefd[2];
  xpipe (pipefd);
  ret = fcntl (pipefd[0], F_SETFL, O_NONBLOCK);
  TEST_VERIFY_EXIT (ret == 0);
  fp = fdopen (pipefd[0], "r");
  TEST_VERIFY_EXIT (fp != NULL);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, EAGAIN);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  /* Make sure error state is sticky (even after more data is
     available to read).  */
  xwrite (pipefd[1], "x\n", 2);
  ret = wrap_getline (&lineptr, &size, fp);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (!!ferror (fp), 1);
  TEST_COMPARE (feof (fp), 0);
  fclose (fp);
  free (lineptr);
  return 0;
}

#include <support/test-driver.c>
