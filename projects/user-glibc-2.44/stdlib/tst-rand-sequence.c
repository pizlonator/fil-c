/* Test that rand () evaluates to a special expected sequence.
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

/* This test is borderline invalid because the rand function is not
   guaranteed to produce the same sequence of numbers on every
   architecture or glibc build.  However, some software packages have
   test suites that depend on the specific sequence of numbers return
   here, so this test verifies that they do not change unexpectedly.  */

#include <stdlib.h>
#include <support/check.h>

static int
do_test (void)
{
  TEST_COMPARE (rand (), 1804289383);
  TEST_COMPARE (rand (), 846930886);
  TEST_COMPARE (rand (), 1681692777);
  TEST_COMPARE (rand (), 1714636915);
  TEST_COMPARE (rand (), 1957747793);
  TEST_COMPARE (rand (), 424238335);
  TEST_COMPARE (rand (), 719885386);
  TEST_COMPARE (rand (), 1649760492);
  TEST_COMPARE (rand (), 596516649);
  TEST_COMPARE (rand (), 1189641421);

  srand (1009);
  TEST_COMPARE (rand (), 176208083);
  TEST_COMPARE (rand (), 1650100842);
  TEST_COMPARE (rand (), 1813188575);
  TEST_COMPARE (rand (), 2064804480);
  TEST_COMPARE (rand (), 1944264725);
  TEST_COMPARE (rand (), 1673642853);
  TEST_COMPARE (rand (), 1582759448);
  TEST_COMPARE (rand (), 309901569);
  TEST_COMPARE (rand (), 444031692);
  TEST_COMPARE (rand (), 1926035991);

  return 0;
}

#include <support/test-driver.c>
