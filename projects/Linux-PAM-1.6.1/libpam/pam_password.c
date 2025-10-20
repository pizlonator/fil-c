/* pam_password.c - PAM Password Management */

/*
 * $Id$
 */

#include "pam_private.h"

#include <stdio.h>
#include <stdlib.h>

int pam_chauthtok(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("called."));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    /* applications are not allowed to set these flags */
    if (flags & (PAM_PRELIM_CHECK | PAM_UPDATE_AUTHTOK)) {
      pam_syslog (pamh, LOG_ERR,
		  "PAM_PRELIM_CHECK or PAM_UPDATE_AUTHTOK set by application");
      return PAM_SYSTEM_ERR;
    }

    if (pamh->former.choice == PAM_NOT_STACKED) {
	_pam_start_timer(pamh);    /* we try to make the time for a failure
				      independent of the time it takes to
				      fail */
	_pam_sanitize(pamh);
	pamh->former.update = PAM_FALSE;
    }

    /* first call to check if there will be a problem */
    if (pamh->former.update ||
	(retval = _pam_dispatch(pamh, flags|PAM_PRELIM_CHECK,
				PAM_CHAUTHTOK)) == PAM_SUCCESS) {
	D(("completed check ok: former=%d", pamh->former.update));
	pamh->former.update = PAM_TRUE;
	retval = _pam_dispatch(pamh, flags|PAM_UPDATE_AUTHTOK,
			       PAM_CHAUTHTOK);
    }

    /* if we completed we should clean up */
    if (retval != PAM_INCOMPLETE) {
	_pam_sanitize(pamh);
	pamh->former.update = PAM_FALSE;
	_pam_await_timer(pamh, retval);   /* if unsuccessful then wait now */
	D(("exiting %d - %d", retval, pamh->former.choice));
    } else {
	D(("will resume when ready"));
    }

    return retval;
}
