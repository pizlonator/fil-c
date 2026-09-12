/* Test powr.
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

static const struct test_ff_f_data powr_test_data[] =
  {
    TEST_ff_f (powr, qnan_value, 0, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, 0, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, minus_zero, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, minus_zero, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, snan_value, 0, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, 0, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, minus_zero, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, minus_zero, qnan_value, INVALID_EXCEPTION),

    TEST_ff_f (powr, plus_zero, plus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, plus_zero, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_zero, plus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_zero, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, -min_subnorm_value, plus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, -min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, -min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, -1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, 1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, -2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, 2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, -max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_subnorm_value, max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, -min_value, plus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, -min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, -min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, -1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, 1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, -2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, 2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, -max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, -1.0, plus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, -min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, -min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, -1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, 1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, -2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, 2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, -max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0, max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, -2.0, plus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, -min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, -min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, -1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, 1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, -2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, 2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, -max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -2.0, max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, -max_value, plus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, -min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, -min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, min_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, -1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, 1.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, -2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, 2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, -max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, 1.1L, plus_infty, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, plus_infty, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, -1.1L, plus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, plus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, 0.9L, plus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, 1e-7L, plus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, -0.9L, plus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1e-7L, plus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, 1.1L, minus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, minus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, -1.1L, minus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, minus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, 0.9L, minus_infty, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, 1e-7L, minus_infty, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, -0.9L, minus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1e-7L, minus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, plus_infty, 1e-7L, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, 1, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, 1e7L, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, min_subnorm_value, plus_infty, ERRNO_UNCHANGED),

    TEST_ff_f (powr, plus_infty, -1e-7L, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, -1, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, -1e7L, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, -min_subnorm_value, 0, ERRNO_UNCHANGED),

    TEST_ff_f (powr, minus_infty, 1, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 11, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 1001, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, minus_infty, 2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 12, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 1002, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 1.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 11.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 1001.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, minus_infty, -1, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -11, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -1001, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, minus_infty, -2, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -12, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -1002, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -1.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -11.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -1001.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, qnan_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, qnan_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -qnan_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -qnan_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, qnan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, -qnan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, qnan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, -qnan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, 0, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, 0, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, 0, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, 0, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, 1, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, 1, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, 1, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, 1, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -1, qnan_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1, -qnan_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -1, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, qnan_value, 1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, 1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, snan_value, 1, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, 1, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, qnan_value, -1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, -1, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, snan_value, -1, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, -1, qnan_value, INVALID_EXCEPTION),

    /* powr (x, qNaN or sNaN) == qNaN.  */
    TEST_ff_f (powr, 3.0, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, 3.0, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, minus_zero, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, minus_zero, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, -qnan_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -3.0, qnan_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -3.0, -qnan_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, qnan_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -qnan_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, 3.0, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, 3.0, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, minus_zero, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, minus_zero, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, plus_infty, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, plus_infty, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -3.0, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -3.0, -snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, minus_infty, snan_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, minus_infty, -snan_value, qnan_value, INVALID_EXCEPTION),

    TEST_ff_f (powr, qnan_value, 3.0, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, 3.0, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, -3.0, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, -3.0, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, plus_infty, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, plus_infty, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, minus_infty, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, minus_infty, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, 2.5, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, 2.5, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, -2.5, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, -2.5, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, qnan_value, -min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, -qnan_value, -min_subnorm_value, qnan_value, NO_INEXACT_EXCEPTION|ERRNO_UNCHANGED),
    TEST_ff_f (powr, snan_value, 3.0, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, 3.0, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, -3.0, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, -3.0, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, plus_infty, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, plus_infty, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, minus_infty, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, minus_infty, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, 2.5, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, 2.5, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, -2.5, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, -2.5, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, min_subnorm_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, min_subnorm_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, snan_value, -min_subnorm_value, qnan_value, INVALID_EXCEPTION),
    TEST_ff_f (powr, -snan_value, -min_subnorm_value, qnan_value, INVALID_EXCEPTION),

    TEST_ff_f (powr, 1, plus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1, plus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, 1, minus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1, minus_infty, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    /* powr (inf, +-0) is a NaN.  */
    TEST_ff_f (powr, plus_infty, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, plus_infty, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, minus_zero, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, -0.1L, 1.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -0.1L, -1.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -10.1L, 1.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -10.1L, -1.1L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.01L, min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.01L, -min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0L, min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -1.0L, -min_subnorm_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, 0, -1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, 0, -11, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, 0, -0xffffff, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
#if !TEST_COND_binary32
    TEST_ff_f (powr, 0, -0x1.fffffffffffffp+52L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
#endif
#if MANT_DIG >= 64
    TEST_ff_f (powr, 0, -0x1.fffffffffffffffep+63L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
# if MANT_DIG >= 106
    TEST_ff_f (powr, 0, -0x1.ffffffffffffffffffffffffff8p+105L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
# endif
# if MANT_DIG >= 113
    TEST_ff_f (powr, 0, -0x1.ffffffffffffffffffffffffffffp+112L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
# endif
#endif
    TEST_ff_f (powr, minus_zero, -1, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -11L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -0xffffff, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -0x1fffffe, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
#if !TEST_COND_binary32
    TEST_ff_f (powr, minus_zero, -0x1.fffffffffffffp+52L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -0x1.fffffffffffffp+53L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
#endif
#if MANT_DIG >= 64
    TEST_ff_f (powr, minus_zero, -0x1.fffffffffffffffep+63L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -0x1.fffffffffffffffep+64L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE|XFAIL_ROUNDING_IBM128_LIBGCC),
# if MANT_DIG >= 106
    TEST_ff_f (powr, minus_zero, -0x1.ffffffffffffffffffffffffff8p+105L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -0x1.ffffffffffffffffffffffffff8p+106L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE|XFAIL_ROUNDING_IBM128_LIBGCC),
# endif
# if MANT_DIG >= 113
    TEST_ff_f (powr, minus_zero, -0x1.ffffffffffffffffffffffffffffp+112L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -0x1.ffffffffffffffffffffffffffffp+113L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
# endif
#endif

    TEST_ff_f (powr, 0, -2, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, 0, -11.1L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, 0, -min_subnorm_value, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, 0, -0x1p24, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, 0, -0x1p127, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, 0, -max_value, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -2, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -11.1L, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -min_subnorm_value, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -0x1p24, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -0x1p127, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),
    TEST_ff_f (powr, minus_zero, -max_value, plus_infty, DIVIDE_BY_ZERO_EXCEPTION|ERRNO_ERANGE),

    TEST_ff_f (powr, 0, plus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, minus_zero, plus_infty, 0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, 0, minus_infty, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, minus_zero, minus_infty, plus_infty, ERRNO_UNCHANGED),

    /* powr (x, +inf) == +inf for |x| > 1.  */
    TEST_ff_f (powr, 1.5, plus_infty, plus_infty, ERRNO_UNCHANGED),

    /* powr (x, +inf) == +0 for |x| < 1.  */
    TEST_ff_f (powr, 0.5, plus_infty, 0.0, ERRNO_UNCHANGED),

    /* powr (x, -inf) == +0 for |x| > 1.  */
    TEST_ff_f (powr, 1.5, minus_infty, 0.0, ERRNO_UNCHANGED),

    /* powr (x, -inf) == +inf for |x| < 1.  */
    TEST_ff_f (powr, 0.5, minus_infty, plus_infty, ERRNO_UNCHANGED),

    /* powr (+inf, y) == +inf for y > 0.  */
    TEST_ff_f (powr, plus_infty, 2, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, 0xffffff, plus_infty, ERRNO_UNCHANGED),
#if !TEST_COND_binary32
    TEST_ff_f (powr, plus_infty, 0x1.fffffffffffffp+52L, plus_infty, ERRNO_UNCHANGED),
#endif
#if MANT_DIG >= 64
    TEST_ff_f (powr, plus_infty, 0x1.fffffffffffffffep+63L, plus_infty, ERRNO_UNCHANGED),
# if MANT_DIG >= 106
    TEST_ff_f (powr, plus_infty, 0x1.ffffffffffffffffffffffffff8p+105L, plus_infty, ERRNO_UNCHANGED),
# endif
# if MANT_DIG >= 113
    TEST_ff_f (powr, plus_infty, 0x1.ffffffffffffffffffffffffffffp+112L, plus_infty, ERRNO_UNCHANGED),
# endif
#endif
    TEST_ff_f (powr, plus_infty, 0x1p24, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, 0x1p127, plus_infty, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, max_value, plus_infty, ERRNO_UNCHANGED),

    /* powr (+inf, y) == +0 for y < 0.  */
    TEST_ff_f (powr, plus_infty, -1, 0.0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, -0xffffff, 0.0, ERRNO_UNCHANGED),
#if !TEST_COND_binary32
    TEST_ff_f (powr, plus_infty, -0x1.fffffffffffffp+52L, 0.0, ERRNO_UNCHANGED),
#endif
#if MANT_DIG >= 64
    TEST_ff_f (powr, plus_infty, -0x1.fffffffffffffffep+63L, 0.0, ERRNO_UNCHANGED),
# if MANT_DIG >= 106
    TEST_ff_f (powr, plus_infty, -0x1.ffffffffffffffffffffffffff8p+105L, 0.0, ERRNO_UNCHANGED),
# endif
# if MANT_DIG >= 113
    TEST_ff_f (powr, plus_infty, -0x1.ffffffffffffffffffffffffffffp+112L, 0.0, ERRNO_UNCHANGED),
# endif
#endif
    TEST_ff_f (powr, plus_infty, -0x1p24, 0.0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, -0x1p127, 0.0, ERRNO_UNCHANGED),
    TEST_ff_f (powr, plus_infty, -max_value, 0.0, ERRNO_UNCHANGED),

    /* powr (-inf, y) is a NaN (unlike pow) for y an odd integer > 0.  */
    TEST_ff_f (powr, minus_infty, 27, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0xffffff, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0x1fffffe, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
#if !TEST_COND_binary32
    TEST_ff_f (powr, minus_infty, 0x1.fffffffffffffp+52L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0x1.fffffffffffffp+53L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
#endif
#if MANT_DIG >= 64
    TEST_ff_f (powr, minus_infty, 0x1.fffffffffffffffep+63L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0x1.fffffffffffffffep+64L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM|XFAIL_ROUNDING_IBM128_LIBGCC),
# if MANT_DIG >= 106
    TEST_ff_f (powr, minus_infty, 0x1.ffffffffffffffffffffffffff8p+105L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0x1.ffffffffffffffffffffffffff8p+106L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM|XFAIL_ROUNDING_IBM128_LIBGCC),
# endif
# if MANT_DIG >= 113
    TEST_ff_f (powr, minus_infty, 0x1.ffffffffffffffffffffffffffffp+112L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0x1.ffffffffffffffffffffffffffffp+113L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
# endif
#endif

    /* powr (-inf, y) is a NaN (unlike pow) for y > 0 and not an odd integer.  */
    TEST_ff_f (powr, minus_infty, 0.5, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 28, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0x1p24, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, 0x1p127, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    /* powr (-inf, y) is a NaN (unlike pow) for y an odd integer < 0. */
    TEST_ff_f (powr, minus_infty, -3, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0xffffff, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0x1fffffe, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
#if !TEST_COND_binary32
    TEST_ff_f (powr, minus_infty, -0x1.fffffffffffffp+52L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0x1.fffffffffffffp+53L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
#endif
#if MANT_DIG >= 64
    TEST_ff_f (powr, minus_infty, -0x1.fffffffffffffffep+63L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0x1.fffffffffffffffep+64L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM|XFAIL_ROUNDING_IBM128_LIBGCC),
# if MANT_DIG >= 106
    TEST_ff_f (powr, minus_infty, -0x1.ffffffffffffffffffffffffff8p+105L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0x1.ffffffffffffffffffffffffff8p+106L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM|XFAIL_ROUNDING_IBM128_LIBGCC),
# endif
# if MANT_DIG >= 113
    TEST_ff_f (powr, minus_infty, -0x1.ffffffffffffffffffffffffffffp+112L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0x1.ffffffffffffffffffffffffffffp+113L, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
# endif
#endif
    /* powr (-inf, y) is a NaN (unlike pow) for y < 0 and not an odd integer.  */
    TEST_ff_f (powr, minus_infty, -2.0, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0x1p24, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -0x1p127, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, minus_infty, -max_value, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, -max_value, 0.5, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, 1.5, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -max_value, 1000.5, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),

    TEST_ff_f (powr, -min_value, 0.5, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, 1.5, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    TEST_ff_f (powr, -min_value, 1000.5, qnan_value, INVALID_EXCEPTION|ERRNO_EDOM),
    AUTO_TESTS_ff_f (powr),
  };

static void
powr_test (void)
{
  ALL_RM_TEST (powr, 0, powr_test_data, RUN_TEST_LOOP_ff_f, END);
}

static void
do_test (void)
{
  powr_test ();
}

/*
 * Local Variables:
 * mode:c
 * End:
 */
