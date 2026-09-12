/* Test feature wrapper for formatted 'vfprintf' output.
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

#include <stdarg.h>
#include <stdio.h>

static int
printf_under_test (const char *restrict fmt, ...)
{
  va_list ap;
  int result;

  va_start (ap, fmt);
  result = vfprintf (stdout, fmt, ap);
  va_end (ap);
  if (result < 0)
    perror ("vfprintf");
  return result;
}
