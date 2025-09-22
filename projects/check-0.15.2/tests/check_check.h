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

#ifndef CHECK_CHECK_H
#define CHECK_CHECK_H

#ifndef TIMEOUT_TESTS_ENABLED
#define TIMEOUT_TESTS_ENABLED 1
#endif

/*
 * Certain unit tests are known to leak memory. This
 * #define will prevent those unit tests from being built
 * if the program is to be used against valgrind.
 */
#ifndef MEMORY_LEAKING_TESTS_ENABLED
#define MEMORY_LEAKING_TESTS_ENABLED 1
#endif

extern int sub_ntests;

void fork_setup (void);
void fork_teardown (void);
void setup_fixture(void);
void teardown_fixture (void);
void setup (void);
void cleanup (void);
Suite *make_sub_suite(void);
Suite *make_sub2_suite(void);
Suite *make_master_suite(void);
Suite *make_list_suite(void);
Suite *make_msg_suite(void);
Suite *make_log_suite(void);
Suite *make_log_internal_suite(void);
Suite *make_limit_suite(void);
Suite *make_fork_suite(void);
Suite *make_fixture_suite(void);
Suite *make_pack_suite(void);
Suite *make_exit_suite(void);
Suite *make_selective_suite(void);
Suite *make_tag_suite(void);

extern int master_tests_lineno[];
void init_master_tests_lineno(int num_master_tests);

/**
 * Record a test name.
 *
 * This is used to record the test names of each test in
 * check_check_sub.c. This allows the test name to be written
 * in the master_tests table in check_check_master.c and have
 * it verified at test time. With this data, one can easily
 * determine the name of a failed test.
 */
void record_test_name(const char* test_name);

/**
 * Retrieve the next recorded test which was run, or
 * NULL if no further tests are recorded.
 */
char* get_next_test_name(FILE * file);

/**
 * Record a line number for a test which is to fail.
 *
 * This is used to record the failure line numbers for
 * all tests in check_check_sub.c. Simply make this
 * call right before an assert to record the proper
 * line number. The line number is adjusted +1 internally,
 * to account for making this call before the failure.
 */
void record_failure_line_num(const int line);

/**
 * Once the failure file numbers have been recorded
 * to file and the file has been rewind(), this
 * call will extract the next line number from the
 * file.
 *
 * If there are no more lines to read, -1 is returned.
 */
long get_next_failure_line_num(FILE * file);

#endif /* CHECK_CHECK_H */
