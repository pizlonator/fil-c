/* Test for BZ #27650, formatted input matching beyond INT_MAX.
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

#include <error.h>
#include <errno.h>
#include <limits.h>
#include <mcheck.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <support/check.h>
#include <support/test-driver.h>

/* Produce a stream of more than INT_MAX characters via buffer BUF of
   size SIZE according to bookkeeping in COOKIE and then return EOF.  */

static ssize_t
io_read (void *cookie, char *buf, size_t size)
{
  unsigned int *written = cookie;
  unsigned int w = *written;

  if (w > INT_MAX)
    return 0;

  memset (buf, 'a', size);
  *written = w + size;
  return size;
}

/* Consume a stream of more than INT_MAX characters from an artificial
   input stream of which none is the new line character.  The call to
   fscanf is supposed to complete upon the EOF condition of input,
   however in the presence of BZ #27650 it will terminate prematurely
   with characters still outstanding in input.  Diagnose the condition
   and return status accordingly.  */

int
do_test (void)
{
  static cookie_io_functions_t io_funcs = { .read = io_read };
  unsigned int written = 0;
  FILE *in;
  int v;

  mtrace ();

  in = fopencookie (&written, "r", io_funcs);
  if (in == NULL)
    {
      FAIL ("fopencookie: %m");
      goto out;
    }

  v = fscanf (in, "%*[^\n]");
  if (ferror (in))
    {
      FAIL ("fscanf: input failure, at %u: %m", written);
      goto out_close;
    }
  else if (v == EOF)
    {
      FAIL ("fscanf: unexpected end of file, at %u", written);
      goto out_close;
    }

  if (!feof (in))
    {
      v = fgetc (in);
      if (ferror (in))
	FAIL ("fgetc: input failure: %m");
      else if (v == EOF)
	FAIL ("fgetc: unexpected end of file after missing end of file");
      else if (v == '\n')
	FAIL ("unexpected new line character received");
      else
	FAIL ("character received after end of file expected: \\x%02x", v);
    }

out_close:
  if (fclose (in) != 0)
    FAIL ("fclose: %m");

out:
  return EXIT_SUCCESS;
}

#define TIMEOUT (DEFAULT_TIMEOUT * 8)
#include <support/test-driver.c>
