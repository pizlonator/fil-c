/*
 * Copyright (C) 2013 Peter Schiffer.
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "xalloc.h"
#include "xvasprintf.h"

#include "common.h"

const char *program_name = "read";

/* Must be 8194 or bigger */
#define RANDOM_STR_LEN 9000

/* Unit test for bug: https://bugzilla.redhat.com/show_bug.cgi?id=876108 */
START_TEST (test_read_long_line)
{
	/* Generate long random string */
	static const char *alphanum = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	                              "abcdefghijklmnopqrstuvwxyz";
	char random_string[RANDOM_STR_LEN] = "";
	unsigned i;

	for (i = 0; i < RANDOM_STR_LEN - 2; i++) {
		random_string[i] = alphanum[rand () % (strlen (alphanum) - 1)];
	}
	random_string[RANDOM_STR_LEN - 1] = '\0';

	/* Write the random string to file */
	char *testfile = xasprintf ("%s/test", temp_dir);
	FILE *tfd = fopen (testfile, "w");
	if (!tfd) {
		ck_abort_msg ("fopen failed: %s", strerror (errno));
		return;
	}
	fprintf (tfd, "%s\n", random_string);
	fclose (tfd);

	char *expected_output = xasprintf ("%s\n", random_string);

	pipeline *p;
	const char *line;
	char *read_result = NULL, *temp = NULL;

	/* File must be read twice to reproduce the bug */
	p = pipeline_new ();
	pipeline_want_infile (p, testfile);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	while ((line = pipeline_readline (p)) != NULL) {
		if (read_result) {
			temp = read_result;
			read_result = xasprintf ("%s%s", read_result, line);
			free (temp);
		} else {
			read_result = xasprintf ("%s", line);
		}
	}
	pipeline_free (p);
	ck_assert_str_eq (read_result, expected_output);

	free (read_result);
	read_result = NULL;

	p = pipeline_new ();
	pipeline_want_infile (p, testfile);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	while ((line = pipeline_readline (p)) != NULL) {
		if (read_result) {
			temp = read_result;
			read_result = xasprintf ("%s%s", read_result, line);
			free (temp);
		} else {
			read_result = xasprintf ("%s", line);
		}
	}
	pipeline_free (p);
	ck_assert_str_eq (read_result, expected_output);

	free (testfile);
	free (expected_output);
	free (read_result);
}
END_TEST

/* Write the first character of a string quickly, followed by the rest of it
 * a little later.
 */
static void slow_line_helper (void *data)
{
	const char *s = data;
	struct timeval timeout;

	setbuf (stdout, NULL);
	putchar (s[0]);
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;
	select (0, NULL, NULL, NULL, &timeout);
	fputs (s + 1, stdout);
}

START_TEST (test_read_readline_slow)
{
	pipeline *p;
	pipecmd *cmd;

	p = pipeline_new ();
	cmd = pipecmd_new_function ("slow_line_helper", slow_line_helper, free,
	                            xstrdup ("a line\nsome more"));
	pipeline_command (p, cmd);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	ck_assert_str_eq (pipeline_readline (p), "a line\n");
	pipeline_free (p);
}
END_TEST

static Suite *read_suite (void)
{
	Suite *s = suite_create ("Read");

	TEST_CASE_WITH_FIXTURE (s, read, long_line, temp_dir_setup,
	                        temp_dir_teardown);
	TEST_CASE (s, read, readline_slow);

	return s;
}

MAIN (read)
