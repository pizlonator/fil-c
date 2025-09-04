/* Test strftime and strptime after 2038-01-19 03:14:07 UTC (bug 30053).
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

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>

static int
do_test (void)
{
  TEST_VERIFY_EXIT (setenv ("TZ", "UTC0", 1) == 0);
  tzset ();
  if (sizeof (time_t) > 4)
    {
      time_t wrap = (time_t) 2147483648LL;
      char buf[80];
      struct tm *tm = gmtime (&wrap);
      TEST_VERIFY_EXIT (tm != NULL);
      TEST_VERIFY_EXIT (strftime (buf, sizeof buf, "%s", tm) > 0);
      puts (buf);
      TEST_VERIFY (strcmp (buf, "2147483648") == 0);

      struct tm tm2;
      char *p = strptime (buf, "%s", &tm2);
      TEST_VERIFY_EXIT (p != NULL && *p == '\0');
      time_t t = mktime (&tm2);
      printf ("%lld\n", (long long) t);
      TEST_VERIFY (t == wrap);
    }
  else
    FAIL_UNSUPPORTED ("32-bit time_t");
  return 0;
}

#include <support/test-driver.c>
