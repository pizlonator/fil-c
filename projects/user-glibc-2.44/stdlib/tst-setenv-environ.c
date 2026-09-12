/* Test using setenv with updated environ.
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

#include <stdlib.h>
#include <support/check.h>

extern char **environ;

int
do_test (void)
{
  char *valp;
  static char *dummy_environ[] = { NULL };
  environ = dummy_environ;
  setenv ("A", "1", 0);
  valp = getenv ("A");
  TEST_VERIFY_EXIT (valp[0] == '1' && valp[1] == '\0');
  return 0;
}

#include <support/test-driver.c>
