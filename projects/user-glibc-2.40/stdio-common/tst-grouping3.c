/* Test printf with grouping and padding (bug 30068)
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <locale.h>
#include <stdio.h>
#include <support/check.h>
#include <support/support.h>

static int
do_test (void)
{
  char buf[80];

  xsetlocale (LC_NUMERIC, "de_DE.UTF-8");

  /* The format string has the following conversion specifier:
     '  - Use thousands grouping.
     +  - The result of a signed conversion shall begin with a sign.
     -  - Left justified.
     13 - Minimum 13 bytes of width.
     9  - Minimum 9 digits of precision.

     In bug 30068 the grouping characters were not accounted for in
     the width, and were added after the fact resulting in a 15-byte
     output instead of a 13-byte output.  The two additional bytes
     come from the locale-specific thousands separator.  This increase
     in size could result in a buffer overflow if a reasonable caller
     calculated the size of the expected buffer using nl_langinfo to
     determine the sie of THOUSEP in bytes.

     This bug is distinct from bug 23432 which has to do with the
     minimum precision calculation (digit based).  */
  sprintf (buf, "%+-'13.9d", 1234567);
  TEST_COMPARE_STRING (buf, "+001.234.567 ");

  return 0;
}

#include <support/test-driver.c>
