/* Regression test for fmemopen bug BZ 34006.
   Copyright The GNU Toolchain Authors.
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

#include <support/xstdio.h>
#include <support/check.h>
#include <stdio.h>

static int
do_test (void)
{
  char buf[5] = "1";
  FILE *fp = xfmemopen (buf, 4, "a+");
  setbuf (fp, NULL);
  TEST_VERIFY (fseek (fp, 3, SEEK_SET) == 0);
  TEST_VERIFY (fwrite ("XXXX", 1, 4, fp) > 0);
  TEST_COMPARE_STRING (buf, "1XXX");
  fclose (fp);

  return 0;
}

#include <support/test-driver.c>
