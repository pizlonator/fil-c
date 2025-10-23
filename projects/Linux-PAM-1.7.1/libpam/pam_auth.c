/*
 * pam_auth.c -- PAM authentication
 *
 * $Id$
 *
 */

#include "pam_private.h"
#include "pam_prelude.h"

#include <stdio.h>
#include <stdlib.h>

int pam_authenticate(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("called."));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    if (pamh->former.choice == PAM_NOT_STACKED) {
	_pam_sanitize(pamh);
	_pam_start_timer(pamh);    /* we try to make the time for a failure
				      independent of the time it takes to
				      fail */
    }

    retval = _pam_dispatch(pamh, flags, PAM_AUTHENTICATE);

    if (retval != PAM_INCOMPLETE) {
	_pam_sanitize(pamh);
	_pam_await_timer(pamh, retval);   /* if unsuccessful then wait now */
	D(("exiting"));
    } else {
	D(("will resume when ready"));
    }

#ifdef PRELUDE
    prelude_send_alert(pamh, retval);
#endif

    return retval;
}

int pam_setcred(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("called."));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    if (! flags) {
	flags = PAM_ESTABLISH_CRED;
    }

    retval = _pam_dispatch(pamh, flags, PAM_SETCRED);

    D(("exiting"));

    return retval;
}
