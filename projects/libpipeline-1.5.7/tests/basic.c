/*
 * Copyright (C) 2010-2017 Colin Watson.
 *
 * This file is part of libpipeline.
 *
 * libpipeline is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * libpipeline is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libpipeline; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "dirname.h"
#include "xalloc.h"

#include "common.h"

const char *program_name = "basic";

START_TEST (test_basic_status)
{
	pipeline *p;

	p = pipeline_new_command_args ("true", (void *) 0);
	ck_assert_msg (pipeline_run (p) == 0, "true did not return 0");
	p = pipeline_new_command_args ("false", (void *) 0);
	ck_assert_msg (pipeline_run (p) != 0, "false returned 0");
}
END_TEST

START_TEST (test_basic_args)
{
	pipeline *p;
	const char *line;

	p = pipeline_new_command_args ("echo", "foo", (void *) 0);
	pipeline_want_out (p, -1);
	ck_assert_msg (pipecmd_get_nargs (pipeline_get_command (p, 0)) == 2,
	               "Number of arguments != 2");
	pipeline_start (p);
	line = pipeline_readline (p);
	ck_assert_ptr_ne (line, NULL);
	ck_assert_msg (!strcmp (line, "foo\n"),
	               "Incorrect output from 'echo foo': '%s'", line);
	ck_assert_msg (pipeline_wait (p) == 0, "'echo foo' did not return 0");
	pipeline_free (p);

	p = pipeline_new_command_args ("echo", "foo", "bar", (void *) 0);
	pipeline_want_out (p, -1);
	ck_assert_msg (pipecmd_get_nargs (pipeline_get_command (p, 0)) == 3,
	               "Number of arguments != 3");
	pipeline_start (p);
	line = pipeline_readline (p);
	ck_assert_ptr_ne (line, NULL);
	ck_assert_msg (!strcmp (line, "foo bar\n"),
	               "Incorrect output from 'echo foo bar': '%s'", line);
	ck_assert_msg (pipeline_wait (p) == 0,
	               "'echo foo bar' did not return 0");
	pipeline_free (p);
}
END_TEST

START_TEST (test_basic_pipeline)
{
	pipeline *p;
	const char *line;

	p = pipeline_new ();
	pipeline_command_args (p, "echo", "foo", (void *) 0);
	pipeline_command_args (p, "sed", "-e", "s/foo/bar/", (void *) 0);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	line = pipeline_readline (p);
	ck_assert_ptr_ne (line, NULL);
	ck_assert_msg (!strcmp (line, "bar\n"),
	               "Incorrect output from 'echo foo | sed -e s/foo/bar/': "
	               "'%s'",
	               line);
	ck_assert_msg (pipeline_wait (p) == 0,
	               "'echo foo | sed -e 's/foo/bar/' did not return 0");
	pipeline_free (p);
}
END_TEST

START_TEST (test_basic_wait_all)
{
	pipeline *p;
	int *statuses;
	int n_statuses;

	p = pipeline_new ();
	pipeline_command_args (p, SHELL, "-c", "exit 2", (void *) 0);
	pipeline_command_args (p, SHELL, "-c", "exit 3", (void *) 0);
	pipeline_command_args (p, "true", (void *) 0);
	pipeline_start (p);
	ck_assert_int_eq (pipeline_wait_all (p, &statuses, &n_statuses), 127);
	ck_assert_int_eq (n_statuses, 3);
	ck_assert_int_eq (statuses[0], 2 * 256);
	ck_assert_int_eq (statuses[1], 3 * 256);
	ck_assert_int_eq (statuses[2], 0);
	pipeline_free (p);
	free (statuses);
}
END_TEST

START_TEST (test_basic_setenv)
{
	pipeline *p;

	p = pipeline_new_command_args (SHELL, "-c", "exit $TEST1", (void *) 0);
	pipecmd_setenv (pipeline_get_command (p, 0), "TEST1", "10");
	ck_assert_int_eq (pipeline_run (p), 10);
}
END_TEST

START_TEST (test_basic_unsetenv)
{
	pipeline *p;

	setenv ("TEST2", "foo", 1);

	p = pipeline_new_command_args (SHELL, "-c", "echo $TEST2", (void *) 0);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	ck_assert_str_eq (pipeline_readline (p), "foo\n");
	pipeline_wait (p);
	pipeline_free (p);

	p = pipeline_new_command_args (SHELL, "-c", "echo $TEST2", (void *) 0);
	pipecmd_unsetenv (pipeline_get_command (p, 0), "TEST2");
	pipeline_want_out (p, -1);
	pipeline_start (p);
	ck_assert_str_eq (pipeline_readline (p), "\n");
	pipeline_wait (p);
	pipeline_free (p);
}
END_TEST

START_TEST (test_basic_clearenv)
{
	pipeline *p, *p2;

	setenv ("TEST3", "foo", 1);

	p = pipeline_new_command_args (SHELL, "-c", "echo $TEST3; echo $TEST4",
	                               (void *) 0);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	ck_assert_str_eq (pipeline_readline (p), "foo\n");
	ck_assert_str_eq (pipeline_readline (p), "\n");
	pipeline_wait (p);

	pipecmd_clearenv (pipeline_get_command (p, 0));
	pipeline_start (p);
	ck_assert_str_eq (pipeline_readline (p), "\n");
	ck_assert_str_eq (pipeline_readline (p), "\n");
	pipeline_wait (p);

	pipecmd_setenv (pipeline_get_command (p, 0), "TEST4", "bar");
	pipeline_start (p);
	ck_assert_str_eq (pipeline_readline (p), "\n");
	ck_assert_str_eq (pipeline_readline (p), "bar\n");
	pipeline_wait (p);

	p2 = pipeline_new ();
	pipeline_command (p2, pipecmd_dup (pipeline_get_command (p, 0)));
	pipeline_want_out (p2, -1);
	pipeline_start (p2);
	ck_assert_str_eq (pipeline_readline (p2), "\n");
	ck_assert_str_eq (pipeline_readline (p2), "bar\n");
	pipeline_wait (p2);
	pipeline_free (p2);
	pipeline_free (p);
}
END_TEST

START_TEST (test_basic_chdir)
{
	pipeline *p;
	const char *raw_line;
	char *line, *end;
	char *child_base, *expected_base;

	p = pipeline_new_command_args ("pwd", (void *) 0);
	pipecmd_chdir (pipeline_get_command (p, 0), temp_dir);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	raw_line = xstrdup (pipeline_readline (p));
	ck_assert_ptr_ne (raw_line, NULL);
	line = xstrdup (raw_line);
	end = line + strlen (line);
	if (end > line && *(end - 1) == '\n')
		*(end - 1) = '\0';
	child_base = base_name (line);
	expected_base = base_name (temp_dir);
	ck_assert_str_eq (child_base, expected_base);
	free (expected_base);
	free (child_base);
	free (line);
	pipeline_wait (p);
	pipeline_free (p);
}
END_TEST

START_TEST (test_basic_fchdir)
{
	pipeline *p;
	int temp_dir_fd;
	const char *raw_line;
	char *line, *end;
	char *child_base, *expected_base;

	p = pipeline_new_command_args ("pwd", (void *) 0);
	temp_dir_fd = open (temp_dir, O_RDONLY | O_DIRECTORY);
	ck_assert_int_ge (temp_dir_fd, 0);
	pipecmd_fchdir (pipeline_get_command (p, 0), temp_dir_fd);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	raw_line = xstrdup (pipeline_readline (p));
	ck_assert_ptr_ne (raw_line, NULL);
	line = xstrdup (raw_line);
	end = line + strlen (line);
	if (end > line && *(end - 1) == '\n')
		*(end - 1) = '\0';
	child_base = base_name (line);
	expected_base = base_name (temp_dir);
	ck_assert_str_eq (child_base, expected_base);
	free (expected_base);
	free (child_base);
	free (line);
	close (temp_dir_fd);
	pipeline_wait (p);
	pipeline_free (p);
}
END_TEST

/* This is of course better done using pipecmd_setenv, but setting an
 * environment variable makes for an easy test.
 */
static void pre_exec (void *data _GL_UNUSED)
{
	setenv ("TEST1", "10", 1);
}

START_TEST (test_basic_pre_exec)
{
	pipeline *p;

	p = pipeline_new_command_args (SHELL, "-c", "exit $TEST1", (void *) 0);
	pipecmd_pre_exec (pipeline_get_command (p, 0), pre_exec, NULL, NULL);
	ck_assert_msg (pipeline_run (p) == 10, "TEST1 not set properly");
}
END_TEST

START_TEST (test_basic_sequence)
{
	pipeline *p;
	pipecmd *cmd1, *cmd2, *cmd3, *seq;

	p = pipeline_new ();
	cmd1 = pipecmd_new_args ("echo", "foo", (void *) 0);
	cmd2 = pipecmd_new_args ("echo", "bar", (void *) 0);
	cmd3 = pipecmd_new_args ("echo", "baz", (void *) 0);
	seq = pipecmd_new_sequence ("echo*3", cmd1, cmd2, cmd3, (void *) 0);
	pipeline_command (p, seq);
	pipeline_command_args (p, "xargs", (void *) 0);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	ck_assert_str_eq (pipeline_readline (p), "foo bar baz\n");
	pipeline_wait (p);
	pipeline_free (p);
}
END_TEST

static Suite *basic_suite (void)
{
	Suite *s = suite_create ("Basic");

	TEST_CASE (s, basic, status);
	TEST_CASE (s, basic, args);
	TEST_CASE (s, basic, pipeline);
	TEST_CASE (s, basic, wait_all);
	TEST_CASE (s, basic, setenv);
	TEST_CASE (s, basic, unsetenv);
	TEST_CASE (s, basic, clearenv);
	TEST_CASE (s, basic, pre_exec);
	TEST_CASE_WITH_FIXTURE (s, basic, chdir, temp_dir_setup,
	                        temp_dir_teardown);
	TEST_CASE_WITH_FIXTURE (s, basic, fchdir, temp_dir_setup,
	                        temp_dir_teardown);
	TEST_CASE (s, basic, sequence);

	return s;
}

MAIN (basic)
