/* Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test that we can use a truncated timezone-file, where the time-type
   at index 0 is not indexed by the transition-types array (and the
   transition-types array does not contain at least both one DST and one
   normal time members).  */

static int
do_test (void)
{
  if (setenv ("TZ", "XT5", 1))
    {
      puts ("setenv failed.");
      return 1;
    }

  tzset ();

  return
    /* Sanity-check that we got the right abbreviation for DST.  For
       normal time, we're likely to get "-00" (the "unspecified" marker),
       even though the POSIX timezone string says "-04".  Let's not test
       that.  */
    !(strcmp (tzname[1], "-03") == 0);
}
#include <support/test-driver.c>
