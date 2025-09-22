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
#include <string.h>
#include <check.h>
#include "check_check.h"
#include "check_str.h"


static SRunner *sr;

static void limit_setup (void)
{
  Suite *s = suite_create("Empty");
  sr = srunner_create(s);
  srunner_run_all(sr, CK_VERBOSE);
}

static void limit_teardown (void)
{
  srunner_free(sr);
} 

START_TEST(test_summary)
{
  char * string = sr_stat_str(sr);
  ck_assert_msg(strcmp(string,
		     "100%: Checks: 0, Failures: 0, Errors: 0") == 0,
	      "Bad statistics string for empty suite");
  free(string);
}
END_TEST

Suite *make_limit_suite (void)
{
  Suite *s = suite_create("Limit");
  TCase *tc = tcase_create("Empty");

  tcase_add_test(tc,test_summary);
  tcase_add_unchecked_fixture(tc,limit_setup,limit_teardown);
  
  suite_add_tcase(s, tc);

  return s;
}
