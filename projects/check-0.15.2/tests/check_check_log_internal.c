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

/* Tests for log related stuff in check which need non-exported functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include <check_list.h>
#include <check_impl.h>
#include <check_log.h>
#include "check_check.h"


#if ENABLE_SUBUNIT
START_TEST(test_init_logging_subunit)
{
  /* init_logging with CK_SUBUNIT sets stdout 
   * to a subunit function, not any log.
   */
  Log * first_log = NULL;
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);
  srunner_init_logging(sr, CK_SUBUNIT);
  check_list_front (sr->loglst);
  ck_assert_msg (!check_list_at_end(sr->loglst), "No entries in log list");
  first_log = (Log *)check_list_val(sr->loglst);
  ck_assert_msg (first_log != NULL, "log is NULL");
  check_list_advance(sr->loglst);
  ck_assert_msg(check_list_at_end(sr->loglst), "More than one entry in log list");
  ck_assert_msg(first_log->lfun == subunit_lfun,
              "Log function is not the subunit lfun.");
  srunner_end_logging(sr);
  srunner_free(sr);
}
END_TEST
#endif

Suite *make_log_internal_suite(void)
{
  Suite *s;

#if ENABLE_SUBUNIT
  TCase *tc_core_subunit;
  s = suite_create("Log");
  tc_core_subunit = tcase_create("Core SubUnit");
  suite_add_tcase(s, tc_core_subunit);
  tcase_add_test(tc_core_subunit, test_init_logging_subunit);
#else
  s = suite_create("Log");
#endif
  
  return s;
}

