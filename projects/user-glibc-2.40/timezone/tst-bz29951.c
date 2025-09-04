/* Check that daylight is set if the last DST transition did not change offset.
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

#include <errno.h>
#include <stdlib.h>
#include <support/check.h>
#include <time.h>

/* Set the specified time zone with error checking.  */
static void
set_timezone (const char *name)
{
  TEST_VERIFY (setenv ("TZ", name, 1) == 0);
  errno = 0;
  tzset ();
  TEST_COMPARE (errno, 0);
}

static int
do_test (void)
{
  /* Test zone based on tz-2022g version of Africa/Tripoli.  The last
     DST transition coincided with a change in the standard time
     offset, effectively making it a no-op.

     Africa/Tripoli  Thu Oct 24 23:59:59 2013 UT
       = Fri Oct 25 01:59:59 2013 CEST isdst=1 gmtoff=7200
     Africa/Tripoli  Fri Oct 25 00:00:00 2013 UT
       = Fri Oct 25 02:00:00 2013 EET isdst=0 gmtoff=7200
   */
  set_timezone ("XT6");
  TEST_VERIFY (daylight != 0);
  TEST_COMPARE (timezone, -7200);

  /* Check that localtime re-initializes the two variables.  */
  daylight = timezone = 17;
  time_t t = 844034401;
  struct tm *tm = localtime (&t);
  TEST_VERIFY (daylight != 0);
  TEST_COMPARE (timezone, -7200);
  TEST_COMPARE (tm->tm_year, 96);
  TEST_COMPARE (tm->tm_mon, 8);
  TEST_COMPARE (tm->tm_mday, 29);
  TEST_COMPARE (tm->tm_hour, 23);
  TEST_COMPARE (tm->tm_min, 0);
  TEST_COMPARE (tm->tm_sec, 1);
  TEST_COMPARE (tm->tm_gmtoff, 3600);
  TEST_COMPARE (tm->tm_isdst, 0);

  return 0;
}

#include <support/test-driver.c>
