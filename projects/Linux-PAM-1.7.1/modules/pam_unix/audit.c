#include "config.h"

#ifdef HAVE_LIBAUDIT

#include <errno.h>
#include <unistd.h>

#include <libaudit.h>

#include <security/_pam_types.h>

#include "audit.h"
#include "passverify.h"

int audit_log(int type, const char *uname, int retval)
{
	int audit_fd, rc;

	audit_fd = audit_open();
	if (audit_fd < 0) {
		/* You get these error codes only when the kernel doesn't have
		 * audit compiled in. */
		if (errno == EINVAL || errno == EPROTONOSUPPORT ||
		    errno == EAFNOSUPPORT)
			return PAM_SUCCESS;

		helper_log_err(LOG_CRIT, "audit_open() failed: %m");
		return PAM_AUTH_ERR;
	}



	rc = audit_log_acct_message(audit_fd, type, NULL, "PAM:" HELPER_COMPILE,
		uname, -1, NULL, NULL, NULL, retval == PAM_SUCCESS);
	if (rc == -EPERM && geteuid() != 0) {
		rc = 0;
	}

	audit_close(audit_fd);

	return rc < 0 ? PAM_AUTH_ERR : PAM_SUCCESS;
}

#endif /* HAVE_LIBAUDIT */
