/*
 * pam_rhosts module
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

#include <pwd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>

#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>
#include "pam_inline.h"

int pam_sm_authenticate (pam_handle_t *pamh, int flags, int argc,
			 const char **argv)
{
    const char *luser = NULL;
    const char *ruser = NULL, *rhost = NULL;
    const char *opt_superuser = NULL;
    const void *c_void;
    int opt_debug = 0;
    int opt_silent;
    int as_root;
    int retval;

    opt_silent = flags & PAM_SILENT;

    while (argc-- > 0) {
      const char *str;

      if (strcmp(*argv, "debug") == 0)
	opt_debug = 1;
      else if (strcmp (*argv, "silent") == 0 || strcmp(*argv, "suppress") == 0)
	opt_silent = 1;
      else if ((str = pam_str_skip_prefix(*argv, "superuser=")) != NULL)
	opt_superuser = str;
      else
	pam_syslog(pamh, LOG_WARNING, "unrecognized option '%s'", *argv);

      ++argv;
    }

    retval = pam_get_item (pamh, PAM_RHOST, &c_void);
    if (retval != PAM_SUCCESS) {
      pam_syslog(pamh, LOG_ERR, "could not get the remote host name");
      return retval;
    }
    rhost = c_void;

    retval = pam_get_item(pamh, PAM_RUSER, &c_void);
    ruser = c_void;
    if (retval != PAM_SUCCESS) {
      pam_syslog(pamh, LOG_ERR, "could not get the remote username");
      return retval;
    }

    retval = pam_get_user(pamh, &luser, NULL);
    if (retval != PAM_SUCCESS) {
      pam_syslog(pamh, LOG_NOTICE, "cannot determine local user name: %s",
		 pam_strerror(pamh, retval));
      return retval;
    }

    if (rhost == NULL || ruser == NULL)
      return PAM_AUTH_ERR;

    if (opt_superuser && strcmp(opt_superuser, luser) == 0)
      as_root = 1;
    else {
      struct passwd *lpwd;

      lpwd = pam_modutil_getpwnam(pamh, luser);
      if (lpwd == NULL) {
	if (opt_debug)
	  /* don't print by default, could be the user's password */
	  pam_syslog(pamh, LOG_DEBUG,
		     "user '%s' unknown to this system", luser);
	return PAM_USER_UNKNOWN;

      }
      as_root = (lpwd->pw_uid == 0);
    }

#ifdef HAVE_RUSEROK_AF
    retval = ruserok_af (rhost, as_root, ruser, luser, PF_UNSPEC);
#else
    retval = ruserok (rhost, as_root, ruser, luser);
#endif
    if (retval != 0) {
      if (!opt_silent || opt_debug)
	pam_syslog(pamh, LOG_WARNING, "denied access to %s@%s as %s",
		   ruser, rhost, luser);
      return PAM_AUTH_ERR;
    } else {
      if (!opt_silent || opt_debug)
	pam_syslog(pamh, LOG_NOTICE, "allowed access to %s@%s as %s",
		   ruser, rhost, luser);
      return PAM_SUCCESS;
    }
}


int
pam_sm_setcred (pam_handle_t *pamh UNUSED, int flags UNUSED,
		int argc UNUSED, const char **argv UNUSED)
{
  return PAM_SUCCESS;
}
