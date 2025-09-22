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

#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "check_check.h"

static SRunner *sr;
static int test_tc11_executed;
static int test_tc12_executed;
static int test_tc21_executed;

static void reset_executed (void)
{
  test_tc11_executed = 0;
  test_tc12_executed = 0;
  test_tc21_executed = 0;
}

START_TEST(test_tc11)
{
  test_tc11_executed = 1;
}
END_TEST

START_TEST(test_tc12)
{
  test_tc12_executed = 1;
}
END_TEST

START_TEST(test_tc21)
{
  test_tc21_executed = 1;
}
END_TEST

static void selective_setup (void)
{
  Suite *s1, *s2;
  TCase *tc11, *tc12, *tc21;

  /*
   * Create a test suite 'suite1' with two test cases 'tcase11' and
   * 'tcase12' containing a single test each.
   */
  s1 = suite_create ("suite1");
  tc11 = tcase_create ("tcase11");
  tcase_add_test (tc11, test_tc11);
  tc12 = tcase_create ("tcase12");
  tcase_add_test (tc12, test_tc12);
  suite_add_tcase (s1, tc11);
  suite_add_tcase (s1, tc12);
  
  /* This line intentionally attempts to add an already
   * added test case twice, to ensure it is not added
   * again. If it was added again, when the test cases
   * are freed a double-free failure will occur. */
  suite_add_tcase (s1, tc12);

  /*
   * Create a test suite 'suite2' with one test case 'test21'
   * containing two tests.
   */
  s2 = suite_create ("suite2");
  tc21 = tcase_create ("tcase21");
  tcase_add_test (tc21, test_tc21);
  suite_add_tcase (s2, tc21);

  sr = srunner_create (s1);
  srunner_add_suite (sr, s2);
  srunner_set_fork_status (sr, CK_NOFORK);
}

static void selective_teardown (void)
{
  srunner_free (sr);
}


START_TEST(test_srunner_run_run_all)
{
  /* This test exercises the srunner_run function for the case where
     both sname and tcname are NULL.  That means to run all the test
     cases in all the defined suites.  */
  srunner_run (sr,
               NULL, /* NULL tsuite name.  */
               NULL, /* NULL tcase name.  */
               CK_VERBOSE);

  ck_assert_msg (srunner_ntests_run(sr) == 3,
               "Not all tests were executed.");

  reset_executed ();
}
END_TEST

START_TEST(test_srunner_run_suite)
{
  /* This test makes the srunner_run function to run all the test
     cases of a existing suite.  */
  srunner_run (sr,
               "suite1",
               NULL,  /* NULL tcase name.  */
               CK_VERBOSE);

  ck_assert_msg (test_tc11_executed 
               && test_tc12_executed
               && !test_tc21_executed,
               "Expected tests were not executed.");

  reset_executed ();
}
END_TEST

START_TEST(test_srunner_run_no_suite)
{
  /* This test makes the srunner_run function to run all the test
     cases of a non-existing suite.  */
  srunner_run (sr,
               "non-existing-suite",
               NULL, /* NULL tcase name. */
               CK_VERBOSE);
  
  ck_assert_msg (!(test_tc11_executed
           || test_tc12_executed
           || test_tc21_executed),
           "An unexpected test was executed.");

  reset_executed ();
}
END_TEST

START_TEST(test_srunner_run_tcase)
{
  /* This test makes the srunner_run function to run an specific
     existing test case.  */
  srunner_run (sr,
               NULL,  /* NULL suite name.  */
               "tcase12",
               CK_VERBOSE);

  ck_assert_msg (!test_tc11_executed
               && test_tc12_executed
               && !test_tc21_executed,
               "Expected tests were not executed.");
  
  reset_executed ();
}
END_TEST

START_TEST(test_srunner_no_tcase)
{
  /* This test makes the srunner_run function to run a non-existant
     test case.  */
  srunner_run (sr,
               NULL,  /* NULL suite name.  */
               "non-existant-test-case",
               CK_VERBOSE);

  ck_assert_msg (!(test_tc11_executed
           || test_tc12_executed
           || test_tc21_executed),
           "An unexpected test was executed.");

  reset_executed ();
}
END_TEST

START_TEST(test_srunner_suite_tcase)
{
  /* This test makes the srunner_run function to run a specific test
     case of a specific test suite.  */
  srunner_run (sr,
               "suite2",
               "tcase21",
               CK_VERBOSE);
  
  ck_assert_msg (!test_tc11_executed
               && !test_tc12_executed
               && test_tc21_executed,
               "Expected tests were not executed.");

  reset_executed ();
}
END_TEST

START_TEST(test_srunner_suite_no_tcase)
{
  /* This test makes the srunner_run function to run a non existant
     test case of a specific test suite.  */
  srunner_run (sr,
               "suite1",
               "non-existant-test-case",
               CK_VERBOSE);

  ck_assert_msg (!(test_tc11_executed
           || test_tc12_executed
           || test_tc21_executed),
           "An unexpected test was executed.");

  reset_executed ();
}
END_TEST


#if HAVE_DECL_SETENV
START_TEST(test_srunner_run_suite_env)
{
  /* This test makes the srunner_run_all function to run all the test
     cases of a existing suite.  */
  setenv ("CK_RUN_SUITE", "suite1", 1);
  srunner_run_all (sr, CK_VERBOSE);

  ck_assert_msg (test_tc11_executed 
               && test_tc12_executed
               && !test_tc21_executed,
               "Expected tests were not executed.");

  reset_executed ();
  unsetenv ("CK_RUN_SUITE");
}
END_TEST

START_TEST(test_srunner_run_no_suite_env)
{
  /* This test makes the srunner_run_all function to run all the test
     cases of a non-existing suite.  */
  setenv ("CK_RUN_SUITE", "non-existing-suite", 1);
  srunner_run_all (sr, CK_VERBOSE);
  
  ck_assert_msg (!(test_tc11_executed
           || test_tc12_executed
           || test_tc21_executed),
           "An unexpected test was executed.");

  reset_executed ();
  unsetenv ("CK_RUN_SUITE");
}
END_TEST

START_TEST(test_srunner_run_tcase_env)
{
  /* This test makes the srunner_run_all function to run an specific
     existing test case.  */
  setenv ("CK_RUN_CASE", "tcase12", 1);
  srunner_run_all (sr, CK_VERBOSE);

  ck_assert_msg (!test_tc11_executed
               && test_tc12_executed
               && !test_tc21_executed,
               "Expected tests were not executed.");
  
  reset_executed ();
  unsetenv ("CK_RUN_CASE");
}
END_TEST

START_TEST(test_srunner_no_tcase_env)
{
  /* This test makes the srunner_run_all function to run a
     non-existant test case.  */
  setenv ("CK_RUN_CASE", "non-existant-test-case", 1);
  srunner_run_all (sr, CK_VERBOSE);

  ck_assert_msg (!(test_tc11_executed
           || test_tc12_executed
           || test_tc21_executed),
           "An unexpected test was executed.");

  reset_executed ();
  unsetenv ("CK_RUN_CASE");
}
END_TEST

START_TEST(test_srunner_suite_tcase_env)
{
  /* This test makes the srunner_run_all function to run a specific test
     case of a specific test suite.  */
  setenv ("CK_RUN_SUITE", "suite2", 1);
  setenv ("CK_RUN_CASE", "tcase21", 1);
  srunner_run_all (sr, CK_VERBOSE);
  
  ck_assert_msg (!test_tc11_executed
               && !test_tc12_executed
               && test_tc21_executed,
               "Expected tests were not executed.");

  reset_executed ();
  unsetenv ("CK_RUN_SUITE");
  unsetenv ("CK_RUN_CASE");
}
END_TEST

START_TEST(test_srunner_suite_no_tcase_env)
{
  /* This test makes the srunner_run_all function to run a non
     existant test case of a specific test suite.  */
  setenv ("CK_RUN_SUITE", "suite1", 1);
  setenv ("CK_RUN_CASE", "non-existant-test-case", 1);
  srunner_run_all (sr, CK_VERBOSE);

  ck_assert_msg (!(test_tc11_executed
           || test_tc12_executed
           || test_tc21_executed),
           "An unexpected test was executed.");

  reset_executed ();
  unsetenv ("CK_RUN_SUITE");
  unsetenv ("CK_RUN_CASE");
}
END_TEST
#endif /* HAVE_DECL_SETENV */

Suite *make_selective_suite (void)
{
  Suite *s = suite_create ("SelectiveTesting");
  TCase *tc = tcase_create ("Core");

  suite_add_tcase (s, tc);
  tcase_add_test (tc, test_srunner_run_run_all);
  tcase_add_test (tc, test_srunner_run_suite);
  tcase_add_test (tc, test_srunner_run_no_suite);
  tcase_add_test (tc, test_srunner_run_tcase);
  tcase_add_test (tc, test_srunner_no_tcase);
  tcase_add_test (tc, test_srunner_suite_tcase);
  tcase_add_test (tc, test_srunner_suite_no_tcase);

#if HAVE_DECL_SETENV
  tcase_add_test (tc, test_srunner_run_suite_env);
  tcase_add_test (tc, test_srunner_run_no_suite_env);
  tcase_add_test (tc, test_srunner_run_tcase_env);
  tcase_add_test (tc, test_srunner_no_tcase_env);
  tcase_add_test (tc, test_srunner_suite_tcase_env);
  tcase_add_test (tc, test_srunner_suite_no_tcase_env);
#endif /* HAVE_DECL_SETENV */

  tcase_add_unchecked_fixture (tc,
                               selective_setup,
                               selective_teardown);
  return s;
}
