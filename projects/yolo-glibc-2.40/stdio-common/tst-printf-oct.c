/* Test printf width padding with octal format
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

#include <stdio.h>
#include <support/check.h>
#include <support/support.h>

static int
do_test (void)
{
  char buf[80];

  sprintf (buf, "%-7o", 123);
  TEST_COMPARE_STRING (buf, "173    ");

  sprintf (buf, "%#-7o", 123);
  TEST_COMPARE_STRING (buf, "0173   ");

  sprintf (buf, "%7o", 123);
  TEST_COMPARE_STRING (buf, "    173");

  sprintf (buf, "%#7o", 123);
  TEST_COMPARE_STRING (buf, "   0173");

  sprintf (buf, "%07o", 123);
  TEST_COMPARE_STRING (buf, "0000173");

  sprintf (buf, "%#07o", 123);
  TEST_COMPARE_STRING (buf, "0000173");

  return 0;
}

#include <support/test-driver.c>
