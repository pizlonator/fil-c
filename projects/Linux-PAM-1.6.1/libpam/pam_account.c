/* pam_account.c - PAM Account Management */

#include "pam_private.h"

#include <stdio.h>

int pam_acct_mgmt(pam_handle_t *pamh, int flags)
{
    int retval;

    D(("called"));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

    retval = _pam_dispatch(pamh, flags, PAM_ACCOUNT);

    return retval;
}
