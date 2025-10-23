/*
 * pam_unix account management
 *
 * Copyright Elliot Lee, 1996.  All rights reserved.
 * Copyright Jan Rękorajski, 1999.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <syslog.h>
#include <pwd.h>
#include <shadow.h>
#include <time.h>		/* for time() */
#include <errno.h>
#include <sys/wait.h>

#include <security/_pam_macros.h>

#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>

#include "pam_i18n.h"
#include "pam_inline.h"
#include "support.h"
#include "passverify.h"

int _unix_run_verify_binary(pam_handle_t *pamh, unsigned long long ctrl,
	const char *user, int *daysleft)
{
  int retval=0, child, fds[2];
  struct sigaction newsa, oldsa;
  D(("running verify_binary"));

  /* create a pipe for the messages */
  if (pipe(fds) != 0) {
    D(("could not make pipe"));
    pam_syslog(pamh, LOG_ERR, "Could not make pipe: %m");
    return PAM_AUTH_ERR;
  }
  D(("called."));

  if (off(UNIX_NOREAP, ctrl)) {
    /*
     * This code arranges that the demise of the child does not cause
     * the application to receive a signal it is not expecting - which
     * may kill the application or worse.
     *
     * The "noreap" module argument is provided so that the admin can
     * override this behavior.
     */
     memset(&newsa, '\0', sizeof(newsa));
     newsa.sa_handler = SIG_DFL;
     sigaction(SIGCHLD, &newsa, &oldsa);
  }

  /* fork */
  child = fork();
  if (child == 0) {
    static char *envp[] = { NULL };
    const char *args[] = { NULL, NULL, NULL, NULL };

    /* XXX - should really tidy up PAM here too */

    /* reopen stdout as pipe */
    if (dup2(fds[1], STDOUT_FILENO) != STDOUT_FILENO) {
      pam_syslog(pamh, LOG_ERR, "dup2 of %s failed: %m", "stdout");
      _exit(PAM_AUTHINFO_UNAVAIL);
    }

    if (pam_modutil_sanitize_helper_fds(pamh, PAM_MODUTIL_PIPE_FD,
					PAM_MODUTIL_IGNORE_FD,
					PAM_MODUTIL_PIPE_FD) < 0) {
      _exit(PAM_AUTHINFO_UNAVAIL);
    }

    /* must set the real uid to 0 so the helper will not error
       out if pam is called from setuid binary (su, sudo...) */
    if (setuid(0) == -1) {
      uid_t euid = geteuid();
      pam_syslog(pamh, euid == 0 ? LOG_ERR : LOG_DEBUG, "setuid failed: %m");
      if (euid == 0) {
	printf("-1\n");
	fflush(stdout);
	_exit(PAM_AUTHINFO_UNAVAIL);
      }
    }

    /* exec binary helper */
    args[0] = CHKPWD_HELPER;
    args[1] = user;
    args[2] = "chkexpiry";

    DIAG_PUSH_IGNORE_CAST_QUAL;
    execve(CHKPWD_HELPER, (char *const *) args, envp);
    DIAG_POP_IGNORE_CAST_QUAL;

    pam_syslog(pamh, LOG_ERR, "helper binary execve failed: %m");
    /* should not get here: exit with error */
    D(("helper binary is not available"));
    printf("-1\n");
    fflush(stdout);
    _exit(PAM_AUTHINFO_UNAVAIL);
  } else {
    close(fds[1]);
    if (child > 0) {
      char buf[32];
      int rc=0;
      /* wait for helper to complete: */
      while ((rc=waitpid(child, &retval, 0)) < 0 && errno == EINTR);
      if (rc<0) {
	pam_syslog(pamh, LOG_ERR, "unix_chkpwd waitpid returned %d: %m", rc);
	retval = PAM_AUTH_ERR;
      } else if (!WIFEXITED(retval)) {
        pam_syslog(pamh, LOG_ERR, "unix_chkpwd abnormal exit: %d", retval);
        retval = PAM_AUTH_ERR;
      } else {
	retval = WEXITSTATUS(retval);
        rc = pam_modutil_read(fds[0], buf, sizeof(buf) - 1);
	if(rc > 0) {
	      buf[rc] = '\0';
	      if (sscanf(buf,"%d", daysleft) != 1 )
	        retval = PAM_AUTH_ERR;
	    }
	else {
	    pam_syslog(pamh, LOG_ERR, "read unix_chkpwd output error %d: %m", rc);
	    retval = PAM_AUTH_ERR;
	  }
      }
    } else {
      pam_syslog(pamh, LOG_ERR, "Fork failed: %m");
      D(("fork failed"));
      retval = PAM_AUTH_ERR;
    }
    close(fds[0]);
  }

  if (off(UNIX_NOREAP, ctrl)) {
        sigaction(SIGCHLD, &oldsa, NULL);   /* restore old signal handler */
  }

  D(("Returning %d",retval));
  return retval;
}

/*
 * PAM framework looks for this entry-point to pass control to the
 * account management module.
 */

int
pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	unsigned long long ctrl;
	const void *void_uname;
	const char *uname;
	int retval, daysleft = -1;
	char buf[256];

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, NULL, argc, argv);

	retval = pam_get_item(pamh, PAM_USER, &void_uname);
	uname = void_uname;
	D(("user = `%s'", uname));
	if (retval != PAM_SUCCESS || uname == NULL) {
		pam_syslog(pamh, LOG_ERR,
			 "could not identify user (from uid=%lu)",
			 (unsigned long int)getuid());
		return PAM_USER_UNKNOWN;
	}

	retval = _unix_verify_user(pamh, ctrl, uname, &daysleft);

	if (on(UNIX_NO_PASS_EXPIRY, ctrl)) {
		const void *pretval = NULL;
		int authrv = PAM_AUTHINFO_UNAVAIL; /* authentication not called */

		if (pam_get_data(pamh, "unix_setcred_return", &pretval) == PAM_SUCCESS
			&& pretval)
			authrv = *(const int *)pretval;

		if (authrv != PAM_SUCCESS
			&& (retval == PAM_NEW_AUTHTOK_REQD || retval == PAM_AUTHTOK_EXPIRED))
			retval = PAM_SUCCESS;
	}

	switch (retval) {
	case PAM_ACCT_EXPIRED:
		pam_syslog(pamh, LOG_NOTICE,
			"account %s has expired (account expired)",
			uname);
		_make_remark(pamh, ctrl, PAM_ERROR_MSG,
			_("Your account has expired; please contact your system administrator."));
		break;
	case PAM_NEW_AUTHTOK_REQD:
		if (daysleft == 0) {
			pam_syslog(pamh, LOG_NOTICE,
				"expired password for user %s (root enforced)",
				uname);
			_make_remark(pamh, ctrl, PAM_ERROR_MSG,
				_("You are required to change your password immediately (administrator enforced)."));
		} else {
			pam_syslog(pamh, LOG_DEBUG,
				"expired password for user %s (password aged)",
				uname);
			_make_remark(pamh, ctrl, PAM_ERROR_MSG,
				_("You are required to change your password immediately (password expired)."));
		}
		break;
	case PAM_AUTHTOK_EXPIRED:
		pam_syslog(pamh, LOG_NOTICE,
			"account %s has expired (failed to change password)",
			uname);
		_make_remark(pamh, ctrl, PAM_ERROR_MSG,
			_("Your account has expired; please contact your system administrator."));
		break;
	case PAM_AUTHTOK_ERR:
		/*
		 * We ignore "password changed too early" error
		 * as it is relevant only for password change.
		 */
		retval = PAM_SUCCESS;
		/* fallthrough */
	case PAM_SUCCESS:
		if (daysleft >= 0) {
			pam_syslog(pamh, LOG_DEBUG,
				"password for user %s will expire in %d days",
				uname, daysleft);
#if defined HAVE_DNGETTEXT && defined ENABLE_NLS
			pam_sprintf(buf,
				dngettext(PACKAGE,
				  "Warning: your password will expire in %d day.",
				  "Warning: your password will expire in %d days.",
				  daysleft),
				daysleft);
#else
			if (daysleft == 1)
			    pam_sprintf(buf,
				_("Warning: your password will expire in %d day."),
				daysleft);
			else
			    pam_sprintf(buf,
			    /* TRANSLATORS: only used if dngettext is not supported */
				_("Warning: your password will expire in %d days."),
				daysleft);
#endif
			_make_remark(pamh, ctrl, PAM_TEXT_INFO, buf);
		}
	}

	D(("all done"));

	return retval;
}
