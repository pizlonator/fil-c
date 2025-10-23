/*
 * $Id$
 *
 * This file was written by Andrew G. Morgan <morgan@parc.power.net>
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <security/pam_misc.h>
#include "pam_inline.h"

/*
 * This function should be used to carefully dispose of the copied
 * environment.
 *
 *     usage:     env = pam_misc_drop_env(env);
 */

char **pam_misc_drop_env(char **dump)
{
    size_t i;

    for (i=0; dump[i] != NULL; ++i) {
	D(("dump[%zu]=`%s'", i, dump[i]));
	pam_overwrite_string(dump[i]);
	_pam_drop(dump[i]);
    }
    _pam_drop(dump);

    return NULL;
}

/*
 *  This function takes the supplied environment and uploads it to be
 *  the PAM one.
 */

int pam_misc_paste_env(pam_handle_t *pamh, const char * const * user_env)
{
    for (; user_env && *user_env; ++user_env) {
	int retval;

	D(("uploading: %s", *user_env));
	retval = pam_putenv(pamh, *user_env);
	if (retval != PAM_SUCCESS) {
	    D(("error setting %s: %s", *user_env, pam_strerror(pamh,retval)));
	    return retval;
	}
    }
    D(("done."));
    return PAM_SUCCESS;
}

/*
 * This is a wrapper to make pam behave in the way that setenv() does.
 */

int pam_misc_setenv(pam_handle_t *pamh, const char *name
		    , const char *value, int readonly)
{
    char *tmp;
    int retval;

    if (readonly) {
	const char *etmp;

	/* we check if the variable is there already */
	etmp = pam_getenv(pamh, name);
	if (etmp != NULL) {
	    D(("failed to set readonly variable: %s", name));
	    return PAM_PERM_DENIED;          /* not allowed to overwrite */
	}
    }
    if ((tmp = pam_asprintf("%s=%s", name, value)) != NULL) {
	D(("pam_putt()ing: %s", tmp));
	retval = pam_putenv(pamh, tmp);
	pam_overwrite_string(tmp);           /* purge */
	_pam_drop(tmp);                      /* forget */
    } else {
	D(("malloc failure"));
	retval = PAM_BUF_ERR;
    }

    return retval;
}
