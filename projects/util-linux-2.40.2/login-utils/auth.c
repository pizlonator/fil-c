/*
 *   auth.c -- PAM authorization code, common between chsh and chfn
 *   (c) 2012 by Cody Maloney <cmaloney@theoreticalchaos.com>
 *
 *   this program is free software.  you can redistribute it and
 *   modify it under the terms of the gnu general public license.
 *   there is no warranty.
 *
 */

#include <security/pam_appl.h>
#ifdef HAVE_SECURITY_PAM_MISC_H
# include <security/pam_misc.h>
#elif defined(HAVE_SECURITY_OPENPAM_H)
# include <security/openpam.h>
#endif

#include "c.h"
#include "auth.h"

static int pam_fail_check(pam_handle_t *pamh, int retcode)
{
	if (retcode == PAM_SUCCESS)
		return 0;
	warnx("%s", pam_strerror(pamh, retcode));
	pam_end(pamh, retcode);
	return 1;
}

int auth_pam(const char *service_name, uid_t uid, const char *username)
{
	if (uid != 0) {
		pam_handle_t *pamh = NULL;
#ifdef HAVE_SECURITY_PAM_MISC_H
		struct pam_conv conv = { misc_conv, NULL };
#elif defined(HAVE_SECURITY_OPENPAM_H)
		struct pam_conv conv = { openpam_ttyconv, NULL };
#endif
		int retcode;

		retcode = pam_start(service_name, username, &conv, &pamh);
		if (pam_fail_check(pamh, retcode))
			return FALSE;

		retcode = pam_authenticate(pamh, 0);
		if (pam_fail_check(pamh, retcode))
			return FALSE;

		retcode = pam_acct_mgmt(pamh, 0);
		if (retcode == PAM_NEW_AUTHTOK_REQD)
			retcode =
			    pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
		if (pam_fail_check(pamh, retcode))
			return FALSE;

		retcode = pam_setcred(pamh, 0);
		if (pam_fail_check(pamh, retcode))
			return FALSE;

		pam_end(pamh, 0);
		/* no need to establish a session; this isn't a
		 * session-oriented activity...  */
	}
	return TRUE;
}
