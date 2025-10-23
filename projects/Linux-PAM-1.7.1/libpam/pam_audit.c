/* pam_audit.c -- Instrumentation code for Linux Auditing System  */

/* (C) 2005-2006 Red Hat, Inc. -- Licensing details are in the COPYING
   file accompanying the Linux-PAM source distribution.

   Authors:
   Steve Grubb <sgrubb@redhat.com> */

#include "pam_private.h"
#include "pam_modutil_private.h"
#include "pam_inline.h"

#ifdef HAVE_LIBAUDIT
#include <stdio.h>
#include <syslog.h>
#include <libaudit.h>
#include <pwd.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#define PAMAUDIT_LOGGED 1

static int
_pam_audit_writelog(pam_handle_t *pamh, int audit_fd, int type,
	const char *message, const char *grantors, int retval)
{
  static int old_errno = -1;
  int rc = -ENOMEM;
  char *buf;
  const char *grantors_field = " grantors=";

  if (grantors == NULL) {
      grantors = "";
      grantors_field = "";
  }

  if ((buf = pam_asprintf("PAM:%s%s%s", message, grantors_field, grantors)) != NULL) {
      rc = audit_log_acct_message(audit_fd, type, NULL, buf,
	(retval != PAM_USER_UNKNOWN && pamh->user) ? pamh->user : "?",
	-1, pamh->rhost, NULL, pamh->tty, retval == PAM_SUCCESS);
      free(buf);
  }

  /* libaudit sets errno to its own negative error code. This can be
     an official errno number, but must not. It can also be a audit
     internal error code. Which makes errno useless :-((. Try the
     best to fix it. */
  errno = -rc;

  pamh->audit_state |= PAMAUDIT_LOGGED;

  if (rc < 0) {
      if (rc == -EPERM)
          return 0;
      if (errno != old_errno) {
          old_errno = errno;
          pam_syslog (pamh, LOG_CRIT, "audit_log_acct_message() failed: %m");
      }
  }
  return rc;
}

static int
_pam_audit_open(pam_handle_t *pamh)
{
  int audit_fd;
  audit_fd = audit_open();
  if (audit_fd < 0) {
    /* You get these error codes only when the kernel doesn't have
     * audit compiled in. */
    if (errno == EINVAL || errno == EPROTONOSUPPORT ||
        errno == EAFNOSUPPORT)
        return -2;

    /* this should only fail in case of extreme resource shortage,
     * need to prevent login in that case for CAPP compliance.
     */
    pam_syslog(pamh, LOG_CRIT, "audit_open() failed: %m");
    return -1;
  }

  return audit_fd;
}

static int
_pam_list_grantors(struct handler *hlist, int retval, char **list)
{
  *list = NULL;

  if (retval == PAM_SUCCESS) {
    struct handler *h;
    char *p = NULL;
    size_t len = 0;

    for (h = hlist; h != NULL; h = h->next) {
      if (h->grantor) {
        len += strlen(h->mod_name) + 1;
      }
    }

    if (len == 0) {
      return 0;
    }

    *list = malloc(len);
    if (*list == NULL) {
      return -1;
    }

    for (h = hlist; h != NULL; h = h->next) {
      if (h->grantor) {
        if (p == NULL) {
          p = *list;
        } else {
          p = stpcpy(p, ",");
        }

        p = stpcpy(p, h->mod_name);
      }
    }
  }

  return 0;
}

int
_pam_auditlog(pam_handle_t *pamh, int action, int retval, int flags, struct handler *h)
{
  const char *message;
  int type;
  int audit_fd;
  char *grantors;

  if ((audit_fd=_pam_audit_open(pamh)) == -1) {
    return PAM_SYSTEM_ERR;
  } else if (audit_fd == -2) {
    return retval;
  }

  switch (action) {
  case PAM_AUTHENTICATE:
    message = "authentication";
    type = AUDIT_USER_AUTH;
    break;
  case PAM_OPEN_SESSION:
    message = "session_open";
    type = AUDIT_USER_START;
    break;
  case PAM_CLOSE_SESSION:
    message = "session_close";
    type = AUDIT_USER_END;
    break;
  case PAM_ACCOUNT:
    message = "accounting";
    type = AUDIT_USER_ACCT;
    break;
  case PAM_CHAUTHTOK:
    message = "chauthtok";
    type = AUDIT_USER_CHAUTHTOK;
    break;
  case PAM_SETCRED:
    message = "setcred";
    if (flags & PAM_ESTABLISH_CRED)
	type = AUDIT_CRED_ACQ;
    else if ((flags & PAM_REINITIALIZE_CRED) || (flags & PAM_REFRESH_CRED))
	type = AUDIT_CRED_REFR;
    else if (flags & PAM_DELETE_CRED)
	type = AUDIT_CRED_DISP;
    else
        type = AUDIT_USER_ERR;
    break;
  case _PAM_ACTION_DONE:
    message = "bad_ident";
    type = AUDIT_USER_ERR;
    break;
  default:
    message = "UNKNOWN";
    type = AUDIT_USER_ERR;
    pam_syslog(pamh, LOG_CRIT, "_pam_auditlog() should never get here");
    retval = PAM_SYSTEM_ERR;
  }

  if (_pam_list_grantors(h, retval, &grantors) < 0) {
    /* allocation failure */
    pam_syslog(pamh, LOG_CRIT, "_pam_list_grantors() failed: %m");
    retval = PAM_SYSTEM_ERR;
  }

  if (_pam_audit_writelog(pamh, audit_fd, type, message,
      grantors ? grantors : "?", retval) < 0)
    retval = PAM_SYSTEM_ERR;

  free(grantors);

  audit_close(audit_fd);
  return retval;
}

int
_pam_audit_end(pam_handle_t *pamh, int status UNUSED)
{
  if (! (pamh->audit_state & PAMAUDIT_LOGGED)) {
    /* PAM library is being shut down without any of the audited
     * stacks having been run. Assume that this is sshd faking
     * things for an unknown user.
     */
    _pam_auditlog(pamh, _PAM_ACTION_DONE, PAM_USER_UNKNOWN, 0, NULL);
  }

  return 0;
}

int
pam_modutil_audit_write(pam_handle_t *pamh, int type,
    const char *message, int retval)
{
  int audit_fd;
  int rc;

  if ((audit_fd=_pam_audit_open(pamh)) == -1) {
    return PAM_SYSTEM_ERR;
  } else if (audit_fd == -2) {
    return retval;
  }

  rc = _pam_audit_writelog(pamh, audit_fd, type, message, NULL, retval);

  audit_close(audit_fd);

  return rc < 0 ? PAM_SYSTEM_ERR : PAM_SUCCESS;
}

#else
int pam_modutil_audit_write(pam_handle_t *pamh UNUSED, int type UNUSED,
    const char *message UNUSED, int retval UNUSED)
{
  return PAM_SUCCESS;
}
#endif /* HAVE_LIBAUDIT */
