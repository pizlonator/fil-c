/*
 * pam_unix authentication management
 *
 * Copyright Alexander O. Yuriev, 1996.  All rights reserved.
 * NIS+ support by Thorsten Kukuk <kukuk@weber.uni-paderborn.de>
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include "support.h"

/*
 * PAM framework looks for these entry-points to pass control to the
 * authentication module.
 */

/* Fun starts here :)

 * pam_sm_authenticate() performs UNIX/shadow authentication
 *
 *      First, if shadow support is available, attempt to perform
 *      authentication using shadow passwords. If shadow is not
 *      available, or user does not have a shadow password, fallback
 *      onto a normal UNIX authentication
 */

#define AUTH_RETURN						\
do {									\
	D(("recording return code for next time [%d]",		\
				retval));			\
	*ret_data = retval;					\
	pam_set_data(pamh, "unix_setcred_return",		\
			 (void *) ret_data, setcred_free);	\
	D(("done. [%s]", pam_strerror(pamh, retval)));		\
	return retval;						\
} while (0)


static void
setcred_free (pam_handle_t *pamh UNUSED, void *ptr, int err UNUSED)
{
	free (ptr);
}

int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	unsigned long long ctrl;
	int retval, *ret_data = NULL;
	const char *name;
	const char *p;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, NULL, argc, argv);

	/* Get a few bytes so we can pass our return value to
	   pam_sm_setcred() and pam_sm_acct_mgmt(). */
	ret_data = malloc(sizeof(int));
	if (!ret_data) {
		D(("cannot malloc ret_data"));
		pam_syslog(pamh, LOG_CRIT,
				"pam_unix_auth: cannot allocate ret_data");
		return PAM_BUF_ERR;
	}

	/* get the user'name' */

	retval = pam_get_user(pamh, &name, NULL);
	if (retval == PAM_SUCCESS) {
		/*
		 * Various libraries at various times have had bugs related to
		 * '+' or '-' as the first character of a user name. Don't
		 * allow these characters here.
		 */
		if (name[0] == '-' || name[0] == '+') {
			pam_syslog(pamh, LOG_NOTICE, "bad username [%s]", name);
			retval = PAM_USER_UNKNOWN;
			AUTH_RETURN;
		}
		if (on(UNIX_DEBUG, ctrl))
			pam_syslog(pamh, LOG_DEBUG, "username [%s] obtained", name);
	} else {
		if (retval == PAM_CONV_AGAIN) {
			D(("pam_get_user/conv() function is not ready yet"));
			/* it is safe to resume this function so we translate this
			 * retval to the value that indicates we're happy to resume.
			 */
			retval = PAM_INCOMPLETE;
		} else if (on(UNIX_DEBUG, ctrl)) {
			pam_syslog(pamh, LOG_DEBUG, "could not obtain username");
		}
		AUTH_RETURN;
	}

	/* if this user does not have a password... */

	if (_unix_blankpasswd(pamh, ctrl, name)) {
		pam_syslog(pamh, LOG_DEBUG, "user [%s] has blank password; authenticated without it", name);
		name = NULL;
		retval = PAM_SUCCESS;
		AUTH_RETURN;
	}
	/* get this user's authentication token */

	retval = pam_get_authtok(pamh, PAM_AUTHTOK, &p , NULL);
	if (retval != PAM_SUCCESS) {
		if (retval != PAM_CONV_AGAIN) {
			pam_syslog(pamh, LOG_CRIT,
			    "auth could not identify password for [%s]", name);
		} else {
			D(("conversation function is not ready yet"));
			/*
			 * it is safe to resume this function so we translate this
			 * retval to the value that indicates we're happy to resume.
			 */
			retval = PAM_INCOMPLETE;
		}
		name = NULL;
		AUTH_RETURN;
	}
	D(("user=%s, password=[%s]", name, p));

	/* verify the password of this user */
	retval = _unix_verify_password(pamh, name, p, ctrl);
	name = p = NULL;

	AUTH_RETURN;
}


/*
 * The only thing _pam_set_credentials_unix() does is initialization of
 * UNIX group IDs.
 *
 * Well, everybody but me on linux-pam is convinced that it should not
 * initialize group IDs, so I am not doing it but don't say that I haven't
 * warned you. -- AOY
 */

int
pam_sm_setcred (pam_handle_t *pamh, int flags,
		int argc, const char **argv)
{
	int retval;
	const void *pretval = NULL;
	unsigned long long ctrl;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, NULL, argc, argv);

	retval = PAM_SUCCESS;

	D(("recovering return code from auth call"));
	/* We will only find something here if UNIX_LIKE_AUTH is set --
	   don't worry about an explicit check of argv. */
	if (on(UNIX_LIKE_AUTH, ctrl)
	    && pam_get_data(pamh, "unix_setcred_return", &pretval) == PAM_SUCCESS
	    && pretval) {
	        retval = *(const int *)pretval;
		pam_set_data(pamh, "unix_setcred_return", NULL, NULL);
		D(("recovered data indicates that old retval was %d", retval));
	}

	return retval;
}
