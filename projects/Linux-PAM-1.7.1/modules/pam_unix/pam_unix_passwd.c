/*
 * pam_unix password management
 *
 * Main coding by Elliot Lee <sopwith@redhat.com>, Red Hat Software.
 * Copyright (C) 1996.
 * Copyright (c) Jan Rękorajski, 1999.
 * Copyright (c) Red Hat, Inc., 2007, 2008.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <syslog.h>
#include <shadow.h>
#include <time.h>		/* for time() */
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>

#include "pam_inline.h"
#include "pam_i18n.h"
#include "pam_cc_compat.h"
#include "md5.h"
#include "support.h"
#include "passverify.h"
#include "bigcrypt.h"

#ifdef HAVE_NIS
# include <rpc/rpc.h>
# include <rpcsvc/yp_prot.h>
# include <rpcsvc/ypclnt.h>

# include "yppasswd.h"

# if !defined(HAVE_DECL_GETRPCPORT) &&!defined(HAVE_RPCB_GETADDR)
extern int getrpcport(const char *host, unsigned long prognum,
		      unsigned long versnum, unsigned int proto);
# endif				/* GNU libc 2.1 */
#endif

/*
   How it works:
   Gets in username (has to be done) from the calling program
   Does authentication of user (only if we are not running as root)
   Gets new password/checks for sanity
   Sets it.
 */

#define MAX_PASSWD_TRIES	3

#ifdef HAVE_NIS
#ifdef HAVE_RPCB_GETADDR
static unsigned short
__taddr2port (const struct netconfig *nconf, const struct netbuf *nbuf)
{
  unsigned short port = 0;
  struct __rpc_sockinfo si;
  struct sockaddr_in *sin;
  struct sockaddr_in6 *sin6;
  if (!__rpc_nconf2sockinfo(nconf, &si))
    return 0;

  switch (si.si_af)
    {
    case AF_INET:
      sin = nbuf->buf;
      port = sin->sin_port;
      break;
    case AF_INET6:
      sin6 = nbuf->buf;
      port = sin6->sin6_port;
      break;
    default:
      break;
    }

  return htons (port);
}
#endif

static char *getNISserver(pam_handle_t *pamh, unsigned long long ctrl)
{
	char *master;
	char *domainname;
	int port, err;
#if defined(HAVE_RPCB_GETADDR)
	struct netconfig *nconf;
	struct netbuf svcaddr;
	char addrbuf[INET6_ADDRSTRLEN];
	void *handle;
	int found;
#endif


#ifdef HAVE_YP_GET_DEFAULT_DOMAIN
	if ((err = yp_get_default_domain(&domainname)) != 0) {
		pam_syslog(pamh, LOG_WARNING, "can't get local yp domain: %s",
			 yperr_string(err));
		return NULL;
	}
#elif defined(HAVE_GETDOMAINNAME)
	char domainname_res[256];

	if (getdomainname (domainname_res, sizeof (domainname_res)) == 0)
	  {
	    if (strcmp (domainname_res, "(none)") == 0)
	      {
		/* If domainname is not set, some systems will return "(none)" */
		domainname_res[0] = '\0';
	      }
	    domainname = domainname_res;
	  }
	else domainname = NULL;
#endif

	if ((err = yp_master(domainname, "passwd.byname", &master)) != 0) {
		pam_syslog(pamh, LOG_WARNING, "can't find the master ypserver: %s",
			 yperr_string(err));
		return NULL;
	}
#ifdef HAVE_RPCB_GETADDR
	svcaddr.len = 0;
	svcaddr.maxlen = sizeof (addrbuf);
	svcaddr.buf = addrbuf;
	port = 0;
	found = 0;

	handle = setnetconfig();
	while ((nconf = getnetconfig(handle)) != NULL) {
	  if (!strcmp(nconf->nc_proto, "udp")) {
	    if (rpcb_getaddr(YPPASSWDPROG, YPPASSWDPROC_UPDATE,
			     nconf, &svcaddr, master)) {
              port = __taddr2port (nconf, &svcaddr);
              endnetconfig (handle);
              found=1;
              break;
            }

	    if (rpc_createerr.cf_stat != RPC_UNKNOWNHOST) {
	      clnt_pcreateerror (master);
              pam_syslog (pamh, LOG_ERR,
			  "rpcb_getaddr (%s) failed!", master);
              return NULL;
            }
	  }
	}

	if (!found) {
	  pam_syslog (pamh, LOG_ERR,
		      "Cannot find suitable transport for protocol 'udp'");
	  return NULL;
	}
#else
	port = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE, IPPROTO_UDP);
#endif
	if (port == 0) {
		pam_syslog(pamh, LOG_WARNING,
		         "yppasswdd not running on NIS master host");
		return NULL;
	}
	if (port >= IPPORT_RESERVED) {
		pam_syslog(pamh, LOG_WARNING,
		         "yppasswd daemon running on illegal port");
		return NULL;
	}
	if (on(UNIX_DEBUG, ctrl)) {
	  pam_syslog(pamh, LOG_DEBUG, "Use NIS server on %s with port %d",
		     master, port);
	}
	return master;
}
#endif

#ifdef WITH_SELINUX

static int _unix_run_update_binary(pam_handle_t *pamh, unsigned long long ctrl, const char *user,
    const char *fromwhat, const char *towhat, int remember)
{
    int retval, child, fds[2];
    struct sigaction newsa, oldsa;

    D(("called."));
    /* create a pipe for the password */
    if (pipe(fds) != 0) {
	D(("could not make pipe"));
	return PAM_AUTH_ERR;
    }

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
	const char *args[] = { NULL, NULL, NULL, NULL, NULL, NULL };
        char buffer[16];

	/* XXX - should really tidy up PAM here too */

	/* reopen stdin as pipe */
	if (dup2(fds[0], STDIN_FILENO) != STDIN_FILENO) {
		pam_syslog(pamh, LOG_ERR, "dup2 of %s failed: %m", "stdin");
		_exit(PAM_AUTHINFO_UNAVAIL);
	}

	if (pam_modutil_sanitize_helper_fds(pamh, PAM_MODUTIL_IGNORE_FD,
					    PAM_MODUTIL_PIPE_FD,
					    PAM_MODUTIL_PIPE_FD) < 0) {
		_exit(PAM_AUTHINFO_UNAVAIL);
	}

	/* exec binary helper */
	args[0] = UPDATE_HELPER;
	args[1] = user;
	args[2] = "update";
	if (on(UNIX_SHADOW, ctrl))
		args[3] = "1";
	else
		args[3] = "0";

        pam_sprintf(buffer, "%d", remember);
        args[4] = buffer;

	DIAG_PUSH_IGNORE_CAST_QUAL;
	execve(UPDATE_HELPER, (char *const *) args, envp);
	DIAG_POP_IGNORE_CAST_QUAL;

	/* should not get here: exit with error */
	pam_syslog(pamh, LOG_ERR, "failed to execute %s: %m", UPDATE_HELPER);
	_exit(PAM_AUTHINFO_UNAVAIL);
    } else if (child > 0) {
	/* wait for child */
	/* if the stored password is NULL */
        int rc=0;
	if (fromwhat) {
	    int len = strlen(fromwhat);

	    if (len > PAM_MAX_RESP_SIZE)
	      len = PAM_MAX_RESP_SIZE;
	    pam_modutil_write(fds[1], fromwhat, len);
	}
        pam_modutil_write(fds[1], "", 1);
	if (towhat) {
	    int len = strlen(towhat);

	    if (len > PAM_MAX_RESP_SIZE)
	      len = PAM_MAX_RESP_SIZE;
	    pam_modutil_write(fds[1], towhat, len);
        }
        pam_modutil_write(fds[1], "", 1);

	close(fds[0]);       /* close here to avoid possible SIGPIPE above */
	close(fds[1]);
	/* wait for helper to complete: */
	while ((rc=waitpid(child, &retval, 0)) < 0 && errno == EINTR);
	if (rc<0) {
	  pam_syslog(pamh, LOG_ERR, "unix_update waitpid failed: %m");
	  retval = PAM_AUTHTOK_ERR;
	} else if (!WIFEXITED(retval)) {
          pam_syslog(pamh, LOG_ERR, "unix_update abnormal exit: %d", retval);
          retval = PAM_AUTHTOK_ERR;
        } else {
	  retval = WEXITSTATUS(retval);
	}
    } else {
	D(("fork failed"));
	close(fds[0]);
	close(fds[1]);
	retval = PAM_AUTH_ERR;
    }

    if (off(UNIX_NOREAP, ctrl)) {
        sigaction(SIGCHLD, &oldsa, NULL);   /* restore old signal handler */
    }

    return retval;
}
#endif

static int check_old_password(const char *forwho, const char *newpass)
{
	char *buf = NULL;
	char *s_pas;
	int retval = PAM_SUCCESS;
	FILE *opwfile;
	size_t n = 0;
	size_t len = strlen(forwho);

	opwfile = fopen(OLD_PASSWORDS_FILE, "re");
	if (opwfile == NULL)
		return PAM_ABORT;

	for (; getline(&buf, &n, opwfile) != -1; pam_overwrite_n(buf, n)) {
		if (!strncmp(buf, forwho, len) && buf[len] == ':') {
			char *sptr;
			buf[strlen(buf) - 1] = '\0';
			/* s_luser = */ strtok_r(buf, ":", &sptr);
			/* s_uid = */ strtok_r(NULL, ":", &sptr);
			/* s_npas = */ strtok_r(NULL, ":", &sptr);
			s_pas = strtok_r(NULL, ",", &sptr);
			while (s_pas != NULL) {
				char *md5pass = Goodcrypt_md5(newpass, s_pas);
				if (md5pass == NULL || !strcmp(md5pass, s_pas)) {
					_pam_delete(md5pass);
					retval = PAM_AUTHTOK_ERR;
					break;
				}
				s_pas = strtok_r(NULL, ",", &sptr);
				_pam_delete(md5pass);
			}
			break;
		}
	}
	pam_overwrite_n(buf, n);
	free(buf);
	fclose(opwfile);

	return retval;
}

static int _do_setpass(pam_handle_t* pamh, const char *forwho,
		       const char *fromwhat,
		       char *towhat, unsigned long long ctrl, int remember)
{
	struct passwd *pwd = NULL;
	int retval = 0;
	int unlocked = 0;

	D(("called"));

	pwd = getpwnam(forwho);

	if (pwd == NULL) {
		retval = PAM_AUTHTOK_ERR;
		goto done;
	}

	if (on(UNIX_NIS, ctrl) && _unix_comesfromsource(pamh, forwho, 0, 1)) {
#ifdef HAVE_NIS
	  char *master;

	  if ((master=getNISserver(pamh, ctrl)) != NULL) {
		struct timeval timeout;
		struct yppasswd yppwd;
		CLIENT *clnt;
		int status;
		enum clnt_stat err;

		/* Unlock passwd file to avoid deadlock */
		unlock_pwdf();
		unlocked = 1;

		/* Initialize password information */
		yppwd.newpw.pw_passwd = pwd->pw_passwd;
		yppwd.newpw.pw_name = pwd->pw_name;
		yppwd.newpw.pw_uid = pwd->pw_uid;
		yppwd.newpw.pw_gid = pwd->pw_gid;
		yppwd.newpw.pw_gecos = pwd->pw_gecos;
		yppwd.newpw.pw_dir = pwd->pw_dir;
		yppwd.newpw.pw_shell = pwd->pw_shell;
		yppwd.oldpass = fromwhat ? strdup (fromwhat) : strdup ("");
		yppwd.newpw.pw_passwd = towhat;

		D(("Set password %s for %s", yppwd.newpw.pw_passwd, forwho));

		/* The yppasswd.x file said `unix authentication required',
		 * so I added it. This is the only reason it is in here.
		 * My yppasswdd doesn't use it, but maybe some others out there
		 * do.                                        --okir
		 */
		clnt = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
		clnt->cl_auth = authunix_create_default();
		memset((char *) &status, '\0', sizeof(status));
		timeout.tv_sec = 25;
		timeout.tv_usec = 0;
		err = clnt_call(clnt, YPPASSWDPROC_UPDATE,
				(xdrproc_t) xdr_yppasswd, (char *) &yppwd,
				(xdrproc_t) xdr_int, (char *) &status,
				timeout);

		free (yppwd.oldpass);

		if (err) {
			_make_remark(pamh, ctrl, PAM_TEXT_INFO,
				clnt_sperrno(err));
		} else if (status) {
			D(("Error while changing NIS password.\n"));
		}
		D(("The password has%s been changed on %s.",
		   (err || status) ? " not" : "", master));
		pam_syslog(pamh, LOG_NOTICE, "password%s changed for %s on %s",
			 (err || status) ? " not" : "", pwd->pw_name, master);

		auth_destroy(clnt->cl_auth);
		clnt_destroy(clnt);
		if (err || status) {
			_make_remark(pamh, ctrl, PAM_TEXT_INFO,
				_("NIS password could not be changed."));
			retval = PAM_TRY_AGAIN;
		}
#ifdef PAM_DEBUG
		sleep(5);
#endif
	    } else {
		    retval = PAM_TRY_AGAIN;
	    }
#else
          if (on(UNIX_DEBUG, ctrl)) {
            pam_syslog(pamh, LOG_DEBUG, "No NIS support available");
          }

          retval = PAM_TRY_AGAIN;
#endif
	}

	if (_unix_comesfromsource(pamh, forwho, 1, 0)) {
		if(unlocked) {
			if (lock_pwdf() != PAM_SUCCESS) {
				return PAM_AUTHTOK_LOCK_BUSY;
			}
		}
#ifdef WITH_SELINUX
	        if (unix_selinux_confined())
			  return _unix_run_update_binary(pamh, ctrl, forwho, fromwhat, towhat, remember);
#endif
		/* first, save old password */
		if (save_old_password(pamh, forwho, fromwhat, remember)) {
			retval = PAM_AUTHTOK_ERR;
			goto done;
		}
		if (on(UNIX_SHADOW, ctrl) || is_pwd_shadowed(pwd)) {
			retval = unix_update_shadow(pamh, forwho, towhat);
			if (retval == PAM_SUCCESS)
				if (!is_pwd_shadowed(pwd))
					retval = unix_update_passwd(pamh, forwho, "x");
		} else {
			retval = unix_update_passwd(pamh, forwho, towhat);
		}
	}


done:
	unlock_pwdf();

	return retval;
}

static int _unix_verify_shadow(pam_handle_t *pamh, const char *user, unsigned long long ctrl)
{
	struct passwd *pwent = NULL;	/* Password and shadow password */
	struct spwd *spent = NULL;	/* file entries for the user */
	int daysleft;
	int retval;

	retval = get_account_info(pamh, user, &pwent, &spent);
	if (retval == PAM_USER_UNKNOWN) {
		return retval;
	}

	if (retval == PAM_SUCCESS && spent == NULL)
		return PAM_SUCCESS;

	if (retval == PAM_UNIX_RUN_HELPER) {
		retval = _unix_run_verify_binary(pamh, ctrl, user, &daysleft);
		if (retval == PAM_AUTH_ERR || retval == PAM_USER_UNKNOWN)
			return retval;
	}
	else if (retval == PAM_SUCCESS)
		retval = check_shadow_expiry(pamh, spent, &daysleft);

	if (on(UNIX__IAMROOT, ctrl) || retval == PAM_NEW_AUTHTOK_REQD)
		return PAM_SUCCESS;

	return retval;
}

static int _pam_unix_approve_pass(pam_handle_t * pamh
				  ,unsigned long long ctrl
				  ,const char *pass_old
				  ,const char *pass_new,
                                  int pass_min_len)
{
	const void *user;
	const char *remark = NULL;
	int retval = PAM_SUCCESS;

	D(("&new=%p, &old=%p", pass_new, pass_old));
	D(("new=[%s]", pass_new));
	D(("old=[%s]", pass_old));

	if (pass_new == NULL || (pass_old && !strcmp(pass_old, pass_new))) {
		if (on(UNIX_DEBUG, ctrl)) {
			pam_syslog(pamh, LOG_DEBUG, "bad authentication token");
		}
		_make_remark(pamh, ctrl, PAM_ERROR_MSG, pass_new == NULL ?
			_("No password has been supplied.") :
			_("The password has not been changed."));
		return PAM_AUTHTOK_ERR;
	}
	/*
	 * if one wanted to hardwire authentication token strength
	 * checking this would be the place - AGM
	 */

	retval = pam_get_item(pamh, PAM_USER, &user);
	if (retval != PAM_SUCCESS) {
		if (on(UNIX_DEBUG, ctrl)) {
			pam_syslog(pamh, LOG_ERR, "Can not get username");
			return PAM_AUTHTOK_ERR;
		}
	}

	if (strlen(pass_new) > PAM_MAX_RESP_SIZE) {
		remark = _("You must choose a shorter password.");
		D(("length exceeded [%s]", remark));
	} else if (off(UNIX__IAMROOT, ctrl)) {
		if ((int)strlen(pass_new) < pass_min_len) {
		  remark = _("You must choose a longer password.");
		  D(("length check [%s]", remark));
		}
		if (on(UNIX_REMEMBER_PASSWD, ctrl)) {
			if ((retval = check_old_password(user, pass_new)) == PAM_AUTHTOK_ERR)
			  remark = _("Password has been already used. Choose another.");
			if (retval == PAM_ABORT) {
				pam_syslog(pamh, LOG_ERR, "can't open %s file to check old passwords",
					OLD_PASSWORDS_FILE);
				return retval;
			}
		}
	}
	if (remark) {
		_make_remark(pamh, ctrl, PAM_ERROR_MSG, remark);
		retval = PAM_AUTHTOK_ERR;
	}
	return retval;
}

int
pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	unsigned long long ctrl, lctrl;
	int retval;
	int remember = -1;
	int rounds = 0;
	int pass_min_len = 0;
	struct passwd *pwd;

	/* <DO NOT free() THESE> */
	const char *user;
	const void *item;
	const char *pass_old, *pass_new;
	/* </DO NOT free() THESE> */

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, &remember, &rounds, &pass_min_len,
	                 argc, argv);

	/*
	 * First get the name of a user
	 */
	retval = pam_get_user(pamh, &user, NULL);
	if (retval == PAM_SUCCESS) {
		/*
		 * Various libraries at various times have had bugs related to
		 * '+' or '-' as the first character of a user name. Don't
		 * allow them.
		 */
		if (user[0] == '-' || user[0] == '+') {
			pam_syslog(pamh, LOG_NOTICE, "bad username [%s]", user);
			return PAM_USER_UNKNOWN;
		}
		if (retval == PAM_SUCCESS && on(UNIX_DEBUG, ctrl))
			pam_syslog(pamh, LOG_DEBUG, "username [%s] obtained",
			         user);
	} else {
		if (on(UNIX_DEBUG, ctrl))
			pam_syslog(pamh, LOG_DEBUG,
			         "password - could not identify user");
		return retval;
	}

	D(("Got username of %s", user));

	/*
	 * Before we do anything else, check to make sure that the user's
	 * info is in one of the databases we can modify from this module,
	 * which currently is 'files' and 'nis'.  We have to do this because
	 * getpwnam() doesn't tell you *where* the information it gives you
	 * came from, nor should it.  That's our job.
	 */
	if (_unix_getpwnam(pamh, user, 1, on(UNIX_NIS, ctrl), &pwd) == 0) {
		pam_syslog(pamh, LOG_DEBUG,
			 "user \"%s\" does not exist in /etc/passwd%s",
			 user, on(UNIX_NIS, ctrl) ? " or NIS" : "");
		return PAM_USER_UNKNOWN;
	} else if (pwd == NULL) {
		pam_syslog(pamh, LOG_DEBUG,
			 "user \"%s\" has corrupted passwd entry",
			 user);
		return PAM_USER_UNKNOWN;
	}

	/*
	 * This is not an AUTH module!
	 */
	if (on(UNIX__NONULL, ctrl))
		set(UNIX__NULLOK, ctrl);

	if (on(UNIX__PRELIM, ctrl)) {
		/*
		 * obtain and verify the current password (OLDAUTHTOK) for
		 * the user.
		 */
		D(("prelim check"));

		if (_unix_blankpasswd(pamh, ctrl, user)) {
			return PAM_SUCCESS;
		} else if (off(UNIX__IAMROOT, ctrl) ||
			   (on(UNIX_NIS, ctrl) && _unix_comesfromsource(pamh, user, 0, 1))) {
			/* instruct user what is happening */
			if (off(UNIX__QUIET, ctrl)) {
				retval = pam_info(pamh, _("Changing password for %s."), user);
				if (retval != PAM_SUCCESS)
					return retval;
			}
			retval = pam_get_authtok(pamh, PAM_OLDAUTHTOK, &pass_old, NULL);

			if (retval != PAM_SUCCESS) {
				pam_syslog(pamh, LOG_NOTICE,
				    "password - (old) token not obtained");
				return retval;
			}
			/* verify that this is the password for this user */

			retval = _unix_verify_password(pamh, user, pass_old, ctrl);
		} else {
			D(("process run by root so do nothing this time around"));
			pass_old = NULL;
			retval = PAM_SUCCESS;	/* root doesn't have to */
		}

		if (retval != PAM_SUCCESS) {
			D(("Authentication failed"));
			pass_old = NULL;
			return retval;
		}
		pass_old = NULL;
		retval = _unix_verify_shadow(pamh,user, ctrl);
		if (retval == PAM_AUTHTOK_ERR) {
			if (off(UNIX__IAMROOT, ctrl))
				_make_remark(pamh, ctrl, PAM_ERROR_MSG,
					     _("You must wait longer to change your password."));
			else
				retval = PAM_SUCCESS;
		}
	} else if (on(UNIX__UPDATE, ctrl)) {
		/*
		 * tpass is used below to store the _pam_md() return; it
		 * should be _pam_delete()'d.
		 */

		char *tpass = NULL;
		int retry = 0;

		/*
		 * obtain the proposed password
		 */

		D(("do update"));

		/*
		 * get the old token back. NULL was ok only if root [at this
		 * point we assume that this has already been enforced on a
		 * previous call to this function].
		 */

		retval = pam_get_item(pamh, PAM_OLDAUTHTOK, &item);

		if (retval != PAM_SUCCESS) {
			pam_syslog(pamh, LOG_NOTICE, "user not authenticated");
			return retval;
		}
		pass_old = item;
		D(("pass_old [%s]", pass_old));

		D(("get new password now"));

		lctrl = ctrl;

		if (on(UNIX_USE_AUTHTOK, lctrl)) {
			set(UNIX_USE_FIRST_PASS, lctrl);
		}
		if (on(UNIX_USE_FIRST_PASS, lctrl)) {
			retry = MAX_PASSWD_TRIES-1;
		}
		retval = PAM_AUTHTOK_ERR;
		while ((retval != PAM_SUCCESS) && (retry++ < MAX_PASSWD_TRIES)) {
			/*
			 * use_authtok is to force the use of a previously entered
			 * password -- needed for pluggable password strength checking
			 */

			retval = pam_get_authtok(pamh, PAM_AUTHTOK, &pass_new, NULL);

			if (retval != PAM_SUCCESS) {
				if (on(UNIX_DEBUG, ctrl)) {
					pam_syslog(pamh, LOG_ERR,
						 "password - new password not obtained");
				}
				pass_old = NULL;	/* tidy up */
				return retval;
			}
			D(("returned to _unix_chauthtok"));

			/*
			 * At this point we know who the user is and what they
			 * propose as their new password. Verify that the new
			 * password is acceptable.
			 */

			if (*pass_new == '\0') {	/* "\0" password = NULL */
				pass_new = NULL;
			}
			retval = _pam_unix_approve_pass(pamh, ctrl, pass_old,
			                                pass_new, pass_min_len);

			if (retval != PAM_SUCCESS) {
				pam_set_item(pamh, PAM_AUTHTOK, NULL);
			}
		}

		if (retval != PAM_SUCCESS) {
			pam_syslog(pamh, LOG_NOTICE,
			         "new password not acceptable");
			pass_new = pass_old = NULL;	/* tidy up */
			return retval;
		}
		if (lock_pwdf() != PAM_SUCCESS) {
			return PAM_AUTHTOK_LOCK_BUSY;
		}

		if (pass_old) {
			retval = _unix_verify_password(pamh, user, pass_old, ctrl);
			if (retval != PAM_SUCCESS) {
				pam_syslog(pamh, LOG_NOTICE, "user password changed by another process");
				unlock_pwdf();
				return retval;
			}
		}

		retval = _unix_verify_shadow(pamh, user, ctrl);
		if (retval != PAM_SUCCESS) {
			pam_syslog(pamh, LOG_NOTICE, "user shadow entry expired");
			unlock_pwdf();
			return retval;
		}

		retval = _pam_unix_approve_pass(pamh, ctrl, pass_old, pass_new,
		                                pass_min_len);
		if (retval != PAM_SUCCESS) {
			pam_syslog(pamh, LOG_NOTICE,
			         "new password not acceptable 2");
			pass_new = pass_old = NULL;	/* tidy up */
			unlock_pwdf();
			return retval;
		}

		/*
		 * By reaching here we have approved the passwords and must now
		 * rebuild the password database file.
		 */

		/*
		 * First we encrypt the new password.
		 */

		tpass = create_password_hash(pamh, pass_new, ctrl, rounds);
		if (tpass == NULL) {
			pam_syslog(pamh, LOG_CRIT,
				"crypt() failure or out of memory for password");
			pass_new = pass_old = NULL;	/* tidy up */
			unlock_pwdf();
			return PAM_BUF_ERR;
		}

		D(("password processed"));

		/* update the password database(s) -- race conditions..? */

		retval = _do_setpass(pamh, user, pass_old, tpass, ctrl,
		                     remember);
	        /* _do_setpass has called unlock_pwdf for us */

		_pam_delete(tpass);
		pass_old = pass_new = NULL;
	} else {		/* something has broken with the module */
		pam_syslog(pamh, LOG_CRIT,
		         "password received unknown request");
		retval = PAM_ABORT;
	}

	D(("retval was %d", retval));

	return retval;
}
