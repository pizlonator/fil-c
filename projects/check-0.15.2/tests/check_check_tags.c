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
#include <check.h>
#include <assert.h>
#include "check_check.h"

enum {
    RED,
    BLUE,
    PURPLE,
    YELLOW,
    BLACK,
    MAX_TESTS
} test_ids;

/*
 * Flag per test which is set true when that test runs
 */
unsigned int test_executed[MAX_TESTS];

static void reset_executed(void)
{
    memset(test_executed, 0, sizeof(test_executed));
}

START_TEST(red_test1)
{
    test_executed[RED] = 1;
}
END_TEST

START_TEST(blue_test1)
{
    test_executed[BLUE] = 1;
}
END_TEST

START_TEST(purple_test1)
{
    test_executed[PURPLE] = 1;
}
END_TEST

START_TEST(yellow_test1)
{
    test_executed[YELLOW] = 1;
}
END_TEST

START_TEST(black_test1)
{
    test_executed[BLACK] = 1;
}
END_TEST


static SRunner *make_tagged_testrunner(void)
{

    SRunner *sr;
    Suite *s1, *s2;

    TCase *red, *blue, *purple, *yellow, *black;

    s1 = suite_create("RedBlue Suite");

    red = tcase_create("Red");
    tcase_set_tags(red, "Red");
    suite_add_tcase (s1, red);
    tcase_add_test(red, red_test1);

    blue = tcase_create("Blue");
    tcase_set_tags(blue, "Blue");
    suite_add_tcase (s1, blue);
    tcase_add_test(blue, blue_test1);

    s2 = suite_create("PurpleYellowBlack Suite");

    purple = tcase_create("Purple");
    tcase_set_tags(purple, "Red Blue");
    suite_add_tcase (s2, purple);
    tcase_add_test(purple, purple_test1);

    yellow = tcase_create("Yellow");
    tcase_set_tags(yellow, "Yellow");
    suite_add_tcase (s2, yellow);
    tcase_add_test(yellow, yellow_test1);

    black = tcase_create("Black");
    suite_add_tcase (s2, black);
    tcase_add_test(black, black_test1);

    sr = srunner_create(s1);
    srunner_add_suite(sr, s2);

    return sr;
}

static   SRunner *sr = NULL;

static void tag_test_setup(void)
{
  sr = make_tagged_testrunner();
  srunner_set_fork_status(sr, CK_NOFORK);
}

static void tag_test_teardown (void)
{
    srunner_free (sr);
}

/*
 * Show that with no filter we run all the tests
 */
START_TEST(null_filter)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, NULL, NULL, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

} END_TEST

START_TEST(include_nothing)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "", NULL, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

START_TEST(exclude_nothing)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, NULL, "", CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

} END_TEST

START_TEST(include_nothing_exclude_nothing)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "", "", CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

START_TEST(include_yellow)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "Yellow", NULL, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST


START_TEST(include_red)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "Red", NULL, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

START_TEST(include_red_blue)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "Red Blue", NULL, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

START_TEST(include_red_blue_yellow)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "Red Blue Yellow", NULL, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

START_TEST(exclude_yellow)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, NULL, "Yellow", CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

} END_TEST

START_TEST(exclude_red)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, NULL, "Red", CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

} END_TEST

START_TEST(exclude_red_blue)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, NULL, "Red Blue", CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

} END_TEST

START_TEST(exclude_red_blue_yellow)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, NULL, "Red Blue Yellow", CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

} END_TEST

#if HAVE_DECL_SETENV

/* env var driven tests */

START_TEST(include_yellow_env)
{
    reset_executed();

    setenv ("CK_INCLUDE_TAGS", "Yellow", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_INCLUDE_TAGS");

} END_TEST


START_TEST(include_red_env)
{
    reset_executed();

    setenv ("CK_INCLUDE_TAGS", "Red", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_INCLUDE_TAGS");

} END_TEST

START_TEST(include_red_blue_env)
{
    reset_executed();

    setenv ("CK_INCLUDE_TAGS", "Red  Blue", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_INCLUDE_TAGS");

} END_TEST

START_TEST(include_red_blue_yellow_env)
{
    reset_executed();

    setenv ("CK_INCLUDE_TAGS", "Red Blue Yellow", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_INCLUDE_TAGS");

} END_TEST

START_TEST(exclude_yellow_env)
{
    reset_executed();

    setenv ("CK_EXCLUDE_TAGS", "Yellow", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

    unsetenv ("CK_EXCLUDE_TAGS");

} END_TEST

START_TEST(exclude_red_env)
{
    reset_executed();

    setenv ("CK_EXCLUDE_TAGS", "Red", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

    unsetenv ("CK_EXCLUDE_TAGS");

} END_TEST

START_TEST(exclude_red_blue_env)
{
    reset_executed();

    setenv ("CK_EXCLUDE_TAGS", "Red Blue", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

    unsetenv ("CK_EXCLUDE_TAGS");

} END_TEST

START_TEST(exclude_red_blue_yellow_env)
{
    reset_executed();

    setenv ("CK_EXCLUDE_TAGS", "Red Blue Yellow", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

    unsetenv ("CK_EXCLUDE_TAGS");

} END_TEST

START_TEST(include_red_case_red_env)
{
    reset_executed();

    setenv ("CK_INCLUDE_TAGS", "Red Yellow", 1);
    setenv ("CK_RUN_CASE", "Red", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_INCLUDE_TAGS");
    unsetenv ("CK_RUN_CASE");

} END_TEST

START_TEST(include_red_case_blue_env)
{
    reset_executed();

    setenv ("CK_INCLUDE_TAGS", "Red Yellow", 1);
    setenv ("CK_RUN_CASE", "Blue", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_INCLUDE_TAGS");
    unsetenv ("CK_RUN_CASE");

} END_TEST

START_TEST(exclude_red_case_red_env)
{
    reset_executed();

    setenv ("CK_EXCLUDE_TAGS", "Red Yellow", 1);
    setenv ("CK_RUN_CASE", "Red", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_EXCLUDE_TAGS");
    unsetenv ("CK_RUN_CASE");

} END_TEST

START_TEST(exclude_red_case_blue_env)
{
    reset_executed();

    setenv ("CK_EXCLUDE_TAGS", "Red Yellow", 1);
    setenv ("CK_RUN_CASE", "Blue", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_EXCLUDE_TAGS");
    unsetenv ("CK_RUN_CASE");

} END_TEST

START_TEST(include_red_suite_redblue_env)
{
    reset_executed();

    setenv ("CK_INCLUDE_TAGS", "Red Yellow", 1);
    setenv ("CK_RUN_SUITE", "RedBlue Suite", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_INCLUDE_TAGS");
    unsetenv ("CK_RUN_SUITE");

} END_TEST

START_TEST(include_red_suite_purpleyellowblack_env)
{
    reset_executed();

    setenv ("CK_INCLUDE_TAGS", "Red Yellow", 1);
    setenv ("CK_RUN_SUITE", "PurpleYellowBlack Suite", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_INCLUDE_TAGS");
    unsetenv ("CK_RUN_SUITE");

} END_TEST

START_TEST(exclude_red_suite_redblue_env)
{
    reset_executed();

    setenv ("CK_EXCLUDE_TAGS", "Red Yellow", 1);
    setenv ("CK_RUN_SUITE", "RedBlue Suite", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

    unsetenv ("CK_EXCLUDE_TAGS");
    unsetenv ("CK_RUN_SUITE");

} END_TEST

START_TEST(exclude_red_suite_purpleyellowblack_env)
{
    reset_executed();

    setenv ("CK_EXCLUDE_TAGS", "Red Yellow", 1);
    setenv ("CK_RUN_SUITE", "PurpleYellowBlack Suite", 1);
    srunner_run_all(sr, CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(test_executed[BLACK]);

    unsetenv ("CK_EXCLUDE_TAGS");
    unsetenv ("CK_RUN_SUITE");

} END_TEST


#endif /* HAVE_DECL_SETENV */


START_TEST(include_red_exclude_red)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "Red", "Red", CK_VERBOSE);

    ck_assert(!test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

START_TEST(include_red_exclude_blue)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "Red", "Blue", CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(!test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

START_TEST(include_red_include_red)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "Red Red Red", NULL, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(!test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

START_TEST(include_w_spaces)
{
    reset_executed();

    srunner_run_tagged(sr, NULL, NULL, "  Red    Blue ", NULL, CK_VERBOSE);

    ck_assert(test_executed[RED]);
    ck_assert(test_executed[BLUE]);
    ck_assert(test_executed[PURPLE]);
    ck_assert(!test_executed[YELLOW]);
    ck_assert(!test_executed[BLACK]);

} END_TEST

Suite *make_tag_suite(void)
{
    TCase *no_filters;
    TCase *include_filters, *exclude_filters;
#if HAVE_DECL_SETENV
    TCase *include_filters_env, *exclude_filters_env;
#endif /* HAVE_DECL_SETENV */
    TCase *include_exclude_filters, *strange_filters;
    TCase *tag_plus_suite_env, *tag_plus_case_env;
    Suite *s;

    s = suite_create("Check Tag Filtering");

    no_filters = tcase_create("no tag filters");
    suite_add_tcase (s, no_filters);
    tcase_add_test(no_filters, null_filter);
    tcase_add_test(no_filters, include_nothing);
    tcase_add_test(no_filters, exclude_nothing);
    tcase_add_unchecked_fixture (no_filters,
				 tag_test_setup,
				 tag_test_teardown);

    include_filters = tcase_create("include tags");
    suite_add_tcase (s, include_filters);
    tcase_add_test(include_filters, include_yellow);
    tcase_add_test(include_filters, include_red);
    tcase_add_test(include_filters, include_red_blue);
    tcase_add_test(include_filters, include_red_blue_yellow);
    tcase_add_unchecked_fixture (include_filters,
				 tag_test_setup,
				 tag_test_teardown);

    exclude_filters = tcase_create("exclude tags");
    suite_add_tcase (s, exclude_filters);
    tcase_add_test(exclude_filters, exclude_yellow);
    tcase_add_test(exclude_filters, exclude_red);
    tcase_add_test(exclude_filters, exclude_red_blue);
    tcase_add_test(exclude_filters, exclude_red_blue_yellow);
    tcase_add_unchecked_fixture (exclude_filters,
				 tag_test_setup,
				 tag_test_teardown);

#if HAVE_DECL_SETENV

    include_filters_env = tcase_create("include tags via env");
    suite_add_tcase (s, include_filters_env);
    tcase_add_test(include_filters_env, include_yellow_env);
    tcase_add_test(include_filters_env, include_red_env);
    tcase_add_test(include_filters_env, include_red_blue_env);
    tcase_add_test(include_filters_env, include_red_blue_yellow_env);
    tcase_add_unchecked_fixture (include_filters_env,
				 tag_test_setup,
				 tag_test_teardown);

    exclude_filters_env = tcase_create("exclude tags via env");
    suite_add_tcase (s, exclude_filters_env);
    tcase_add_test(exclude_filters_env, exclude_yellow_env);
    tcase_add_test(exclude_filters_env, exclude_red_env);
    tcase_add_test(exclude_filters_env, exclude_red_blue_env);
    tcase_add_test(exclude_filters_env, exclude_red_blue_yellow_env);
    tcase_add_unchecked_fixture (exclude_filters_env,
				 tag_test_setup,
				 tag_test_teardown);

    tag_plus_suite_env = tcase_create("combining tag filters with suite selection");
    suite_add_tcase (s, tag_plus_suite_env);
    tcase_add_test(tag_plus_suite_env, include_red_suite_redblue_env);
    tcase_add_test(tag_plus_suite_env, include_red_suite_purpleyellowblack_env);
    tcase_add_test(tag_plus_suite_env, exclude_red_suite_redblue_env);
    tcase_add_test(tag_plus_suite_env, exclude_red_suite_purpleyellowblack_env);
    tcase_add_unchecked_fixture (tag_plus_suite_env,
				 tag_test_setup,
				 tag_test_teardown);

    tag_plus_case_env = tcase_create("combining tag filters with case selection");
    suite_add_tcase (s, tag_plus_case_env);
    tcase_add_test(tag_plus_case_env, include_red_case_red_env);
    tcase_add_test(tag_plus_case_env, include_red_case_blue_env);
    tcase_add_test(tag_plus_case_env, exclude_red_case_red_env);
    tcase_add_test(tag_plus_case_env, exclude_red_case_blue_env);
    tcase_add_unchecked_fixture (tag_plus_case_env,
				 tag_test_setup,
				 tag_test_teardown);

#endif /* HAVE_DECL_SETENV  */

    include_exclude_filters = tcase_create("include and exclude tags");
    suite_add_tcase (s, include_exclude_filters);
    tcase_add_test(include_exclude_filters, include_nothing_exclude_nothing);
    tcase_add_test(include_exclude_filters, include_red_exclude_blue);
    tcase_add_test(include_exclude_filters, include_red_exclude_red);
    tcase_add_unchecked_fixture (include_exclude_filters,
				 tag_test_setup,
				 tag_test_teardown);

    strange_filters = tcase_create("strange tag filters");
    suite_add_tcase (s, strange_filters);
    tcase_add_test(strange_filters, include_red_include_red);
    tcase_add_test(strange_filters, include_w_spaces);
    tcase_add_unchecked_fixture (strange_filters,
				 tag_test_setup,
				 tag_test_teardown);

    return s;
}
