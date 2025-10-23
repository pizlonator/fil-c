/* pam_start.c */

/* Creator Marc Ewing
 * Maintained by AGM
 *
 * $Id$
 *
 */

#include "pam_private.h"
#include "pam_i18n.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

static int _pam_start_internal (
    const char *service_name,
    const char *user,
    const struct pam_conv *pam_conversation,
    const char *confdir,
    pam_handle_t **pamh)
{
    D(("called pam_start: [%s] [%s] [%p] [%p]"
       ,service_name, user, pam_conversation, pamh));

#if defined HAVE_BINDTEXTDOMAIN && defined ENABLE_NLS
    /* Bind text domain to pull in PAM translations for a case where
       linux-pam is installed to non-default prefix.

       It is safe to call bindtextdomain() from multiple threads, but it
       has a chance to have some overhead. Let's try to do it once (or a
       small number of times as `bound_text_domain` is not protected by
       a lock. */
    static int bound_text_domain = 0;
    if (!bound_text_domain) {
	bound_text_domain = 1;
	bindtextdomain(PACKAGE, LOCALEDIR);
    }
#endif

    if (pamh == NULL) {
	pam_syslog(NULL, LOG_CRIT,
		   "pam_start: invalid argument: pamh == NULL");
	return (PAM_SYSTEM_ERR);
    }

    if (service_name == NULL) {
	pam_syslog(NULL, LOG_CRIT,
		   "pam_start: invalid argument: service == NULL");
	return (PAM_SYSTEM_ERR);
    }

    if (pam_conversation == NULL) {
	pam_syslog(NULL, LOG_CRIT,
		   "pam_start: invalid argument: conv == NULL");
	return (PAM_SYSTEM_ERR);
    }

    if ((*pamh = calloc(1, sizeof(**pamh))) == NULL) {
	pam_syslog(NULL, LOG_CRIT, "pam_start: calloc failed for *pamh");
	return (PAM_BUF_ERR);
    }

    /* All service names should be files below /etc/pam.d and nothing
       else. Forbid paths. */
    if (strrchr(service_name, '/') != NULL)
	service_name = strrchr(service_name, '/') + 1;

    /* Mark the caller as the application - permission to do certain
       things is limited to a module or an application */

    __PAM_TO_APP(*pamh);

    if (((*pamh)->service_name = _pam_strdup(service_name)) == NULL) {
	pam_syslog(*pamh, LOG_CRIT,
		   "pam_start: _pam_strdup failed for service name");
	_pam_drop(*pamh);
	return (PAM_BUF_ERR);
    } else {
	char *tmp;

	for (tmp=(*pamh)->service_name; *tmp; ++tmp)
	    *tmp = tolower((unsigned char)*tmp);                   /* require lower case */
    }

    if (user) {
	if (((*pamh)->user = _pam_strdup(user)) == NULL) {
	    pam_syslog(*pamh, LOG_CRIT,
		       "pam_start: _pam_strdup failed for user");
	    _pam_drop((*pamh)->service_name);
	    _pam_drop(*pamh);
	    return (PAM_BUF_ERR);
	}
    } else
	(*pamh)->user = NULL;

    if (confdir) {
	if (((*pamh)->confdir = _pam_strdup(confdir)) == NULL) {
	    pam_syslog(*pamh, LOG_CRIT,
		       "pam_start: _pam_strdup failed for confdir");
	    _pam_drop((*pamh)->service_name);
	    _pam_drop((*pamh)->user);
	    _pam_drop(*pamh);
	    return (PAM_BUF_ERR);
	}
    } else
	(*pamh)->confdir = NULL;

    (*pamh)->tty = NULL;
    (*pamh)->prompt = NULL;              /* prompt for pam_get_user() */
    (*pamh)->ruser = NULL;
    (*pamh)->rhost = NULL;
    (*pamh)->authtok = NULL;
    (*pamh)->oldauthtok = NULL;
    (*pamh)->fail_delay.delay_fn_ptr = NULL;
    (*pamh)->former.choice = PAM_NOT_STACKED;
    (*pamh)->former.substates = NULL;
#ifdef HAVE_LIBAUDIT
    (*pamh)->audit_state = 0;
#endif
    (*pamh)->xdisplay = NULL;
    (*pamh)->authtok_type = NULL;
    (*pamh)->authtok_verified = 0;
    memset (&((*pamh)->xauth), 0, sizeof ((*pamh)->xauth));

    if (((*pamh)->pam_conversation = (struct pam_conv *)
	  malloc(sizeof(struct pam_conv))) == NULL) {
	pam_syslog(*pamh, LOG_CRIT, "pam_start: malloc failed for pam_conv");
	_pam_drop((*pamh)->service_name);
	_pam_drop((*pamh)->user);
	_pam_drop((*pamh)->confdir);
	_pam_drop(*pamh);
	return (PAM_BUF_ERR);
    } else {
	memcpy((*pamh)->pam_conversation, pam_conversation,
	       sizeof(struct pam_conv));
    }

    (*pamh)->data = NULL;
    if ( _pam_make_env(*pamh) != PAM_SUCCESS ) {
	pam_syslog(*pamh,LOG_ERR,"pam_start: failed to initialize environment");
	_pam_drop((*pamh)->pam_conversation);
	_pam_drop((*pamh)->service_name);
	_pam_drop((*pamh)->user);
	_pam_drop((*pamh)->confdir);
	_pam_drop(*pamh);
	return PAM_ABORT;
    }

    _pam_reset_timer(*pamh);         /* initialize timer support */

    _pam_start_handlers(*pamh);                   /* cannot fail */

    /* According to the SunOS man pages, loading modules and resolving
     * symbols happens on the first call from the application. */

    if ( _pam_init_handlers(*pamh) != PAM_SUCCESS ) {
	pam_syslog(*pamh, LOG_ERR, "pam_start: failed to initialize handlers");
	_pam_free_handlers(*pamh);
	_pam_drop_env(*pamh);                 /* purge the environment */
	_pam_drop((*pamh)->pam_conversation);
	_pam_drop((*pamh)->service_name);
	_pam_drop((*pamh)->user);
	_pam_drop((*pamh)->confdir);
	_pam_drop(*pamh);
	return PAM_ABORT;
    }

    D(("exiting successfully"));

    return PAM_SUCCESS;
}

int pam_start_confdir (
    const char *service_name,
    const char *user,
    const struct pam_conv *pam_conversation,
    const char *confdir,
    pam_handle_t **pamh)
{
    return _pam_start_internal(service_name, user, pam_conversation,
			       confdir, pamh);
}

int pam_start (
    const char *service_name,
    const char *user,
    const struct pam_conv *pam_conversation,
    pam_handle_t **pamh)
{
    return _pam_start_internal(service_name, user, pam_conversation,
			       NULL, pamh);
}
