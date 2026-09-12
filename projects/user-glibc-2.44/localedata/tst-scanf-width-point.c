/* Verify multibyte decimal point extending beyond scanf field width.
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

#include <locale.h>
#include <stdio.h>
#include <string.h>

#include <libc-diag.h>
#include <support/check.h>
#include <support/xstdio.h>

#define PD "\xd9\xab"

static int
do_test (void)
{
  if (setlocale (LC_ALL, "ps_AF.UTF-8") == NULL)
    FAIL_EXIT1 ("setlocale (LC_ALL, \"ps_AF.UTF-8\")");

  char s[] = "1" PD;
  FILE *f = xfmemopen (s, strlen (s), "r");

  /* This should succeed parsing a floating-point number, and leave '\xd9',
     '\xab' in the input.  */
  double d;
  int c;
  TEST_VERIFY_EXIT (fscanf (f, "%2lf%n", &d, &c) == 1);
  TEST_VERIFY_EXIT (d == 1.0);
  TEST_VERIFY_EXIT (c == 1);
  TEST_VERIFY_EXIT (fgetc (f) == 0xd9);
  TEST_VERIFY_EXIT (fgetc (f) == 0xab);

  xfclose (f);

  return 0;
}

#include <support/test-driver.c>
