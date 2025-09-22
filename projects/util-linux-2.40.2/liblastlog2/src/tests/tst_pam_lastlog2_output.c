/* SPDX-License-Identifier: BSD-2-Clause

  Copyright (c) 2023, Thorsten Kukuk <kukuk@suse.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

/* Test case:
   Store defined data into the database, read it, create the time
   string like pam_lastlog2 does, compare the result.
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "lastlog2P.h"

const char *expected = "Last login: Mon Mar 13 07:13:41 UTC 2023 from 192.168.122.1 on pts/0";
const time_t login_time = 1678691621;
int
main(void)
{
	const char *user = "root";
	struct ll2_context *context = ll2_new_context("pam_lastlog2-output.db");
	int64_t ll_time = 0;
	char *tty = NULL;
	char *rhost = NULL;
	char *date = NULL;
	char the_time[256];
	char *error = NULL;
	char *output = NULL;

	if (ll2_write_entry (context, user, login_time, "pts/0",
		       "192.168.122.1", NULL, &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		} else
			fprintf (stderr, "ll2_write_entry failed\n");
		ll2_unref_context(context);
		return 1;
	}

	if (ll2_read_entry (context, user, &ll_time, &tty, &rhost,
			    NULL, &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		} else
			fprintf (stderr, "Unknown error reading database %s", context ? context->lastlog2_path : "NULL");
		ll2_unref_context(context);
		return 1;
	}

	if (ll_time) {
		struct tm *tm, tm_buf;
		/* this is necessary if you compile this on architectures with
		   a 32bit time_t type. */
		time_t t_time = ll_time;

		if ((tm = localtime_r (&t_time, &tm_buf)) != NULL) {
			strftime (the_time, sizeof (the_time),
				  " %a %b %e %H:%M:%S %Z %Y", tm);
			date = the_time;
		}
	}

	if (asprintf (&output, "Last login:%s%s%s%s%s",
		      date ? date : "",
		      rhost ? " from " : "",
		      rhost ? rhost : "",
		      tty ? " on " : "",
		      tty ? tty : "") < 0) {
		fprintf (stderr, "Out of memory!\n");
		ll2_unref_context(context);
		return 1;
	}

	if (strcmp (output, expected) != 0) {
		fprintf (stderr, "Output '%s'\n does not match '%s'\n",
			 output, expected);
		ll2_unref_context(context);
		return 1;
	}

	ll2_unref_context(context);
	free (output);
	free (tty);
	free (rhost);

	return 0;
}
