/* Copyright (C) 1994-2026 Free Software Foundation, Inc.
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

#include <_itoa.h>
#include <array_length.h>
#include <assert.h>
#include <intprops.h>
#include <libintl.h>
#include <string.h>
#include <stdio.h>

extern const char *__progname;

/* This function, when passed an error number, a filename, and a line
   number, prints a message on the standard error stream of the form:
	a.c:10: foobar: Unexpected error: Computer bought the farm
   It then aborts program execution via a call to `abort'.  */
void
__assert_perror_fail (int errnum,
		      const char *file, unsigned int line,
		      const char *function)
{
  char errbuf[1024];

  const char *e = __strerror_r (errnum, errbuf, sizeof errbuf);

  char linebuf[INT_BUFSIZE_BOUND (unsigned int)];
  array_end (linebuf)[-1] = '\0';
  char *linestr = _itoa_word (line, array_end (linebuf) - 1, 10, 0);

  __libc_message (_("%s%s%s:%s: %s%sUnexpected error: %s.\n"),
		  __progname,
		  __progname[0] ? ": " : "",
		  file,
		  linestr,
		  function ? function : "",
		  function ? ": " : "",
		  e);
}
libc_hidden_def (__assert_perror_fail)
