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
#include <string.h>
#include <check.h>
#include "check_check.h"

#if HAVE_DECL_SETENV
/* save environment variable's value and set new value */
static int save_set_env(const char *name, const char *value,
                        const char **old_value)
{
  *old_value = getenv(name);
  return setenv(name, value, 1);
}

/* restore environment variable's old value, handle cases where
   variable must be unset (old value is NULL) */
static int restore_env(const char *name, const char *old_value)
{
  int res;
  if (old_value == NULL) {
     res = unsetenv(name);
  } else {
     res = setenv(name, old_value, 1);
  }
  return res;
}
#endif /* HAVE_DECL_SETENV */

START_TEST(test_set_log)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  srunner_set_log (sr, "test_log");

  ck_assert_msg (srunner_has_log (sr), "SRunner not logging");
  ck_assert_msg (strcmp(srunner_log_fname(sr), "test_log") == 0,
	       "Bad file name returned");

  srunner_free(sr);
}
END_TEST

#if HAVE_DECL_SETENV
/* Test enabling logging via environment variable */
START_TEST(test_set_log_env)
{
  const char *old_val;
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  /* check that setting log file via environment variable works */
  ck_assert_msg(save_set_env("CK_LOG_FILE_NAME", "test_log", &old_val) == 0,
              "Failed to set environment variable");

  ck_assert_msg (srunner_has_log (sr), "SRunner not logging");
  ck_assert_msg (strcmp(srunner_log_fname(sr), "test_log") == 0,
	       "Bad file name returned");

  /* check that explicit call to srunner_set_log()
     overrides environment variable */
  srunner_set_log (sr, "test2_log");

  ck_assert_msg (srunner_has_log (sr), "SRunner not logging");
  ck_assert_msg (strcmp(srunner_log_fname(sr), "test2_log") == 0,
	       "Bad file name returned");

  /* restore old environment */
  ck_assert_msg(restore_env("CK_LOG_FILE_NAME", old_val) == 0,
              "Failed to restore environment variable");

  srunner_free(sr);
}
END_TEST
#endif /* HAVE_DECL_SETENV */

START_TEST(test_no_set_log)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  ck_assert_msg (!srunner_has_log (sr), "SRunner not logging");
  ck_assert_msg (srunner_log_fname(sr) == NULL, "Bad file name returned");

  srunner_free(sr);
}
END_TEST

START_TEST(test_double_set_log)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  srunner_set_log (sr, "test_log");
  srunner_set_log (sr, "test2_log");

  ck_assert_msg(strcmp(srunner_log_fname(sr), "test_log") == 0,
	      "Log file is initialize only and shouldn't be changeable once set");

  srunner_free(sr);
}
END_TEST


START_TEST(test_set_xml)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  srunner_set_xml (sr, "test_log.xml");

  ck_assert_msg (srunner_has_xml (sr), "SRunner not logging XML");
  ck_assert_msg (strcmp(srunner_xml_fname(sr), "test_log.xml") == 0,
	       "Bad file name returned");
         
  srunner_free(sr);
}
END_TEST

#if HAVE_DECL_SETENV
/* Test enabling XML logging via environment variable */
START_TEST(test_set_xml_env)
{
  const char *old_val;
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  /* check that setting XML log file via environment variable works */
  ck_assert_msg(save_set_env("CK_XML_LOG_FILE_NAME", "test_log.xml", &old_val) == 0,
              "Failed to set environment variable");

  ck_assert_msg (srunner_has_xml (sr), "SRunner not logging XML");
  ck_assert_msg (strcmp(srunner_xml_fname(sr), "test_log.xml") == 0,
	       "Bad file name returned");

  /* check that explicit call to srunner_set_xml()
     overrides environment variable */
  srunner_set_xml (sr, "test2_log.xml");

  ck_assert_msg (srunner_has_xml (sr), "SRunner not logging XML");
  ck_assert_msg (strcmp(srunner_xml_fname(sr), "test2_log.xml") == 0,
	       "Bad file name returned");

  /* restore old environment */
  ck_assert_msg(restore_env("CK_XML_LOG_FILE_NAME", old_val) == 0,
              "Failed to restore environment variable");
  
  srunner_free(sr);
}
END_TEST
#endif /* HAVE_DECL_SETENV */

START_TEST(test_no_set_xml)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  ck_assert_msg (!srunner_has_xml (sr), "SRunner not logging XML");
  ck_assert_msg (srunner_xml_fname(sr) == NULL, "Bad file name returned");
  
  srunner_free(sr);
}
END_TEST

START_TEST(test_double_set_xml)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  srunner_set_xml (sr, "test_log.xml");
  srunner_set_xml (sr, "test2_log.xml");

  ck_assert_msg(strcmp(srunner_xml_fname(sr), "test_log.xml") == 0,
	      "XML Log file is initialize only and shouldn't be changeable once set");
  
  srunner_free(sr);
}
END_TEST

START_TEST(test_set_tap)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  srunner_set_tap (sr, "test_log.tap");

  ck_assert_msg (srunner_has_tap (sr), "SRunner not logging TAP");
  ck_assert_msg (strcmp(srunner_tap_fname(sr), "test_log.tap") == 0,
	       "Bad file name returned");

  srunner_free(sr);
}
END_TEST

#if HAVE_DECL_SETENV
/* Test enabling TAP logging via environment variable */
START_TEST(test_set_tap_env)
{
  const char *old_val;
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  /* check that setting XML log file via environment variable works */
  ck_assert_msg(save_set_env("CK_TAP_LOG_FILE_NAME", "test_log.tap", &old_val) == 0,
              "Failed to set environment variable");

  ck_assert_msg (srunner_has_tap (sr), "SRunner not logging TAP");
  ck_assert_msg (strcmp(srunner_tap_fname(sr), "test_log.tap") == 0,
	       "Bad file name returned");

  /* check that explicit call to srunner_set_tap()
     overrides environment variable */
  srunner_set_tap (sr, "test2_log.tap");

  ck_assert_msg (srunner_has_tap (sr), "SRunner not logging TAP");
  ck_assert_msg (strcmp(srunner_tap_fname(sr), "test2_log.tap") == 0,
	       "Bad file name returned");

  /* restore old environment */
  ck_assert_msg(restore_env("CK_TAP_LOG_FILE_NAME", old_val) == 0,
              "Failed to restore environment variable");

  srunner_free(sr);
}
END_TEST
#endif /* HAVE_DECL_SETENV */

START_TEST(test_no_set_tap)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  ck_assert_msg (!srunner_has_tap (sr), "SRunner not logging TAP");
  ck_assert_msg (srunner_tap_fname(sr) == NULL, "Bad file name returned");

  srunner_free(sr);
}
END_TEST

START_TEST(test_double_set_tap)
{
  Suite *s = suite_create("Suite");
  SRunner *sr = srunner_create(s);

  srunner_set_tap (sr, "test_log.tap");
  srunner_set_tap (sr, "test2_log.tap");

  ck_assert_msg(strcmp(srunner_tap_fname(sr), "test_log.tap") == 0,
	      "TAP Log file is initialize only and shouldn't be changeable once set");

  srunner_free(sr);
}
END_TEST

Suite *make_log_suite(void)
{

  Suite *s;
  TCase *tc_core, *tc_core_xml, *tc_core_tap;

  s = suite_create("Log");
  tc_core = tcase_create("Core");
  tc_core_xml = tcase_create("Core XML");
  tc_core_tap = tcase_create("Core TAP");

  suite_add_tcase(s, tc_core);
  tcase_add_test(tc_core, test_set_log);
#if HAVE_DECL_SETENV
  tcase_add_test(tc_core, test_set_log_env);
#endif /* HAVE_DECL_SETENV */
  tcase_add_test(tc_core, test_no_set_log);
  tcase_add_test(tc_core, test_double_set_log);

  suite_add_tcase(s, tc_core_xml);
  tcase_add_test(tc_core_xml, test_set_xml);
#if HAVE_DECL_SETENV
  tcase_add_test(tc_core_xml, test_set_xml_env);
#endif /* HAVE_DECL_SETENV */
  tcase_add_test(tc_core_xml, test_no_set_xml);
  tcase_add_test(tc_core_xml, test_double_set_xml);

  suite_add_tcase(s, tc_core_tap);
  tcase_add_test(tc_core_tap, test_set_tap);
#if HAVE_DECL_SETENV
  tcase_add_test(tc_core_tap, test_set_tap_env);
#endif /* HAVE_DECL_SETENV */
  tcase_add_test(tc_core_tap, test_no_set_tap);
  tcase_add_test(tc_core_tap, test_double_set_tap);

  return s;
}

