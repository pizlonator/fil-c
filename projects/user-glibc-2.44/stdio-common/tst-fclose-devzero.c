/* Test that always-zero lseek does not cause fclose failure after fread.
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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <support/check.h>
#include <support/xstdio.h>

int
do_test (void)
{
  for (int do_ftello = 0; do_ftello < 2; ++do_ftello)
    {
      FILE *fp = xfopen ("/dev/zero", "r");
      char buf[17];
      memset (buf, 0xcc, sizeof (buf));
      xfread (buf, 1, sizeof (buf), fp);
      static const char zeros[sizeof (buf)] = { 0 };
      TEST_COMPARE_BLOB (buf, sizeof (buf), zeros, sizeof (zeros));
      if (do_ftello)
        {
          errno = 0;
          TEST_COMPARE (ftello (fp), -1);
          TEST_COMPARE (errno, ESPIPE);
        }
      /* Do not use xfclose because it flushes first.  */
      TEST_COMPARE (fclose (fp), 0);
    }

  return 0;
}

#include <support/test-driver.c>
