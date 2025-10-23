/* pam_end.c */

/*
 * $Id$
 */

#include "pam_private.h"
#include "pam_inline.h"

#include <stdlib.h>

int pam_end(pam_handle_t *pamh, int pam_status)
{
    int ret;

    D(("called."));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (__PAM_FROM_MODULE(pamh)) {
	D(("called from module!?"));
	return PAM_SYSTEM_ERR;
    }

#ifdef HAVE_LIBAUDIT
    _pam_audit_end(pamh, pam_status);
#endif

    /* first liberate the modules (it is not inconceivable that the
       modules may need to use the service_name etc. to clean up) */

    _pam_free_data(pamh, pam_status);

    /* now drop all modules */

    if ((ret = _pam_free_handlers(pamh)) != PAM_SUCCESS) {
	return ret;                 /* error occurred */
    }

    /* from this point we cannot call the modules any more. Free the remaining
       memory used by the Linux-PAM interface */

    _pam_drop_env(pamh);                      /* purge the environment */

    pam_overwrite_string(pamh->authtok);      /* blank out old token */
    _pam_drop(pamh->authtok);

    pam_overwrite_string(pamh->oldauthtok);   /* blank out old token */
    _pam_drop(pamh->oldauthtok);

    pam_overwrite_string(pamh->former.prompt);
    _pam_drop(pamh->former.prompt);           /* drop saved prompt */

    pam_overwrite_string(pamh->service_name);
    _pam_drop(pamh->service_name);

    pam_overwrite_string(pamh->user);
    _pam_drop(pamh->user);

    pam_overwrite_string(pamh->confdir);
    _pam_drop(pamh->confdir);

    pam_overwrite_string(pamh->prompt);
    _pam_drop(pamh->prompt);                  /* prompt for pam_get_user() */

    pam_overwrite_string(pamh->tty);
    _pam_drop(pamh->tty);

    pam_overwrite_string(pamh->rhost);
    _pam_drop(pamh->rhost);

    pam_overwrite_string(pamh->ruser);
    _pam_drop(pamh->ruser);

    _pam_drop(pamh->pam_conversation);
    pamh->fail_delay.delay_fn_ptr = NULL;

    _pam_drop(pamh->former.substates);

    pam_overwrite_string(pamh->xdisplay);
    _pam_drop(pamh->xdisplay);

    pam_overwrite_string(pamh->xauth.name);
    _pam_drop(pamh->xauth.name);
    pam_overwrite_n(pamh->xauth.data, (unsigned int)pamh->xauth.datalen);
    _pam_drop(pamh->xauth.data);
    pam_overwrite_object(&pamh->xauth);

    pam_overwrite_string(pamh->authtok_type);
    _pam_drop(pamh->authtok_type);

    /* and finally liberate the memory for the pam_handle structure */

    _pam_drop(pamh);

    D(("exiting successfully"));

    return PAM_SUCCESS;
}
