/* Test gmtime_r().
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

int
main (void)
{
  {
    time_t t = 1509000003; /* 2017-10-26 06:40:03 */
    struct tm tm;
    struct tm *result = gmtime_r (&t, &tm);
    ASSERT (result == &tm);
    ASSERT (result->tm_sec == 3);
    ASSERT (result->tm_min == 40);
    ASSERT (result->tm_hour == 6);
    ASSERT (result->tm_mday == 26);
    ASSERT (result->tm_mon == 10 - 1);
    ASSERT (result->tm_year == 2017 - 1900);
    ASSERT (result->tm_wday == 4);
    ASSERT (result->tm_yday == 298);
    ASSERT (result->tm_isdst == 0);
#if HAVE_STRUCT_TM_TM_GMTOFF /* glibc, musl, macOS, FreeBSD, NetBSD, OpenBSD, Minix, Cygwin, Android */
    ASSERT (result->tm_gmtoff == 0);
#endif
#if HAVE_STRUCT_TM_TM_ZONE /* glibc, musl, macOS, FreeBSD, NetBSD, OpenBSD, Minix, Cygwin, Android */
    printf ("tm_zone = %s\n", result->tm_zone == NULL ? "(null)" : result->tm_zone);
    ASSERT (strcmp (result->tm_zone, "GMT") == 0 /* glibc, NetBSD, OpenBSD, Minix, Cygwin, Android */
            || strcmp (result->tm_zone, "UTC") == 0 /* musl, macOS, FreeBSD */);
#endif
  }

  return test_exit_status;
}
