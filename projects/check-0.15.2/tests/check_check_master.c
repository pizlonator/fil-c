/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "../lib/libcompat.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if ENABLE_REGEX
# include <regex.h>
#endif
#include <check.h>
#include <assert.h>
#include "check_check.h"

int sub_nfailed;
int sub_ntests;

TestResult **tr_fail_array;
TestResult **tr_all_array;

FILE * test_names_file = NULL;
char * test_names_file_name = NULL;
FILE * line_num_failures = NULL;
char * line_num_failures_file_name = NULL;

enum ck_test_msg_type_t {
#if ENABLE_REGEX
  /* For tests with different output on different platforms */
  CK_MSG_REGEXP,
#endif
  /* Simple text */
  CK_MSG_TEXT
};

#define MAXSTR 300

typedef struct {
  const char *tcname;
  const char *test_name;
  int failure_type;
  enum ck_test_msg_type_t msg_type;
  const char *msg;
} master_test_t;

#define SIG_STR_LEN 128
static char signal_11_str[SIG_STR_LEN];
static char signal_11_8_str[SIG_STR_LEN];
static char signal_8_str[SIG_STR_LEN];

static master_test_t master_tests[] = {
  { "Simple Tests", "test_lno", CK_FAILURE, CK_MSG_TEXT, "Failure expected" },
#if defined(HAVE_FORK) && HAVE_FORK==1
  { "Simple Tests", "test_mark_lno", CK_ERROR, CK_MSG_TEXT,   "Early exit with return value 1" },
#endif
  { "Simple Tests", "test_pass", CK_PASS, CK_MSG_TEXT,    "Passed" },
  { "Simple Tests", "test_fail_unless", CK_FAILURE, CK_MSG_TEXT, "This test should fail" },
  { "Simple Tests", "test_fail_if_pass", CK_PASS, CK_MSG_TEXT,    "Passed" },
  { "Simple Tests", "test_fail_if_fail", CK_FAILURE, CK_MSG_TEXT, "This test should fail" },
  { "Simple Tests", "test_fail_null_msg", CK_FAILURE, CK_MSG_TEXT, "Assertion '2 == 3' failed" },
#if defined(__GNUC__)
  { "Simple Tests", "test_fail_no_msg", CK_FAILURE, CK_MSG_TEXT, "Assertion '4 == 5' failed" },
#endif /* __GNUC__ */
  { "Simple Tests", "test_fail_if_null_msg", CK_FAILURE, CK_MSG_TEXT, "Failure '2 != 3' occurred" },
#if defined(__GNUC__)
  { "Simple Tests", "test_fail_if_no_msg", CK_FAILURE, CK_MSG_TEXT, "Failure '4 != 5' occurred" },
#endif /* __GNUC__ */
  { "Simple Tests", "test_fail_vararg_msg_1", CK_FAILURE, CK_MSG_TEXT, "3 != 4" },
  { "Simple Tests", "test_fail_vararg_msg_2", CK_FAILURE, CK_MSG_TEXT, "5 != 6" },
  { "Simple Tests", "test_fail_vararg_msg_3", CK_FAILURE, CK_MSG_TEXT, "7 == 7" },
#if defined(__GNUC__)
  { "Simple Tests", "test_fail_empty", CK_FAILURE, CK_MSG_TEXT, "Failed" },
#endif /* __GNUC__ */
  { "Simple Tests", "test_ck_abort", CK_FAILURE, CK_MSG_TEXT, "Failed" },
  { "Simple Tests", "test_ck_abort_msg", CK_FAILURE, CK_MSG_TEXT, "Failure expected" },
  { "Simple Tests", "test_ck_abort_msg_null", CK_FAILURE, CK_MSG_TEXT, "Failed" },
  { "Simple Tests", "test_ck_assert", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == y' failed" },
  { "Simple Tests", "test_ck_assert_null", CK_FAILURE, CK_MSG_TEXT, "Assertion '0' failed" },
  { "Simple Tests", "test_ck_assert_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '1%f == 1' failed" },
  { "Simple Tests", "test_ck_assert_int_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == y' failed: x == 3, y == 4" },
  { "Simple Tests", "test_ck_assert_int_eq_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d == 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_int_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x != y' failed: x == 3, y == 3" },
  { "Simple Tests", "test_ck_assert_int_ne_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d != 3%f' failed: 3%d == 1, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_int_lt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x < x' failed: x == 2, x == 2" },
  { "Simple Tests", "test_ck_assert_int_lt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d < 3%f' failed: 3%d == 1, 3%f == 0" },
  { "Simple Tests", "test_ck_assert_int_le", CK_FAILURE, CK_MSG_TEXT, "Assertion 'y <= x' failed: y == 3, x == 2" },
  { "Simple Tests", "test_ck_assert_int_le_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d <= 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_int_gt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'y > y' failed: y == 3, y == 3" },
  { "Simple Tests", "test_ck_assert_int_gt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d > 3%f' failed: 3%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_int_ge", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x >= y' failed: x == 2, y == 3" },
  { "Simple Tests", "test_ck_assert_int_ge_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d >= 4%f' failed: 3%d == 0, 4%f == 1" },
  { "Simple Tests", "test_ck_assert_int_expr", CK_PASS, CK_MSG_TEXT,    "Passed" },
  { "Simple Tests", "test_ck_assert_uint_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == y' failed: x == 3, y == 4" },
  { "Simple Tests", "test_ck_assert_uint_eq_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d == 1%f' failed: 3%d == 1, 1%f == 0" },
  { "Simple Tests", "test_ck_assert_uint_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x != y' failed: x == 3, y == 3" },
  { "Simple Tests", "test_ck_assert_uint_ne_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '1%d != 1%f' failed: 1%d == 0, 1%f == 0" },
  { "Simple Tests", "test_ck_assert_uint_lt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x < x' failed: x == 2, x == 2" },
  { "Simple Tests", "test_ck_assert_uint_lt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d < 1%f' failed: 3%d == 1, 1%f == 0" },
  { "Simple Tests", "test_ck_assert_uint_le", CK_FAILURE, CK_MSG_TEXT, "Assertion 'y <= x' failed: y == 3, x == 2" },
  { "Simple Tests", "test_ck_assert_uint_le_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d <= 1%f' failed: 3%d == 1, 1%f == 0" },
  { "Simple Tests", "test_ck_assert_uint_gt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'y > y' failed: y == 3, y == 3" },
  { "Simple Tests", "test_ck_assert_uint_gt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '1%d > 3%f' failed: 1%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_uint_ge", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x >= y' failed: x == 2, y == 3" },
  { "Simple Tests", "test_ck_assert_uint_ge_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '1%d >= 3%f' failed: 1%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_uint_expr", CK_PASS, CK_MSG_TEXT,    "Passed" },
  /* Tests on float macros */
  { "Simple Tests", "test_ck_assert_float_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == y' failed: x == 1.1, y == 1.2" },
  { "Simple Tests", "test_ck_assert_float_eq_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d == 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_float_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x != y' failed: x == 1.1, y == 1.1" },
  { "Simple Tests", "test_ck_assert_float_ne_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '1%d != 1%f' failed: 1%d == 1, 1%f == 1" },
  { "Simple Tests", "test_ck_assert_float_lt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x < y' failed: x == 2, y == 1.5" },
  { "Simple Tests", "test_ck_assert_float_lt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d < 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_float_le", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x <= y' failed: x == 2, y == 1.5" },
  { "Simple Tests", "test_ck_assert_float_le_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d <= 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_float_gt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x > y' failed: x == 2.5, y == 3" },
  { "Simple Tests", "test_ck_assert_float_gt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d > 3%f' failed: 2%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_float_ge", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x >= y' failed: x == 2.5, y == 3" },
  { "Simple Tests", "test_ck_assert_float_ge_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d >= 3%f' failed: 2%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_float_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_float_eq_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(y - x) < t' failed: x == 0.001, y == 0.003, t == 0.001" },
  { "Simple Tests", "test_ck_assert_float_eq_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(2%f - 3%d) < 2%p' failed: 3%d == 1, 2%f == 0, 2%p == 0" },
  { "Simple Tests", "test_ck_assert_float_ne_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(y - x) >= t' failed: x == 0.001, y == 0.002, t == 0.01" },
  { "Simple Tests", "test_ck_assert_float_ne_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(3%f - 3%d) >= 3%p' failed: 3%d == 1, 3%f == 1, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_float_ge_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x >= y, error < t' failed: x == 0.01, y == 0.03, t == 0.01" },
  { "Simple Tests", "test_ck_assert_float_ge_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d >= 3%f, error < 3%p' failed: 2%d == 0, 3%f == 1, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_float_le_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'y <= x, error < t' failed: y == 0.03, x == 0.01, t == 0.01" },
  { "Simple Tests", "test_ck_assert_float_le_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d <= 2%f, error < 3%p' failed: 3%d == 1, 2%f == 0, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_float_tol_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_float_finite", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is finite' failed: x == inf" },
  { "Simple Tests", "test_ck_assert_float_finite_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x*(1%d) is finite' failed: x*(1%d) == inf" },
  { "Simple Tests", "test_ck_assert_float_infinite", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is infinite' failed: x == 0" },
  { "Simple Tests", "test_ck_assert_float_infinite_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d is infinite' failed: 2%d == 0" },
  { "Simple Tests", "test_ck_assert_float_nan", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is NaN' failed: x == inf" },
  { "Simple Tests", "test_ck_assert_float_nan_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d is NaN' failed: 2%d == 0" },
#if ENABLE_REGEX
  { "Simple Tests", "test_ck_assert_float_nonnan", CK_FAILURE, CK_MSG_REGEXP, "^Assertion 'x is not NaN' failed: x == -?nan$" },
  { "Simple Tests", "test_ck_assert_float_nonnan_with_mod", CK_FAILURE, CK_MSG_REGEXP, "^Assertion '\\(2%s\\)\\*x is not NaN' failed: \\(2%s\\)\\*x == -?nan$" },
#else
  { "Simple Tests", "test_ck_assert_float_nonnan", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_float_nonnan_with_mod", CK_PASS, CK_MSG_TEXT, "Passed" },
#endif
  { "Simple Tests", "test_ck_assert_float_nan_and_inf_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  /* End of tests on float macros */
  /* Tests on double macros */
  { "Simple Tests", "test_ck_assert_double_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == y' failed: x == 1.1, y == 1.2" },
  { "Simple Tests", "test_ck_assert_double_eq_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d == 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_double_eq_with_promotion", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_double_eq_with_conv", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == 0.1' failed: x == 0.1, 0.1 == 0.1" },
  { "Simple Tests", "test_ck_assert_double_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x != y' failed: x == 1.1, y == 1.1" },
  { "Simple Tests", "test_ck_assert_double_ne_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '1%d != 1%f' failed: 1%d == 1, 1%f == 1" },
  { "Simple Tests", "test_ck_assert_double_lt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x < y' failed: x == 2, y == 1.5" },
  { "Simple Tests", "test_ck_assert_double_lt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d < 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_double_le", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x <= y' failed: x == 2, y == 1.5" },
  { "Simple Tests", "test_ck_assert_double_le_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d <= 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_double_gt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x > y' failed: x == 2.5, y == 3" },
  { "Simple Tests", "test_ck_assert_double_gt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d > 3%f' failed: 2%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_double_ge", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x >= y' failed: x == 2.5, y == 3" },
  { "Simple Tests", "test_ck_assert_double_ge_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d >= 3%f' failed: 2%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_double_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_double_eq_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(y - x) < t' failed: x == 0.001, y == 0.002, t == 0.001" },
  { "Simple Tests", "test_ck_assert_double_eq_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(2%f - 3%d) < 2%p' failed: 3%d == 1, 2%f == 0, 2%p == 0" },
  { "Simple Tests", "test_ck_assert_double_ne_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(y - x) >= t' failed: x == 0.001, y == 0.002, t == 0.01" },
  { "Simple Tests", "test_ck_assert_double_ne_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(3%f - 3%d) >= 3%p' failed: 3%d == 1, 3%f == 1, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_double_ge_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x >= y, error < t' failed: x == 0.01, y == 0.03, t == 0.01" },
  { "Simple Tests", "test_ck_assert_double_ge_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d >= 3%f, error < 3%p' failed: 2%d == 0, 3%f == 1, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_double_le_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'y <= x, error < t' failed: y == 0.03, x == 0.01, t == 0.01" },
  { "Simple Tests", "test_ck_assert_double_le_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d <= 2%f, error < 3%p' failed: 3%d == 1, 2%f == 0, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_double_tol_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_double_finite", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is finite' failed: x == inf" },
  { "Simple Tests", "test_ck_assert_double_finite_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x*(1%d) is finite' failed: x*(1%d) == inf" },
  { "Simple Tests", "test_ck_assert_double_infinite", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is infinite' failed: x == 0" },
  { "Simple Tests", "test_ck_assert_double_infinite_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d is infinite' failed: 2%d == 0" },
  { "Simple Tests", "test_ck_assert_double_nan", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is NaN' failed: x == inf" },
  { "Simple Tests", "test_ck_assert_double_nan_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d is NaN' failed: 2%d == 0" },
#if ENABLE_REGEX
  { "Simple Tests", "test_ck_assert_double_nonnan", CK_FAILURE, CK_MSG_REGEXP, "^Assertion 'x is not NaN' failed: x == -?nan$" },
  { "Simple Tests", "test_ck_assert_double_nonnan_with_mod", CK_FAILURE, CK_MSG_REGEXP, "^Assertion '\\(2%s\\)\\*x is not NaN' failed: \\(2%s\\)\\*x == -?nan$" },
#else
  { "Simple Tests", "test_ck_assert_double_nonnan", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_double_nonnan_with_mod", CK_PASS, CK_MSG_TEXT, "Passed" },
#endif
  { "Simple Tests", "test_ck_assert_double_nan_and_inf_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  /* End of tests on double macros */
  /* Tests on long double macros */
  { "Simple Tests", "test_ck_assert_ldouble_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == y' failed: x == 1.1, y == 1.2" },
  { "Simple Tests", "test_ck_assert_ldouble_eq_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d == 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_ldouble_eq_with_promotion", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_ldouble_eq_with_conv", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == 1.1' failed: x == 1.1, 1.1 == 1.1" },
  { "Simple Tests", "test_ck_assert_ldouble_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x != y' failed: x == 1.1, y == 1.1" },
  { "Simple Tests", "test_ck_assert_ldouble_ne_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '1%d != 1%f' failed: 1%d == 1, 1%f == 1" },
  { "Simple Tests", "test_ck_assert_ldouble_lt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x < y' failed: x == 2, y == 1.5" },
  { "Simple Tests", "test_ck_assert_ldouble_lt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d < 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_ldouble_le", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x <= y' failed: x == 2, y == 1.5" },
  { "Simple Tests", "test_ck_assert_ldouble_le_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d <= 2%f' failed: 3%d == 1, 2%f == 0" },
  { "Simple Tests", "test_ck_assert_ldouble_gt", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x > y' failed: x == 2.5, y == 3" },
  { "Simple Tests", "test_ck_assert_ldouble_gt_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d > 3%f' failed: 2%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_ldouble_ge", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x >= y' failed: x == 2.5, y == 3" },
  { "Simple Tests", "test_ck_assert_ldouble_ge_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d >= 3%f' failed: 2%d == 0, 3%f == 1" },
  { "Simple Tests", "test_ck_assert_ldouble_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_ldouble_eq_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(y - x) < t' failed: x == 0.001, y == 0.002, t == 0.001" },
  { "Simple Tests", "test_ck_assert_ldouble_eq_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(2%f - 3%d) < 2%p' failed: 3%d == 1, 2%f == 0, 2%p == 0" },
  { "Simple Tests", "test_ck_assert_ldouble_ne_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(y - x) >= t' failed: x == 0.001, y == 0.002, t == 0.01" },
  { "Simple Tests", "test_ck_assert_ldouble_ne_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'fabsl(3%f - 3%d) >= 3%p' failed: 3%d == 1, 3%f == 1, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_ldouble_ge_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x >= y, error < t' failed: x == 0.01, y == 0.03, t == 0.01" },
  { "Simple Tests", "test_ck_assert_ldouble_ge_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d >= 3%f, error < 3%p' failed: 2%d == 0, 3%f == 1, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_ldouble_le_tol", CK_FAILURE, CK_MSG_TEXT, "Assertion 'y <= x, error < t' failed: y == 0.03, x == 0.01, t == 0.01" },
  { "Simple Tests", "test_ck_assert_ldouble_le_tol_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '3%d <= 2%f, error < 3%p' failed: 3%d == 1, 2%f == 0, 3%p == 1" },
  { "Simple Tests", "test_ck_assert_ldouble_tol_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_ldouble_finite", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is finite' failed: x == inf" },
  { "Simple Tests", "test_ck_assert_ldouble_finite_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x*(1%d) is finite' failed: x*(1%d) == inf" },
  { "Simple Tests", "test_ck_assert_ldouble_infinite", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is infinite' failed: x == 0" },
  { "Simple Tests", "test_ck_assert_ldouble_infinite_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d is infinite' failed: 2%d == 0" },
  { "Simple Tests", "test_ck_assert_ldouble_nan", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x is NaN' failed: x == inf" },
  { "Simple Tests", "test_ck_assert_ldouble_nan_with_mod", CK_FAILURE, CK_MSG_TEXT, "Assertion '2%d is NaN' failed: 2%d == 0" },
#if ENABLE_REGEX
  { "Simple Tests", "test_ck_assert_ldouble_nonnan", CK_FAILURE, CK_MSG_REGEXP, "^Assertion 'x is not NaN' failed: x == -?nan$" },
  { "Simple Tests", "test_ck_assert_ldouble_nonnan_with_mod", CK_FAILURE, CK_MSG_REGEXP, "^Assertion '\\(2%s\\)\\*x is not NaN' failed: \\(2%s\\)\\*x == -?nan$" },
#else
  { "Simple Tests", "test_ck_assert_ldouble_nonnan", CK_PASS, CK_MSG_TEXT, "Passed" },
  { "Simple Tests", "test_ck_assert_ldouble_nonnan_with_mod", CK_PASS, CK_MSG_TEXT, "Passed" },
#endif
  { "Simple Tests", "test_ck_assert_ldouble_nan_and_inf_with_expr", CK_PASS, CK_MSG_TEXT, "Passed" },
  /* End of tests on long double macros */
  { "Simple Tests", "test_percent_n_escaped", CK_FAILURE, CK_MSG_TEXT, "Assertion 'returnsZero(\"%n\") == 1' failed: returnsZero(\"%n\") == 0, 1 == 1" },
  { "Simple Tests", "test_ck_assert_str_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion '\"test1\" == s' failed: \"test1\" == \"test1\", s == \"test2\"" },
  { "Simple Tests", "test_ck_assert_str_eq_with_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 't == s' failed: t == (null), s == (null)" },
  { "Simple Tests", "test_ck_assert_str_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 't != s' failed: t == \"test2\", s == \"test2\"" },
  { "Simple Tests", "test_ck_assert_str_ne_with_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 't != s' failed: t == \"test\", s == (null)" },
  { "Simple Tests", "test_ck_assert_str_lt", CK_FAILURE, CK_MSG_TEXT, "Assertion 's < s' failed: s == \"test1\", s == \"test1\"" },
  { "Simple Tests", "test_ck_assert_str_lt_with_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 's < t' failed: s == (null), t == \"test\"" },
  { "Simple Tests", "test_ck_assert_str_le", CK_FAILURE, CK_MSG_TEXT, "Assertion 't <= s' failed: t == \"test2\", s == \"test1\"" },
  { "Simple Tests", "test_ck_assert_str_le_with_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 't <= s' failed: t == (null), s == (null)" },
  { "Simple Tests", "test_ck_assert_str_gt", CK_FAILURE, CK_MSG_TEXT, "Assertion 't > t' failed: t == \"test2\", t == \"test2\"" },
  { "Simple Tests", "test_ck_assert_str_gt_with_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 't > s' failed: t == \"test\", s == (null)" },
  { "Simple Tests", "test_ck_assert_str_ge", CK_FAILURE, CK_MSG_TEXT, "Assertion 's >= t' failed: s == \"test1\", t == \"test2\"" },
  { "Simple Tests", "test_ck_assert_str_ge_with_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 's >= t' failed: s == (null), t == (null)" },
  { "Simple Tests", "test_ck_assert_str_expr", CK_PASS, CK_MSG_TEXT,    "Passed" },
  { "Simple Tests", "test_ck_assert_pstr_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion '\"test1\" == s' failed: \"test1\" == \"test1\", s == \"test\"" },
  { "Simple Tests", "test_ck_assert_pstr_eq_with_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 't == s' failed: t == \"test\", s == (null)" },
  { "Simple Tests", "test_ck_assert_pstr_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 't != s' failed: t == \"test2\", s == \"test2\"" },
  { "Simple Tests", "test_ck_assert_pstr_ne_with_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 't != s' failed: t == (null), s == (null)" },
  { "Simple Tests", "test_ck_assert_ptr_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == y' failed: x == 0x1, y == 0x2" },
  { "Simple Tests", "test_ck_assert_ptr_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x != z' failed: x == 0x1, z == 0x1" },
  { "Simple Tests", "test_ck_assert_ptr_null", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x == NULL' failed: x == 0x1" },
  { "Simple Tests", "test_ck_assert_ptr_nonnull", CK_FAILURE, CK_MSG_TEXT, "Assertion 'x != NULL' failed: x == 0" },
  { "Simple Tests", "test_ck_assert_mem_eq", CK_FAILURE, CK_MSG_TEXT, "Assertion '\"\\x00\\x00\\x00\\x00\\x01\" == s' failed: \"\\x00\\x00\\x00\\x00\\x01\" == \"0000000001\", s == \"0000000002\"" },
  { "Simple Tests", "test_ck_assert_mem_ne", CK_FAILURE, CK_MSG_TEXT, "Assertion 't != s' failed: t == \"0000000002\", s == \"0000000002\"" },
  { "Simple Tests", "test_ck_assert_mem_lt", CK_FAILURE, CK_MSG_TEXT, "Assertion 's < s' failed: s == \"0000000001\", s == \"0000000001\"" },
  { "Simple Tests", "test_ck_assert_mem_le", CK_FAILURE, CK_MSG_TEXT, "Assertion 't <= s' failed: t == \"0000000002\", s == \"0000000001\"" },
  { "Simple Tests", "test_ck_assert_mem_gt", CK_FAILURE, CK_MSG_TEXT, "Assertion 't > t' failed: t == \"0000000002\", t == \"0000000002\"" },
  { "Simple Tests", "test_ck_assert_mem_ge", CK_FAILURE, CK_MSG_TEXT, "Assertion 's >= t' failed: s == \"0000000001\", t == \"0000000002\"" },
  { "Simple Tests", "test_ck_assert_mem_zerolen", CK_PASS, CK_MSG_TEXT,    "Passed" },
  { "Simple Tests", "test_ck_assert_mem_eq_exact", CK_FAILURE, CK_MSG_TEXT, "Assertion 't == s' failed: t == \"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001\", s == \"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002\"" },
  { "Simple Tests", "test_ck_assert_mem_eq_longer", CK_FAILURE, CK_MSG_TEXT, "Assertion 't == s' failed: t == \"000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000..\", s == \"000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000..\"" },
  
#if defined(HAVE_FORK) && HAVE_FORK==1
  { "Signal Tests", "test_segv", CK_ERROR, CK_MSG_TEXT,   signal_11_str },
  { "Signal Tests", "test_segv_pass", CK_PASS, CK_MSG_TEXT,    "Passed" },
  { "Signal Tests", "test_segv", CK_ERROR, CK_MSG_TEXT,   signal_11_8_str },
  { "Signal Tests", "test_non_signal_8", CK_FAILURE, CK_MSG_TEXT, "Early exit with return value 0" },
  { "Signal Tests", "test_fail_unless", CK_FAILURE, CK_MSG_TEXT, "Early exit with return value 1" },
#if !defined(__CYGWIN__)
  { "Signal Tests", "test_fpe", CK_ERROR, CK_MSG_TEXT,   signal_8_str },
  { "Signal Tests", "test_mark_point", CK_ERROR, CK_MSG_TEXT,   signal_8_str },
#endif /* !defined(__CYGWIN__) */
#endif /* HAVE_FORK */

#if TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) && HAVE_FORK==1
#if HAVE_DECL_SETENV
  { "Environment Integer Timeout Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Environment Integer Timeout Tests", "test_sleep2_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Environment Integer Timeout Tests", "test_sleep5_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Environment Integer Timeout Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  
  { "Environment Double Timeout Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "Environment Double Timeout Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,  "Passed" },
  { "Environment Double Timeout Tests", "test_sleep1_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#endif /* HAVE_LIBRT */
  { "Environment Double Timeout Tests", "test_sleep2_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Environment Double Timeout Tests", "test_sleep5_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Environment Double Timeout Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#endif /* HAVE_DECL_SETENV */

  { "Default Timeout Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "Default Timeout Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Default Timeout Tests", "test_sleep1_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
#endif /* HAVE_LIBRT */
  { "Default Timeout Tests", "test_sleep2_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Default Timeout Tests", "test_sleep5_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Default Timeout Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  
  { "User Integer Timeout Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "User Integer Timeout Tests", "test_sleep2_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "User Integer Timeout Tests", "test_sleep5_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "User Integer Timeout Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },

  { "User Double Timeout Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "User Double Timeout Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "User Double Timeout Tests", "test_sleep1_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#endif /* HAVE_LIBRT */
  { "User Double Timeout Tests", "test_sleep2_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "User Double Timeout Tests", "test_sleep5_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "User Double Timeout Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  
#if HAVE_DECL_SETENV
  { "Environment Integer Timeout Scaling Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "Environment Integer Timeout Scaling Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Environment Integer Timeout Scaling Tests", "test_sleep1_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
#endif /* HAVE_LIBRT */
  { "Environment Integer Timeout Scaling Tests", "test_sleep2_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Environment Integer Timeout Scaling Tests", "test_sleep5_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Environment Integer Timeout Scaling Tests", "test_sleep9_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Environment Integer Timeout Scaling Tests", "test_sleep14_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },

  { "Environment Double Timeout Scaling Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "Environment Double Timeout Scaling Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Environment Double Timeout Scaling Tests", "test_sleep1_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#endif /* HAVE_LIBRT */
  { "Environment Double Timeout Scaling Tests", "test_sleep2_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Environment Double Timeout Scaling Tests", "test_sleep5_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Environment Double Timeout Scaling Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Environment Double Timeout Scaling Tests", "test_sleep14_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  
  { "Timeout Integer Scaling Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "Timeout Integer Scaling Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Timeout Integer Scaling Tests", "test_sleep1_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Timeout Integer Scaling Tests", "test_sleep2_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
#endif /* HAVE_LIBRT */
  { "Timeout Integer Scaling Tests", "test_sleep5_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Timeout Integer Scaling Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  
  { "Timeout Double Scaling Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "Timeout Double Scaling Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "Timeout Double Scaling Tests", "test_sleep1_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
#endif /* HAVE_LIBRT */
  { "Timeout Double Scaling Tests", "test_sleep2_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Timeout Double Scaling Tests", "test_sleep5_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "Timeout Double Scaling Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  
  { "User Integer Timeout Scaling Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "User Integer Timeout Scaling Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "User Integer Timeout Scaling Tests", "test_sleep1_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
#endif /* HAVE_LIBRT */
  { "User Integer Timeout Scaling Tests", "test_sleep2_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "User Integer Timeout Scaling Tests", "test_sleep5_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "User Integer Timeout Scaling Tests", "test_sleep9_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "User Integer Timeout Scaling Tests", "test_sleep14_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  
  { "User Double Timeout Scaling Tests", "test_eternal_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#ifdef HAVE_LIBRT
  { "User Double Timeout Scaling Tests", "test_sleep0_025_pass", CK_PASS, CK_MSG_TEXT,   "Passed" },
  { "User Double Timeout Scaling Tests", "test_sleep1_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#endif /* HAVE_LIBRT */
  { "User Double Timeout Scaling Tests", "test_sleep2_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "User Double Timeout Scaling Tests", "test_sleep5_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "User Double Timeout Scaling Tests", "test_sleep9_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
  { "User Double Timeout Scaling Tests", "test_sleep14_fail", CK_ERROR, CK_MSG_TEXT,  "Test timeout expired" },
#endif /* HAVE_DECL_SETENV */
#endif /* TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) */

#if defined(HAVE_FORK) && HAVE_FORK==1
  { "Limit Tests", "test_early_exit", CK_ERROR, CK_MSG_TEXT,   "Early exit with return value 1" },
#endif /* HAVE_FORK */
#if MEMORY_LEAKING_TESTS_ENABLED
  { "Limit Tests", "test_null", CK_FAILURE, CK_MSG_TEXT, "Completed properly" },
#endif /* MEMORY_LEAKING_TESTS_ENABLED */
  { "Limit Tests", "test_null_2", CK_FAILURE, CK_MSG_TEXT, "Completed properly" },

#if defined(HAVE_FORK) && HAVE_FORK==1
  { "Msg and fork Tests", "test_fork1p_pass", CK_PASS, CK_MSG_TEXT,       "Passed" },
  { "Msg and fork Tests", "test_fork1p_fail", CK_FAILURE, CK_MSG_TEXT,    "Expected fail" },
  { "Msg and fork Tests", "test_fork1c_pass", CK_PASS, CK_MSG_TEXT,       "Passed" },
  { "Msg and fork Tests", "test_fork1c_fail", CK_FAILURE, CK_MSG_TEXT,    "Expected fail" },
  { "Msg and fork Tests", "test_fork2_pass", CK_PASS, CK_MSG_TEXT,       "Passed" },
  { "Msg and fork Tests", "test_fork2_fail", CK_FAILURE, CK_MSG_TEXT,    "Expected fail" },
#endif  /* HAVE_FORK */

#if defined(HAVE_FORK) && HAVE_FORK==1
#if MEMORY_LEAKING_TESTS_ENABLED
  { "Check Errors Tests", "test_invalid_set_fork_status", CK_FAILURE, CK_MSG_TEXT,    "Early exit with return value 2" },
#endif
  { "Check Ignore Exit Handlers", "test_ignore_exit_handlers", CK_FAILURE, CK_MSG_TEXT, "Failed" },
#endif /* HAVE_FORK */

  { "Core", "test_srunner", CK_PASS, CK_MSG_TEXT,    "Passed" },
  { "Core", "test_2nd_suite", CK_FAILURE, CK_MSG_TEXT, "We failed" }
};

static int nr_of_master_tests = sizeof master_tests /sizeof master_tests[0];

START_TEST(test_check_nfailures)
{
  int i;
  int number_failed = 0;
  
  for (i = 0; i < nr_of_master_tests; i++) {
    if (master_tests[i].failure_type != CK_PASS) {
      number_failed++;
    }
  }
  ck_assert_msg (sub_nfailed == number_failed,
               "Unexpected number of failures received, %d, expected %d.",
               sub_nfailed, number_failed);
}
END_TEST

START_TEST(test_check_ntests_run)
{
  ck_assert_msg (sub_ntests == nr_of_master_tests,
               "Unexpected number of tests run %d vs expected %d", sub_ntests, nr_of_master_tests);
}
END_TEST


START_TEST(test_check_failure_msgs)
{
  int i;
  int passed = 0;
  const char *got_msg;
  const char *expected_msg;
  unsigned char not_equal = 0;
  const char *msg_type_str;
  int reg_err;
  char err_text[256];
  TestResult *tr;
#if ENABLE_REGEX
  regex_t re;
#endif

  for (i = 0; i < sub_ntests; i++) {
    master_test_t *master_test = &master_tests[i];

    if (master_test->failure_type == CK_PASS) {
      passed++;
      continue;
    }

    ck_assert_msg(i - passed <= sub_nfailed, NULL);
    tr = tr_fail_array[i - passed];
    ck_assert_msg(tr != NULL, NULL);
    got_msg = tr_msg(tr);
    expected_msg = master_test->msg;

    switch (master_test->msg_type) {
      case CK_MSG_TEXT:
        if (strcmp(got_msg, expected_msg) != 0) {
          not_equal = 1;
        }
        break;
#if ENABLE_REGEX
      case CK_MSG_REGEXP: {
        reg_err = regcomp(&re, expected_msg, REG_EXTENDED | REG_NOSUB);
        if (reg_err) {        
          regerror(reg_err, &re, err_text, sizeof(err_text));
          ck_assert_msg(reg_err == 0,
                  "For test %d:%s:%s Expected regexp '%s', but regcomp returned error '%s'",
                  i, master_test->tcname, master_test->test_name, expected_msg,
                  err_text);
        }
        reg_err = regexec(&re, got_msg, 0, NULL, 0);
        regfree(&re);
        if (reg_err) {
          not_equal = 1;
        }
        break;
      }
#endif /* ENABLE_REGEX */
      default:
        /* Program should not reach here */
        break;
    }
    
    if (not_equal) {      
      switch(master_test->msg_type) {
#if ENABLE_REGEX
      case CK_MSG_REGEXP:
        msg_type_str = " regexp";
        break;
#endif
      default:
        msg_type_str = "";
      }

      ck_abort_msg("For test %d:%s:%s Expected%s '%s', got '%s'",
               i, master_test->tcname, master_test->test_name, msg_type_str,
               expected_msg, got_msg);
    }
  }
}
END_TEST
  
START_TEST(test_check_failure_lnos)
{
  int i;
  long line_no;
  int passed = 0;
  int number_failed;
  TestResult *tr;
  
  /* Create list of line numbers where failures occurred */
  rewind(line_num_failures);

  for (i = 0; i < sub_ntests; i++) {
    if (master_tests[i].failure_type == CK_PASS) {
      passed++;
      continue;
    }

    number_failed = i - passed;

    ck_assert_msg(i - passed <= sub_nfailed, NULL);
    tr = tr_fail_array[number_failed];
    ck_assert_msg(tr != NULL, NULL);
    line_no = get_next_failure_line_num(line_num_failures);

    if(line_no == -1)
    {
      ck_abort_msg("Did not find the %dth failure line number for suite %s, msg %s",
        (number_failed+1), tr_tcname(tr), tr_msg(tr));
    }

    if (line_no > 0 && tr_lno(tr) != line_no) {
      ck_abort_msg("For test %d (failure %d): Expected lno %ld, got %d for suite %s, msg %s",
               i, number_failed, line_no, tr_lno(tr), tr_tcname(tr), tr_msg(tr));
    }    
  }

  /* At this point, there should be no remaining failures */
  line_no = get_next_failure_line_num(line_num_failures);
  ck_assert_msg(line_no == -1,
    "No more failure line numbers expected, but found %ld", line_no);
}
END_TEST

START_TEST(test_check_failure_ftypes)
{
  int i;
  int passed = 0;
  TestResult *tr;
  
  for (i = 0; i < sub_ntests; i++) {
    if (master_tests[i].failure_type == CK_PASS) {
      passed++;
      continue;
    }

    ck_assert_msg(i - passed <= sub_nfailed, NULL);
    tr = tr_fail_array[i - passed];
    ck_assert_msg(tr != NULL, NULL);
    ck_assert_msg(master_tests[i].failure_type == tr_rtype(tr),
                "Failure type wrong for test %d:%s:%s",
                i, master_tests[i].tcname, master_tests[i].test_name);
  }
}
END_TEST

START_TEST(test_check_failure_lfiles)
{
  int i;
  for (i = 0; i < sub_nfailed; i++) {
    TestResult *tr = tr_fail_array[i];
    ck_assert_msg(tr != NULL, NULL);
    ck_assert_msg(tr_lfile(tr) != NULL, "Bad file name for test %d", i);
    ck_assert_msg(strstr(tr_lfile(tr), "check_check_sub.c") != 0,
                "Bad file name for test %d:%s:%s",
                i, master_tests[i].tcname, master_tests[i].test_name);
  }
}
END_TEST

START_TEST(test_check_tcnames)
{
  const char *tcname;   
  tcname = tr_tcname(tr_all_array[_i]);
  if (strcmp(tcname, master_tests[_i].tcname) != 0) {
    ck_abort_msg("Expected '%s', got '%s' for test %d:%s",
         master_tests[_i].tcname, tcname,
         _i, master_tests[_i].test_name);
  } 
}
END_TEST

START_TEST(test_check_test_names)
{
  int i;

  rewind(test_names_file);

  for (i = 0; i < sub_ntests; i++)
  {
      char* test_name = get_next_test_name(test_names_file);

      if(test_name == NULL || strcmp(master_tests[i].test_name, test_name) != 0)
      {
          ck_abort_msg("Expected test name '%s' but found '%s' for test %d:%s",
                  master_tests[i].test_name,
                  (test_name == NULL ? "(null)" : test_name),
                  i, master_tests[i].tcname);
      }

      free(test_name);
  }
}
END_TEST

START_TEST(test_check_all_msgs)
{
  const char *got_msg = tr_msg(tr_all_array[_i]);
  master_test_t *master_test = &master_tests[_i];
  const char *expected_msg = master_test->msg;
  unsigned char not_equal = 0;
#if ENABLE_REGEX
  regex_t re;
#endif

  switch (master_test->msg_type) {
    case CK_MSG_TEXT:
      if (strcmp(got_msg, expected_msg) != 0) {
        not_equal = 1;
      }
      break;
#if ENABLE_REGEX
    case CK_MSG_REGEXP: {
      int reg_err = regcomp(&re, expected_msg, REG_EXTENDED | REG_NOSUB);
      if (reg_err) {      
        char err_text[256];
        regerror(reg_err, &re, err_text, sizeof(err_text));
        ck_assert_msg(reg_err == 0,
                  "For test %d:%s:%s Expected regexp '%s', but regcomp returned error '%s'",
                  _i, master_test->tcname, master_test->test_name, expected_msg,
                  err_text);
      }
      reg_err = regexec(&re, got_msg, 0, NULL, 0);
      regfree(&re);
      if (reg_err) {
        not_equal = 1;
      }
      break;
    }
#endif /* ENABLE_REGEX */
    default:
      /* Program should not reach here */
      break;
  }

  if (not_equal) {    
    const char *msg_type_str;
    switch(master_test->msg_type) {
#if ENABLE_REGEX
    case CK_MSG_REGEXP:
      msg_type_str = " regexp";
      break;
#endif
    default:
      msg_type_str = "";
    }

    ck_abort_msg("For test %i:%s:%s expected%s '%s', got '%s'",
       _i, master_test->tcname, master_test->test_name, msg_type_str,
       expected_msg, got_msg);
  }
}
END_TEST  

START_TEST(test_check_all_ftypes)
{
  ck_assert_msg(master_tests[_i].failure_type == tr_rtype(tr_all_array[_i]),
              "For test %d:%s:%s failure type wrong, expected %d but got %d",
              _i, master_tests[_i].tcname, master_tests[_i].test_name,
              master_tests[_i].failure_type, tr_rtype(tr_all_array[_i]));
}
END_TEST

int test_fixture_val = 0;
static void test_fixture_setup(void)
{
  test_fixture_val = 1;
}

START_TEST(test_setup)
{
  ck_assert_msg (test_fixture_val == 1,
               "Value not setup or changed across tests correctly");
  test_fixture_val = 2;
}
END_TEST

static void test_fixture_teardown (void)
{
  test_fixture_val = 3;
}

START_TEST(test_teardown)
{
  ck_assert_msg (test_fixture_val == 3,
                "Value not changed correctly in teardown");
}
END_TEST  


Suite *make_master_suite (void)
{
  Suite *s;
  TCase *tc_core;
  TCase *tc_fixture;
  TCase *tc_post_teardown;
  
  s = suite_create("Master");
  tc_core = tcase_create("Core Tests");
  tc_fixture = tcase_create("Fixture Setup Tests");
  suite_add_tcase (s, tc_core);
  tcase_add_test (tc_core, test_check_nfailures);
  tcase_add_test (tc_core, test_check_ntests_run);
  tcase_add_test (tc_core, test_check_failure_msgs);
  tcase_add_test (tc_core, test_check_failure_ftypes);
  tcase_add_test (tc_core, test_check_failure_lnos);
  tcase_add_test (tc_core, test_check_failure_lfiles);
  tcase_add_test (tc_core, test_check_test_names);
  tcase_add_loop_test (tc_core, test_check_tcnames, 0, sub_ntests);
  tcase_add_loop_test (tc_core, test_check_all_msgs, 0, sub_ntests);
  tcase_add_loop_test (tc_core, test_check_all_ftypes, 0, nr_of_master_tests);
  tcase_add_unchecked_fixture(tc_fixture, test_fixture_setup,
                              test_fixture_teardown);
  /* add the test 3 times to make sure we adequately test
     preservation of fixture values across tests, regardless
     of the order in which tests are added to the test case */
  tcase_add_test (tc_fixture, test_setup);
#if defined(HAVE_FORK) && HAVE_FORK==1
  /* The remaining test runs only work if fork() is available. */
  tcase_add_test (tc_fixture, test_setup);
  tcase_add_test (tc_fixture, test_setup);
#endif /* HAVE_FORK */
  suite_add_tcase (s, tc_fixture);
  tc_post_teardown = tcase_create ("Fixture Teardown Tests");
  tcase_add_test (tc_post_teardown, test_teardown);
  suite_add_tcase (s, tc_post_teardown);
  return s;
}

static void init_signal_strings(void)
{
  /* strsignal may overwrite the string returned by the previous call */
  char *s8 = strdup(strsignal(8));
  char *s11 = strdup(strsignal(11));
  int n;
  n = snprintf(signal_11_str, SIG_STR_LEN, "Received signal 11 (%s)", s11);
  assert(n < SIG_STR_LEN);
  n = snprintf(signal_11_8_str, SIG_STR_LEN, "Received signal 11 (%s), expected 8 (%s)", s11, s8);
  assert(n < SIG_STR_LEN);
  n = snprintf(signal_8_str, SIG_STR_LEN, "Received signal 8 (%s)", s8);
  assert(n < SIG_STR_LEN);
  free(s8);
  free(s11);
}

void setup (void)
{
  Suite *s = make_sub_suite();
  SRunner *sr = srunner_create(s);

  init_signal_strings();

  /*
   * Create files that will contain the test names and line numbers of the failures
   * in check_check_sub.c, as they occur.
   */
#if !HAVE_MKSTEMP
  test_names_file_name = tempnam(NULL, "check_test_names_");
  test_names_file = fopen(test_names_file_name, "w+b");
  line_num_failures_file_name = tempnam(NULL, "check_error_linenums_");
  line_num_failures = fopen(line_num_failures_file_name, "w+b");
#else
  test_names_file_name = strdup("check_test_names__XXXXXX");
  assert(test_names_file_name != NULL && "strdup() failed");
  test_names_file = fdopen(mkstemp(test_names_file_name), "w+b");
  line_num_failures_file_name = strdup("check_error_linenums_XXXXXX");
  assert(line_num_failures_file_name != NULL && "strdup() failed");
  line_num_failures = fdopen(mkstemp(line_num_failures_file_name), "w+b");
#endif

  srunner_add_suite(sr, make_sub2_suite());

  srunner_run_all(sr, CK_VERBOSE);
  tr_fail_array = srunner_failures(sr);
  tr_all_array = srunner_results(sr);
  sub_nfailed = srunner_ntests_failed(sr);
  sub_ntests = srunner_ntests_run(sr);
}

void cleanup (void)
{
  fclose(line_num_failures);
  line_num_failures = NULL;
  unlink(line_num_failures_file_name);
  free(line_num_failures_file_name);
  line_num_failures_file_name = NULL;

  fclose(test_names_file);
  test_names_file = NULL;
  unlink(test_names_file_name);
  free(test_names_file_name);
  test_names_file_name = NULL;
}

void record_test_name(const char* test_name)
{
  int result;

  if(test_names_file == NULL)
  {
    /*
     * The file may not be setup. This may be because some of the tests
     * are being reused outside of the master suite. This is OK.
     * If the master suite runs and does not find test names it will
     * fail as expected.
     */
     fprintf(stderr, "Test name file not setup, not reporting test failure");
     return;
  }

  fprintf(test_names_file, "%s\n", test_name);

  result = fflush(test_names_file);
  if(result != 0)
  {
    fprintf(stderr, "%s:%d: Error in call to fflush", __FILE__, __LINE__);
    exit(1);
  }
}

char* get_next_test_name(FILE * file)
{
  char * line = NULL;
  size_t length;
  ssize_t written;

  written = getline(&line, &length, file);
  /**
   * getline() will leave a \n at the end of the line,
   * remove it if it is present.
   */
  if(written > 0 && line[written-1] == '\n')
  {
      line[written-1] = '\0';
  }

  return line;
}

void record_failure_line_num(int linenum)
{
  size_t to_write;
  size_t written;
  int result, chars_printed;
  char string[16];

  /*
   * Because this call will occur right before a failure,
   * add +1 so the linenum will be that of the failure
   */
   linenum += 1;

  chars_printed = snprintf(string, sizeof(string), "%d\n", linenum);
  if(chars_printed <= 0 || (size_t) chars_printed >= sizeof(string))
  {
    fprintf(stderr, "%s:%d: Error in call to snprintf:", __FILE__, __LINE__);
    exit(1);
  }
  to_write = (size_t) chars_printed;

  if(line_num_failures == NULL)
  {
    /*
     * The file may not be setup. This may be because some of the tests
     * are being reused outside of the master suite. This is OK.
     * If the master suite runs and does not find line numbers it will
     * fail as expected.
     */
     fprintf(stderr, "Line number file not setup, not reporting test failure     line: %s", string);
     return;
  }

  written = fwrite(string, 1, to_write, line_num_failures);
  if(written != to_write)
  {
    fprintf(stderr, "%s:%d: Error in call to fwrite, wrote %zd instead of %zu:", __FILE__, __LINE__, written, to_write);
    exit(1);
  }

  result = fflush(line_num_failures);
  if(result != 0)
  {
    fprintf(stderr, "%s:%d: Error in call to fflush", __FILE__, __LINE__);
    exit(1);
  }
}

long get_next_failure_line_num(FILE * file)
{
  char * line = NULL;
  char * end = NULL;
  size_t length;
  ssize_t written;
  long value = -1;

  written = getline(&line, &length, file);

  if(written > 0)
  {
    /*
     * getline() will leave the \n at the end of the parsed line, if
     * it is found. Remove this before passing to strtol, so we
     * may detect invalid characters by checking for \0 instead
     */

    if(line[written-1] == '\n')
    {
      line[written-1] = '\0';
    }

    value = strtol(line, &end, 10);

    if(value <= 0 || *end != '\0')
    {
      fprintf(stderr, "%s:%d: Failed to convert next failure line number, found '%s'\n",
        __FILE__, __LINE__, line);
      exit(1);
    }
  }

  free(line);

  return value;
}
