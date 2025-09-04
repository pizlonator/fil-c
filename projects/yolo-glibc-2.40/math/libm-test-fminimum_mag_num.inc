/* Test fminimum_mag_num.
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

static const struct test_ff_f_data fminimum_mag_num_test_data[] =
  {
    TEST_ff_f (fminimum_mag_num, 0, 0, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, minus_zero, minus_zero, minus_zero, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 0, minus_zero, minus_zero, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, minus_zero, 0, minus_zero, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, min_subnorm_value, min_subnorm_value, min_subnorm_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, min_subnorm_value, -min_subnorm_value, -min_subnorm_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -min_subnorm_value, min_subnorm_value, -min_subnorm_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -min_subnorm_value, -min_subnorm_value, -min_subnorm_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, min_value, min_value, min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, min_value, -min_value, -min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -min_value, min_value, -min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -min_value, -min_value, -min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, max_value, max_value, max_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, max_value, -max_value, -max_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -max_value, max_value, -max_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -max_value, -max_value, -max_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, max_value, min_value, min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, max_value, -min_value, -min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -max_value, min_value, min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -max_value, -min_value, -min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, min_value, max_value, min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, min_value, -max_value, min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -min_value, max_value, -min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -min_value, -max_value, -min_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 9, 0, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 0, 9, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -9, 0, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 0, -9, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),

    TEST_ff_f (fminimum_mag_num, plus_infty, 9, 9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 9, plus_infty, 9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, plus_infty, -9, -9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -9, plus_infty, -9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, minus_infty, 9, 9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, minus_infty, -9, -9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 9, minus_infty, 9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -9, minus_infty, -9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),

    TEST_ff_f (fminimum_mag_num, 0, qnan_value, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 0, -qnan_value, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, minus_zero, qnan_value, minus_zero, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, minus_zero, -qnan_value, minus_zero, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 9, qnan_value, 9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 9, -qnan_value, 9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -9, qnan_value, -9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -9, -qnan_value, -9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, 0, snan_value, 0, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, 0, -snan_value, 0, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, minus_zero, snan_value, minus_zero, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, minus_zero, -snan_value, minus_zero, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, 9, snan_value, 9, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, 9, -snan_value, 9, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -9, snan_value, -9, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -9, -snan_value, -9, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, qnan_value, 0, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -qnan_value, 0, 0, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, qnan_value, minus_zero, minus_zero, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -qnan_value, minus_zero, minus_zero, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, qnan_value, 9, 9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -qnan_value, 9, 9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, qnan_value, -9, -9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -qnan_value, -9, -9, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, snan_value, 0, 0, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, 0, 0, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, snan_value, minus_zero, minus_zero, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, minus_zero, minus_zero, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, snan_value, 9, 9, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, 9, 9, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, snan_value, -9, -9, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, -9, -9, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, plus_infty, qnan_value, plus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, plus_infty, -qnan_value, plus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, minus_infty, qnan_value, minus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, minus_infty, -qnan_value, minus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, plus_infty, snan_value, plus_infty, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, plus_infty, -snan_value, plus_infty, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, minus_infty, snan_value, minus_infty, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, minus_infty, -snan_value, minus_infty, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, qnan_value, plus_infty, plus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -qnan_value, plus_infty, plus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, qnan_value, minus_infty, minus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -qnan_value, minus_infty, minus_infty, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, snan_value, plus_infty, plus_infty, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, plus_infty, plus_infty, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, snan_value, minus_infty, minus_infty, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, minus_infty, minus_infty, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, qnan_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, qnan_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -qnan_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, -qnan_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (fminimum_mag_num, qnan_value, snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, qnan_value, -snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -qnan_value, snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -qnan_value, -snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, snan_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, snan_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, snan_value, snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, snan_value, -snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
    TEST_ff_f (fminimum_mag_num, -snan_value, -snan_value, qnan_value, NO_INEXACT_EXCEPTION|INVALID_EXCEPTION),
  };

static void
fminimum_mag_num_test (void)
{
  ALL_RM_TEST (fminimum_mag_num, 1, fminimum_mag_num_test_data, RUN_TEST_LOOP_ff_f, END);
}

static void
do_test (void)
{
  fminimum_mag_num_test ();
}

/*
 * Local Variables:
 * mode:c
 * End:
 */
