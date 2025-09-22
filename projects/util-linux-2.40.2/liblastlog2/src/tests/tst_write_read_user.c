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
   Create an entry, rename that entry, and try to read the old and
   new entry again. Reading the old entry should fail.
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "lastlog2P.h"

static int
test_args (struct ll2_context *context, const char *user, int64_t ll_time,
	   const char *tty, const char *rhost, const char *service)
{
	char *error = NULL;
	int64_t res_time;
	char *res_tty = NULL;
	char *res_rhost = NULL;
	char *res_service = NULL;

	if (ll2_write_entry (context, user, ll_time, tty, rhost, service, &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		} else
			fprintf (stderr, "ll2_write_entry failed\n");
		return 1;
	}

	if (ll2_read_entry (context, user, &res_time, &res_tty, &res_rhost, &res_service, &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		} else
			fprintf (stderr, "Unknown error reading database %s", context->lastlog2_path);
		return 1;
	}

	if (ll_time != res_time) {
		fprintf (stderr, "Wrong time: got %lld, expect %lld\n",
			 (long long int)res_time, (long long int)ll_time);
		return 1;
	}

	if ((tty == NULL && res_tty != NULL) ||
	    (tty != NULL && res_tty == NULL) ||
	    (tty != NULL && res_tty != NULL && strcmp (tty, res_tty) != 0)) {
		fprintf (stderr, "Wrong tty: got %s, expect %s\n", tty, res_tty);
		return 1;
	}

	if ((rhost == NULL && res_rhost != NULL) ||
	    (rhost != NULL && res_rhost == NULL) ||
	    (rhost != NULL && res_rhost != NULL && strcmp (rhost, res_rhost) != 0)) {
		fprintf (stderr, "Wrong rhost: got %s, expect %s\n", rhost, res_rhost);
		return 1;
	}

	if ((service == NULL && res_service != NULL) ||
	    (service != NULL && res_service == NULL) ||
	    (service != NULL && res_service != NULL && strcmp (service, res_service) != 0)) {
		fprintf (stderr, "Wrong service: got %s, expect %s\n", service, res_service);
		return 1;
	}


	free (res_tty);
	free (res_rhost);
	free (res_service);

	return 0;
}

int
main(void)
{
	struct ll2_context *context = ll2_new_context("tst-write-read-user.db");
	char *error = NULL;
	int64_t res_time;
	char *res_tty = NULL;
	char *res_rhost = NULL;
	char *res_service = NULL;

	if (test_args (context, "user1", time (NULL), "test-tty", "localhost", "test") != 0) {
		ll2_unref_context(context);
		return 1;
	}
	if (test_args (context, "user2", 0, NULL, NULL, NULL) != 0) {
		ll2_unref_context(context);
		return 1;
	}
	if (test_args (context, "user3", time (NULL), NULL, NULL, NULL) != 0) {
		ll2_unref_context(context);
		return 1;
	}
	if (test_args (context, "user4", time (NULL), "test-tty", NULL, NULL) != 0) {
		ll2_unref_context(context);
		return 1;
	}
	if (test_args (context, "user5", time (NULL), NULL, "localhost", NULL) != 0) {
		ll2_unref_context(context);
		return 1;
	}

	/* Checking errno if the db file does not exist */
	struct ll2_context *context_not_found = ll2_new_context("no_file");
	if (ll2_read_entry (context_not_found, "user", &res_time, &res_tty, &res_rhost, &res_service, &error) != 0) {
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
				ll2_unref_context(context_not_found);
				return 0;
			}
			fprintf (stderr, "errno: %s\n",
				 strerror (errno));
		} else {
			fprintf (stderr, "errno: NULL\n");
		}

		ll2_unref_context(context_not_found);
		ll2_unref_context(context);
		return 1;
	}

	ll2_unref_context(context);
	return 0;
}
