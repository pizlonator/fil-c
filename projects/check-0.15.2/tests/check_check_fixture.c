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
#include <check.h>
#include "check_error.h"
#include "check_str.h"
#include "check_check.h"

static char errm[200];

static void fixture_sub_setup (void)
{
  ck_abort_msg("Test failure in fixture");
}

static SRunner *fixture_sr;

void setup_fixture (void)
{
  TCase *tc;
  Suite *fixture_s;
  
  fixture_s = suite_create("Fix Sub");
  tc = tcase_create("Fix Sub");
  tcase_add_unchecked_fixture(tc, fixture_sub_setup, NULL);
  suite_add_tcase (fixture_s, tc);
  fixture_sr = srunner_create(fixture_s);
  srunner_run_all(fixture_sr,CK_VERBOSE);
}

void teardown_fixture (void)
{
  srunner_free(fixture_sr);
}

START_TEST(test_fixture_fail_counts)
{
  int nrun, nfail;

  nrun = srunner_ntests_run(fixture_sr);
  nfail = srunner_ntests_failed(fixture_sr);

  ck_assert_msg (nrun == 1 && nfail == 1,
	       "Counts for run and fail for fixture failure not correct");
}
END_TEST

START_TEST(test_print_counts)
{
  char *srstat = sr_stat_str(fixture_sr);
  const char *exp = "0%: Checks: 1, Failures: 1, Errors: 0";

  ck_assert_msg(strcmp(srstat, exp) == 0,
	      "SRunner stat string incorrect with setup failure");
  free(srstat);
}
END_TEST

START_TEST(test_setup_failure_msg)
{
  TestResult **tra;
  char *trm;
  const char *trmexp = "check_check_fixture.c:36:S:Fix Sub:unchecked_setup:0: Test failure in fixture";

  tra = srunner_failures(fixture_sr);
  trm = tr_str(tra[0]);
  free(tra);

  if (strstr(trm, trmexp) == 0) {
    snprintf(errm, sizeof(errm),
	     "Bad setup tr msg (%s)", trm);
    
    ck_abort_msg("%s", errm);
  }
  free(trm);
}
END_TEST

#if defined(HAVE_FORK) && HAVE_FORK==1
int testval_up;
int testval_down;

static void sub_ch_setup_norm (void)
{
  testval_up += 2;
}

static void sub_ch_teardown_norm(void)
{
  testval_down += 2;
}

START_TEST(test_sub_ch_setup_norm)
{
  if (testval_up == 1)
    ck_abort_msg("Setup not run correctly");
  else if (testval_up > 3)
    ck_abort_msg("Test side-effects persist across runs");
  testval_up++;
}
END_TEST

START_TEST(test_ch_setup)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;

  s = suite_create("Fixture Norm Sub");
  tc = tcase_create("Fixture Norm Sub");
  sr = srunner_create(s);
  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_sub_ch_setup_norm);
  tcase_add_test(tc,test_sub_ch_setup_norm);
  tcase_add_checked_fixture(tc,sub_ch_setup_norm,sub_ch_teardown_norm);
  srunner_run_all(sr, CK_VERBOSE);

  ck_assert_msg(srunner_ntests_failed(sr) == 0,
	      "Checked setup not being run correctly");

  srunner_free(sr);
}
END_TEST

static void setup_sub_fail (void)
{
  ck_abort_msg("Failed setup"); /* check_check_fixture.c:130 */
}

static void teardown_sub_fail (void)
{
  ck_abort_msg("Failed teardown");
}

static void setup_sub_signal (void)
{
  mark_point();
  raise(SIGFPE);
}

static void teardown_sub_signal(void)
{
  mark_point();
  raise(SIGFPE);
}

START_TEST(test_sub_fail)
{
  ck_abort_msg("Should never run");
}
END_TEST

START_TEST(test_sub_pass)
{
  ck_assert_msg(1 == 1, "Always pass");
}
END_TEST

START_TEST(test_ch_setup_fail)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;
  TestResult ** tr;
  char *strstat;
  char *trm;

  s = suite_create("Setup Fail");
  tc = tcase_create("Setup Fail");
  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_sub_fail);
  tcase_add_checked_fixture(tc,setup_sub_fail, NULL);
  sr = srunner_create(s);
  srunner_run_all(sr,CK_VERBOSE);

  ck_assert_msg (srunner_ntests_run(sr) == 1,
	       "Test run counts not correct for checked setup failure");
  ck_assert_msg (srunner_ntests_failed(sr) == 1,
	       "Failure counts not correct for checked setup failure");

  strstat= sr_stat_str(sr);

  ck_assert_msg(strcmp(strstat,
		     "0%: Checks: 1, Failures: 1, Errors: 0") == 0,
	      "SRunner stat string incorrect with checked setup failure");
  free(strstat);

  tr = srunner_failures(sr);
  trm = tr_str(tr[0]);
   /* Search for check_check_fixture.c:150 if this fails. */
  if (strstr(trm,
	     "check_check_fixture.c:150:S:Setup Fail:test_sub_fail:0: Failed setup")
      == 0) {
    snprintf(errm, sizeof(errm),
	     "Bad failed checked setup tr msg (%s)", trm);
    
    ck_abort_msg("%s", errm);
  }
  free(trm);
  free(tr);
}
END_TEST

START_TEST(test_ch_setup_fail_nofork)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;

  s = suite_create("Setup Fail Nofork");
  tc = tcase_create("Setup Fail Nofork");
  suite_add_tcase(s, tc);
  tcase_add_test(tc, test_sub_fail);
  tcase_add_checked_fixture(tc, setup_sub_fail, NULL);
  sr = srunner_create(s);
  srunner_set_fork_status(sr, CK_NOFORK);
  srunner_run_all(sr, CK_VERBOSE);

  ck_assert_msg (srunner_ntests_run(sr) == 1,
	       "Test run counts not correct for checked setup failure");
  ck_assert_msg (srunner_ntests_failed(sr) == 1,
	       "Failure counts not correct for checked setup failure");
  srunner_free(sr);
}
END_TEST

START_TEST(test_ch_setup_fail_nofork_2)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;

  s = suite_create("Setup Fail Nofork 2");
  tc = tcase_create("Setup Fail Nofork 2");
  suite_add_tcase(s, tc);
  tcase_add_test(tc, test_sub_fail);
  tcase_add_checked_fixture(tc, sub_ch_setup_norm, NULL);
  tcase_add_checked_fixture(tc, setup_sub_fail, NULL);
  sr = srunner_create(s);
  srunner_set_fork_status(sr, CK_NOFORK);
  srunner_run_all(sr, CK_VERBOSE);

  ck_assert_msg (srunner_ntests_run(sr) == 1,
	       "Test run counts not correct for checked setup failure");
  ck_assert_msg (srunner_ntests_failed(sr) == 1,
	       "Failure counts not correct for checked setup failure");
  srunner_free(sr);
}
END_TEST

START_TEST(test_ch_setup_pass_nofork)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;

  s = suite_create("Setup Pass Multiple fixtures");
  tc = tcase_create("Setup Pass Multiple fixtures");
  suite_add_tcase(s, tc);
  tcase_add_test(tc, test_sub_pass);
  tcase_add_checked_fixture(tc, sub_ch_setup_norm, sub_ch_teardown_norm);
  tcase_add_checked_fixture(tc, sub_ch_setup_norm, sub_ch_teardown_norm);
  tcase_add_checked_fixture(tc, sub_ch_setup_norm, sub_ch_teardown_norm);
  sr = srunner_create(s);
  srunner_set_fork_status(sr, CK_NOFORK);
  testval_up = 1;
  testval_down = 1;
  srunner_run_all(sr, CK_VERBOSE);
  ck_assert_msg(testval_up == 7, "Multiple setups failed");
  ck_assert_msg(testval_down == 7, "Multiple teardowns failed");

  ck_assert_msg (srunner_ntests_run(sr) == 1,
	       "Test run counts not correct for checked setup failure");
  ck_assert_msg (srunner_ntests_failed(sr) == 0,
	       "Failure counts not correct for checked setup failure");
  srunner_free(sr);
}
END_TEST

/* This test currently does not work on Cygwin, as it results in a
 * SIGSEGV instead of a SIGFPE. However, a simple program that installs
 * a SIGFPE handler then raise(SIGFPE) works as expected. Further
 * investigation is necessary. */
#if !defined(__CYGWIN__)
/*
 * This test will fail without fork, as it results in a checked
 * fixture raising a signal, which terminates the test runner early.
 */
START_TEST(test_ch_setup_sig)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;
  TestResult **tr;
  char *strstat;
  char *trm;

  s = suite_create("Setup Sig");
  tc = tcase_create("Setup Sig");
  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_sub_fail);
  tcase_add_checked_fixture(tc,setup_sub_signal, NULL);
  sr = srunner_create(s);
  srunner_run_all(sr,CK_VERBOSE);

  ck_assert_msg (srunner_ntests_failed(sr) == 1,
	       "Failure counts not correct for checked setup signal");
  ck_assert_msg (srunner_ntests_run(sr) == 1,
	       "Test run counts not correct for checked setup signal");

  strstat= sr_stat_str(sr);

  ck_assert_msg(strcmp(strstat,
		     "0%: Checks: 1, Failures: 0, Errors: 1") == 0,
	      "SRunner stat string incorrect with checked setup signal");
  free(strstat);

  tr = srunner_failures(sr);
  trm = tr_str(tr[0]);

  if (strstr(trm,
	     "check_check_fixture.c:160:S:Setup Sig:test_sub_fail:0: "
	     "(after this point) Received signal 8")
      == 0) {
    snprintf(errm, sizeof(errm),
	     "Msg was (%s)", trm);
    
    ck_abort_msg("%s", errm);
  }
  free(trm);
  srunner_free(sr);
  free(tr);
}
END_TEST
#endif /* !defined(__CYGWIN__) */

static void sub_ch_setup_dual_1(void)
{
  ck_assert_msg(testval_up == 1, "Wrong start value");
  testval_up += 2;
}

static void sub_ch_setup_dual_2(void)
{
  ck_assert_msg(testval_up == 3, "First setup failed");
  testval_up += 3;
}

START_TEST(test_sub_two_setups)
{
  ck_assert_msg(testval_up == 6, "Multiple setups failed");
}
END_TEST

/*
 * This test will not work without fork, as checked fixtures are
 * not supported
 */
START_TEST(test_ch_setup_two_setups_fork)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;

  s = suite_create("Fixture Two setups");
  tc = tcase_create("Fixture Two setups");
  sr = srunner_create(s);
  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_sub_two_setups);
  tcase_add_checked_fixture(tc,sub_ch_setup_dual_1,NULL);
  tcase_add_checked_fixture(tc,sub_ch_setup_dual_2,NULL);
  testval_up = 1;
  srunner_run_all(sr, CK_VERBOSE);

  ck_assert_msg(srunner_ntests_failed(sr) == 0,
	      "Problem with several setups");

  srunner_free(sr);
}
END_TEST

START_TEST(test_ch_teardown_fail)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;
  TestResult **tr;
  char *strstat;
  char *trm;

  s = suite_create("Teardown Fail");
  tc = tcase_create("Teardown Fail");
  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_sub_pass);
  tcase_add_checked_fixture(tc,NULL, teardown_sub_fail);
  sr = srunner_create(s);
  srunner_run_all(sr,CK_VERBOSE);

  ck_assert_msg (srunner_ntests_failed(sr) == 1,
	       "Failure counts not correct for checked teardown failure");
  ck_assert_msg (srunner_ntests_run(sr) == 1,
	       "Test run counts not correct for checked teardown failure");

  strstat= sr_stat_str(sr);

  ck_assert_msg(strcmp(strstat,
		     "0%: Checks: 1, Failures: 1, Errors: 0") == 0,
	      "SRunner stat string incorrect with checked setup failure");
  free(strstat);

  tr = srunner_failures(sr);
  trm = tr_str(tr[0]);

  if (strstr(trm,
	     "check_check_fixture.c:155:S:Teardown Fail:test_sub_pass:0: Failed teardown")
      == 0) {
    snprintf(errm, sizeof(errm),
	     "Bad failed checked teardown tr msg (%s)", trm);
    
    ck_abort_msg("%s", errm);
  }
  free(trm);
  free(tr);
}
END_TEST

START_TEST(test_ch_teardown_fail_nofork)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;
  TestResult **tr;
  char *strstat;
  char *trm;

  s = suite_create("Teardown Fail No Fork");
  tc = tcase_create("Teardown Fail No Fork");
  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_sub_pass);
  tcase_add_checked_fixture(tc,NULL, teardown_sub_fail);
  sr = srunner_create(s);
  srunner_set_fork_status(sr, CK_NOFORK);
  srunner_run_all(sr,CK_VERBOSE);

  ck_assert_msg (srunner_ntests_failed(sr) == 1,
	       "Failure counts not correct for checked teardown failure");
  ck_assert_msg (srunner_ntests_run(sr) == 1,
	       "Test run counts not correct for checked teardown failure");

  strstat= sr_stat_str(sr);

  ck_assert_msg(strcmp(strstat,
		     "0%: Checks: 1, Failures: 1, Errors: 0") == 0,
	      "SRunner stat string incorrect with checked setup failure");
  free(strstat);

  tr = srunner_failures(sr);
  trm = tr_str(tr[0]);

  if (strstr(trm,
	     "check_check_fixture.c:155:S:Teardown Fail No Fork:test_sub_pass:0: Failed teardown")
      == 0) {
    snprintf(errm, sizeof(errm),
	     "Bad failed checked teardown tr msg (%s)", trm);
    
    ck_abort_msg("%s", errm);
  }
  free(trm);
  free(tr);
}
END_TEST

/* This test currently does not work on Cygwin, as it results in a
 * SIGSEGV instead of a SIGFPE. However, a simple program that installs
 * a SIGFPE handler then raise(SIGFPE) works as expected. Further
 * investigation is necessary. */
#if !defined(__CYGWIN__)
/*
 * This test will fail without fork, as it results in a checked
 * fixture raising a signal, which terminates the test runner early.
 */

START_TEST(test_ch_teardown_sig)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;
  TestResult **tr;
  char *strstat;
  char *trm;

  s = suite_create("Teardown Sig");
  tc = tcase_create("Teardown Sig");
  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_sub_pass);
  tcase_add_checked_fixture(tc,NULL, teardown_sub_signal);
  sr = srunner_create(s);
  srunner_run_all(sr,CK_VERBOSE);

  ck_assert_msg (srunner_ntests_failed(sr) == 1,
	       "Failure counts not correct for checked teardown signal");
  ck_assert_msg (srunner_ntests_run(sr) == 1,
	       "Test run counts not correct for checked teardown signal");

  strstat= sr_stat_str(sr);

  ck_assert_msg(strcmp(strstat,
		     "0%: Checks: 1, Failures: 0, Errors: 1") == 0,
	      "SRunner stat string incorrect with checked teardown signal");
  free(strstat);

  tr = srunner_failures(sr);
  trm = tr_str(tr[0]);

  if (strstr(trm,
	     "check_check_fixture.c:166:S:Teardown Sig:test_sub_pass:0: "
	     "(after this point) Received signal 8")
      == 0) {
    snprintf(errm, sizeof(errm),
	     "Bad msg (%s)", trm);
    
    ck_abort_msg("%s", errm);
  }
  free(trm);
  srunner_free(sr);
  free(tr);
}
END_TEST
#endif /* !defined(__CYGWIN__) */

/* Teardowns are run in reverse order */
static void sub_ch_teardown_dual_1(void)
{
  ck_assert_msg(testval_down == 6, "Second teardown failed");
}

static void sub_ch_teardown_dual_2(void)
{
  ck_assert_msg(testval_down == 3, "First teardown failed");
  testval_down += 3;
}

START_TEST(test_sub_two_teardowns)
{
  testval_down += 2;
}
END_TEST

/*
 * This test will not work without fork, as checked fixtures are
 * not supported
 */
START_TEST(test_ch_teardown_two_teardowns_fork)
{
  TCase *tc;
  Suite *s;
  SRunner *sr;
  int nr_of_failures;
  char errm[1024] = {0};

  s = suite_create("Fixture Two teardowns");
  tc = tcase_create("Fixture Two teardowns");
  sr = srunner_create(s);
  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_sub_two_teardowns);
  tcase_add_checked_fixture(tc,NULL,sub_ch_teardown_dual_1);
  tcase_add_checked_fixture(tc,NULL,sub_ch_teardown_dual_2);
  testval_down = 1;
  srunner_run_all(sr, CK_VERBOSE);
  
  nr_of_failures = srunner_ntests_failed(sr);
  if (nr_of_failures > 0) {
    TestResult **tra = srunner_failures(sr);
    int i;

    for (i = 0; i < nr_of_failures; i++) {
      char *trm = tr_str(tra[i]);
      if (strlen(errm) + strlen(trm) > 1022) {
        free(trm);
        break;
      } 
      strcat(errm, trm);
      strcat(errm, "\n");
      free(trm);
    }
    free(tra);
  }
  ck_assert_msg(nr_of_failures == 0, "Problem with several teardowns\n %s",
              errm);

  srunner_free(sr);
}
END_TEST
#endif /* HAVE_FORK */

Suite *make_fixture_suite (void)
{

  Suite *s;
  TCase *tc;

  s = suite_create("Fixture");
  tc = tcase_create("Core");

  suite_add_tcase (s, tc);
  tcase_add_test(tc,test_fixture_fail_counts);
  tcase_add_test(tc,test_print_counts);
  tcase_add_test(tc,test_setup_failure_msg);

#if defined(HAVE_FORK) && HAVE_FORK==1
  /*
   * This test assumes that CK_FORK is being used,
   * as it tests that side effects from checked
   * fixtures do not persist between tests.
   */
  tcase_add_test(tc,test_ch_setup);
  
  tcase_add_test(tc,test_ch_setup_fail);
  tcase_add_test(tc,test_ch_setup_fail_nofork);
  tcase_add_test(tc,test_ch_setup_fail_nofork_2);
  tcase_add_test(tc,test_ch_setup_pass_nofork);
#if !defined(__CYGWIN__)
  tcase_add_test(tc,test_ch_setup_sig);
#endif /* !defined(__CYGWIN__) */
  tcase_add_test(tc,test_ch_setup_two_setups_fork);
  tcase_add_test(tc,test_ch_teardown_fail);
  tcase_add_test(tc,test_ch_teardown_fail_nofork);
#if !defined(__CYGWIN__)
  tcase_add_test(tc,test_ch_teardown_sig);
#endif /* !defined(__CYGWIN__) */
  tcase_add_test(tc,test_ch_teardown_two_teardowns_fork);
#endif

  return s;
}
