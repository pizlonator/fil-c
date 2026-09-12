/* Test exp2m1.
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

#include "libm-test-driver.c"

static const struct test_f_f_data exp2m1_test_data[] =
  {
    TEST_f_f (exp2m1, plus_infty, plus_infty, ERRNO_UNCHANGED),
    TEST_f_f (exp2m1, minus_infty, -1, ERRNO_UNCHANGED),
    TEST_f_f (exp2m1, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_f_f (exp2m1, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_f_f (exp2m1, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_f_f (exp2m1, -snan_value, qnan_value, INVALID_EXCEPTION),

    AUTO_TESTS_f_f (exp2m1),
  };

static void
exp2m1_test (void)
{
  ALL_RM_TEST (exp2m1, 0, exp2m1_test_data, RUN_TEST_LOOP_f_f, END);
}

static void
do_test (void)
{
  exp2m1_test ();
}

/*
 * Local Variables:
 * mode:c
 * End:
 */
