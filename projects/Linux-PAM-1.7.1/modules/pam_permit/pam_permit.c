/*
 * pam_permit module
 *
 * Written by Andrew Morgan <morgan@parc.power.net> 1996/3/11
 */

#include "config.h"
#include <stdio.h>

#include <security/pam_modules.h>
#include <security/_pam_macros.h>

#define DEFAULT_USER "nobody"

/* --- authentication management functions --- */

int
pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
		    int argc UNUSED, const char **argv UNUSED)
{
    int retval;
    const char *user=NULL;

    /*
     * authentication requires we know who the user wants to be
     */
    retval = pam_get_user(pamh, &user, NULL);
    if (retval != PAM_SUCCESS) {
	D(("get user returned error: %s", pam_strerror(pamh,retval)));
	return retval;
    }
    if (*user == '\0') {
	D(("username not known"));
	retval = pam_set_item(pamh, PAM_USER, (const void *) DEFAULT_USER);
	if (retval != PAM_SUCCESS)
	    return PAM_USER_UNKNOWN;
    }
    user = NULL;                                            /* clean up */

    return PAM_SUCCESS;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
	       int argc UNUSED, const char **argv UNUSED)
{
     return PAM_SUCCESS;
}

/* --- account management functions --- */

int
pam_sm_acct_mgmt(pam_handle_t *pamh UNUSED, int flags UNUSED,
		 int argc UNUSED, const char **argv UNUSED)
{
     return PAM_SUCCESS;
}

/* --- password management --- */

int
pam_sm_chauthtok(pam_handle_t *pamh UNUSED, int flags UNUSED,
		 int argc UNUSED, const char **argv UNUSED)
{
     return PAM_SUCCESS;
}

/* --- session management --- */

int
pam_sm_open_session(pam_handle_t *pamh UNUSED, int flags UNUSED,
		    int argc UNUSED, const char **argv UNUSED)
{
    return PAM_SUCCESS;
}

int
pam_sm_close_session(pam_handle_t *pamh UNUSED, int flags UNUSED,
		     int argc UNUSED, const char **argv UNUSED)
{
     return PAM_SUCCESS;
}

/* end of module definition */
