/* Test atan2pi.
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

static const struct test_ff_f_data atan2pi_test_data[] =
  {
    /* atan2pi (y,inf) == +0 for finite y > 0 or +0.  */
    TEST_ff_f (atan2pi, 1, plus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_zero, plus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, min_value, plus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, min_subnorm_value, plus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, max_value, plus_infty, 0, ERRNO_UNCHANGED),

    /* atan2pi (y,inf) == -0 for finite y < 0 or -0.  */
    TEST_ff_f (atan2pi, -1, plus_infty, minus_zero, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_zero, plus_infty, minus_zero, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -min_value, plus_infty, minus_zero, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -min_subnorm_value, plus_infty, minus_zero, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -max_value, plus_infty, minus_zero, ERRNO_UNCHANGED),

    /* atan2pi(+inf, x) == 1/2 for finite x.  */
    TEST_ff_f (atan2pi, plus_infty, 1, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, plus_zero, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, min_value, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, min_subnorm_value, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, max_value, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, -1, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, minus_zero, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, -min_value, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, -min_subnorm_value, 0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, -max_value, 0.5, ERRNO_UNCHANGED),

    /* atan2pi(-inf, x) == -1/2 for finite x.  */
    TEST_ff_f (atan2pi, minus_infty, 1, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, plus_zero, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, min_value, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, min_subnorm_value, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, max_value, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, -1, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, minus_zero, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, -min_value, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, -min_subnorm_value, -0.5, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, -max_value, -0.5, ERRNO_UNCHANGED),

    /* atan2pi (y,-inf) == 1 for finite y > 0 or +0.  */
    TEST_ff_f (atan2pi, 1, minus_infty, 1.0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_zero, minus_infty, 1.0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, min_value, minus_infty, 1.0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, min_subnorm_value, minus_infty, 1.0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, max_value, minus_infty, 1.0, ERRNO_UNCHANGED),

    /* atan2pi (y,-inf) == -1 for finite y < 0 or -0.  */
    TEST_ff_f (atan2pi, -1, minus_infty, -1.0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_zero, minus_infty, -1.0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -min_value, minus_infty, -1.0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -min_subnorm_value, minus_infty, -1.0, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -max_value, minus_infty, -1.0, ERRNO_UNCHANGED),

    TEST_ff_f (atan2pi, plus_infty, plus_infty, 0.25, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, plus_infty, -0.25, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, minus_infty, 0.75, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, minus_infty, -0.75, ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, snan_value, qnan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, -qnan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, qnan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, -qnan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, qnan_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, qnan_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -qnan_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -qnan_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, qnan_value, plus_infty, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, minus_infty, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, plus_zero, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, minus_zero, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, 1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, -1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, min_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, -min_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, -min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, max_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, qnan_value, -max_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, plus_infty, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, minus_infty, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, plus_zero, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, minus_zero, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, 1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, -1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, min_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, -min_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, -min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, max_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -qnan_value, -max_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, snan_value, plus_infty, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, minus_infty, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, plus_zero, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, minus_zero, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, 1, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, -1, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, min_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, -min_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, min_subnorm_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, -min_subnorm_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, max_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, snan_value, -max_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, plus_infty, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, minus_infty, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, plus_zero, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, minus_zero, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, 1, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, -1, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, min_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, -min_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, min_subnorm_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, -min_subnorm_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, max_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -snan_value, -max_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, plus_infty, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_zero, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_zero, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, 1, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -1, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, min_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -min_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, min_subnorm_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -min_subnorm_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, max_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -max_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_infty, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_zero, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, minus_zero, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, 1, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -1, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, min_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -min_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, min_subnorm_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -min_subnorm_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, max_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, -max_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (atan2pi, plus_infty, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, minus_infty, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, plus_zero, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, minus_zero, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, 1, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -1, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, min_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -min_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, min_subnorm_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -min_subnorm_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, max_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -max_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, plus_infty, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, minus_infty, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, plus_zero, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, minus_zero, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, 1, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -1, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, min_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -min_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, min_subnorm_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -min_subnorm_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, max_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (atan2pi, -max_value, -snan_value, qnan_value, INVALID_EXCEPTION),

    AUTO_TESTS_ff_f (atan2pi),
  };

static void
atan2pi_test (void)
{
  ALL_RM_TEST (atan2pi, 0, atan2pi_test_data, RUN_TEST_LOOP_ff_f, END);
}

static void
do_test (void)
{
  atan2pi_test ();
}

/*
 * Local Variables:
 * mode:c
 * End:
 */
