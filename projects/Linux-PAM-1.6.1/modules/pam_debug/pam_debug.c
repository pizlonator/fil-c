/*
 * pam_debug module
 *
 * Written by Andrew Morgan <morgan@kernel.org> 2001/02/04
 *
 * This module is intended as a debugging aide for determining how
 * the PAM stack is operating.
 */

#include "config.h"
#include <stdio.h>

#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_ext.h>

#define _PAM_ACTION_UNDEF (-10)
#include "../../libpam/pam_tokens.h"

#define DEFAULT_USER "nobody"

/* --- authentication management functions --- */

static void state(pam_handle_t *pamh, const char *text)
{
    if (pam_info(pamh, "%s", text) != PAM_SUCCESS) {
	D(("pam_info failed"));
    }
}

static int parse_args(int retval, const char *event,
		      pam_handle_t *pamh, int argc, const char **argv)
{
    int i;

    for (i=0; i<argc; ++i) {
	size_t length = strlen(event);
	if (!strncmp(event, argv[i], length) && (argv[i][length] == '=')) {
	    int j;
	    const char *return_string = argv[i] + (length+1);

	    for (j=0; j<_PAM_RETURN_VALUES; ++j) {
		if (!strcmp(return_string, _pam_token_returns[j])) {
		    retval = j;
		    state(pamh, argv[i]);
		    break;
		}
	    }
	    break;
	}
    }

    return retval;
}

int pam_sm_authenticate(pam_handle_t *pamh, int flags UNUSED,
			int argc, const char **argv)
{
    return parse_args(PAM_SUCCESS, "auth", pamh, argc, argv);
}

int pam_sm_setcred(pam_handle_t *pamh, int flags UNUSED,
		   int argc, const char **argv)
{
    return parse_args(PAM_SUCCESS, "cred", pamh, argc, argv);
}

/* --- account management functions --- */

int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
    return parse_args(PAM_SUCCESS, "acct", pamh, argc, argv);
}

/* --- password management --- */

int pam_sm_chauthtok(pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
    if (flags & PAM_PRELIM_CHECK) {
	return parse_args(PAM_SUCCESS, "prechauthtok", pamh, argc, argv);
    } else {
	return parse_args(PAM_SUCCESS, "chauthtok", pamh, argc, argv);
    }
}

/* --- session management --- */

int pam_sm_open_session(pam_handle_t *pamh, int flags UNUSED,
			int argc, const char **argv)
{
    return parse_args(PAM_SUCCESS, "open_session", pamh, argc, argv);
}

int pam_sm_close_session(pam_handle_t *pamh, int flags UNUSED,
			 int argc, const char **argv)
{
    return parse_args(PAM_SUCCESS, "close_session", pamh, argc, argv);
}

/* end of module definition */
