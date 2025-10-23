/*
 * pam_unix session management
 *
 * Copyright Alexander O. Yuriev, 1996.  All rights reserved.
 * Copyright Jan Rękorajski, 1999.  All rights reserved.
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>

#include "pam_inline.h"
#include "support.h"

/*
 * PAM framework looks for these entry-points to pass control to the
 * session module.
 */

int
pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	const char *user_name, *service;
	unsigned long long ctrl;
	int retval;
	const char *login_name;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, NULL, argc, argv);

	retval = pam_get_item(pamh, PAM_USER, (const void **) &user_name);
	if (user_name == NULL || *user_name == '\0' || retval != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_ERR,
			"open_session - error recovering username");
		return PAM_SESSION_ERR;		/* How did we get authenticated with
						   no username?! */
	}
	retval = pam_get_item(pamh, PAM_SERVICE, (const void **) &service);
	if (service == NULL || *service == '\0' || retval != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_CRIT,
			"open_session - error recovering service");
		return PAM_SESSION_ERR;
	}
	login_name = pam_modutil_getlogin(pamh);
	if (login_name == NULL) {
		login_name = "";
	}
	if (off (UNIX_QUIET, ctrl)) {
		char uid[32];
		struct passwd *pwd = pam_modutil_getpwnam (pamh, user_name);
		if (pwd == NULL) {
			pam_sprintf(uid, "getpwnam error");
		}
		else {
			pam_sprintf(uid, "%u", pwd->pw_uid);
		}
		pam_syslog(pamh, LOG_INFO, "session opened for user %s(uid=%s) by %s(uid=%lu)", user_name, uid, login_name, (unsigned long)getuid());
	}
	return PAM_SUCCESS;
}

int
pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	const char *user_name, *service;
	unsigned long long ctrl;
	int retval;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, NULL, argc, argv);

	retval = pam_get_item(pamh, PAM_USER, (const void **) &user_name);
	if (user_name == NULL || *user_name == '\0' || retval != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_ERR,
			"close_session - error recovering username");
		return PAM_SESSION_ERR;		/* How did we get authenticated with
						   no username?! */
	}
	retval = pam_get_item(pamh, PAM_SERVICE, (const void **) &service);
	if (service == NULL || *service == '\0' || retval != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_CRIT,
			"close_session - error recovering service");
		return PAM_SESSION_ERR;
	}
	if (off (UNIX_QUIET, ctrl))
		pam_syslog(pamh, LOG_INFO, "session closed for user %s",
			user_name);

	return PAM_SUCCESS;
}
