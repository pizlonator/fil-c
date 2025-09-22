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
   Create an entry with an INT64_MAX-1000 timestamp, store that,
   read that again and make sure the timestamp is correct.
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "lastlog2P.h"

#define BIG_TIME_VALUE (INT64_MAX - 1000)

int
main(void)
{
	const char *user = "y2038";
	struct ll2_context *context = ll2_new_context("y2038-sqlite3-time.db");
	int64_t ll_time = 0;
	char *error = NULL;

	printf ("Big time value is: %lld\n", (long long int)BIG_TIME_VALUE);

	if (ll2_write_entry (context, user, BIG_TIME_VALUE, NULL, NULL,
			     NULL, &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		} else
			fprintf (stderr, "ll2_write_entry failed\n");
		ll2_unref_context(context);
		return 1;
	}

	if (ll2_read_entry (context, user, &ll_time, NULL, NULL, NULL,
			    &error) != 0) {
		if (error) {
			fprintf (stderr, "%s\n", error);
			free (error);
		} else
			fprintf (stderr, "Unknown error reading database %s", context->lastlog2_path);
		ll2_unref_context(context);
		return 1;
	}

	if (ll_time != BIG_TIME_VALUE) {
		fprintf (stderr, "write/read entry time mismatch: written: %lld, got: %lld\n",
			 (long long int)BIG_TIME_VALUE, (long long int)ll_time);
		ll2_unref_context(context);
		return 1;
	}

	ll2_unref_context(context);
	return 0;
}
