/*
 * pam_rootok module
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/3/11
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>

#include <security/pam_modules.h>
#include <security/pam_ext.h>

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/avc.h>
#endif

#ifdef HAVE_LIBAUDIT
#include <libaudit.h>
#endif

/* argument parsing */

#define PAM_DEBUG_ARG       01

static int
_pam_parse (const pam_handle_t *pamh, int argc, const char **argv)
{
    int ctrl=0;

    /* step through arguments */
    for (ctrl=0; argc-- > 0; ++argv) {

	/* generic options */

	if (!strcmp(*argv,"debug"))
	    ctrl |= PAM_DEBUG_ARG;
	else {
	    pam_syslog(pamh, LOG_ERR, "unknown option: %s", *argv);
	}
    }

    return ctrl;
}

#ifdef WITH_SELINUX
static int
PAM_FORMAT((printf, 2, 3))
log_callback (int type UNUSED, const char *fmt, ...)
{
    va_list ap;

#ifdef HAVE_LIBAUDIT
    int audit_fd = audit_open();

    if (audit_fd >= 0) {
	char *buf;
	int ret;

	va_start(ap, fmt);
	ret = vasprintf (&buf, fmt, ap);
	va_end(ap);
	if (ret < 0) {
		audit_close(audit_fd);
		return 0;
	}
	(void) !audit_log_user_avc_message(audit_fd, AUDIT_USER_AVC, buf,
					   NULL, NULL, NULL, 0);
	audit_close(audit_fd);
	free(buf);
	return 0;
    }

#endif
    va_start(ap, fmt);
    vsyslog (LOG_USER | LOG_INFO, fmt, ap);
    va_end(ap);
    return 0;
}

static int
selinux_check_root (void)
{
    int status = -1;
    char *user_context_raw;
    union selinux_callback old_callback;

    if (is_selinux_enabled() < 1)
	return 0;

    old_callback = selinux_get_callback(SELINUX_CB_LOG);
    /* setup callbacks */
    selinux_set_callback(SELINUX_CB_LOG, (union selinux_callback) &log_callback);
    if ((status = getprevcon_raw(&user_context_raw)) < 0) {
	selinux_set_callback(SELINUX_CB_LOG, old_callback);
	return status;
    }

    status = selinux_check_access(user_context_raw, user_context_raw, "passwd", "rootok", NULL);

    selinux_set_callback(SELINUX_CB_LOG, old_callback);
    freecon(user_context_raw);
    return status;
}
#endif

static int
check_for_root (pam_handle_t *pamh, int ctrl)
{
    int retval = PAM_AUTH_ERR;

    if (getuid() == 0)
#ifdef WITH_SELINUX
      if (selinux_check_root() == 0 || security_getenforce() == 0)
#endif
	retval = PAM_SUCCESS;

    if (ctrl & PAM_DEBUG_ARG) {
       pam_syslog(pamh, LOG_DEBUG, "root check %s",
	          (retval==PAM_SUCCESS) ? "succeeded" : "failed");
    }

    return retval;
}

/* --- management functions --- */

int
pam_sm_authenticate (pam_handle_t *pamh, int flags UNUSED,
		     int argc, const char **argv)
{
    int ctrl;

    ctrl = _pam_parse(pamh, argc, argv);

    return check_for_root (pamh, ctrl);
}

int
pam_sm_setcred (pam_handle_t *pamh UNUSED, int flags UNUSED,
		int argc UNUSED, const char **argv UNUSED)
{
    return PAM_SUCCESS;
}

int
pam_sm_acct_mgmt (pam_handle_t *pamh, int flags UNUSED,
		  int argc, const char **argv)
{
    int ctrl;

    ctrl = _pam_parse(pamh, argc, argv);

    return check_for_root (pamh, ctrl);
}

int
pam_sm_chauthtok (pam_handle_t *pamh, int flags UNUSED,
		  int argc, const char **argv)
{
    int ctrl;

    ctrl = _pam_parse(pamh, argc, argv);

    return check_for_root (pamh, ctrl);
}

/* end of module definition */
