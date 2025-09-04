/* Test printf with grouping and large width (bug 29530)
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

#include <locale.h>
#include <stdio.h>
#include <support/check.h>
#include <support/support.h>

static int
do_test (void)
{
  const int field_width = 1000;
  char buf[field_width + 1];

  xsetlocale (LC_NUMERIC, "de_DE.UTF-8");

  /* This used to crash in group_number.  */
  TEST_COMPARE (sprintf (buf, "%'*d", field_width, 1000), field_width);
  TEST_COMPARE_STRING (buf + field_width - 6, " 1.000");

  return 0;
}

#include <support/test-driver.c>
