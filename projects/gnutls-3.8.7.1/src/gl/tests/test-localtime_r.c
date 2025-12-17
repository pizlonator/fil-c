/* Test localtime_r().
   Copyright (C) 2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2024.  */

#include <config.h>

/* Specification.  */
#include <time.h>

#include <string.h>

#include "macros.h"


/* Some common time zone name.  */

#if defined _WIN32 && !defined __CYGWIN__
/* Cf. <https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/default-time-zones>
   or <https://ss64.com/timezones.html>  */
# define FRENCH_TZ  "Romance Standard Time"
#else
/* Cf. <https://en.wikipedia.org/wiki/List_of_tz_database_time_zones>  */
# define FRENCH_TZ  "Europe/Paris"
#endif


int
main (void)
{
  setenv ("TZ", FRENCH_TZ, 1);

  /* Check that this TZ works.  */
  {
    time_t t = 0; /* 1970-01-01 01:00:00 */
    struct tm *result = localtime (&t);
    if (! (result
           && result->tm_sec == 0
           && result->tm_min == 0
           && result->tm_hour == 1
           && result->tm_mday == 1
           && result->tm_mon == 1 - 1
           && result->tm_year == 1970 - 1900
           && result->tm_wday == 4
           && result->tm_yday == 0
           && result->tm_isdst == 0))
      {
        fputs ("Skipping test: TZ='" FRENCH_TZ "' is not Paris time\n",
               stderr);
        return 77;
      }
  }

  /* Note: The result->tm_gmtoff values and the result->tm_zone values are the
     same (3600, "CET" or 7200, "CEST") across all tested platforms:
     glibc, musl, macOS, FreeBSD, NetBSD, OpenBSD, Minix, Cygwin, Android.  */

  /* A time point when DST was in effect.  */
  {
    time_t t = 1178467200; /* 2007-05-06 18:00:00 */
    struct tm tm;
    struct tm *result = localtime_r (&t, &tm);
    ASSERT (result == &tm);
    ASSERT (result->tm_sec == 0);
    ASSERT (result->tm_min == 0);
    ASSERT (result->tm_hour == 18);
    ASSERT (result->tm_mday == 6);
    ASSERT (result->tm_mon == 5 - 1);
    ASSERT (result->tm_year == 2007 - 1900);
    ASSERT (result->tm_wday == 0);
    ASSERT (result->tm_yday == 125);
    ASSERT (result->tm_isdst == 1);
#if HAVE_STRUCT_TM_TM_GMTOFF
    ASSERT (result->tm_gmtoff == (1 + result->tm_isdst) * 3600);
#endif
#if HAVE_STRUCT_TM_TM_ZONE
    printf ("tm_zone = %s\n", result->tm_zone == NULL ? "(null)" : result->tm_zone);
    ASSERT (strcmp (result->tm_zone, "CEST") == 0);
#endif
  }

  /* 1 second before and 1 second after the DST interval started.  */
  {
    time_t t = 1174784399; /* 2007-03-25 01:59:59 */
    struct tm tm;
    struct tm *result = localtime_r (&t, &tm);
    ASSERT (result == &tm);
    ASSERT (result->tm_sec == 59);
    ASSERT (result->tm_min == 59);
    ASSERT (result->tm_hour == 1);
    ASSERT (result->tm_mday == 25);
    ASSERT (result->tm_mon == 3 - 1);
    ASSERT (result->tm_year == 2007 - 1900);
    ASSERT (result->tm_wday == 0);
    ASSERT (result->tm_yday == 83);
    ASSERT (result->tm_isdst == 0);
#if HAVE_STRUCT_TM_TM_GMTOFF
    ASSERT (result->tm_gmtoff == (1 + result->tm_isdst) * 3600);
#endif
#if HAVE_STRUCT_TM_TM_ZONE
    printf ("tm_zone = %s\n", result->tm_zone == NULL ? "(null)" : result->tm_zone);
    ASSERT (strcmp (result->tm_zone, "CET") == 0);
#endif
  }
  {
    time_t t = 1174784401; /* 2007-03-25 03:00:01 */
    struct tm tm;
    struct tm *result = localtime_r (&t, &tm);
    ASSERT (result == &tm);
    ASSERT (result->tm_sec == 1);
    ASSERT (result->tm_min == 0);
    ASSERT (result->tm_hour == 3);
    ASSERT (result->tm_mday == 25);
    ASSERT (result->tm_mon == 3 - 1);
    ASSERT (result->tm_year == 2007 - 1900);
    ASSERT (result->tm_wday == 0);
    ASSERT (result->tm_yday == 83);
    ASSERT (result->tm_isdst == 1);
#if HAVE_STRUCT_TM_TM_GMTOFF
    ASSERT (result->tm_gmtoff == (1 + result->tm_isdst) * 3600);
#endif
#if HAVE_STRUCT_TM_TM_ZONE
    printf ("tm_zone = %s\n", result->tm_zone == NULL ? "(null)" : result->tm_zone);
    ASSERT (strcmp (result->tm_zone, "CEST") == 0);
#endif
  }

  /* 1 second before and 1 second after the DST interval ended.  */
  {
    time_t t = 1193533199; /* 2007-10-28 02:59:59 */
    struct tm tm;
    struct tm *result = localtime_r (&t, &tm);
    ASSERT (result == &tm);
    ASSERT (result->tm_sec == 59);
    ASSERT (result->tm_min == 59);
    ASSERT (result->tm_hour == 2);
    ASSERT (result->tm_mday == 28);
    ASSERT (result->tm_mon == 10 - 1);
    ASSERT (result->tm_year == 2007 - 1900);
    ASSERT (result->tm_wday == 0);
    ASSERT (result->tm_yday == 300);
    ASSERT (result->tm_isdst == 1);
#if HAVE_STRUCT_TM_TM_GMTOFF
    ASSERT (result->tm_gmtoff == (1 + result->tm_isdst) * 3600);
#endif
#if HAVE_STRUCT_TM_TM_ZONE
    printf ("tm_zone = %s\n", result->tm_zone == NULL ? "(null)" : result->tm_zone);
    ASSERT (strcmp (result->tm_zone, "CEST") == 0);
#endif
  }
  {
    time_t t = 1193533201; /* 2007-10-28 02:00:01 */
    struct tm tm;
    struct tm *result = localtime_r (&t, &tm);
    ASSERT (result == &tm);
    ASSERT (result->tm_sec == 1);
    ASSERT (result->tm_min == 0);
    ASSERT (result->tm_hour == 2);
    ASSERT (result->tm_mday == 28);
    ASSERT (result->tm_mon == 10 - 1);
    ASSERT (result->tm_year == 2007 - 1900);
    ASSERT (result->tm_wday == 0);
    ASSERT (result->tm_yday == 300);
    ASSERT (result->tm_isdst == 0);
#if HAVE_STRUCT_TM_TM_GMTOFF
    ASSERT (result->tm_gmtoff == (1 + result->tm_isdst) * 3600);
#endif
#if HAVE_STRUCT_TM_TM_ZONE
    printf ("tm_zone = %s\n", result->tm_zone == NULL ? "(null)" : result->tm_zone);
    ASSERT (strcmp (result->tm_zone, "CET") == 0);
#endif
  }

  return test_exit_status;
}
