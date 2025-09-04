/* Test narrowing sqrt.
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
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

static const struct test_a_f_data sqrt_test_data[] =
  {
    TEST_a_f (sqrt, arg_qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_a_f (sqrt, -arg_qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_a_f (sqrt, arg_snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_a_f (sqrt, -arg_snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_a_f (sqrt, arg_plus_infty, plus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),

    /* sqrt (x) == qNaN plus invalid exception for x < 0.  */
    TEST_a_f (sqrt, -1, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_a_f (sqrt, -1.1L, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_a_f (sqrt, -arg_min_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_a_f (sqrt, -arg_min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_a_f (sqrt, -arg_max_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_a_f (sqrt, arg_minus_infty, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION|ERRNO_EDOM),

    AUTO_TESTS_a_f (sqrt),
  };

static void
sqrt_test (void)
{
  ALL_RM_TEST (sqrt, 1, sqrt_test_data, RUN_TEST_LOOP_a_f, END);
}

static void
do_test (void)
{
  sqrt_test ();
}

/*
 * Local Variables:
 * mode:c
 * End:
 */
