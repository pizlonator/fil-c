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



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <check.h>
#include "config.h"

START_TEST(test_pass)
{
    ck_assert_msg(1 == 1, "Shouldn't see this");
}
END_TEST

START_TEST(test_fail)
{
    ck_abort_msg("Failure");
}
END_TEST

/*
 * This test will fail without fork, as it will result in the
 * unit test runniner exiting early.
 */
#if defined(HAVE_FORK) && HAVE_FORK==1
START_TEST(test_exit)
{
    exit(1);
}
END_TEST
#endif /* HAVE_FORK */

/*
 * This test will intentionally mess up the unit testing program
 * when fork is unavailable. The purpose of including it is to
 * ensure that the tap output is correct when a test crashes.
 */
START_TEST(test_abort)
{
    exit(1);
}
END_TEST

START_TEST(test_pass2)
{
    ck_assert_msg(1 == 1, "Shouldn't see this");
}
END_TEST

START_TEST(test_loop)
{
    ck_assert_msg(_i == 1, "Iteration %d failed", _i);
}
END_TEST

START_TEST(test_xml_esc_fail_msg)
{
    ck_abort_msg("fail \" ' < > & \x9 \xA" "X""\x08"" message"); /* backspace char \x08 deletes the X */
}
END_TEST

static Suite *make_log1_suite(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("S1");
    tc = tcase_create("Core");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_pass);
    tcase_add_test(tc, test_fail);
#if defined(HAVE_FORK) && HAVE_FORK==1
    tcase_add_test(tc, test_exit);
#endif /* HAVE_FORK */

    return s;
}

static Suite *make_log2_suite(int include_exit_test)
{
    Suite *s;
    TCase *tc;

    s = suite_create("S2");
    tc = tcase_create("Core");
    suite_add_tcase(s, tc);
    if(include_exit_test == 1)
    {
        tcase_add_test(tc, test_abort);
    }
    tcase_add_test(tc, test_pass2);
    tcase_add_loop_test(tc, test_loop, 0, 3);

    return s;
}

/* check that XML special characters are properly escaped in XML log file */
static Suite *make_xml_esc_suite(void)
{
    Suite *s;
    TCase *tc;

    s = suite_create("XML escape \" ' < > & \x9 \xA" "X""\x08"" tests"); /* backspace char \x08 deletes the X */
    tc = tcase_create("description \" ' < > & \x9 \xA" "X""\x08"" end"); /* backspace char \x08 deletes the X */
    suite_add_tcase(s, tc);

    tcase_add_test(tc, test_xml_esc_fail_msg);

    return s;
}

static void print_usage(void)
{
    printf("Usage: ex_output (CK_SILENT | CK_MINIMAL | CK_NORMAL | CK_VERBOSE | CK_ENV");
#if ENABLE_SUBUNIT
    printf(" | CK_SUBUNIT");
#endif
    printf(")\n");
    printf("                 (STDOUT | STDOUT_DUMP | LOG | LOG_STDOUT | TAP | TAP_STDOUT | XML | XML_STDOUT)\n");
    printf("                 (NORMAL | EXIT_TEST)\n");
    printf("   If CK_ENV is used, the environment variable CK_VERBOSITY can be set to\n");
    printf("   one of these: silent, minimal, or verbose. If it is not set to these, or\n");
    printf("   if CK_VERBOSITY is not set, then CK_NORMAL will be used\n");
    printf("   If testing the CK_[LOG|TAP_LOG|XML_LOG]_FILE_NAME env var and setting it to '-',\n");
    printf("   then use the following mode: CK_SILENT STDOUT [NORMAL|EXIT_TEST].\n");
}

static void run_tests(enum print_output printmode, char *log_type, int include_exit_test)
{
    SRunner *sr;
    int dump_everything_to_stdout = 0;

    sr = srunner_create(make_log1_suite());
    srunner_add_suite(sr, make_log2_suite(include_exit_test));
    srunner_add_suite(sr, make_xml_esc_suite());

    if(strcmp(log_type, "STDOUT") == 0)
    {
        /* Nothing else to do here */
    }
    else if(strcmp(log_type, "STDOUT_DUMP") == 0)
    {
        /*
         * Dump each type to stdout, in addition to printing out
         * the configured print level.
         */
        dump_everything_to_stdout = 1;
    }
    else if(strcmp(log_type, "LOG") == 0)
    {
        srunner_set_log(sr, "test.log");
    }
    else if(strcmp(log_type, "LOG_STDOUT") == 0)
    {
        srunner_set_log(sr, "-");
    }
    else if(strcmp(log_type, "TAP") == 0)
    {
        srunner_set_tap(sr, "test.tap");
    }
    else if(strcmp(log_type, "TAP_STDOUT") == 0)
    {
        srunner_set_tap(sr, "-");
    }
    else if(strcmp(log_type, "XML") == 0)
    {
        srunner_set_xml(sr, "test.xml");
    }
    else if(strcmp(log_type, "XML_STDOUT") == 0)
    {
        srunner_set_xml(sr, "-");
    }
    else
    {
        print_usage();
        exit(EXIT_FAILURE);
    }

    srunner_run_all(sr, printmode);

    if(dump_everything_to_stdout)
    {
        srunner_print(sr, CK_SILENT);
        srunner_print(sr, CK_MINIMAL);
        srunner_print(sr, CK_NORMAL);
        srunner_print(sr, CK_VERBOSE);
        srunner_print(sr, CK_ENV);
#if ENABLE_SUBUNIT
        /*
         * Note that this call does not contribute anything, as
         * subunit is not fully considered an 'output mode'.
         */
        srunner_print(sr, CK_SUBUNIT);
#endif
    }

    srunner_free(sr);
}

#define OUTPUT_TYPE_ARG       1
#define LOG_TYPE_ARG          2
#define INCLUDE_EXIT_TEST_ARG 3
int main(int argc, char **argv)
{
    enum print_output printmode;
    int include_exit_test;

    if(argc != 4)
    {
        print_usage();
        return EXIT_FAILURE;
    }

    if(strcmp(argv[OUTPUT_TYPE_ARG], "CK_SILENT") == 0)
    {
        printmode = CK_SILENT;
    }
    else if(strcmp(argv[OUTPUT_TYPE_ARG], "CK_MINIMAL") == 0)
    {
        printmode = CK_MINIMAL;
    }
    else if(strcmp(argv[OUTPUT_TYPE_ARG], "CK_NORMAL") == 0)
    {
        printmode = CK_NORMAL;
    }
    else if(strcmp(argv[OUTPUT_TYPE_ARG], "CK_VERBOSE") == 0)
    {
        printmode = CK_VERBOSE;
    }
    else if(strcmp(argv[OUTPUT_TYPE_ARG], "CK_ENV") == 0)
    {
        printmode = CK_ENV;
    }
#if ENABLE_SUBUNIT
    else if(strcmp(argv[OUTPUT_TYPE_ARG], "CK_SUBUNIT") == 0)
    {
        printmode = CK_SUBUNIT;
    }
#endif
    else
    {
        print_usage();
        return EXIT_FAILURE;
    }

    if(strcmp(argv[INCLUDE_EXIT_TEST_ARG], "NORMAL") == 0)
    {
        include_exit_test = 0;
    }
    else if(strcmp(argv[INCLUDE_EXIT_TEST_ARG], "EXIT_TEST") == 0)
    {
        include_exit_test = 1;
    }
    else
    {
        print_usage();
        return EXIT_FAILURE;
    }

    run_tests(printmode, argv[LOG_TYPE_ARG], include_exit_test);

    return EXIT_SUCCESS;
}
