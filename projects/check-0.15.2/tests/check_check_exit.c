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
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "check_check.h"

START_TEST(test_early_exit_normal)
{
	exit(0);
	ck_abort_msg("Should've exitted...");
}
END_TEST

START_TEST(test_early_exit_with_allowed_error)
{
	exit(-1);
	ck_abort_msg("Should've exitted...");
}
END_TEST

START_TEST(loop_early_exit_normal)
{
	exit(0);
	ck_abort_msg("Should've exitted...");
}
END_TEST

START_TEST(loop_early_exit_allowed_exit)
{
	exit(-2);
	ck_abort_msg("Should've exitted...");
}
END_TEST

START_TEST(loop_early_exit_signal_segv)
{
	raise (SIGSEGV);
	ck_abort_msg("Should've exitted...");
}
END_TEST

Suite *make_exit_suite(void)
{
  Suite *s;
  TCase *tc;

  s = suite_create("Exit");
  tc = tcase_create("Core");

  suite_add_tcase (s, tc);
  tcase_add_test(tc,test_early_exit_normal);
  tcase_add_exit_test(tc,test_early_exit_with_allowed_error,-1);
  tcase_add_loop_test(tc,loop_early_exit_normal,0,5);
  tcase_add_loop_exit_test(tc,loop_early_exit_allowed_exit,-2,0,5);
  tcase_add_loop_test_raise_signal(tc, loop_early_exit_signal_segv, SIGSEGV, 0, 5);
  return s;
}
