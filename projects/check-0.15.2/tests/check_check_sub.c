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

#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <check.h>
#include "check_check.h"


START_TEST(test_lno)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_abort_msg("Failure expected");
}
END_TEST

#if defined(HAVE_FORK) && HAVE_FORK==1
START_TEST(test_mark_lno)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  mark_point();
  exit(EXIT_FAILURE); /* should fail with mark_point above as line */
}
END_TEST
#endif /* HAVE_FORK */

START_TEST(test_pass)
{
  record_test_name(tcase_name());

  ck_assert_msg(1 == 1, "This test should pass");
  ck_assert_msg(9999, "This test should pass");
}
END_TEST

START_TEST(test_fail_unless)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  fail_unless(1 == 2, "This test should fail");
}
END_TEST

START_TEST(test_fail_if_pass)
{
  record_test_name(tcase_name());

  fail_if(1 == 2, "This test should pass");
  fail_if(0, "This test should pass");
}
END_TEST

START_TEST(test_fail_if_fail)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  fail_if(1 == 1, "This test should fail");
}
END_TEST

START_TEST(test_fail_null_msg)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  fail_unless(2 == 3, NULL);
}
END_TEST

#if defined(__GNUC__)
START_TEST(test_fail_no_msg)
{
  record_test_name(tcase_name());

  /* taking out the NULL provokes an ISO C99 warning in GCC */
  record_failure_line_num(__LINE__);
  fail_unless(4 == 5, NULL);
}
END_TEST
#endif /* __GNUC__ */
START_TEST(test_fail_if_null_msg)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  fail_if(2 != 3, NULL);
}
END_TEST

#if defined(__GNUC__)
START_TEST(test_fail_if_no_msg)
{
  record_test_name(tcase_name());

  /* taking out the NULL provokes an ISO C99 warning in GCC */
  record_failure_line_num(__LINE__);
  fail_if(4 != 5, NULL);
}
END_TEST
#endif /* __GNUC__ */
START_TEST(test_fail_vararg_msg_1)
{
  int x = 3;
  int y = 4;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  fail_unless(x == y, "%d != %d", x, y);
}
END_TEST

START_TEST(test_fail_vararg_msg_2)
{
  int x = 5;
  int y = 6;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  fail_if(x != y, "%d != %d", x, y);
}
END_TEST

START_TEST(test_fail_vararg_msg_3)
{
  int x = 7;
  int y = 7;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  fail("%d == %d", x, y);
}
END_TEST

#if defined(__GNUC__)
START_TEST(test_fail_empty)
{
  record_test_name(tcase_name());

  /* plain fail() doesn't compile with xlc in C mode because of `, ## __VA_ARGS__' problem */
  /* on the other hand, taking out the NULL provokes an ISO C99 warning in GCC */
  record_failure_line_num(__LINE__);
  fail(NULL);
}
END_TEST
#endif /* __GNUC__ */

START_TEST(test_ck_abort)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_abort();
}
END_TEST

START_TEST(test_ck_abort_msg)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_abort_msg("Failure expected");
}
END_TEST

/* FIXME: perhaps passing NULL to ck_abort_msg should be an error. */
START_TEST(test_ck_abort_msg_null)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_abort_msg(NULL);
}
END_TEST

/* These ck_assert tests are all designed to fail on the last
   assertion. */

START_TEST(test_ck_assert)
{
  int x = 3;
  int y = 3;

  record_test_name(tcase_name());

  ck_assert(1);
  ck_assert(x == y);
  y++;
  ck_assert(x != y);
  record_failure_line_num(__LINE__);
  ck_assert(x == y);
}
END_TEST

START_TEST(test_ck_assert_null)
{
  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert(0);
}
END_TEST

START_TEST(test_ck_assert_with_mod)
{
  int f = 1;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert(1%f == 1);
}
END_TEST

START_TEST(test_ck_assert_int_eq)
{
  int x = 3;
  int y = 3;

  record_test_name(tcase_name());

  ck_assert_int_eq(x, y);
  y++;
  record_failure_line_num(__LINE__);
  ck_assert_int_eq(x, y);
}
END_TEST

START_TEST(test_ck_assert_int_eq_with_mod)
{
  int d = 2;
  int f = 1;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_int_eq(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_int_ne)
{
  int x = 3;
  int y = 2;

  record_test_name(tcase_name());

  ck_assert_int_ne(x, y);
  y++;
  record_failure_line_num(__LINE__);
  ck_assert_int_ne(x, y);
}
END_TEST

START_TEST(test_ck_assert_int_ne_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_int_ne(3%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_int_lt)
{
  int x = 2;
  int y = 3;

  record_test_name(tcase_name());

  ck_assert_int_lt(x, y);
  record_failure_line_num(__LINE__);
  ck_assert_int_lt(x, x);
}
END_TEST

START_TEST(test_ck_assert_int_lt_with_mod)
{
  int d = 2;
  int f = 1;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_int_lt(3%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_int_le)
{
  int x = 2;
  int y = 3;

  record_test_name(tcase_name());

  ck_assert_int_le(x, y);
  ck_assert_int_le(x, x);
  record_failure_line_num(__LINE__);
  ck_assert_int_le(y, x);
}
END_TEST

START_TEST(test_ck_assert_int_le_with_mod)
{
  int d = 2;
  int f = 1;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_int_le(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_int_gt)
{
  int x = 2;
  int y = 3;

  record_test_name(tcase_name());

  ck_assert_int_gt(y, x);
  record_failure_line_num(__LINE__);
  ck_assert_int_gt(y, y);
}
END_TEST

START_TEST(test_ck_assert_int_gt_with_mod)
{
  int d = 1;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_int_gt(3%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_int_ge)
{
  int x = 2;
  int y = 3;

  record_test_name(tcase_name());

  ck_assert_int_ge(y, x);
  ck_assert_int_ge(y, x);
  record_failure_line_num(__LINE__);
  ck_assert_int_ge(x, y);
}
END_TEST

START_TEST(test_ck_assert_int_ge_with_mod)
{
  int d = 1;
  int f = 3;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_int_ge(3%d, 4%f);
}
END_TEST

START_TEST(test_ck_assert_int_expr)
{
  int x = 1;
  int y = 0;

  record_test_name(tcase_name());

  ck_assert_int_eq(x, ++y);
  ck_assert_int_eq(x, y);
} END_TEST

START_TEST(test_ck_assert_uint_eq)
{
  unsigned int x = 3;
  unsigned int y = 3;

  record_test_name(tcase_name());

  ck_assert_uint_eq(x, y);
  y++;
  record_failure_line_num(__LINE__);
  ck_assert_uint_eq(x, y);
}
END_TEST

START_TEST(test_ck_assert_uint_eq_with_mod)
{
  int d = 2;
  int f = 1;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_uint_eq(3%d, 1%f);
}
END_TEST

START_TEST(test_ck_assert_uint_ne)
{
  unsigned int x = 3;
  unsigned int y = 2;

  record_test_name(tcase_name());

  ck_assert_uint_ne(x, y);
  y++;
  record_failure_line_num(__LINE__);
  ck_assert_uint_ne(x, y);
}
END_TEST

START_TEST(test_ck_assert_uint_ne_with_mod)
{
  int d = 1;
  int f = 1;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_uint_ne(1%d, 1%f);
}
END_TEST

START_TEST(test_ck_assert_uint_lt)
{
  unsigned int x = 2;
  unsigned int y = 3;

  record_test_name(tcase_name());

  ck_assert_uint_lt(x, y);
  record_failure_line_num(__LINE__);
  ck_assert_uint_lt(x, x);
}
END_TEST

START_TEST(test_ck_assert_uint_lt_with_mod)
{
  int d = 2;
  int f = 1;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_uint_lt(3%d, 1%f);
}
END_TEST

START_TEST(test_ck_assert_uint_le)
{
  unsigned int x = 2;
  unsigned int y = 3;

  record_test_name(tcase_name());

  ck_assert_uint_le(x, y);
  ck_assert_uint_le(x, x);
  record_failure_line_num(__LINE__);
  ck_assert_uint_le(y, x);
}
END_TEST

START_TEST(test_ck_assert_uint_le_with_mod)
{
  int d = 2;
  int f = 1;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_uint_le(3%d, 1%f);
}
END_TEST

START_TEST(test_ck_assert_uint_gt)
{
  unsigned int x = 2;
  unsigned int y = 3;

  record_test_name(tcase_name());

  ck_assert_uint_gt(y, x);
  record_failure_line_num(__LINE__);
  ck_assert_uint_gt(y, y);
}
END_TEST

START_TEST(test_ck_assert_uint_gt_with_mod)
{
  int d = 1;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_uint_gt(1%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_uint_ge)
{
  unsigned int x = 2;
  unsigned int y = 3;

  record_test_name(tcase_name());

  ck_assert_uint_ge(y, x);
  ck_assert_uint_ge(y, x);
  record_failure_line_num(__LINE__);
  ck_assert_uint_ge(x, y);
}
END_TEST

START_TEST(test_ck_assert_uint_ge_with_mod)
{
  int d = 1;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_uint_ge(1%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_uint_expr)
{
  unsigned int x = 1;
  unsigned int y = 0;

  record_test_name(tcase_name());

  ck_assert_uint_eq(x, ++y);
  ck_assert_uint_eq(x, y);
} END_TEST

START_TEST(test_ck_assert_float_eq)
{
  float x = 1.1f;
  float y = 1.1f;

  record_test_name(tcase_name());

  ck_assert_float_eq(x, y);
  y+=0.1f;
  record_failure_line_num(__LINE__);
  ck_assert_float_eq(x, y);
}
END_TEST

START_TEST(test_ck_assert_float_eq_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_eq(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_float_ne)
{
  float x = 1.1f;
  float y = 1.2f;

  record_test_name(tcase_name());

  ck_assert_float_ne(x, y);
  y = x;
  record_failure_line_num(__LINE__);
  ck_assert_float_ne(x, y);
}
END_TEST

START_TEST(test_ck_assert_float_ne_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_ne(1%d, 1%f);
}
END_TEST

START_TEST(test_ck_assert_float_lt)
{
  float x = 2.0f;
  float y = 2.5f;

  record_test_name(tcase_name());

  ck_assert_float_lt(x, y);
  y-=1.0f;
  record_failure_line_num(__LINE__);
  ck_assert_float_lt(x, y);
}
END_TEST

START_TEST(test_ck_assert_float_lt_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_lt(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_float_le)
{
  float x = 2.0f;
  float y = 2.5f;

  record_test_name(tcase_name());

  ck_assert_float_le(x, y);
  ck_assert_float_le(x, x);
  y-=1.0f;
  record_failure_line_num(__LINE__);
  ck_assert_float_le(x, y);
}
END_TEST

START_TEST(test_ck_assert_float_le_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_le(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_float_gt)
{
  float x = 2.5f;
  float y = 2.0f;

  record_test_name(tcase_name());

  ck_assert_float_gt(x, y);
  y+=1.0f;
  record_failure_line_num(__LINE__);
  ck_assert_float_gt(x, y);
}
END_TEST

START_TEST(test_ck_assert_float_gt_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_gt(2%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_float_ge)
{
  float x = 2.5f;
  float y = 2.0f;

  record_test_name(tcase_name());

  ck_assert_float_ge(x, y);
  ck_assert_float_ge(x, x);
  y+=1.0f;
  record_failure_line_num(__LINE__);
  ck_assert_float_ge(x, y);
}
END_TEST

START_TEST(test_ck_assert_float_ge_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_ge(2%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_float_with_expr)
{
  float x[] = {NAN, 1.1f, 1.1f, 1.2f, 1.2f, NAN};
  float y = 1.1f;
  int i;

  record_test_name(tcase_name());

  i = 1;
  ck_assert_float_eq(x[i++], y);
  ck_assert_float_eq(x[i++], y);

  i = 4;
  ck_assert_float_ne(x[i--], y);
  ck_assert_float_ne(x[i--], y);

  y = 1.15f;

  i = 1;
  ck_assert_float_le(x[i++], y);
  ck_assert_float_le(x[i++], y);

  i = 1;
  ck_assert_float_lt(x[i++], y);
  ck_assert_float_lt(x[i++], y);

  i = 4;
  ck_assert_float_gt(x[i--], y);
  ck_assert_float_gt(x[i--], y);

  i = 4;
  ck_assert_float_ge(x[i--], y);
  ck_assert_float_ge(x[i--], y);
}
END_TEST

START_TEST(test_ck_assert_float_eq_tol)
{
  float x = 0.0001f;
  float y = 0.0003f;
  float t = 0.001f;

  record_test_name(tcase_name());

  ck_assert_float_eq_tol(x, y, t);
  ck_assert_float_eq_tol(x, x, t);
  x*=10.0f;
  y*=10.0f;
  t*=10.0f;
  ck_assert_float_eq_tol(x, y, t);
  t/=10.0f;
  record_failure_line_num(__LINE__);
  ck_assert_float_eq_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_float_eq_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_eq_tol(3%d, 2%f, 2%p);
}
END_TEST

START_TEST(test_ck_assert_float_ne_tol)
{
  float x = 0.0001f;
  float y = 0.0002f;
  float t = 0.0001f;

  record_test_name(tcase_name());

  ck_assert_float_ne_tol(x, y, t);
  x*=10.0f;
  y*=10.0f;
  t*=10.0f;
  ck_assert_float_ne_tol(x, y, t);
  t*=10.0f;
  record_failure_line_num(__LINE__);
  ck_assert_float_ne_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_float_ne_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_ne_tol(3%d, 3%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_float_ge_tol)
{
  float x = 0.001f;
  float y = 0.003f;
  float t = 0.001f;

  record_test_name(tcase_name());

  ck_assert_float_ge_tol(y, x, t);
  ck_assert_float_ge_tol(x, x, t);
  ck_assert_float_ge_tol(y, y, t);
  x*=10.0f;
  y*=10.0f;
  t*=10.0f;
  ck_assert_float_ge_tol(y, x, t);
  ck_assert_float_ge_tol(x, x, t);
  ck_assert_float_ge_tol(y, y, t);
  record_failure_line_num(__LINE__);
  ck_assert_float_ge_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_float_ge_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_ge_tol(2%d, 3%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_float_le_tol)
{
  float x = 0.001f;
  float y = 0.003f;
  float t = 0.001f;

  record_test_name(tcase_name());

  ck_assert_float_le_tol(x, y, t);
  ck_assert_float_le_tol(x, x, t);
  ck_assert_float_le_tol(y, y, t);
  x*=10.0f;
  y*=10.0f;
  t*=10.0f;
  ck_assert_float_le_tol(x, y, t);
  ck_assert_float_le_tol(x, x, t);
  ck_assert_float_le_tol(y, y, t);
  record_failure_line_num(__LINE__);
  ck_assert_float_le_tol(y, x, t);
}
END_TEST

START_TEST(test_ck_assert_float_le_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_le_tol(3%d, 2%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_float_tol_with_expr)
{
  float x[] = {NAN, 1.1f, 1.1f, 1.2f, 1.2f, NAN};
  float y = 1.1f;
  float t = 0.01f;
  int i;

  record_test_name(tcase_name());

  i = 1;
  ck_assert_float_eq_tol(x[i++], y, t);
  ck_assert_float_eq_tol(x[i++], y, t);

  i = 4;
  ck_assert_float_ne_tol(x[i--], y, t);
  ck_assert_float_ne_tol(x[i--], y, t);

  y = 1.15f;

  i = 1;
  ck_assert_float_le_tol(x[i++], y, t);
  ck_assert_float_le_tol(x[i++], y, t);

  i = 4;
  ck_assert_float_ge_tol(x[i--], y, t);
  ck_assert_float_ge_tol(x[i--], y, t);
}
END_TEST

START_TEST(test_ck_assert_float_finite)
{
  float x = 0.0001f;
  float t = 1.0f;

  record_test_name(tcase_name());

  ck_assert_float_finite(x);
  /* MS VS doesn't allow explicit division by zero */
  x = 1.0f / (1.0f - t);
  record_failure_line_num(__LINE__);
  ck_assert_float_finite(x);
}
END_TEST

START_TEST(test_ck_assert_float_finite_with_mod)
{
  int d = 2;
  float t = 1.0f;
  float x = 1.0f / (1.0f - t);

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_finite(x*(1%d));
}
END_TEST

START_TEST(test_ck_assert_float_infinite)
{
  float t = 1.0f;
  float x = 1.0f / (1.0f - t);

  record_test_name(tcase_name());

  ck_assert_float_infinite(x);
  x = -1.0f / (1.0f - t);
  ck_assert_float_infinite(x);
  x = 0.0f;
  record_failure_line_num(__LINE__);
  ck_assert_float_infinite(x);
}
END_TEST

START_TEST(test_ck_assert_float_infinite_with_mod)
{
  int d = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_infinite(2%d);
}
END_TEST

START_TEST(test_ck_assert_float_nan)
{
  float t = 1.0f;
  float x = 0.0f / (1.0f - t);

  record_test_name(tcase_name());

  ck_assert_float_nan(x);
  x = 1.0f / (1.0f - t);
  record_failure_line_num(__LINE__);
  ck_assert_float_nan(x);
}
END_TEST

START_TEST(test_ck_assert_float_nan_with_mod)
{
  int d = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_float_nan(2%d);
}
END_TEST

START_TEST(test_ck_assert_float_nonnan)
{
  float x = 0.0f;
  float t = 1.0f;

  record_test_name(tcase_name());

  ck_assert_float_nonnan(x);
#if ENABLE_REGEX
  x = 0.0f / (1.0f - t);
  record_failure_line_num(__LINE__);
  ck_assert_float_nonnan(x);
#else
  (void)t; /* unused */
#endif
}
END_TEST

START_TEST(test_ck_assert_float_nonnan_with_mod)
{
  int s = 2;
  float t = 1.0f;
  float x;

  record_test_name(tcase_name());

  ck_assert_float_nonnan(2%s);
#if ENABLE_REGEX
  x = 0.0f / (1.0f - t);
  record_failure_line_num(__LINE__);
  ck_assert_float_nonnan((2%s)*x);
#else
  (void)x; /* unused */
  (void)t; /* unused */
#endif
}
END_TEST

START_TEST(test_ck_assert_float_nan_and_inf_with_expr)
{
  float x[] = {0.0f, 0.0f, INFINITY, INFINITY, NAN, NAN, 0.0f, 0.0f, NAN};
  int i = 0;

  record_test_name(tcase_name());

  ck_assert_float_finite(x[i++]);
  ck_assert_float_finite(x[i++]);
  ck_assert_float_infinite(x[i++]);
  ck_assert_float_infinite(x[i++]);
  ck_assert_float_nan(x[i++]);
  ck_assert_float_nan(x[i++]);
  ck_assert_float_nonnan(x[i++]);
  ck_assert_float_nonnan(x[i++]);
}
END_TEST

START_TEST(test_ck_assert_double_eq)
{
  double x = 1.1;
  double y = 1.1;

  record_test_name(tcase_name());

  ck_assert_double_eq(x, y);
  y+=0.1;
  record_failure_line_num(__LINE__);
  ck_assert_double_eq(x, y);
}
END_TEST

START_TEST(test_ck_assert_double_eq_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_eq(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_double_eq_with_promotion)
{
  float x = 0.1F;
  double y = x;

  record_test_name(tcase_name());

  ck_assert_double_eq(x, y);
}
END_TEST

START_TEST(test_ck_assert_double_eq_with_conv)
{
  float x = 0.1F;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_eq(x, 0.1);
}
END_TEST

START_TEST(test_ck_assert_double_ne)
{
  double x = 1.1;
  double y = 1.2;

  record_test_name(tcase_name());

  ck_assert_double_ne(x, y);
  y = x;
  record_failure_line_num(__LINE__);
  ck_assert_double_ne(x, y);
}
END_TEST

START_TEST(test_ck_assert_double_ne_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_ne(1%d, 1%f);
}
END_TEST

START_TEST(test_ck_assert_double_lt)
{
  double x = 2.0;
  double y = 2.5;

  record_test_name(tcase_name());

  ck_assert_double_lt(x, y);
  y-=1;
  record_failure_line_num(__LINE__);
  ck_assert_double_lt(x, y);
}
END_TEST

START_TEST(test_ck_assert_double_lt_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_lt(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_double_le)
{
  double x = 2.0;
  double y = 2.5;

  record_test_name(tcase_name());

  ck_assert_double_le(x, y);
  ck_assert_double_le(x, x);
  y-=1;
  record_failure_line_num(__LINE__);
  ck_assert_double_le(x, y);
}
END_TEST

START_TEST(test_ck_assert_double_le_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_le(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_double_gt)
{
  double x = 2.5;
  double y = 2.0;

  record_test_name(tcase_name());

  ck_assert_double_gt(x, y);
  y+=1;
  record_failure_line_num(__LINE__);
  ck_assert_double_gt(x, y);
}
END_TEST

START_TEST(test_ck_assert_double_gt_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_gt(2%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_double_ge)
{
  double x = 2.5;
  double y = 2.0;

  record_test_name(tcase_name());

  ck_assert_double_ge(x, y);
  ck_assert_double_ge(x, x);
  y+=1;
  record_failure_line_num(__LINE__);
  ck_assert_double_ge(x, y);
}
END_TEST

START_TEST(test_ck_assert_double_ge_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_ge(2%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_double_with_expr)
{
  double x[] = {NAN, 1.1, 1.1, 1.2, 1.2, NAN};
  double y = 1.1;
  int i;

  record_test_name(tcase_name());

  i = 1;
  ck_assert_double_eq(x[i++], y);
  ck_assert_double_eq(x[i++], y);

  i = 4;
  ck_assert_double_ne(x[i--], y);
  ck_assert_double_ne(x[i--], y);

  y = 1.15;

  i = 1;
  ck_assert_double_le(x[i++], y);
  ck_assert_double_le(x[i++], y);

  i = 1;
  ck_assert_double_lt(x[i++], y);
  ck_assert_double_lt(x[i++], y);

  i = 4;
  ck_assert_double_gt(x[i--], y);
  ck_assert_double_gt(x[i--], y);

  i = 4;
  ck_assert_double_ge(x[i--], y);
  ck_assert_double_ge(x[i--], y);
}
END_TEST

START_TEST(test_ck_assert_double_eq_tol)
{
  double x = 0.0001;
  double y = 0.0002;
  double t = 0.001;

  record_test_name(tcase_name());

  ck_assert_double_eq_tol(x, y, t);
  ck_assert_double_eq_tol(x, x, t);
  x*=10;
  y*=10;
  t*=10;
  ck_assert_double_eq_tol(x, y, t);
  t/=10;
  record_failure_line_num(__LINE__);
  ck_assert_double_eq_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_double_eq_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_eq_tol(3%d, 2%f, 2%p);
}
END_TEST

START_TEST(test_ck_assert_double_ne_tol)
{
  double x = 0.0001;
  double y = 0.0002;
  double t = 0.0001;

  record_test_name(tcase_name());

  ck_assert_double_ne_tol(x, y, t);
  x*=10;
  y*=10;
  t*=10;
  ck_assert_double_ne_tol(x, y, t);
  t*=10;
  record_failure_line_num(__LINE__);
  ck_assert_double_ne_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_double_ne_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_ne_tol(3%d, 3%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_double_ge_tol)
{
  double x = 0.001;
  double y = 0.003;
  double t = 0.001;

  record_test_name(tcase_name());

  ck_assert_double_ge_tol(y, x, t);
  ck_assert_double_ge_tol(x, x, t);
  ck_assert_double_ge_tol(y, y, t);
  x*=10.0;
  y*=10.0;
  t*=10.0;
  ck_assert_double_ge_tol(y, x, t);
  ck_assert_double_ge_tol(x, x, t);
  ck_assert_double_ge_tol(y, y, t);
  record_failure_line_num(__LINE__);
  ck_assert_double_ge_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_double_ge_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_ge_tol(2%d, 3%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_double_le_tol)
{
  double x = 0.001;
  double y = 0.003;
  double t = 0.001;

  record_test_name(tcase_name());

  ck_assert_double_le_tol(x, y, t);
  ck_assert_double_le_tol(x, x, t);
  ck_assert_double_le_tol(y, y, t);
  x*=10.0;
  y*=10.0;
  t*=10.0;
  ck_assert_double_le_tol(x, y, t);
  ck_assert_double_le_tol(x, x, t);
  ck_assert_double_le_tol(y, y, t);
  record_failure_line_num(__LINE__);
  ck_assert_double_le_tol(y, x, t);
}
END_TEST

START_TEST(test_ck_assert_double_le_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_le_tol(3%d, 2%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_double_tol_with_expr)
{
  double x[] = {NAN, 1.1, 1.1, 1.2, 1.2, NAN};
  double y = 1.1;
  double t = 0.01;
  int i;

  record_test_name(tcase_name());

  i = 1;
  ck_assert_double_eq_tol(x[i++], y, t);
  ck_assert_double_eq_tol(x[i++], y, t);

  i = 4;
  ck_assert_double_ne_tol(x[i--], y, t);
  ck_assert_double_ne_tol(x[i--], y, t);

  y = 1.15;

  i = 1;
  ck_assert_double_le_tol(x[i++], y, t);
  ck_assert_double_le_tol(x[i++], y, t);

  i = 4;
  ck_assert_double_ge_tol(x[i--], y, t);
  ck_assert_double_ge_tol(x[i--], y, t);
}
END_TEST

START_TEST(test_ck_assert_double_finite)
{
  double x = 0.0001;
  double t = 1;

  record_test_name(tcase_name());

  ck_assert_double_finite(x);
  /* MS VS doesn't allow explicit division by zero */
  x = 1.0 / (1.0 - t);
  record_failure_line_num(__LINE__);
  ck_assert_double_finite(x);
}
END_TEST

START_TEST(test_ck_assert_double_finite_with_mod)
{
  int d = 2;
  double t = 1;
  double x = 1.0 / (1.0 - t);

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_finite(x*(1%d));
}
END_TEST

START_TEST(test_ck_assert_double_infinite)
{
  double t = 1;
  double x = 1.0 / (1.0 - t);

  record_test_name(tcase_name());

  ck_assert_double_infinite(x);
  x = -1.0 / (1.0 - t);
  ck_assert_double_infinite(x);
  x = 0;
  record_failure_line_num(__LINE__);
  ck_assert_double_infinite(x);
}
END_TEST

START_TEST(test_ck_assert_double_infinite_with_mod)
{
  int d = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_infinite(2%d);
}
END_TEST

START_TEST(test_ck_assert_double_nan)
{
  double t = 1;
  double x = 0.0 / (1.0 - t);

  record_test_name(tcase_name());

  ck_assert_double_nan(x);
  x = 1.0 / (1.0 - t);
  record_failure_line_num(__LINE__);
  ck_assert_double_nan(x);
}
END_TEST

START_TEST(test_ck_assert_double_nan_with_mod)
{
  int d = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_double_nan(2%d);
}
END_TEST

START_TEST(test_ck_assert_double_nonnan)
{
  double x = 0;
  double t = 1;

  record_test_name(tcase_name());

  ck_assert_double_nonnan(x);
#if ENABLE_REGEX
  x = 0.0 / (1.0 - t);
  record_failure_line_num(__LINE__);
  ck_assert_double_nonnan(x);
#else
  (void)t; /* unused */
#endif
}
END_TEST

START_TEST(test_ck_assert_double_nonnan_with_mod)
{
  int s = 2;
  double t = 1.0;
  double x;

  record_test_name(tcase_name());

  ck_assert_double_nonnan(2%s);
#if ENABLE_REGEX
  x = 0.0 / (1.0 - t);
  record_failure_line_num(__LINE__);
  ck_assert_double_nonnan((2%s)*x);
#else
  (void)t; /* unused */
  (void)x; /* unused */
#endif
}
END_TEST

START_TEST(test_ck_assert_double_nan_and_inf_with_expr)
{
  double x[] = {0.0, 0.0, INFINITY, INFINITY, NAN, NAN, 0.0, 0.0, NAN};
  int i = 0;

  record_test_name(tcase_name());

  ck_assert_double_finite(x[i++]);
  ck_assert_double_finite(x[i++]);
  ck_assert_double_infinite(x[i++]);
  ck_assert_double_infinite(x[i++]);
  ck_assert_double_nan(x[i++]);
  ck_assert_double_nan(x[i++]);
  ck_assert_double_nonnan(x[i++]);
  ck_assert_double_nonnan(x[i++]);
}
END_TEST

START_TEST(test_ck_assert_ldouble_eq)
{
  long double x = 1.1l;
  long double y = 1.1l;

  record_test_name(tcase_name());

  ck_assert_ldouble_eq(x, y);
  y+=0.1l;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_eq(x, y);
}
END_TEST

START_TEST(test_ck_assert_ldouble_eq_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_eq(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_ldouble_eq_with_promotion)
{
  float x = 1.1F;
  long double y = x;

  record_test_name(tcase_name());

  ck_assert_ldouble_eq(x, y);
}
END_TEST

START_TEST(test_ck_assert_ldouble_eq_with_conv)
{
  float x = 1.1F;
  long double y = x;

  record_test_name(tcase_name());

  ck_assert_ldouble_eq(x, y);
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_eq(x, 1.1);
}
END_TEST

START_TEST(test_ck_assert_ldouble_ne)
{
  long double x = 1.1l;
  long double y = 1.2l;

  record_test_name(tcase_name());

  ck_assert_ldouble_ne(x, y);
  y = x;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_ne(x, y);
}
END_TEST

START_TEST(test_ck_assert_ldouble_ne_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_ne(1%d, 1%f);
}
END_TEST

START_TEST(test_ck_assert_ldouble_lt)
{
  long double x = 2.0l;
  long double y = 2.5l;

  record_test_name(tcase_name());

  ck_assert_ldouble_lt(x, y);
  y-=1.0l;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_lt(x, y);
}
END_TEST

START_TEST(test_ck_assert_ldouble_lt_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_lt(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_ldouble_le)
{
  long double x = 2.0l;
  long double y = 2.5l;

  record_test_name(tcase_name());

  ck_assert_ldouble_le(x, y);
  ck_assert_ldouble_le(x, x);
  y-=1.0l;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_le(x, y);
}
END_TEST

START_TEST(test_ck_assert_ldouble_le_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_le(3%d, 2%f);
}
END_TEST

START_TEST(test_ck_assert_ldouble_gt)
{
  long double x = 2.5l;
  long double y = 2.0l;

  record_test_name(tcase_name());

  ck_assert_ldouble_gt(x, y);
  y+=1.0l;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_gt(x, y);
}
END_TEST

START_TEST(test_ck_assert_ldouble_gt_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_gt(2%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_ldouble_ge)
{
  long double x = 2.5l;
  long double y = 2.0l;

  record_test_name(tcase_name());

  ck_assert_ldouble_ge(x, y);
  ck_assert_ldouble_ge(x, x);
  y+=1.0l;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_ge(x, y);
}
END_TEST

START_TEST(test_ck_assert_ldouble_ge_with_mod)
{
  int d = 2;
  int f = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_ge(2%d, 3%f);
}
END_TEST

START_TEST(test_ck_assert_ldouble_with_expr)
{
  long double x[] = {NAN, 1.1l, 1.1l, 1.2l, 1.2l, NAN};
  long double y = 1.1l;
  int i;

  record_test_name(tcase_name());

  i = 1;
  ck_assert_ldouble_eq(x[i++], y);
  ck_assert_ldouble_eq(x[i++], y);

  i = 4;
  ck_assert_ldouble_ne(x[i--], y);
  ck_assert_ldouble_ne(x[i--], y);

  y = 1.15l;

  i = 1;
  ck_assert_ldouble_le(x[i++], y);
  ck_assert_ldouble_le(x[i++], y);

  i = 1;
  ck_assert_ldouble_lt(x[i++], y);
  ck_assert_ldouble_lt(x[i++], y);

  i = 4;
  ck_assert_ldouble_gt(x[i--], y);
  ck_assert_ldouble_gt(x[i--], y);

  i = 4;
  ck_assert_ldouble_ge(x[i--], y);
  ck_assert_ldouble_ge(x[i--], y);
}
END_TEST

START_TEST(test_ck_assert_ldouble_eq_tol)
{
  long double x = 0.0001l;
  long double y = 0.0002l;
  long double t = 0.001l;

  record_test_name(tcase_name());

  ck_assert_ldouble_eq_tol(x, y, t);
  ck_assert_ldouble_eq_tol(x, x, t);
  x*=10.0l;
  y*=10.0l;
  t*=10.0l;
  ck_assert_ldouble_eq_tol(x, y, t);
  t/=10.0l;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_eq_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_ldouble_eq_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_eq_tol(3%d, 2%f, 2%p);
}
END_TEST

START_TEST(test_ck_assert_ldouble_ne_tol)
{
  long double x = 0.0001l;
  long double y = 0.0002l;
  long double t = 0.0001l;

  record_test_name(tcase_name());

  ck_assert_ldouble_ne_tol(x, y, t);
  x*=10.0l;
  y*=10.0l;
  t*=10.0l;
  ck_assert_ldouble_ne_tol(x, y, t);
  t*=10.0l;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_ne_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_ldouble_ne_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_ne_tol(3%d, 3%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_ldouble_ge_tol)
{
  long double x = 0.001l;
  long double y = 0.003l;
  long double t = 0.001l;

  record_test_name(tcase_name());

  ck_assert_ldouble_ge_tol(y, x, t);
  ck_assert_ldouble_ge_tol(x, x, t);
  ck_assert_ldouble_ge_tol(y, y, t);
  x*=10.0l;
  y*=10.0l;
  t*=10.0l;
  ck_assert_ldouble_ge_tol(y, x, t);
  ck_assert_ldouble_ge_tol(x, x, t);
  ck_assert_ldouble_ge_tol(y, y, t);
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_ge_tol(x, y, t);
}
END_TEST

START_TEST(test_ck_assert_ldouble_ge_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_ge_tol(2%d, 3%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_ldouble_le_tol)
{
  long double x = 0.001l;
  long double y = 0.003l;
  long double t = 0.001l;

  record_test_name(tcase_name());

  ck_assert_ldouble_le_tol(x, y, t);
  ck_assert_ldouble_le_tol(x, x, t);
  ck_assert_ldouble_le_tol(y, y, t);
  x*=10.0l;
  y*=10.0l;
  t*=10.0l;
  ck_assert_ldouble_le_tol(x, y, t);
  ck_assert_ldouble_le_tol(x, x, t);
  ck_assert_ldouble_le_tol(y, y, t);
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_le_tol(y, x, t);
}
END_TEST

START_TEST(test_ck_assert_ldouble_le_tol_with_mod)
{
  int d = 2;
  int f = 2;
  int p = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_le_tol(3%d, 2%f, 3%p);
}
END_TEST

START_TEST(test_ck_assert_ldouble_tol_with_expr)
{
  long double x[] = {NAN, 1.1l, 1.1l, 1.2l, 1.2l, NAN};
  long double y = 1.1l;
  long double t = 0.01l;
  int i;

  record_test_name(tcase_name());

  i = 1;
  ck_assert_ldouble_eq_tol(x[i++], y, t);
  ck_assert_ldouble_eq_tol(x[i++], y, t);

  i = 4;
  ck_assert_ldouble_ne_tol(x[i--], y, t);
  ck_assert_ldouble_ne_tol(x[i--], y, t);

  y = 1.15l;

  i = 1;
  ck_assert_ldouble_le_tol(x[i++], y, t);
  ck_assert_ldouble_le_tol(x[i++], y, t);

  i = 4;
  ck_assert_ldouble_ge_tol(x[i--], y, t);
  ck_assert_ldouble_ge_tol(x[i--], y, t);
}
END_TEST

START_TEST(test_ck_assert_ldouble_finite)
{
  long double x = 0.0001l;
  long double t = 1.0l;

  record_test_name(tcase_name());

  ck_assert_ldouble_finite(x);
  /* MS VS doesn't allow explicit division by zero */
  x = 1.0l / (1.0l - t);
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_finite(x);
}
END_TEST

START_TEST(test_ck_assert_ldouble_finite_with_mod)
{
  int d = 2;
  long double t = 1.0l;
  long double x = 1.0l / (1.0l - t);

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_finite(x*(1%d));
}
END_TEST

START_TEST(test_ck_assert_ldouble_infinite)
{
  long double t = 1.0l;
  long double x = 1.0l / (1.0l - t);

  record_test_name(tcase_name());

  ck_assert_ldouble_infinite(x);
  x = -1.0l / (1.0l - t);
  ck_assert_ldouble_infinite(x);
  x = 0.0l;
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_infinite(x);
}
END_TEST

START_TEST(test_ck_assert_ldouble_infinite_with_mod)
{
  int d = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_infinite(2%d);
}
END_TEST

START_TEST(test_ck_assert_ldouble_nan)
{
  long double t = 1.0l;
  long double x = 0.0l / (1.0l - t);

  record_test_name(tcase_name());

  ck_assert_ldouble_nan(x);
  x = 1.0l / (1.0l - t);
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_nan(x);
}
END_TEST

START_TEST(test_ck_assert_ldouble_nan_with_mod)
{
  int d = 2;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_ldouble_nan(2%d);
}
END_TEST

START_TEST(test_ck_assert_ldouble_nonnan)
{
  long double x = 0.0l;
  long double t = 1.0l;

  record_test_name(tcase_name());

  ck_assert_ldouble_nonnan(x);
#if ENABLE_REGEX
  x = 0.0l / (1.0l - t);
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_nonnan(x);
#else
  (void)t; /* unused */
#endif
}
END_TEST

START_TEST(test_ck_assert_ldouble_nonnan_with_mod)
{
  int s = 2;
  long double t = 1.0l;
  long double x;

  record_test_name(tcase_name());

  ck_assert_ldouble_nonnan(2%s);
#if ENABLE_REGEX
  x = 0.0l / (1.0l - t);
  record_failure_line_num(__LINE__);
  ck_assert_ldouble_nonnan((2%s)*x);
#else
  (void)t; /* unused */
  (void)x; /* unused */
#endif
}
END_TEST

START_TEST(test_ck_assert_ldouble_nan_and_inf_with_expr)
{
  long double x[] = {0.0l, 0.0l, INFINITY, INFINITY, NAN, NAN, 0.0l, 0.0l, NAN};
  int i = 0;

  record_test_name(tcase_name());

  ck_assert_ldouble_finite(x[i++]);
  ck_assert_ldouble_finite(x[i++]);
  ck_assert_ldouble_infinite(x[i++]);
  ck_assert_ldouble_infinite(x[i++]);
  ck_assert_ldouble_nan(x[i++]);
  ck_assert_ldouble_nan(x[i++]);
  ck_assert_ldouble_nonnan(x[i++]);
  ck_assert_ldouble_nonnan(x[i++]);
}
END_TEST

int returnsZero(const char* argument);
int returnsZero(const char* argument)
{
    (void)argument;
    return 0;
}

START_TEST(test_percent_n_escaped)
{
  record_test_name(tcase_name());

  /* If the %n is not escaped in the ck macro, then this results in a SEGFAULT */
  record_failure_line_num(__LINE__);
  ck_assert_int_eq(returnsZero("%n"), 1);
} END_TEST

START_TEST(test_ck_assert_str_eq)
{
  const char *s = "test2";

  record_test_name(tcase_name());

  ck_assert_str_eq("test2", s);
  record_failure_line_num(__LINE__);
  ck_assert_str_eq("test1", s);
}
END_TEST

START_TEST(test_ck_assert_str_eq_with_null)
{
  const char *s = NULL;
  const char *t = NULL;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_str_eq(t, s);
}
END_TEST

START_TEST(test_ck_assert_str_ne)
{
  const char *s = "test2";
  const char *t = "test1";

  record_test_name(tcase_name());

  ck_assert_str_ne(t, s);
  t = "test2";
  record_failure_line_num(__LINE__);
  ck_assert_str_ne(t, s);
}
END_TEST

START_TEST(test_ck_assert_str_ne_with_null)
{
  const char *s = NULL;
  const char *t = "test";

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_str_ne(t, s);
}
END_TEST

START_TEST(test_ck_assert_str_lt)
{
  const char *s = "test1";
  const char *t = "test2";

  record_test_name(tcase_name());

  ck_assert_str_lt(s, t);
  record_failure_line_num(__LINE__);
  ck_assert_str_lt(s, s);
}
END_TEST

START_TEST(test_ck_assert_str_lt_with_null)
{
  const char *s = NULL;
  const char *t = "test";

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_str_lt(s, t);
}
END_TEST

START_TEST(test_ck_assert_str_le)
{
  const char *s = "test1";
  const char *t = "test2";

  record_test_name(tcase_name());

  ck_assert_str_le(s, t);
  ck_assert_str_le(s, s);
  record_failure_line_num(__LINE__);
  ck_assert_str_le(t, s);
}
END_TEST

START_TEST(test_ck_assert_str_le_with_null)
{
  const char *s = NULL;
  const char *t = NULL;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_str_le(t, s);
}
END_TEST

START_TEST(test_ck_assert_str_gt)
{
  const char *s = "test1";
  const char *t = "test2";

  record_test_name(tcase_name());

  ck_assert_str_gt(t, s);
  record_failure_line_num(__LINE__);
  ck_assert_str_gt(t, t);
}
END_TEST

START_TEST(test_ck_assert_str_gt_with_null)
{
  const char *s = NULL;
  const char *t = "test";

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_str_gt(t, s);
}
END_TEST

START_TEST(test_ck_assert_str_ge)
{
  const char *s = "test1";
  const char *t = "test2";

  record_test_name(tcase_name());

  ck_assert_str_ge(t, s);
  ck_assert_str_ge(t, t);
  record_failure_line_num(__LINE__);
  ck_assert_str_ge(s, t);
}
END_TEST

START_TEST(test_ck_assert_str_ge_with_null)
{
  const char *s = NULL;
  const char *t = NULL;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_str_ge(s, t);
}
END_TEST

START_TEST(test_ck_assert_str_expr)
{
  const char *s = "test1";
  const char *t[] = { "test1", "test2" };
  int i = -1;

  record_test_name(tcase_name());

  ck_assert_str_eq(s, t[++i]);
  ck_assert_str_eq(s, t[i]);
}
END_TEST

START_TEST(test_ck_assert_pstr_eq)
{
  const char *s = "test";

  record_test_name(tcase_name());

  ck_assert_pstr_eq("test", s);
  ck_assert_pstr_eq(NULL, NULL);
  record_failure_line_num(__LINE__);
  ck_assert_pstr_eq("test1", s);
}
END_TEST

START_TEST(test_ck_assert_pstr_eq_with_null)
{
  const char *t = "test";
  const char *s = NULL;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_pstr_eq(t, s);
}
END_TEST

START_TEST(test_ck_assert_pstr_ne)
{
  const char *t = "test1";
  const char *s = "test2";

  record_test_name(tcase_name());

  ck_assert_pstr_ne(t, s);
  ck_assert_pstr_ne(t, NULL);
  t = "test2";
  record_failure_line_num(__LINE__);
  ck_assert_pstr_ne(t, s);
}
END_TEST

START_TEST(test_ck_assert_pstr_ne_with_null)
{
  const char *s = NULL;
  const char *t = NULL;

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_pstr_ne(t, s);
}
END_TEST

START_TEST(test_ck_assert_ptr_eq)
{
  int * x = (int*)0x1;
  int * y = (int*)0x2;

  record_test_name(tcase_name());

  ck_assert_ptr_eq(NULL, NULL);
  ck_assert_ptr_eq(x,    x);
  record_failure_line_num(__LINE__);
  ck_assert_ptr_eq(x,    y);
}
END_TEST

START_TEST(test_ck_assert_ptr_ne)
{
  int * x = (int*)0x1;
  int * y = (int*)0x2;
  int * z = x;

  record_test_name(tcase_name());

  ck_assert_ptr_ne(x,    y);
  ck_assert_ptr_ne(x,    NULL);
  ck_assert_ptr_ne(NULL, y);
  record_failure_line_num(__LINE__);
  ck_assert_ptr_ne(x,    z);
}
END_TEST

START_TEST(test_ck_assert_ptr_null)
{
  void* x = (void*)0x1;
  void* y = NULL;

  record_test_name(tcase_name());

  ck_assert_ptr_null(y);
  record_failure_line_num(__LINE__);
  ck_assert_ptr_null(x);
}
END_TEST

START_TEST(test_ck_assert_ptr_nonnull)
{
  void* x = NULL;
  void* y = (void*)0x1;

  record_test_name(tcase_name());

  ck_assert_ptr_nonnull(y);
  record_failure_line_num(__LINE__);
  ck_assert_ptr_nonnull(x);
}
END_TEST

START_TEST(test_ck_assert_mem_eq)
{
  const char *s = "\x00\x00\x00\x00\x02";

  record_test_name(tcase_name());

  ck_assert_mem_eq("\x00\x00\x00\x00\x02", s, 5);
  record_failure_line_num(__LINE__);
  ck_assert_mem_eq("\x00\x00\x00\x00\x01", s, 5);
}
END_TEST

START_TEST(test_ck_assert_mem_ne)
{
  const char *s = "\x00\x00\x00\x00\x02";
  const char *t = "\x00\x00\x00\x00\x01";

  record_test_name(tcase_name());

  ck_assert_mem_ne(t, s, 5);
  t = "\x00\x00\x00\x00\x02";
  record_failure_line_num(__LINE__);
  ck_assert_mem_ne(t, s, 5);
}
END_TEST

START_TEST(test_ck_assert_mem_lt)
{
  const char *s = "\x00\x00\x00\x00\x01";
  const char *t = "\x00\x00\x00\x00\x02";

  record_test_name(tcase_name());

  ck_assert_mem_lt(s, t, 5);
  record_failure_line_num(__LINE__);
  ck_assert_mem_lt(s, s, 5);
}
END_TEST

START_TEST(test_ck_assert_mem_le)
{
  const char *s = "\x00\x00\x00\x00\x01";
  const char *t = "\x00\x00\x00\x00\x02";

  record_test_name(tcase_name());

  ck_assert_mem_le(s, t, 5);
  ck_assert_mem_le(s, s, 5);
  record_failure_line_num(__LINE__);
  ck_assert_mem_le(t, s, 5);
}
END_TEST

START_TEST(test_ck_assert_mem_gt)
{
  const char *s = "\x00\x00\x00\x00\x01";
  const char *t = "\x00\x00\x00\x00\x02";

  record_test_name(tcase_name());

  ck_assert_mem_gt(t, s, 5);
  record_failure_line_num(__LINE__);
  ck_assert_mem_gt(t, t, 5);
}
END_TEST

START_TEST(test_ck_assert_mem_ge)
{
  const char *s = "\x00\x00\x00\x00\x01";
  const char *t = "\x00\x00\x00\x00\x02";

  record_test_name(tcase_name());

  ck_assert_mem_ge(t, s, 5);
  ck_assert_mem_ge(t, t, 5);
  record_failure_line_num(__LINE__);
  ck_assert_mem_ge(s, t, 5);
}
END_TEST

START_TEST(test_ck_assert_mem_zerolen)
{
  const char *s = "\x00\x00\x00\x00\x02";
  const char *t = "\x00\x00\x00\x00\x01";

  record_test_name(tcase_name());

  ck_assert_mem_eq(t, s, 0);
}
END_TEST

START_TEST(test_ck_assert_mem_eq_exact)
{
  const char *s = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02";
  const char *t = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01";

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_mem_eq(t, s, 64);
}
END_TEST

START_TEST(test_ck_assert_mem_eq_longer)
{
  const char *s = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02";
  const char *t = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01";

  record_test_name(tcase_name());

  record_failure_line_num(__LINE__);
  ck_assert_mem_eq(t, s, 65);
}
END_TEST

#if defined(HAVE_FORK) && HAVE_FORK == 1
START_TEST(test_segv_pass)
{
  record_test_name(tcase_name());
  /*
   * This test is to be used when it would otherwise not cause a
   * failure. e.g., shen SIGSEGV is expected.
   */
  raise (SIGSEGV);
}
END_TEST

START_TEST(test_segv)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  raise (SIGSEGV);
}
END_TEST

/* This test currently does not work on Cygwin, as it results in a
 * SIGSEGV instead of a SIGFPE. However, a simple program that installs
 * a SIGFPE handler then raise(SIGFPE) works as expected. Further
 * investigation is necessary. */
#if !defined(__CYGWIN__)
START_TEST(test_fpe)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  raise (SIGFPE);
}
END_TEST
#endif /* !defined(__CYGWIN__) */

/*
 * This test is to be used when the test is expected to throw signal 8,
 * but does not, resulting in a failure.
 */
START_TEST(test_non_signal_8)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  exit(0);
}
END_TEST

/* TODO:
   unit test running the same suite in succession */

/* This test currently does not work on Cygwin, as it results in a
 * SIGSEGV instead of a SIGFPE. However, a simple program that installs
 * a SIGFPE handler then raise(SIGFPE) works as expected. Further
 * investigation is necessary. */
#if !defined(__CYGWIN__)
START_TEST(test_mark_point)
{
  int i;
  record_test_name(tcase_name());
  i = 0;
  i++;
  mark_point();
  record_failure_line_num(__LINE__-2); /* -2 as the failure is listed as from mark_point() */
  raise(SIGFPE);
  ck_abort_msg("Shouldn't reach here");
}
END_TEST
#endif /* !defined(__CYGWIN__) */

#endif /* HAVE_FORK */

#if TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) && HAVE_FORK == 1
START_TEST(test_eternal_fail)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  for (;;)
    sleep(1);
}
END_TEST

/* 
 * Only include sub-second timing tests on systems
 * that support librt.
 */
#ifdef HAVE_LIBRT
START_TEST(test_sleep0_025_pass)
{
  record_test_name(tcase_name());
  usleep(25*1000);
}
END_TEST

START_TEST(test_sleep1_pass)
{
  record_test_name(tcase_name());
  sleep(1);
}
END_TEST

START_TEST(test_sleep1_fail)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  sleep(1);
}
END_TEST
#endif /* HAVE_LIBRT */

START_TEST(test_sleep2_pass)
{
  record_test_name(tcase_name());
  sleep(2);
}
END_TEST

START_TEST(test_sleep2_fail)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  sleep(2);
}
END_TEST

START_TEST(test_sleep5_pass)
{
  record_test_name(tcase_name());
  sleep(5);
}
END_TEST

START_TEST(test_sleep5_fail)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  sleep(5);
}
END_TEST

START_TEST(test_sleep9_pass)
{
  record_test_name(tcase_name());
  sleep(9);
}
END_TEST

START_TEST(test_sleep9_fail)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  sleep(9);
}
END_TEST

START_TEST(test_sleep14_fail)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  sleep(14);
  exit(3);
}
END_TEST
#endif /* TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) */

#if defined(HAVE_FORK) && HAVE_FORK==1
START_TEST(test_early_exit)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__-4); /* -4 as the failure is reported at START_TEST() */
  exit(EXIT_FAILURE);
}
END_TEST
#endif /* HAVE_FORK */

/*
 * The following test will leak memory because it is calling
 * APIs inproperly. The leaked memory cannot be free'd, as
 * the methods to do so are static. (No user of Check should
 * call them directly).
 */
#if MEMORY_LEAKING_TESTS_ENABLED
START_TEST(test_null)
{  
  Suite *s;
  TCase *tc;
  
  record_test_name(tcase_name());

  s = suite_create(NULL);
  tc = tcase_create(NULL);
  suite_add_tcase (s, NULL);
  tcase_add_test (tc, NULL);
  srunner_free(srunner_create(NULL));
  srunner_run_all (NULL, (enum print_output)-1);
  srunner_free (NULL);
  record_failure_line_num(__LINE__);
  ck_abort_msg("Completed properly");
}
END_TEST
#endif /* MEMORY_LEAKING_TESTS_ENABLED */

START_TEST(test_null_2)
{
  SRunner *sr = srunner_create(NULL);

  record_test_name(tcase_name());

  srunner_run_all (sr, CK_NORMAL);
  srunner_free (sr);
  ck_assert_int_eq(suite_tcase(NULL, NULL), 0);
  record_failure_line_num(__LINE__);
  ck_abort_msg("Completed properly");
}
END_TEST

#if defined(HAVE_FORK) && HAVE_FORK==1
START_TEST(test_fork1p_pass)
{
  pid_t pid;

  record_test_name(tcase_name());

  if((pid = fork()) < 0) {
    ck_abort_msg("Failed to fork new process");
  } else if (pid > 0) {
    ck_assert_msg(1, NULL);
    kill(pid, SIGKILL);
  } else {
    for (;;) {
      sleep(1);
    }
  }
}
END_TEST

START_TEST(test_fork1p_fail)
{
  pid_t pid;
  
  record_test_name(tcase_name());

  if((pid = fork()) < 0) {
    ck_abort_msg("Failed to fork new process");
  } else if (pid > 0) {
    record_failure_line_num(__LINE__);
    ck_abort_msg("Expected fail");
    kill(pid, SIGKILL);
  } else {
    for (;;) {
      sleep(1);
    }
  }
}
END_TEST

START_TEST(test_fork1c_pass)
{
  pid_t pid;

  record_test_name(tcase_name());
  
  if((pid = check_fork()) < 0) {
    ck_abort_msg("Failed to fork new process");
  } else if (pid > 0) {
    check_waitpid_and_exit(pid);
  } else {
    ck_assert_msg(1, NULL);
    check_waitpid_and_exit(0);
  }
}
END_TEST

START_TEST(test_fork1c_fail)
{
  pid_t pid;

  record_test_name(tcase_name());
  
  if((pid = check_fork()) < 0) {
    ck_abort_msg("Failed to fork new process");
  } else if (pid == 0) {
    record_failure_line_num(__LINE__);
    ck_abort_msg("Expected fail");
    check_waitpid_and_exit(0);
  }
  check_waitpid_and_exit(pid);
}
END_TEST

START_TEST(test_fork2_pass)
{
  pid_t pid;
  pid_t pid2;
  
  record_test_name(tcase_name());

  if((pid = check_fork()) < 0) {
    ck_abort_msg("Failed to fork new process");
  } else if (pid > 0) {
    if((pid2 = check_fork()) < 0) {
      ck_abort_msg("Failed to fork new process");
    } else if (pid2 == 0) {
      ck_assert_msg(1, NULL);
      check_waitpid_and_exit(0);
    }
    check_waitpid_and_exit(pid2);
  }
  check_waitpid_and_exit(pid);
}
END_TEST

START_TEST(test_fork2_fail)
{
  pid_t pid;
  pid_t pid2;
  
  record_test_name(tcase_name());

  if((pid = check_fork()) < 0) {
    ck_abort_msg("Failed to fork new process");
  } else if (pid > 0) {
    if((pid2 = check_fork()) < 0) {
      ck_abort_msg("Failed to fork new process");
    } else if (pid2 == 0) {
      record_failure_line_num(__LINE__);
      ck_abort_msg("Expected fail");
      check_waitpid_and_exit(0);
    }
    check_waitpid_and_exit(pid2);
    ck_abort_msg("Expected fail");
  }
  check_waitpid_and_exit(pid);
}
END_TEST
#endif /* HAVE_FORK */

#if defined(HAVE_FORK) && HAVE_FORK == 1
#if MEMORY_LEAKING_TESTS_ENABLED
START_TEST(test_invalid_set_fork_status)
{
   Suite *s1;
   TCase *tc1;
   SRunner *sr;

   record_test_name(tcase_name());

   record_failure_line_num(__LINE__-9); /* -9 as the failure is reported at START_TEST() */
   s1 = suite_create ("suite1");
   tc1 = tcase_create ("tcase1");
   tcase_add_test (tc1, test_pass);
   sr = srunner_create(s1);
   srunner_set_fork_status (sr, (enum fork_status)-1);
   srunner_run_all(sr, CK_SILENT);
}
END_TEST
#endif /* MEMORY_LEAKING_TESTS_ENABLED */
#endif /* HAVE_FORK */

START_TEST(test_srunner)
{
  Suite *s;
  SRunner *sr;

  record_test_name(tcase_name());

  s = suite_create("Check Servant3");
  ck_assert_msg(s != NULL, NULL);
  sr = srunner_create(NULL);
  ck_assert_msg(sr != NULL, NULL);
  srunner_add_suite(sr, s);
  srunner_free(sr);

  sr = srunner_create(NULL);
  ck_assert_msg(sr != NULL, NULL);
  srunner_add_suite(sr, NULL);
  srunner_free(sr);

  s = suite_create("Check Servant3");
  ck_assert_msg(s != NULL, NULL);
  sr = srunner_create(s);
  ck_assert_msg(sr != NULL, NULL);
  srunner_free(sr);
}
END_TEST

START_TEST(test_2nd_suite)
{
  record_test_name(tcase_name());
  record_failure_line_num(__LINE__);
  ck_abort_msg("We failed");
}
END_TEST

Suite *make_sub2_suite(void)
{
  Suite *s = suite_create("Check Servant2");
  TCase *tc = tcase_create("Core");
  suite_add_tcase(s, tc);
  tcase_add_test(tc, test_srunner);
  tcase_add_test(tc, test_2nd_suite);

  return s;
}

#if defined(HAVE_FORK) && HAVE_FORK == 1
void exit_handler(void);
void exit_handler ()
{
  /* This exit handler should never be executed */
  while(1)
  {
    sleep(1);
  }
}

START_TEST(test_ignore_exit_handlers)
{
  int result;

  record_test_name(tcase_name());

  result = atexit(exit_handler);
  if(result != 0)
  {
    ck_abort_msg("Failed to set an exit handler, test cannot proceed");
  }
  record_failure_line_num(__LINE__);
  ck_abort();
}
END_TEST
#endif /* HAVE_FORK */

Suite *make_sub_suite(void)
{
  Suite *s;

  TCase *tc_simple;
#if defined(HAVE_FORK) && HAVE_FORK==1
  TCase *tc_signal;
#endif
#if TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) && HAVE_FORK == 1
#if HAVE_DECL_SETENV
  TCase *tc_timeout_env_int;
  TCase *tc_timeout_env_double;
#endif /* HAVE_DECL_SETENV */
  TCase *tc_timeout_default;
  TCase *tc_timeout_usr_int;
  TCase *tc_timeout_usr_double;
#if HAVE_DECL_SETENV
  TCase *tc_timeout_env_scale_int;
  TCase *tc_timeout_scale_int;
  TCase *tc_timeout_usr_scale_int;
  TCase *tc_timeout_env_scale_double;
  TCase *tc_timeout_scale_double;
  TCase *tc_timeout_usr_scale_double;
#endif /* HAVE_DECL_SETENV */
#endif /* TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) */
  TCase *tc_limit;
#if defined(HAVE_FORK) && HAVE_FORK==1
  TCase *tc_messaging_and_fork;
  TCase *tc_errors;
  TCase *tc_exit_handlers;
#endif

  s = suite_create("Check Servant");

  tc_simple = tcase_create("Simple Tests");
#if defined(HAVE_FORK) && HAVE_FORK==1
  tc_signal = tcase_create("Signal Tests");
#endif /* HAVE_FORK */
#if TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) && HAVE_FORK == 1
#if HAVE_DECL_SETENV
  setenv("CK_DEFAULT_TIMEOUT", "6", 1);
  tc_timeout_env_int = tcase_create("Environment Integer Timeout Tests");
  unsetenv("CK_DEFAULT_TIMEOUT");
  setenv("CK_DEFAULT_TIMEOUT", "0.5", 1);
  tc_timeout_env_double = tcase_create("Environment Double Timeout Tests");
  unsetenv("CK_DEFAULT_TIMEOUT");
#endif /* HAVE_DECL_SETENV */
  tc_timeout_default = tcase_create("Default Timeout Tests");
  tc_timeout_usr_int = tcase_create("User Integer Timeout Tests");
  tc_timeout_usr_double = tcase_create("User Double Timeout Tests");
#if HAVE_DECL_SETENV
  setenv("CK_TIMEOUT_MULTIPLIER", "2", 1);
  tc_timeout_scale_int = tcase_create("Timeout Integer Scaling Tests");
  tc_timeout_usr_scale_int = tcase_create("User Integer Timeout Scaling Tests");
  setenv("CK_DEFAULT_TIMEOUT", "6", 1);
  tc_timeout_env_scale_int = tcase_create("Environment Integer Timeout Scaling Tests");
  unsetenv("CK_DEFAULT_TIMEOUT");
  unsetenv("CK_TIMEOUT_MULTIPLIER");
  
  setenv("CK_TIMEOUT_MULTIPLIER", "0.35", 1);
  tc_timeout_scale_double = tcase_create("Timeout Double Scaling Tests");
  tc_timeout_usr_scale_double = tcase_create("User Double Timeout Scaling Tests");
  setenv("CK_DEFAULT_TIMEOUT", "0.9", 1);
  tc_timeout_env_scale_double = tcase_create("Environment Double Timeout Scaling Tests");
  unsetenv("CK_DEFAULT_TIMEOUT");
  unsetenv("CK_TIMEOUT_MULTIPLIER");
#endif /* HAVE_DECL_SETENV */
#endif /* TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) */
  tc_limit = tcase_create("Limit Tests");
#if defined(HAVE_FORK) && HAVE_FORK==1
  tc_messaging_and_fork = tcase_create("Msg and fork Tests");
  tc_errors = tcase_create("Check Errors Tests");
  tc_exit_handlers = tcase_create("Check Ignore Exit Handlers");
#endif /* HAVE_FORK */

  suite_add_tcase (s, tc_simple);
#if defined(HAVE_FORK) && HAVE_FORK==1
  suite_add_tcase (s, tc_signal);
#endif /* HAVE_FORK */
#if TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) && HAVE_FORK == 1
#if HAVE_DECL_SETENV
  suite_add_tcase (s, tc_timeout_env_int);
  suite_add_tcase (s, tc_timeout_env_double);
#endif /* HAVE_DECL_SETENV */
  suite_add_tcase (s, tc_timeout_default);
  suite_add_tcase (s, tc_timeout_usr_int);
  suite_add_tcase (s, tc_timeout_usr_double);

#if HAVE_DECL_SETENV
  suite_add_tcase (s, tc_timeout_env_scale_int);
  suite_add_tcase (s, tc_timeout_env_scale_double);
  suite_add_tcase (s, tc_timeout_scale_int);
  suite_add_tcase (s, tc_timeout_scale_double);
  suite_add_tcase (s, tc_timeout_usr_scale_int);
  suite_add_tcase (s, tc_timeout_usr_scale_double);
#endif /* HAVE_DECL_SETENV */
#endif /* TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) */
  suite_add_tcase (s, tc_limit);
#if defined(HAVE_FORK) && HAVE_FORK == 1
  suite_add_tcase (s, tc_messaging_and_fork);
  suite_add_tcase (s, tc_errors);
  suite_add_tcase (s, tc_exit_handlers);
#endif

  tcase_add_test (tc_simple, test_lno);
#if defined(HAVE_FORK) && HAVE_FORK==1
  tcase_add_test (tc_simple, test_mark_lno);
#endif
  tcase_add_test (tc_simple, test_pass);
  tcase_add_test (tc_simple, test_fail_unless);
  tcase_add_test (tc_simple, test_fail_if_pass);
  tcase_add_test (tc_simple, test_fail_if_fail);
  tcase_add_test (tc_simple, test_fail_null_msg);
#if defined(__GNUC__)
  tcase_add_test (tc_simple, test_fail_no_msg);
#endif /* __GNUC__ */
  tcase_add_test (tc_simple, test_fail_if_null_msg);
#if defined(__GNUC__)
  tcase_add_test (tc_simple, test_fail_if_no_msg);
#endif /* __GNUC__ */
  tcase_add_test (tc_simple, test_fail_vararg_msg_1);
  tcase_add_test (tc_simple, test_fail_vararg_msg_2);
  tcase_add_test (tc_simple, test_fail_vararg_msg_3);
#if defined(__GNUC__)
  tcase_add_test (tc_simple, test_fail_empty);
#endif /* __GNUC__ */

  tcase_add_test (tc_simple, test_ck_abort);
  tcase_add_test (tc_simple, test_ck_abort_msg);
  tcase_add_test (tc_simple, test_ck_abort_msg_null);
  tcase_add_test (tc_simple, test_ck_assert);
  tcase_add_test (tc_simple, test_ck_assert_null);
  tcase_add_test (tc_simple, test_ck_assert_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_int_eq);
  tcase_add_test (tc_simple, test_ck_assert_int_eq_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_int_ne);
  tcase_add_test (tc_simple, test_ck_assert_int_ne_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_int_lt);
  tcase_add_test (tc_simple, test_ck_assert_int_lt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_int_le);
  tcase_add_test (tc_simple, test_ck_assert_int_le_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_int_gt);
  tcase_add_test (tc_simple, test_ck_assert_int_gt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_int_ge);
  tcase_add_test (tc_simple, test_ck_assert_int_ge_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_int_expr);
  tcase_add_test (tc_simple, test_ck_assert_uint_eq);
  tcase_add_test (tc_simple, test_ck_assert_uint_eq_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_uint_ne);
  tcase_add_test (tc_simple, test_ck_assert_uint_ne_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_uint_lt);
  tcase_add_test (tc_simple, test_ck_assert_uint_lt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_uint_le);
  tcase_add_test (tc_simple, test_ck_assert_uint_le_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_uint_gt);
  tcase_add_test (tc_simple, test_ck_assert_uint_gt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_uint_ge);
  tcase_add_test (tc_simple, test_ck_assert_uint_ge_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_uint_expr);
  tcase_add_test (tc_simple, test_ck_assert_float_eq);
  tcase_add_test (tc_simple, test_ck_assert_float_eq_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_ne);
  tcase_add_test (tc_simple, test_ck_assert_float_ne_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_lt);
  tcase_add_test (tc_simple, test_ck_assert_float_lt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_le);
  tcase_add_test (tc_simple, test_ck_assert_float_le_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_gt);
  tcase_add_test (tc_simple, test_ck_assert_float_gt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_ge);
  tcase_add_test (tc_simple, test_ck_assert_float_ge_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_with_expr);
  tcase_add_test (tc_simple, test_ck_assert_float_eq_tol);
  tcase_add_test (tc_simple, test_ck_assert_float_eq_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_ne_tol);
  tcase_add_test (tc_simple, test_ck_assert_float_ne_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_ge_tol);
  tcase_add_test (tc_simple, test_ck_assert_float_ge_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_le_tol);
  tcase_add_test (tc_simple, test_ck_assert_float_le_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_tol_with_expr);
  tcase_add_test (tc_simple, test_ck_assert_float_finite);
  tcase_add_test (tc_simple, test_ck_assert_float_finite_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_infinite);
  tcase_add_test (tc_simple, test_ck_assert_float_infinite_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_nan);
  tcase_add_test (tc_simple, test_ck_assert_float_nan_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_nonnan);
  tcase_add_test (tc_simple, test_ck_assert_float_nonnan_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_float_nan_and_inf_with_expr);
  tcase_add_test (tc_simple, test_ck_assert_double_eq);
  tcase_add_test (tc_simple, test_ck_assert_double_eq_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_eq_with_promotion);
  tcase_add_test (tc_simple, test_ck_assert_double_eq_with_conv);
  tcase_add_test (tc_simple, test_ck_assert_double_ne);
  tcase_add_test (tc_simple, test_ck_assert_double_ne_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_lt);
  tcase_add_test (tc_simple, test_ck_assert_double_lt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_le);
  tcase_add_test (tc_simple, test_ck_assert_double_le_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_gt);
  tcase_add_test (tc_simple, test_ck_assert_double_gt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_ge);
  tcase_add_test (tc_simple, test_ck_assert_double_ge_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_with_expr);
  tcase_add_test (tc_simple, test_ck_assert_double_eq_tol);
  tcase_add_test (tc_simple, test_ck_assert_double_eq_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_ne_tol);
  tcase_add_test (tc_simple, test_ck_assert_double_ne_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_ge_tol);
  tcase_add_test (tc_simple, test_ck_assert_double_ge_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_le_tol);
  tcase_add_test (tc_simple, test_ck_assert_double_le_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_tol_with_expr);
  tcase_add_test (tc_simple, test_ck_assert_double_finite);
  tcase_add_test (tc_simple, test_ck_assert_double_finite_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_infinite);
  tcase_add_test (tc_simple, test_ck_assert_double_infinite_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_nan);
  tcase_add_test (tc_simple, test_ck_assert_double_nan_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_nonnan);
  tcase_add_test (tc_simple, test_ck_assert_double_nonnan_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_double_nan_and_inf_with_expr);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_eq);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_eq_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_eq_with_promotion);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_eq_with_conv);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_ne);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_ne_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_lt);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_lt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_le);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_le_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_gt);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_gt_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_ge);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_ge_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_with_expr);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_eq_tol);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_eq_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_ne_tol);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_ne_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_ge_tol);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_ge_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_le_tol);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_le_tol_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_tol_with_expr);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_finite);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_finite_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_infinite);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_infinite_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_nan);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_nan_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_nonnan);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_nonnan_with_mod);
  tcase_add_test (tc_simple, test_ck_assert_ldouble_nan_and_inf_with_expr);
  tcase_add_test (tc_simple, test_percent_n_escaped);
  tcase_add_test (tc_simple, test_ck_assert_str_eq);
  tcase_add_test (tc_simple, test_ck_assert_str_eq_with_null);
  tcase_add_test (tc_simple, test_ck_assert_str_ne);
  tcase_add_test (tc_simple, test_ck_assert_str_ne_with_null);
  tcase_add_test (tc_simple, test_ck_assert_str_lt);
  tcase_add_test (tc_simple, test_ck_assert_str_lt_with_null);
  tcase_add_test (tc_simple, test_ck_assert_str_le);
  tcase_add_test (tc_simple, test_ck_assert_str_le_with_null);
  tcase_add_test (tc_simple, test_ck_assert_str_gt);
  tcase_add_test (tc_simple, test_ck_assert_str_gt_with_null);
  tcase_add_test (tc_simple, test_ck_assert_str_ge);
  tcase_add_test (tc_simple, test_ck_assert_str_ge_with_null);
  tcase_add_test (tc_simple, test_ck_assert_str_expr);
  tcase_add_test (tc_simple, test_ck_assert_pstr_eq);
  tcase_add_test (tc_simple, test_ck_assert_pstr_eq_with_null);
  tcase_add_test (tc_simple, test_ck_assert_pstr_ne);
  tcase_add_test (tc_simple, test_ck_assert_pstr_ne_with_null);
  tcase_add_test (tc_simple, test_ck_assert_ptr_eq);
  tcase_add_test (tc_simple, test_ck_assert_ptr_ne);
  tcase_add_test (tc_simple, test_ck_assert_ptr_null);
  tcase_add_test (tc_simple, test_ck_assert_ptr_nonnull);
  tcase_add_test (tc_simple, test_ck_assert_mem_eq);
  tcase_add_test (tc_simple, test_ck_assert_mem_ne);
  tcase_add_test (tc_simple, test_ck_assert_mem_lt);
  tcase_add_test (tc_simple, test_ck_assert_mem_le);
  tcase_add_test (tc_simple, test_ck_assert_mem_gt);
  tcase_add_test (tc_simple, test_ck_assert_mem_ge);
  tcase_add_test (tc_simple, test_ck_assert_mem_zerolen);
  tcase_add_test (tc_simple, test_ck_assert_mem_eq_exact);
  tcase_add_test (tc_simple, test_ck_assert_mem_eq_longer);

#if defined(HAVE_FORK) && HAVE_FORK==1
  tcase_add_test (tc_signal, test_segv);
  tcase_add_test_raise_signal (tc_signal, test_segv_pass, 11); /* pass  */
  tcase_add_test_raise_signal (tc_signal, test_segv, 8);  /* error */
  tcase_add_test_raise_signal (tc_signal, test_non_signal_8, 8);  /* fail  */
  tcase_add_test_raise_signal (tc_signal, test_fail_unless, 8);  /* fail  */
#if !defined(__CYGWIN__)
  tcase_add_test (tc_signal, test_fpe);
  tcase_add_test (tc_signal, test_mark_point);
#endif /* !defined(__CYGWIN__) */
#endif /* HAVE_FORK */

#if TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) && HAVE_FORK == 1
#if HAVE_DECL_SETENV
  /* tc_timeout_env_int tests have a timeout of 6 seconds */
  tcase_add_test (tc_timeout_env_int, test_eternal_fail);
  tcase_add_test (tc_timeout_env_int, test_sleep2_pass);
  tcase_add_test (tc_timeout_env_int, test_sleep5_pass);
  tcase_add_test (tc_timeout_env_int, test_sleep9_fail);
  
  /* tc_timeout_env_double tests have a timeout of 0.5 seconds */
  tcase_add_test (tc_timeout_env_double, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_env_double, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_env_double, test_sleep1_fail);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_env_double, test_sleep2_fail);
  tcase_add_test (tc_timeout_env_double, test_sleep5_fail);
  tcase_add_test (tc_timeout_env_double, test_sleep9_fail);
#endif /* HAVE_DECL_SETENV */

  /* tc_timeout_default tests have a timeout of 4 seconds */
  tcase_add_test (tc_timeout_default, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_default, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_default, test_sleep1_pass);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_default, test_sleep2_pass);
  tcase_add_test (tc_timeout_default, test_sleep5_fail);
  tcase_add_test (tc_timeout_default, test_sleep9_fail);

  tcase_set_timeout (tc_timeout_usr_int, 6);
  tcase_add_test (tc_timeout_usr_int, test_eternal_fail);
  tcase_add_test (tc_timeout_usr_int, test_sleep2_pass);
  tcase_add_test (tc_timeout_usr_int, test_sleep5_pass);
  tcase_add_test (tc_timeout_usr_int, test_sleep9_fail);

  tcase_set_timeout (tc_timeout_usr_double, 0.5);
  tcase_add_test (tc_timeout_usr_double, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_usr_double, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_usr_double, test_sleep1_fail);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_usr_double, test_sleep2_fail);
  tcase_add_test (tc_timeout_usr_double, test_sleep5_fail);
  tcase_add_test (tc_timeout_usr_double, test_sleep9_fail);
  
#if HAVE_DECL_SETENV
  /* tc_timeout_env_scale_int tests have a timeout of 6 (time) * 2 (multiplier) = 12 seconds */
  tcase_add_test (tc_timeout_env_scale_int, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_env_scale_int, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_env_scale_int, test_sleep1_pass);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_env_scale_int, test_sleep2_pass);
  tcase_add_test (tc_timeout_env_scale_int, test_sleep5_pass);
  tcase_add_test (tc_timeout_env_scale_int, test_sleep9_pass);
  tcase_add_test (tc_timeout_env_scale_int, test_sleep14_fail);

  /* tc_timeout_env_scale_double tests have a timeout of 0.9 (time) * 0.4 (multiplier) = 0.36 seconds */
  tcase_add_test (tc_timeout_env_scale_double, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_env_scale_double, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_env_scale_double, test_sleep1_fail);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_env_scale_double, test_sleep2_fail);
  tcase_add_test (tc_timeout_env_scale_double, test_sleep5_fail);
  tcase_add_test (tc_timeout_env_scale_double, test_sleep9_fail);
  tcase_add_test (tc_timeout_env_scale_double, test_sleep14_fail);

  /* tc_timeout_scale_int tests have a timeout of 2 * 4 (default) = 8 seconds */
  tcase_add_test (tc_timeout_scale_int, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_scale_int, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_scale_int, test_sleep1_pass);
  tcase_add_test (tc_timeout_scale_int, test_sleep2_pass);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_scale_int, test_sleep5_pass);
  tcase_add_test (tc_timeout_scale_int, test_sleep9_fail);

  /* tc_timeout_scale_double tests have a timeout of 4 (default) * 0.3 (multiplier) = 1.6 second */
  tcase_add_test (tc_timeout_scale_double, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_scale_double, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_scale_double, test_sleep1_pass);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_scale_double, test_sleep2_fail);
  tcase_add_test (tc_timeout_scale_double, test_sleep5_fail);
  tcase_add_test (tc_timeout_scale_double, test_sleep9_fail);
  
  setenv("CK_TIMEOUT_MULTIPLIER", "2", 1);
  tcase_set_timeout (tc_timeout_usr_scale_int, 6);
  unsetenv("CK_TIMEOUT_MULTIPLIER");
  tcase_add_test (tc_timeout_usr_scale_int, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_usr_scale_int, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_usr_scale_int, test_sleep1_pass);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_usr_scale_int, test_sleep2_pass);
  tcase_add_test (tc_timeout_usr_scale_int, test_sleep5_pass);
  tcase_add_test (tc_timeout_usr_scale_int, test_sleep9_pass);
  tcase_add_test (tc_timeout_usr_scale_int, test_sleep14_fail);
  
  setenv("CK_TIMEOUT_MULTIPLIER", "0.4", 1);
  tcase_set_timeout (tc_timeout_usr_scale_double, 0.9);
  unsetenv("CK_TIMEOUT_MULTIPLIER");
  tcase_add_test (tc_timeout_usr_scale_double, test_eternal_fail);
#ifdef HAVE_LIBRT
  tcase_add_test (tc_timeout_usr_scale_double, test_sleep0_025_pass);
  tcase_add_test (tc_timeout_usr_scale_double, test_sleep1_fail);
#endif /* HAVE_LIBRT */
  tcase_add_test (tc_timeout_usr_scale_double, test_sleep2_fail);
  tcase_add_test (tc_timeout_usr_scale_double, test_sleep5_fail);
  tcase_add_test (tc_timeout_usr_scale_double, test_sleep9_fail);
  tcase_add_test (tc_timeout_usr_scale_double, test_sleep14_fail);
#endif /* HAVE_DECL_SETENV */
#endif /* TIMEOUT_TESTS_ENABLED && defined(HAVE_FORK) */

#if defined(HAVE_FORK) && HAVE_FORK==1
  tcase_add_test (tc_limit, test_early_exit);
#endif /* HAVE_FORK */
#if MEMORY_LEAKING_TESTS_ENABLED
  tcase_add_test (tc_limit, test_null);
#endif /* MEMORY_LEAKING_TESTS_ENABLED */
  tcase_add_test (tc_limit, test_null_2);

#if defined(HAVE_FORK) && HAVE_FORK==1
  tcase_add_test (tc_messaging_and_fork, test_fork1p_pass);
  tcase_add_test (tc_messaging_and_fork, test_fork1p_fail);
  tcase_add_test (tc_messaging_and_fork, test_fork1c_pass);
  tcase_add_test (tc_messaging_and_fork, test_fork1c_fail);
  tcase_add_test (tc_messaging_and_fork, test_fork2_pass);
  tcase_add_test (tc_messaging_and_fork, test_fork2_fail);

#if MEMORY_LEAKING_TESTS_ENABLED
  tcase_add_test_raise_signal (tc_errors, test_invalid_set_fork_status, 2);
#endif

  tcase_add_test (tc_exit_handlers, test_ignore_exit_handlers);
#endif /* HAVE_FORK */

  return s;
}
