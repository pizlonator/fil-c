/* Test feature wrapper for formatted 'vsscanf' input.
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

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <support/support.h>

static char *sscanf_buf;
static size_t sscanf_buf_size;

static void __attribute__ ((destructor))
scanf_under_test_fini (void)
{
  free (sscanf_buf);
}

static int
scanf_under_test (const char *restrict fmt, ...)
{
  size_t i = 0;
  va_list ap;
  int result;
  int ch;

  do
    {
      ch = read_input ();
      if (ch < 0)
	{
	  result = ch;
	  goto out;
	}
      if (i == sscanf_buf_size)
	{
	  sscanf_buf_size += SIZE_CHUNK;
	  /* Add an extra byte for the terminating null character.  */
	  sscanf_buf = xrealloc (sscanf_buf, sscanf_buf_size + 1);
	}
      sscanf_buf[i++] = ch;
    }
  while (ch != ':');
  sscanf_buf[i++] = '\0';

  ch = ungetc (ch, stdin);
  if (ch == EOF)
    {
      result = INPUT_ERROR;
      goto out;
    }

  va_start (ap, fmt);
  result = vsscanf (sscanf_buf, fmt, ap);
  va_end (ap);
  if (result == EOF)
    result = INPUT_EOF;

out:
  return result;
}
