/* Test that the nsswitch.conf parser is locale agnostic.
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
#include <grp.h>

#include <support/support.h>
#include <support/check.h>

/* Test that the nsswitch.conf parser works correctly in a locale that
   maps ASCII characters to non-ASCII characters in case conversion.
   Bug #33519  */

static int
do_test (void)
{
  xsetlocale (LC_ALL, "tr_TR.UTF-8");

  /* Trigger parsing of nsswitch.conf.  If that fails then the use of any
     NSS function will return an error.  */
  struct group *grp = getgrgid (0);
  TEST_VERIFY (grp != NULL);

  return 0;
}

#include <support/test-driver.c>
