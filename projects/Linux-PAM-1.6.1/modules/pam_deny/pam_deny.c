/*
 * pam_deny module
 *
 * Written by Andrew Morgan <morgan@parc.power.net> 1996/3/11
 */

#include "config.h"
#include <security/pam_modules.h>

/* --- authentication management functions --- */

int
pam_sm_authenticate(pam_handle_t *pamh UNUSED, int flags UNUSED,
		    int argc UNUSED, const char **argv UNUSED)
{
     return PAM_AUTH_ERR;
}

int
pam_sm_setcred(pam_handle_t *pamh UNUSED, int flags UNUSED,
	       int argc UNUSED, const char **argv UNUSED)
{
     return PAM_CRED_ERR;
}

/* --- account management functions --- */

int
pam_sm_acct_mgmt(pam_handle_t *pamh UNUSED, int flags UNUSED,
		 int argc UNUSED, const char **argv UNUSED)
{
     return PAM_AUTH_ERR;
}

/* --- password management --- */

int
pam_sm_chauthtok(pam_handle_t *pamh UNUSED, int flags UNUSED,
		 int argc UNUSED, const char **argv UNUSED)
{
     return PAM_AUTHTOK_ERR;
}

/* --- session management --- */

int
pam_sm_open_session(pam_handle_t *pamh UNUSED, int flags UNUSED,
		    int argc UNUSED, const char **argv UNUSED)
{
    return PAM_SESSION_ERR;
}

int
pam_sm_close_session(pam_handle_t *pamh UNUSED, int flags UNUSED,
		     int argc UNUSED, const char **argv UNUSED)
{
     return PAM_SESSION_ERR;
}

/* end of module definition */
