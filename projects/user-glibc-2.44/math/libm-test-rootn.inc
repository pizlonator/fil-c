/* Test rootn.
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

#include "libm-test-driver.c"

static const struct test_fL_f_data rootn_test_data[] =
  {
    TEST_fL_f (rootn, qnan_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -qnan_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, snan_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -snan_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, plus_infty, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, minus_infty, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, plus_zero, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, minus_zero, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, min_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, min_subnorm_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_subnorm_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, max_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -max_value, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_fL_f (rootn, -min_subnorm_value, 2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_subnorm_value, 4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_subnorm_value, 0x7ffffffffffffffeLL, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_value, 2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_value, 4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_value, 0x7ffffffffffffffeLL, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -1.0, 2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -1.0, 4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -1.0, 0x7ffffffffffffffeLL, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -max_value, 2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -max_value, 4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -max_value, 0x7ffffffffffffffeLL, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, minus_infty, 2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, minus_infty, 4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, minus_infty, 0x7ffffffffffffffeLL, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_fL_f (rootn, -min_subnorm_value, -2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_subnorm_value, -4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_subnorm_value, LLONG_MIN, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_value, -2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_value, -4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -min_value, LLONG_MIN, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -1.0, -2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -1.0, -4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -1.0, LLONG_MIN, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -max_value, -2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -max_value, -4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, -max_value, LLONG_MIN, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, minus_infty, -2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, minus_infty, -4, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_fL_f (rootn, minus_infty, LLONG_MIN, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_fL_f (rootn, qnan_value, 1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, -qnan_value, 1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, qnan_value, -1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, -qnan_value, -1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, qnan_value, 2, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, -qnan_value, 2, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, qnan_value, -2, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, -qnan_value, -2, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, qnan_value, 3, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, -qnan_value, 3, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, qnan_value, -3, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, -qnan_value, -3, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, qnan_value, LLONG_MAX, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, -qnan_value, LLONG_MAX, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, qnan_value, LLONG_MIN, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (rootn, -qnan_value, LLONG_MIN, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),

    TEST_fL_f (rootn, snan_value, 1, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, -snan_value, 1, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, snan_value, -1, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, -snan_value, -1, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, snan_value, 2, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, -snan_value, 2, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, snan_value, -2, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, -snan_value, -2, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, snan_value, 3, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, -snan_value, 3, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, snan_value, -3, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, -snan_value, -3, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, snan_value, LLONG_MAX, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, -snan_value, LLONG_MAX, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, snan_value, LLONG_MIN, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (rootn, -snan_value, LLONG_MIN, qnan_value, INVALID_EXCEPTION),

    TEST_fL_f (rootn, plus_zero, -1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, minus_zero, -1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, plus_zero, -2, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, minus_zero, -2, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, plus_zero, -3, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, minus_zero, -3, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, plus_zero, -4, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, minus_zero, -4, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, plus_zero, -5, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, minus_zero, -5, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, plus_zero, LLONG_MIN, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (rootn, minus_zero, LLONG_MIN, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),

    TEST_fL_f (rootn, plus_infty, 1, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, minus_infty, 1, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, 2, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, 3, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, minus_infty, 3, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, 4, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, 5, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, minus_infty, 5, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, LLONG_MAX, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, minus_infty, LLONG_MAX, minus_infty, ERRNO_UNCHANGED),

    TEST_fL_f (rootn, plus_infty, -1, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, minus_infty, -1, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, -2, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, -3, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, minus_infty, -3, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, -4, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, -5, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, minus_infty, -5, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, -0x7fffffffffffffffLL, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, minus_infty, -0x7fffffffffffffffLL, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (rootn, plus_infty, LLONG_MIN, plus_zero, ERRNO_UNCHANGED),

    AUTO_TESTS_fL_f (rootn),
  };

static void
rootn_test (void)
{
  ALL_RM_TEST (rootn, 0, rootn_test_data, RUN_TEST_LOOP_fL_f, END);
}

static void
do_test (void)
{
  rootn_test ();
}

/*
 * Local Variables:
 * mode:c
 * End:
 */
