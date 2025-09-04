/* libc-internal assert that calls __libc_message.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <intprops.h>
#include <stdio.h>

void
__libc_assert_fail (const char *assertion, const char *file, unsigned int line,
                    const char *function)
{
  char linebuf[INT_BUFSIZE_BOUND (unsigned int)];
  array_end (linebuf)[-1] = '\0';
  char *linestr = _itoa_word (line, array_end (linebuf) - 1, 10, 0);
  __libc_message ("Fatal glibc error: %s:%s (%s): assertion failed: %s\n",
                  file, linestr, function, assertion);
}
