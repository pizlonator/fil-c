/* Test gettimeofday function.
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

#include <sys/time.h>
#include <time.h>

#include <support/check.h>
#include <support/test-driver.h>

/* Compare two struct timeval values, returning a value -1, 0 or 1.  */

int
compare_timeval (const struct timeval *tv1, const struct timeval *tv2)
{
  if (tv1->tv_sec < tv2->tv_sec)
    return -1;
  if (tv1->tv_sec > tv2->tv_sec)
    return 1;
  if (tv1->tv_usec < tv2->tv_usec)
    return -1;
  if (tv1->tv_usec > tv2->tv_usec)
    return 1;
  return 0;
}

int
do_test (void)
{
  struct timeval tv1, tv2, tv3;
  int ret;
  time_t t1;
  /* Verify that the calls to gettimeofday succeed, that the time does
     not decrease, and that time returns a truncated (not rounded)
     version of the time.  */
  t1 = time (NULL);
  TEST_VERIFY_EXIT (t1 != (time_t) -1);
  ret = gettimeofday (&tv1, NULL);
  TEST_VERIFY_EXIT (ret == 0);
  TEST_VERIFY (t1 <= tv1.tv_sec);
  TEST_VERIFY (tv1.tv_usec >= 0);
  TEST_VERIFY (tv1.tv_usec < 1000000);
  ret = gettimeofday (&tv2, NULL);
  TEST_VERIFY_EXIT (ret == 0);
  TEST_VERIFY (compare_timeval (&tv1, &tv2) <= 0);
  TEST_VERIFY (tv2.tv_usec >= 0);
  TEST_VERIFY (tv2.tv_usec < 1000000);
  /* Also verify that after sleeping, the time returned has increased.
     Repeat several times to verify that each time, the time from the
     time function is truncated not rounded.  */
  const struct timespec duration = { .tv_nsec = 100000000 };
  for (int i = 0; i < 10; i++)
    {
      ret = nanosleep (&duration, NULL);
      TEST_VERIFY_EXIT (ret == 0);
      t1 = time (NULL);
      TEST_VERIFY_EXIT (t1 != (time_t) -1);
      ret = gettimeofday (&tv3, NULL);
      TEST_VERIFY_EXIT (ret == 0);
      TEST_VERIFY (compare_timeval (&tv2, &tv3) < 0);
      TEST_VERIFY (t1 <= tv3.tv_sec);
      TEST_VERIFY (tv3.tv_usec >= 0);
      TEST_VERIFY (tv3.tv_usec < 1000000);
      tv2 = tv3;
    }
  /* Also test with the obsolete tz argument not being NULL.  */
  struct timezone tz = { 0 };
  t1 = time (NULL);
  TEST_VERIFY_EXIT (t1 != (time_t) -1);
  ret = gettimeofday (&tv3, &tz);
  TEST_VERIFY_EXIT (ret == 0);
  TEST_VERIFY (t1 <= tv3.tv_sec);
  TEST_VERIFY (compare_timeval (&tv2, &tv3) <= 0);
  TEST_VERIFY (tv3.tv_usec >= 0);
  TEST_VERIFY (tv3.tv_usec < 1000000);
  return 0;
}

#include <support/test-driver.c>
