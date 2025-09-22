/*
 * Copyright (C) 2010 Colin Watson.
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
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "error.h"

#include "common.h"

const char *program_name = "inspect";

START_TEST (test_inspect_command)
{
	pipecmd *cmd;
	char *str;

	cmd = pipecmd_new ("foo");
	str = pipecmd_tostring (cmd);
	ck_assert_str_eq (str, "foo");
	free (str);
	pipecmd_free (cmd);

	cmd = pipecmd_new_args ("foo", "bar", "baz quux", (void *) 0);
	str = pipecmd_tostring (cmd);
	/* TODO: not ideal representation of commands with metacharacters */
	ck_assert_str_eq (str, "foo bar baz quux");
	free (str);
	pipecmd_free (cmd);
}
END_TEST

START_TEST (test_inspect_pipeline)
{
	pipeline *p;
	char *str;

	p = pipeline_new ();
	pipeline_command_args (p, "foo", "bar", (void *) 0);
	pipeline_command_args (p, "grep", "baz", "quux", (void *) 0);
	ck_assert_int_eq (pipeline_get_ncommands (p), 2);
	pipecmd_setenv (pipeline_get_command (p, 1), "KEY", "value");
	str = pipeline_tostring (p);
	ck_assert_str_eq (str, "foo bar | KEY=value grep baz quux");
	free (str);
	pipeline_free (p);
}
END_TEST

static void pid_helper (void *data _GL_UNUSED)
{
	struct sigaction sa;

	/* Get rid of check's SIGTERM handler. */
	memset (&sa, 0, sizeof sa);
	sa.sa_handler = SIG_DFL;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction (SIGTERM, &sa, NULL) == -1)
		error (1, errno, "can't install SIGTERM handler");

	printf ("%ld\n", (long) getpid ());
	fflush (stdout);
	pause ();
}

START_TEST (test_inspect_pid)
{
	pipeline *p;
	pipecmd *cmd;
	const char *line;
	pid_t pid;

	p = pipeline_new ();
	cmd = pipecmd_new_function ("pid_helper", pid_helper, NULL, NULL);
	pipeline_command (p, cmd);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	line = pipeline_readline (p);
	ck_assert_ptr_ne (line, NULL);
	pid = (pid_t) atol (line);
	ck_assert_msg (pid == pipeline_get_pid (p, 0), "pids match");
	/* Note that this test may hang if pipeline_get_pid does not work.
	 * We might be able to fix this by calling setsid at the start of
	 * the test and then killing the process group, but I'm not sure if
	 * that's sufficiently portable.
	 */
	if (pid == pipeline_get_pid (p, 0)) {
		int status;

		/* Suppress "Terminated" errors. */
		setenv ("PIPELINE_QUIET", "1", 1);
		kill (pid, SIGTERM);
		status = pipeline_wait (p);

		ck_assert_msg (status == 128 + SIGTERM,
		               "pid_helper did not indicate SIGTERM");
	}
	pipeline_free (p);
}
END_TEST

static Suite *inspect_suite (void)
{
	Suite *s = suite_create ("Inspect");

	TEST_CASE (s, inspect, command);
	TEST_CASE (s, inspect, pipeline);
	TEST_CASE (s, inspect, pid);

	return s;
}

MAIN (inspect)
