/* Copyright (C) 1991-2026 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <intprops.h>
#include <ldsodefs.h>
#include <libc-pointer-arith.h>
#include <libintl.h>
#include <stdio.h>


extern const char *__progname;

#undef __assert_fail
void
__assert_fail (const char *assertion, const char *file, unsigned int line,
	       const char *function)
{
  char linebuf[INT_BUFSIZE_BOUND (unsigned int)];
  array_end (linebuf)[-1] = '\0';
  char *linestr = _itoa_word (line, array_end (linebuf) - 1, 10, 0);

  __libc_assert (_("%s%s%s:%s: %s%sAssertion `%s' failed.\n"),
		 __progname,
		 __progname[0] ? ": " : "",
		 file,
		 linestr,
		 function ? function : "",
		 function ? ": " : "",
		 assertion);
}
