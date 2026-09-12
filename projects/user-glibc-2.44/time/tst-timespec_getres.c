/* Basic tests for timespec_getres.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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
#include <support/check.h>

static void
test_timespec_getres (int clockid, int timebase)
{
  struct timespec ts;
  TEST_COMPARE (timespec_getres (&ts, TIME_UTC), TIME_UTC);
  /* Expect all supported systems to support 'timebase' with
     resolution better than one second.  */
  TEST_VERIFY (ts.tv_sec == 0);
  TEST_VERIFY (ts.tv_nsec > 0);
  TEST_VERIFY (ts.tv_nsec < 1000000000);
  /* Expect the resolution to be the same as that reported for
     'clockid' with clock_getres.  */
  struct timespec cts;
  TEST_COMPARE (clock_getres (CLOCK_REALTIME, &cts), 0);
  TEST_COMPARE (ts.tv_sec, cts.tv_sec);
  TEST_COMPARE (ts.tv_nsec, cts.tv_nsec);
}

static int
do_test (void)
{
  {
    struct timespec ts;
    /* Invalid timebase.  */
    TEST_COMPARE (timespec_getres (&ts, 0), 0);
    /* Invalid timespec.  */
    TEST_COMPARE (timespec_getres (NULL, 0), 0);
  }

  test_timespec_getres (CLOCK_REALTIME, TIME_UTC);
  test_timespec_getres (CLOCK_MONOTONIC, TIME_MONOTONIC);
  test_timespec_getres (CLOCK_PROCESS_CPUTIME_ID, TIME_ACTIVE);
  test_timespec_getres (CLOCK_THREAD_CPUTIME_ID, TIME_THREAD_ACTIVE);

  return 0;
}

#include <support/test-driver.c>
