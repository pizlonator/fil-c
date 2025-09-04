/* Check that C.UTF-8 uses C translations (bug 16621).
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>

static int
do_test (void)
{
  if (setenv ("LANGUAGE", "de_DE.UTF-8", 1) != 0)
    FAIL_EXIT1 ("setenv");
  if (setenv ("LC_ALL", "C.UTF-8", 1) != 0)
    FAIL_EXIT1 ("setenv");
  xsetlocale (LC_ALL, "");
  TEST_COMPARE_STRING (strerror (ENOENT), "No such file or directory");
  return 0;
}

#include <support/test-driver.c>
