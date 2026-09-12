/* Test time function.
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

#include <time.h>
#include <unistd.h>

#include <support/check.h>
#include <support/test-driver.h>

int
do_test (void)
{
  time_t t1, t2, t3, t4, t5, t6;
  /* Verify that the calls to time succeed, that the value returned
     directly equals that returned through the pointer passed, and
     that the time does not decrease.  */
  t1 = time (&t2);
  TEST_VERIFY_EXIT (t1 != (time_t) -1);
  TEST_VERIFY (t1 == t2);
  t3 = time (NULL);
  TEST_VERIFY_EXIT (t3 != (time_t) -1);
  TEST_VERIFY (t3 >= t1);
  /* Also verify that after sleeping, the time returned has
     increased.  */
  sleep (2);
  t4 = time (&t5);
  TEST_VERIFY_EXIT (t4 != (time_t) -1);
  TEST_VERIFY (t4 == t5);
  TEST_VERIFY (t4 > t3);
  t6 = time (NULL);
  TEST_VERIFY_EXIT (t6 != (time_t) -1);
  TEST_VERIFY (t6 >= t4);
  return 0;
}

#include <support/test-driver.c>
