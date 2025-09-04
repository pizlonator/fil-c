/* Test setting the monotonic clock.
   Copyright (C) 2007-2024 Free Software Foundation, Inc.
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
#include <support/check.h>
#include <time.h>
#include <unistd.h>

int
do_test (void)
{
#if defined CLOCK_MONOTONIC && defined _POSIX_MONOTONIC_CLOCK
  if (sysconf (_SC_MONOTONIC_CLOCK) <= 0)
    FAIL_UNSUPPORTED ("_SC_MONOTONIC_CLOCK not supported");

  struct timespec ts;
  TEST_COMPARE (clock_gettime (CLOCK_MONOTONIC, &ts), 0);

  /* Setting the monotonic clock must fail.  */
  TEST_VERIFY (clock_settime (CLOCK_MONOTONIC, &ts) == -1);
  TEST_VERIFY (errno == EINVAL || errno == EPERM);

  return 0;
#else
  return EXIT_UNSUPPORTED;
#endif
}

#include <support/test-driver.c>
