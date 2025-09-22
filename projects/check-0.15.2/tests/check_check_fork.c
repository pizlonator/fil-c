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
#include <check.h>
#include "check_check.h"


static int counter;
static pid_t mypid;

static void fork_sub_setup (void)
{
  counter = 0;
  mypid = getpid();
}

START_TEST(test_inc)
{
  counter++;
}
END_TEST

START_TEST(test_nofork_sideeffects)
{
  ck_assert_msg(counter == 1,
	      "Side effects not seen across tests");
}
END_TEST

START_TEST(test_nofork_pid)
{
  ck_assert_msg(mypid == getpid(),
	      "Unit test is in a different adresss space from setup code");
}
END_TEST

static Suite *make_fork_sub_suite (void)
{

  Suite *s;
  TCase *tc;

  s = suite_create("Fork Sub");
  tc = tcase_create("Core");

  suite_add_tcase (s, tc);
  tcase_add_unchecked_fixture(tc, fork_sub_setup,NULL);
  tcase_add_test(tc,test_inc);
  tcase_add_test(tc,test_nofork_sideeffects);
  tcase_add_test(tc,test_nofork_pid);

  return s;
}

static SRunner *fork_sr;
static SRunner *fork_dummy_sr;

void fork_setup (void)
{
  fork_sr = srunner_create(make_fork_sub_suite());
  fork_dummy_sr = srunner_create (make_fork_sub_suite());
  srunner_set_fork_status(fork_sr,CK_NOFORK);
  srunner_run_all(fork_sr,CK_VERBOSE);
}

void fork_teardown (void)
{
  srunner_free(fork_sr);
}

START_TEST(test_default_fork)
{
#if defined(HAVE_FORK) && HAVE_FORK == 1
  ck_assert_msg(srunner_fork_status(fork_dummy_sr) == CK_FORK,
	      "Default fork status not set correctly");
#else
  ck_assert_msg(srunner_fork_status(fork_dummy_sr) == CK_NOFORK,
	      "Default fork status not set correctly");
#endif /* HAVE_FORK */
}
END_TEST

START_TEST(test_set_nofork)
{
  srunner_set_fork_status(fork_dummy_sr, CK_NOFORK);
  ck_assert_msg(srunner_fork_status(fork_dummy_sr) == CK_NOFORK,
	      "Fork status not changed correctly");
}
END_TEST

/*
 * The following tests will fail if fork is unavailable, as
 * attempting to set the fork mode as anything but
 * CK_NOFORK is considered an error.
 */
#if defined(HAVE_FORK) && HAVE_FORK==1
START_TEST(test_set_fork)
{
  srunner_set_fork_status(fork_dummy_sr, CK_FORK);
  ck_assert_msg(srunner_fork_status(fork_dummy_sr) == CK_FORK,
	      "Fork status not changed correctly");
}
END_TEST

START_TEST(test_env)
{
  char envvar[] = "CK_FORK=no";
  putenv(envvar);
  ck_assert_msg(srunner_fork_status(fork_dummy_sr) == CK_NOFORK,
	      "Fork status does not obey environment variable");
}
END_TEST

START_TEST(test_env_and_set)
{
  char envvar[] = "CK_FORK=no";
  putenv(envvar);
  srunner_set_fork_status(fork_dummy_sr, CK_FORK);  
  ck_assert_msg(srunner_fork_status(fork_dummy_sr) == CK_FORK,
	      "Explicit setting of fork status should override env");
}
END_TEST
#endif /* HAVE_FORK */

START_TEST(test_nofork)
{
  ck_assert_msg(srunner_ntests_failed(fork_sr) == 0,
	      "Errors on nofork test");
}
END_TEST

Suite *make_fork_suite(void)
{
  Suite *s;
  TCase *tc;

  s = suite_create("Fork");
  tc = tcase_create("Core");

  suite_add_tcase(s, tc);
  tcase_add_test(tc,test_default_fork);
  tcase_add_test(tc,test_set_nofork);
#if defined(HAVE_FORK) && HAVE_FORK==1
  tcase_add_test(tc,test_set_fork);
  tcase_add_test(tc,test_env);
  tcase_add_test(tc,test_env_and_set);
#endif /* HAVE_FORK */
  tcase_add_test(tc,test_nofork);
  
  return s;
}
