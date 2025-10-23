/*
 * pam_canonicalize_user - get user name and canonicalize it
 *
 * Copyright (c) 2023 Dmitry V. Levin <ldv@strace.io>
 *
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
 * the GNU Public License, in which case the provisions of the GPL
 * are required INSTEAD OF the above restrictions.  (This clause is
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

#include "config.h"
#include <string.h>
#include <syslog.h>

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>

int
pam_sm_authenticate(pam_handle_t *pamh UNUSED, int flags UNUSED,
		    int argc UNUSED, const char **argv UNUSED)
{
	const char *user;
	int rc = pam_get_user(pamh, &user, NULL);
	if (rc != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_NOTICE, "cannot determine user name: %s",
			   pam_strerror(pamh, rc));
		return rc == PAM_CONV_AGAIN ? PAM_INCOMPLETE : rc;
	}

	struct passwd *pw = pam_modutil_getpwnam(pamh, user);
	if (!pw) {
		pam_syslog(pamh, LOG_NOTICE, "user unknown");
		return PAM_USER_UNKNOWN;
	}

	if (strcmp(user, pw->pw_name) == 0)
		return PAM_IGNORE;

	rc = pam_set_item(pamh, PAM_USER, pw->pw_name);
	return rc == PAM_SUCCESS ? PAM_IGNORE : rc;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
	       int argc UNUSED, const char **argv UNUSED)
{
	return PAM_IGNORE;
}
