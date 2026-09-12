/* Test for additional fgets error handling.
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

#include <libc-diag.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <limits.h>
#include <mcheck.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <support/support.h>
#include <support/check.h>

/* This avoids compiler warnings about passing NULL where a valid
   pointer is expected.  */
static void *volatile null = NULL;

/* Implementation of our FILE stream backend.  */

static int bytes_read;
static int cookie_valid = 0;
struct Cookie {
  const char *buffer;
  int bufptr;
  int bufsz;
};

#define VALIDATE_COOKIE() if (! cookie_valid) { \
  FAIL ("call to %s after file closed", __FUNCTION__); \
  return -1;    \
  }

static ssize_t
io_read (void *vcookie, char *buf, size_t size)
{
  struct Cookie *cookie = (struct Cookie *) vcookie;

  VALIDATE_COOKIE ();

  if (size > cookie->bufsz - cookie->bufptr)
    size = cookie->bufsz - cookie->bufptr;

  memcpy (buf, cookie->buffer + cookie->bufptr, size);
  cookie->bufptr += size;
  bytes_read += size;
  return size;
}

static ssize_t
io_write (void *vcookie, const char *buf, size_t size)
{
  VALIDATE_COOKIE ();
  FAIL_EXIT1 ("io_write called");
}

static int
io_seek (void *vcookie, off64_t *position, int whence)
{
  VALIDATE_COOKIE ();
  FAIL_EXIT1 ("io_seek called");
}

static int
io_clean (void *vcookie)
{
  struct Cookie *cookie = (struct Cookie *) vcookie;

  VALIDATE_COOKIE ();

  cookie->buffer = NULL;
  cookie->bufsz = 0;
  cookie->bufptr = 0;

  cookie_valid = 0;
  free (cookie);
  return 0;
}

cookie_io_functions_t io_funcs = {
  .read = io_read,
  .write = io_write,
  .seek = io_seek,
  .close = io_clean
};

FILE *
io_open (const char *buffer, int buflen, const char *mode, void **vcookie)
{
  FILE *f;
  struct Cookie *cookie;

  cookie = (struct Cookie *) xcalloc (1, sizeof (struct Cookie));
  *vcookie = cookie;
  cookie_valid = 1;

  cookie->buffer = buffer;
  cookie->bufsz = buflen;
  bytes_read = 0;

  f = fopencookie (cookie, mode, io_funcs);
  if (f == NULL)
    FAIL_EXIT1 ("fopencookie failed");

  clearerr (f);
  return f;
}

/* The test cases.  */

#define my_open(s,l,m) io_open (s, l, m, (void *) &cookie)

#define TEST_COMPARE_0x11(buf, len)			\
  TEST_COMPARE_BLOB (buf + (len), sizeof (buf) - (len),	\
		     buf2, sizeof (buf) - (len));

#define check_flags(f, expected_eof, expected_err)	\
  {							\
    if (expected_eof)					\
      TEST_VERIFY (feof (f) != 0);			\
    else						\
      TEST_VERIFY (feof (f) == 0);			\
    if (expected_err)					\
      TEST_VERIFY (ferror (f) != 0);			\
    else						\
      TEST_VERIFY (ferror (f) == 0);			\
  }

static int
do_test (void)
{
  FILE *f;
  struct Cookie *cookie;
  char buf [10];
  char buf2 [10];
  char *returned_string;

  memset (buf2, 0x11, sizeof (buf2));

  printf ("testing base operation...\n");
  f = my_open ("hello\n", 6, "r");
  memset (buf, 0x11, sizeof (buf));
  returned_string = fgets (buf, sizeof (buf) - 1, f);
  TEST_VERIFY (returned_string == buf);
  TEST_COMPARE_BLOB (buf, bytes_read + 1, "hello\n\0", 7);
  TEST_COMPARE_0x11 (buf, bytes_read + 1);
  check_flags (f, 0, 0);

  fclose (f);

  printf ("testing zero size file...\n");
  f = my_open ("hello\n", 0, "r");
  memset (buf, 0x11, sizeof (buf));
  returned_string = fgets (buf, sizeof (buf) - 1, f);
  TEST_VERIFY (returned_string == NULL);
  TEST_VERIFY (bytes_read == 0);
  check_flags (f, 1, 0);
  fclose (f);

  printf ("testing zero size buffer...\n");
  f = my_open ("hello\n", 6, "r");
  memset (buf, 0x11, sizeof (buf));
  returned_string = fgets (buf, 0, f);
  TEST_VERIFY (returned_string == NULL);
  TEST_VERIFY (bytes_read == 0);
  check_flags (f, 0, 0);
  fclose (f);

  printf ("testing NULL buffer with empty stream...\n");
  f = my_open ("hello\n", 0, "r");
  memset (buf, 0x11, sizeof (buf));

  returned_string = fgets (null, sizeof (buf), f);

  TEST_VERIFY (returned_string == NULL);
  TEST_VERIFY (bytes_read == 0);
  check_flags (f, 1, 0);
  fclose (f);

  printf ("testing embedded NUL...\n");
  f = my_open ("hel\0lo\n", 7, "r");
  memset (buf, 0x11, sizeof (buf));
  returned_string = fgets (buf, sizeof (buf) - 1, f);
  TEST_VERIFY (returned_string == buf);
  TEST_COMPARE_BLOB (buf, bytes_read + 1, "hel\0lo\n\0", 8);
  TEST_COMPARE_0x11 (buf, bytes_read + 1);
  check_flags (f, 0, 0);
  fclose (f);

  printf ("testing writable stream...\n");
  f = my_open ("hel\0lo\n", 7, "w");
  memset (buf, 0x11, sizeof (buf));
  returned_string = fgets (buf, sizeof (buf) - 1, f);
  TEST_VERIFY (returned_string == NULL);
  TEST_VERIFY (bytes_read == 0);
  check_flags (f, 0, 1);
  fclose (f);

  printf ("testing closed fd stream...\n");
  int fd = open ("/dev/null", O_RDONLY);
  f = fdopen (fd, "r");
  close (fd);
  memset (buf, 0x11, sizeof (buf));
  returned_string = fgets (buf, sizeof (buf) - 1, f);
  TEST_VERIFY (returned_string == NULL);
  TEST_VERIFY (bytes_read == 0);
  check_flags (f, 0, 1);
  fclose (f);

#ifdef IO_DEBUG
  /* These tests only pass if glibc is built with -DIO_DEBUG, but are
     included for reference.  */

  printf ("testing NULL descriptor...\n");
  memset (buf, 0x11, sizeof (buf));
  returned_string = fgets (buf, sizeof (buf) - 1, null);
  TEST_VERIFY (returned_string == NULL);
  TEST_VERIFY (bytes_read == 0);

  printf ("testing closed descriptor...\n");
  f = my_open ("hello\n", 7, "r");
  fclose (f);
  memset (buf, 0x11, sizeof (buf));
  returned_string = fgets (buf, sizeof (buf) - 1, f);
  TEST_VERIFY (returned_string == NULL);
  TEST_VERIFY (bytes_read == 0);
#endif

  return 0;
}

#include <support/test-driver.c>
