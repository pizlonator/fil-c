/* pam_item.c */

/*
 * $Id$
 */

#include "pam_private.h"
#include "pam_inline.h"
#include "pam_i18n.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define TRY_SET(X, Y)                      \
do {                                       \
    if ((X) != (Y)) {		           \
	char *_TMP_ = _pam_strdup(Y);      \
	if (_TMP_ == NULL && (Y) != NULL)  \
	    return PAM_BUF_ERR;            \
	free(X);                           \
	(X) = _TMP_;                       \
    }					   \
} while(0)

/* functions */

int pam_set_item (pam_handle_t *pamh, int item_type, const void *item)
{
    int retval;

    D(("called"));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    retval = PAM_SUCCESS;

    switch (item_type) {

    case PAM_SERVICE:
	if (item == NULL) {
	    pam_syslog(pamh, LOG_ERR,
		       "pam_set_item: attempt to set service to NULL");
	    retval = PAM_BAD_ITEM;
	    break;
	}
	/* Setting handlers_loaded to 0 will cause the handlers
	 * to be reloaded on the next call to a service module.
	 */
	pamh->handlers.handlers_loaded = 0;
	TRY_SET(pamh->service_name, item);
	{
	    char *tmp;
	    for (tmp=pamh->service_name; *tmp; ++tmp)
		*tmp = tolower((unsigned char)*tmp);                 /* require lower case */
	}
	break;

    case PAM_USER:
	TRY_SET(pamh->user, item);
	pamh->former.fail_user = PAM_SUCCESS;
	break;

    case PAM_USER_PROMPT:
	TRY_SET(pamh->prompt, item);
	pamh->former.fail_user = PAM_SUCCESS;
	break;

    case PAM_TTY:
	D(("setting tty to %s", (const char *)item));
	TRY_SET(pamh->tty, item);
	break;

    case PAM_RUSER:
	TRY_SET(pamh->ruser, item);
	break;

    case PAM_RHOST:
	TRY_SET(pamh->rhost, item);
	break;

    case PAM_AUTHTOK:
	/*
	 * PAM_AUTHTOK and PAM_OLDAUTHTOK are only accessible from
	 * modules.
	 */
	if (__PAM_FROM_MODULE(pamh)) {
	    if (pamh->authtok != item) {
		pam_overwrite_string(pamh->authtok);
		TRY_SET(pamh->authtok, item);
	    }
	} else {
	    retval = PAM_BAD_ITEM;
	}

	break;

    case PAM_OLDAUTHTOK:
	/*
	 * PAM_AUTHTOK and PAM_OLDAUTHTOK are only accessible from
	 * modules.
	 */
	if (__PAM_FROM_MODULE(pamh)) {
	    if (pamh->oldauthtok != item) {
		pam_overwrite_string(pamh->oldauthtok);
		TRY_SET(pamh->oldauthtok, item);
	    }
	} else {
	    retval = PAM_BAD_ITEM;
	}

	break;

    case PAM_CONV:              /* want to change the conversation function */
	if (item == NULL) {
	    pam_syslog(pamh, LOG_ERR,
		       "pam_set_item: attempt to set conv() to NULL");
	    retval = PAM_PERM_DENIED;
	} else {
	    struct pam_conv *tconv;

	    if ((tconv = malloc(sizeof(struct pam_conv))
		) == NULL) {
		pam_syslog(pamh, LOG_CRIT,
				"pam_set_item: malloc failed for pam_conv");
		retval = PAM_BUF_ERR;
	    } else {
		memcpy(tconv, item, sizeof(struct pam_conv));
		_pam_drop(pamh->pam_conversation);
		pamh->pam_conversation = tconv;
		pamh->former.fail_user = PAM_SUCCESS;
	    }
	}
        break;

    case PAM_FAIL_DELAY:
	pamh->fail_delay.delay_fn_ptr = item;
	break;

    case PAM_XDISPLAY:
	TRY_SET(pamh->xdisplay, item);
	break;

    case PAM_XAUTHDATA:
	if (&pamh->xauth == item)
	    break;
	if (pamh->xauth.namelen) {
	    pam_overwrite_string(pamh->xauth.name);
	    free(pamh->xauth.name);
	}
	if (pamh->xauth.datalen) {
	    pam_overwrite_n(pamh->xauth.data, (unsigned int) pamh->xauth.datalen);
	    free(pamh->xauth.data);
	}
	pamh->xauth = *((const struct pam_xauth_data *) item);
	if ((pamh->xauth.name=_pam_strdup(pamh->xauth.name)) == NULL) {
	    pam_overwrite_object(&pamh->xauth);
	    return PAM_BUF_ERR;
	}
	if ((pamh->xauth.data=_pam_memdup(pamh->xauth.data,
	    pamh->xauth.datalen)) == NULL) {
	    pam_overwrite_string(pamh->xauth.name);
	    free(pamh->xauth.name);
	    pam_overwrite_object(&pamh->xauth);
	    return PAM_BUF_ERR;
	}
	break;

    case PAM_AUTHTOK_TYPE:
	TRY_SET(pamh->authtok_type, item);
	break;

    default:
	retval = PAM_BAD_ITEM;
    }

    return retval;
}

int pam_get_item (const pam_handle_t *pamh, int item_type, const void **item)
{
    int retval = PAM_SUCCESS;

    D(("called."));
    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (item == NULL) {
	pam_syslog(pamh, LOG_ERR,
			"pam_get_item: nowhere to place requested item");
	return PAM_PERM_DENIED;
    }
    else
	*item = NULL;

    switch (item_type) {
    case PAM_SERVICE:
	*item = pamh->service_name;
	break;

    case PAM_USER:
	D(("returning user=%s", pamh->user));
	*item = pamh->user;
	break;

    case PAM_USER_PROMPT:
	D(("returning userprompt=%s", pamh->user));
	*item = pamh->prompt;
	break;

    case PAM_TTY:
	D(("returning tty=%s", pamh->tty));
	*item = pamh->tty;
	break;

    case PAM_RUSER:
	*item = pamh->ruser;
	break;

    case PAM_RHOST:
	*item = pamh->rhost;
	break;

    case PAM_AUTHTOK:
	/*
	 * PAM_AUTHTOK and PAM_OLDAUTHTOK are only accessible from
	 * modules.
	 */
	if (__PAM_FROM_MODULE(pamh)) {
	    *item = pamh->authtok;
	} else {
	    retval = PAM_BAD_ITEM;
	}
	break;

    case PAM_OLDAUTHTOK:
	/*
	 * PAM_AUTHTOK and PAM_OLDAUTHTOK are only accessible from
	 * modules.
	 */
	if (__PAM_FROM_MODULE(pamh)) {
	    *item = pamh->oldauthtok;
	} else {
	    retval = PAM_BAD_ITEM;
	}
	break;

    case PAM_CONV:
	*item = pamh->pam_conversation;
	break;

    case PAM_FAIL_DELAY:
	*item = pamh->fail_delay.delay_fn_ptr;
	break;

    case PAM_XDISPLAY:
	*item = pamh->xdisplay;
	break;

    case PAM_XAUTHDATA:
	*item = &pamh->xauth;
	break;

    case PAM_AUTHTOK_TYPE:
	*item = pamh->authtok_type;
	break;

    default:
	retval = PAM_BAD_ITEM;
    }

    return retval;
}

/*
 * This function is the 'preferred method to obtain the username'.
 */

int pam_get_user(pam_handle_t *pamh, const char **user, const char *prompt)
{
    const char *use_prompt;
    int retval;
    struct pam_message msg;
    const struct pam_message *pmsg;
    struct pam_response *resp;

    D(("called."));

    IF_NO_PAMH(pamh, PAM_SYSTEM_ERR);

    if (user == NULL) {
        /* ensure that the module has supplied a destination */
	pam_syslog(pamh, LOG_ERR, "pam_get_user: nowhere to record username");
	return PAM_SYSTEM_ERR;
    } else
	*user = NULL;

    if (pamh->pam_conversation == NULL) {
	pam_syslog(pamh, LOG_ERR, "pam_get_user: no conv element in pamh");
	return PAM_SYSTEM_ERR;
    }

    if (pamh->user) {    /* have one so return it */
	*user = pamh->user;
	return PAM_SUCCESS;
    }

    if (pamh->former.fail_user != PAM_SUCCESS)
	return pamh->former.fail_user;

    /* will need a prompt */
    if (prompt != NULL)
      use_prompt = prompt;
    else if (pamh->prompt != NULL)
      use_prompt = pamh->prompt;
    else
      use_prompt = _("login:");

    /* If we are resuming an old conversation, we verify that the prompt
       is the same.  Anything else is an error. */
    if (pamh->former.want_user) {
	/* must have a prompt to resume with */
	if (! pamh->former.prompt) {
	    pam_syslog(pamh, LOG_ERR,
		       "pam_get_user: failed to resume with prompt"
			);
	    return PAM_ABORT;
	}

	/* must be the same prompt as last time */
	if (strcmp(pamh->former.prompt, use_prompt)) {
	    pam_syslog(pamh, LOG_ERR,
		       "pam_get_user: resumed with different prompt");
	    return PAM_ABORT;
	}

	/* ok, we can resume where we left off last time */
	pamh->former.want_user = PAM_FALSE;
	pam_overwrite_string(pamh->former.prompt);
	_pam_drop(pamh->former.prompt);
    }

    /* converse with application -- prompt user for a username */
    pmsg = &msg;
    msg.msg_style = PAM_PROMPT_ECHO_ON;
    msg.msg = use_prompt;
    resp = NULL;

    retval = pamh->pam_conversation->
	conv(1, &pmsg, &resp, pamh->pam_conversation->appdata_ptr);

    switch (retval) {
	case PAM_SUCCESS:
	case PAM_BUF_ERR:
	case PAM_CONV_AGAIN:
	case PAM_CONV_ERR:
	    break;
	default:
	    retval = PAM_CONV_ERR;
    }

    switch (retval) {
	case PAM_CONV_AGAIN:
	    /* conversation function is waiting for an event - save state */
	    D(("conversation function is not ready yet"));
	    pamh->former.want_user = PAM_TRUE;
	    pamh->former.prompt = _pam_strdup(use_prompt);
	    break;
	case PAM_SUCCESS:
	    if (resp != NULL && resp->resp != NULL) {
		/*
		 * now we set the PAM_USER item -- this was missing from pre.53
		 * releases. However, reading the Sun manual, it is part of
		 * the standard API.
		 */
		retval = pam_set_item(pamh, PAM_USER, resp->resp);
		*user = pamh->user;
		break;
	    } else {
		/* conversation should have given a response */
		D(("no response provided"));
		retval = PAM_CONV_ERR;
	    }
	    /* fallthrough */
	default:
	    pamh->former.fail_user = retval;
    }

    if (resp) {
	if (retval != PAM_SUCCESS)
	    pam_syslog(pamh, LOG_WARNING,
		       "unexpected response from failed conversation function");
	/*
	 * note 'resp' is allocated by the application and is
         * correctly free()'d here
	 */
	pam_drop_response(resp, 1);
    }

    D(("completed"));
    return retval;        /* pass on any error from conversation */
}
