/* Check if popen return a correct error code if the default shell does not
   exists (BZ#29016).

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

#include <paths.h>
#include <stdio.h>
#include <errno.h>
#include <support/check.h>
#include <support/xunistd.h>

int
do_test (void)
{
  xunlink (_PATH_BSHELL);

  FILE *f = popen ("/non-existent", "r");
  TEST_VERIFY (f == NULL);
  TEST_COMPARE (errno, ENOENT);
  return 0;
}

#include <support/test-driver.c>
