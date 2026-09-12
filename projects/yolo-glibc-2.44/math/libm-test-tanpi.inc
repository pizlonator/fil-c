/* Test tanpi.
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

static const struct test_f_f_data tanpi_test_data[] =
  {
    TEST_f_f (tanpi, plus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_f_f (tanpi, minus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_f_f (tanpi, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_f_f (tanpi, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_f_f (tanpi, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_f_f (tanpi, -snan_value, qnan_value, INVALID_EXCEPTION),

    TEST_f_f (tanpi, 0.5, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 1.5, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 2.5, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 3.5, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 4.5, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0.5, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -1.5, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -2.5, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -3.5, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -4.5, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),

    TEST_f_f (tanpi, 0xffffffp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 0xfffffdp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0xffffffp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0xfffffdp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
#if MANT_DIG >= 53
    TEST_f_f (tanpi, 0x1fffffffffffffp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 0x1ffffffffffffdp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0x1fffffffffffffp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0x1ffffffffffffdp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
#endif
#if MANT_DIG >= 64
    TEST_f_f (tanpi, 0xffffffffffffffffp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 0xfffffffffffffffdp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE|XFAIL_ROUNDING_IBM128_LIBGCC),
    TEST_f_f (tanpi, -0xffffffffffffffffp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0xfffffffffffffffdp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE|XFAIL_ROUNDING_IBM128_LIBGCC),
#endif
#if MANT_DIG >= 106
    TEST_f_f (tanpi, 0x3ffffffffffffffffffffffffffp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 0x3fffffffffffffffffffffffffdp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE|XFAIL_ROUNDING_IBM128_LIBGCC),
    TEST_f_f (tanpi, -0x3ffffffffffffffffffffffffffp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0x3fffffffffffffffffffffffffdp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE|XFAIL_ROUNDING_IBM128_LIBGCC),
#endif
#if MANT_DIG >= 113
    TEST_f_f (tanpi, 0x1ffffffffffffffffffffffffffffp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, 0x1fffffffffffffffffffffffffffdp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0x1ffffffffffffffffffffffffffffp-1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_f_f (tanpi, -0x1fffffffffffffffffffffffffffdp-1, minus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
#endif

    AUTO_TESTS_f_f (tanpi),
  };

static void
tanpi_test (void)
{
  ALL_RM_TEST (tanpi, 0, tanpi_test_data, RUN_TEST_LOOP_f_f, END);
}

static void
do_test (void)
{
  tanpi_test ();
}

/*
 * Local Variables:
 * mode:c
 * End:
 */
