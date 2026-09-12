/* Test pown.
   Copyright (C) 1997-2026 Free Software Foundation, Inc.
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

static const struct test_fL_f_data pown_test_data[] =
  {
    TEST_fL_f (pown, qnan_value, 0, 1, ERRNO_UNCHANGED|NO_TEST_MATHVEC),
    TEST_fL_f (pown, -qnan_value, 0, 1, ERRNO_UNCHANGED|NO_TEST_MATHVEC),
    TEST_fL_f (pown, snan_value, 0, qnan_value, INVALID_EXCEPTION|NO_TEST_MATHVEC),
    TEST_fL_f (pown, -snan_value, 0, qnan_value, INVALID_EXCEPTION|NO_TEST_MATHVEC),

    TEST_fL_f (pown, plus_infty, 1, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, -1, 0, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 1, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 11, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 1001, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 2, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 12, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 1002, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -1, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -11, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -1001, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -2, 0, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -12, 0, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -1002, 0, ERRNO_UNCHANGED),

    TEST_fL_f (pown, qnan_value, 1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (pown, -qnan_value, 1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (pown, snan_value, 1, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (pown, -snan_value, 1, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (pown, qnan_value, -1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (pown, -qnan_value, -1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (pown, snan_value, -1, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (pown, -snan_value, -1, qnan_value, INVALID_EXCEPTION),

    TEST_fL_f (pown, qnan_value, 3, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (pown, -qnan_value, 3, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (pown, qnan_value, -3, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (pown, -qnan_value, -3, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_fL_f (pown, snan_value, 3, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (pown, -snan_value, 3, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (pown, snan_value, -3, qnan_value, INVALID_EXCEPTION),
    TEST_fL_f (pown, -snan_value, -3, qnan_value, INVALID_EXCEPTION),

    /* pown (x, 0) == 1.  */
    TEST_fL_f (pown, plus_infty, 0, 1, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 0, 1, ERRNO_UNCHANGED),

    TEST_fL_f (pown, 0, -1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, 0, -11, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, 0, -0xfffffe, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, 0, -0xffffff, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, 0, -0x1ffffffffffffeLL, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, 0, -0x1fffffffffffffLL, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, 0, -0x7ffffffffffffffeLL, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, 0, -0x7fffffffffffffffLL, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -11L, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0xfffffe, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0xffffff, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0x1fffffe, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0x1ffffffffffffeLL, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0x1fffffffffffffLL, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0x3ffffffffffffeLL, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0x7ffffffffffffffeLL, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0x7fffffffffffffffLL, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE|XFAIL_ROUNDING_IBM128_LIBGCC),

    TEST_fL_f (pown, 0, -2, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, 0, -0x1000000, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -2, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_fL_f (pown, minus_zero, -0x1000000, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),

    /* pown (+inf, y) == +inf for y > 0.  */
    TEST_fL_f (pown, plus_infty, 2, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, 0xffffff, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, 0x1fffffffffffffLL, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, 0x7fffffffffffffffLL, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, 0x1000000, plus_infty, ERRNO_UNCHANGED),

    /* pown (+inf, y) == +0 for y < 0.  */
    TEST_fL_f (pown, plus_infty, -1, 0.0, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, -0xffffff, 0.0, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, -0x1fffffffffffffLL, 0.0, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, -0x7fffffffffffffffLL, 0.0, ERRNO_UNCHANGED),
    TEST_fL_f (pown, plus_infty, -0x1000000, 0.0, ERRNO_UNCHANGED),

    /* pown (-inf, y) == -inf for y an odd integer > 0.  */
    TEST_fL_f (pown, minus_infty, 27, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 0xffffff, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 0x1fffffe, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 0x1fffffffffffffLL, minus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 0x3ffffffffffffeL, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 0x7ffffffffffffffeLL, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 0x7fffffffffffffffLL, minus_infty, ERRNO_UNCHANGED|XFAIL_ROUNDING_IBM128_LIBGCC),

    /* pown (-inf, y) == +inf for y > 0 and not an odd integer.  */
    TEST_fL_f (pown, minus_infty, 28, plus_infty, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, 0x1000000, plus_infty, ERRNO_UNCHANGED),

    /* pown (-inf, y) == -0 for y an odd integer < 0. */
    TEST_fL_f (pown, minus_infty, -3, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0xfffffe, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0xffffff, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0x1fffffe, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0x1ffffffffffffeLL, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0x1fffffffffffffLL, minus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0x3ffffffffffffeLL, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0x7ffffffffffffffeLL, plus_zero, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0x7fffffffffffffffLL, minus_zero, ERRNO_UNCHANGED|XFAIL_ROUNDING_IBM128_LIBGCC),

    /* pown (-inf, y) == +0 for y < 0 and not an odd integer.  */
    TEST_fL_f (pown, minus_infty, -2, 0.0, ERRNO_UNCHANGED),
    TEST_fL_f (pown, minus_infty, -0x1000000, 0.0, ERRNO_UNCHANGED),

    AUTO_TESTS_fL_f (pown),
  };

static void
pown_test (void)
{
  ALL_RM_TEST (pown, 0, pown_test_data, RUN_TEST_LOOP_fL_f, END);
}

static void
do_test (void)
{
  pown_test ();
}

/*
 * Local Variables:
 * mode:c
 * End:
 */
