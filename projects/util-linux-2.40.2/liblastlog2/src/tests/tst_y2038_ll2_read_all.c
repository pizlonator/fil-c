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
   Create an entry with an 3*INT32_MAX timestamp, store that,
   read that via ll2_read_all callback again and make sure the
   timestamp is correct.
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "lastlog2P.h"

#define BIG_TIME_VALUE ((int64_t)3*INT32_MAX)

const char *user = "y2038";
const char *on_tty = "pts/test";
const char *rhost = NULL;
const char *service = "sshd";

static int
check_y2038 (const char *res_user, int64_t ll_time, const char *res_tty,
	     const char *res_rhost, const char *res_service, const char *error)
{

	if (strcmp (user, res_user) != 0) {
		fprintf (stderr, "write/read entry user mismatch: written: %s, got: %s\n",
			 user, res_user);
		exit (1);
	}

	if (ll_time != BIG_TIME_VALUE) {
		fprintf (stderr, "write/read entry time mismatch: written: %lld, got: %lld\n",
			 (long long int)BIG_TIME_VALUE, (long long int)ll_time);
		exit (1);
	}

	if (strcmp (on_tty, res_tty) != 0) {
		fprintf (stderr, "write/read entry tty mismatch: written: %s, got: %s\n",
			 on_tty, res_tty);
		exit (1);
	}

	if (rhost != NULL) {
		fprintf (stderr, "write/read entry rhost mismatch: written: NULL, got: %s\n",
			 res_rhost);
		exit (1);
	}

	if (strcmp (service, res_service) != 0) {
		fprintf (stderr, "write/read entry service mismatch: written: %s, got: %s\n",
			 service, res_service);
		exit (1);
	}

	if (error != NULL) {
		fprintf (stderr, "got error: %s\n",
			 error);
		exit (1);
	}

	return 0;
}

int
main(void)
{
	struct ll2_context *context = ll2_new_context("y2038-ll2_read_all.db");
	char *error = NULL;

	remove (context->lastlog2_path);

	printf ("Big time value is: %lld\n", (long long int)BIG_TIME_VALUE);

	if (ll2_write_entry (context, user, BIG_TIME_VALUE, on_tty, rhost, service,
			     &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		}
		else
			fprintf (stderr, "ll2_write_entry failed\n");
		ll2_unref_context(context);
		return 1;
	}

	if (ll2_read_all (context, check_y2038, &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		} else
			fprintf (stderr, "Couldn't read entries for all users\n");
		ll2_unref_context(context);
		return 1;
	}

	/* Checking errno if the db file does not exist */
	remove (context->lastlog2_path);

	if (ll2_read_all (context, check_y2038, &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		} else
			fprintf (stderr, "Couldn't read entries for all users\n");

		if(errno) {
			if (errno == ENOENT)
			{
				fprintf (stderr, "Returning the correct errno: %s\n",
					 strerror (errno));
				ll2_unref_context(context);
				return 0;
			}
			fprintf (stderr, "errno: %s\n",
				 strerror (errno));
		} else {
			fprintf (stderr, "errno: NULL\n");
		}

		ll2_unref_context(context);
		return 1;
	}

	ll2_unref_context(context);
	return 0;
}
