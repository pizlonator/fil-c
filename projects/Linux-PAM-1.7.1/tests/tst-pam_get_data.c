/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "test_assert.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <pam_private.h>

static void
tst_str_data_cleanup(pam_handle_t *pamh UNUSED, void *data, int error_status)
{
	const char *q = data ? "\"" : "";
	fprintf(stderr,
		"tst_cleanup was called: data=%s%s%s, error_status=%d\n",
		q, data ? (char *) data : "NULL", q, error_status);
	free(data);
}

int
main(void)
{
	const char *service = "dummy";
	const char *user = "root";
	struct pam_conv conv = { NULL, NULL };
	pam_handle_t *pamh;
	void *dataptr;
	const void *constdataptr;

	/* 1: Call with NULL as pam handle */
	ASSERT_NE(PAM_SUCCESS,
		  pam_get_data(NULL, "tst-pam_get_data-1", &constdataptr));

	/* setup pam handle */
	ASSERT_EQ(PAM_SUCCESS, pam_start(service, user, &conv, &pamh));

	/* 2: check for call from application */
	ASSERT_EQ(PAM_SYSTEM_ERR,
		  pam_get_data(pamh, "tst-pam_get_data-2", &constdataptr));

	/* 3: Check that pam data is properly set and replaced */
	__PAM_TO_MODULE(pamh);

	ASSERT_NE(NULL, dataptr = strdup("test3a"));
	ASSERT_EQ(PAM_SUCCESS,
		  pam_set_data(pamh, "tst-pam_get_data-3", dataptr,
			       tst_str_data_cleanup));
	ASSERT_EQ(PAM_SUCCESS,
		  pam_get_data(pamh, "tst-pam_get_data-3", &constdataptr));
	ASSERT_EQ(dataptr, constdataptr);
	ASSERT_EQ(0, strcmp((const char *) constdataptr, "test3a"));

	ASSERT_NE(NULL, dataptr = strdup("test3b"));
	ASSERT_EQ(PAM_SUCCESS,
		  pam_set_data(pamh, "tst-pam_get_data-3", dataptr,
			       tst_str_data_cleanup));
	ASSERT_EQ(PAM_SUCCESS,
		  pam_get_data(pamh, "tst-pam_get_data-3", &constdataptr));
	ASSERT_EQ(dataptr, constdataptr);
	ASSERT_EQ(0, strcmp((const char *) constdataptr, "test3b"));

	ASSERT_EQ(PAM_SUCCESS,
		  pam_set_data(pamh, "tst-pam_get_data-3", NULL,
			       tst_str_data_cleanup));
	ASSERT_EQ(PAM_SUCCESS,
		  pam_get_data(pamh, "tst-pam_get_data-3", &constdataptr));
	ASSERT_EQ(NULL, constdataptr);

	ASSERT_EQ(PAM_NO_MODULE_DATA,
		  pam_get_data(pamh, "tst-pam_get_data-4", &constdataptr));

	__PAM_TO_APP(pamh);

	ASSERT_EQ(PAM_SUCCESS, pam_end(pamh, 987));

	return 0;
}
