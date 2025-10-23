/*
 * $Id$
 *
 * A central point for invoking getlogin(). Hopefully, this is a
 * little harder to spoof than all the other versions that are out
 * there.
 */

#include "pam_modutil_private.h"

#include <stdlib.h>
#include <unistd.h>

#define _PAMMODUTIL_GETLOGIN "_pammodutil_getlogin"

const char *
pam_modutil_getlogin(pam_handle_t *pamh)
{
    int status;
    const void *logname;
    char *curr_user;

    status = pam_get_data(pamh, _PAMMODUTIL_GETLOGIN, &logname);
    if (status == PAM_SUCCESS) {
	return logname;
    }

    logname = getlogin();
    if (logname == NULL) {
      return NULL;
    }

    curr_user = strdup(logname);
    if (curr_user == NULL) {
      return NULL;
    }

    status = pam_set_data(pamh, _PAMMODUTIL_GETLOGIN, curr_user,
			  pam_modutil_cleanup);
    if (status != PAM_SUCCESS) {
      free(curr_user);
      return NULL;
    }

    return curr_user;
}
