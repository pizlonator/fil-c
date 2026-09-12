/* Test errno handling in getrandom (bug 32440).
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

#include <errno.h>
#include <stdlib.h>
#include <support/check.h>
#include <sys/random.h>

static
int do_test (void)
{
  errno = -1181968554;          /* Just a random value.  */
  char buf[4];
  int ret = getrandom (buf, sizeof (buf), -1); /* All flags set.  */
  if (errno != ENOSYS)
    TEST_COMPARE (errno, EINVAL);
  TEST_COMPARE (ret, -1);

  return 0;
}

#include <support/test-driver.c>
