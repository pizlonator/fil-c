/*
 * Copyright (C) 2010, 2012 Colin Watson.
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xalloc.h"
#include "xvasprintf.h"

#include "common.h"

const char *program_name = "redirect";

START_TEST (test_redirect_files)
{
	char *template = xstrdup ("testtmp.XXXXXX");
	int fd;
	FILE *fh;
	pipeline *p;

	fd = mkstemp (template);
	if (fd < 0) {
		ck_abort_msg ("mkstemp failed: %s", strerror (errno));
		return;
	}
	fh = fdopen (fd, "w");
	fprintf (fh, "test data\n");
	fflush (fh);

	p = pipeline_new_command_args ("sed", "-e", "s/$/ out/", (void *) 0);
	pipeline_want_infile (p, template);
	pipeline_want_out (p, -1);
	pipeline_start (p);
	ck_assert_str_eq (pipeline_readline (p), "test data out\n");

	fclose (fh);
	unlink (template);

	pipeline_free (p);
	free (template);
}
END_TEST

START_TEST (test_redirect_outfile)
{
	pipeline *p;
	char *outfile;
	FILE *fh;
	char line[5];

	p = pipeline_new_command_args ("echo", "test", (void *) 0);
	outfile = xasprintf ("%s/test", temp_dir);
	pipeline_want_outfile (p, outfile);
	ck_assert_int_eq (pipeline_run (p), 0);
	fh = fopen (outfile, "r");
	ck_assert_ptr_ne (fh, NULL);
	ck_assert_ptr_ne (fgets (line, 5, fh), NULL);
	ck_assert_str_eq (line, "test");

	fclose (fh);
	free (outfile);
}
END_TEST

static Suite *redirect_suite (void)
{
	Suite *s = suite_create ("Redirect");

	TEST_CASE (s, redirect, files);
	TEST_CASE_WITH_FIXTURE (s, redirect, outfile, temp_dir_setup,
	                        temp_dir_teardown);

	return s;
}

MAIN (redirect)
