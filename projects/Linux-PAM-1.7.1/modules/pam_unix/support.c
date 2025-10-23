/*
 * Copyright information at end of file.
 */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <pwd.h>
#include <shadow.h>
#include <limits.h>
#include <utmp.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/resource.h>
#ifdef HAVE_NIS
#include <rpcsvc/ypclnt.h>
#endif

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <security/pam_modutil.h>

#include "pam_cc_compat.h"
#include "pam_inline.h"
#include "support.h"
#include "passverify.h"

/* this is a front-end for module-application conversations */

int _make_remark(pam_handle_t * pamh, unsigned long long ctrl,
		    int type, const char *text)
{
	int retval = PAM_SUCCESS;

	if (off(UNIX__QUIET, ctrl)) {
		retval = pam_prompt(pamh, type, NULL, "%s", text);
	}
	return retval;
}

static int _unix_strtoi(const char *str, int minval, int *result)
{
	char *ep;
	long value = strtol(str, &ep, 10);
	if (value < minval || value > INT_MAX || str == ep || *ep != '\0') {
		*result = minval;
		return -1;
	}
	*result = (int)value;
	return 0;
}

/*
 * set the control flags for the UNIX module.
 */

unsigned long long _set_ctrl(pam_handle_t *pamh, int flags, int *remember,
			     int *rounds, int *pass_min_len, int argc,
			     const char **argv)
{
	unsigned long long ctrl;
	char *val;
	int j;

	D(("called."));

	ctrl = UNIX_DEFAULTS;	/* the default selection of options */

	/* set some flags manually */

	if (getuid() == 0 && !(flags & PAM_CHANGE_EXPIRED_AUTHTOK)) {
		D(("IAMROOT"));
		set(UNIX__IAMROOT, ctrl);
	}
	if (flags & PAM_UPDATE_AUTHTOK) {
		D(("UPDATE_AUTHTOK"));
		set(UNIX__UPDATE, ctrl);
	}
	if (flags & PAM_PRELIM_CHECK) {
		D(("PRELIM_CHECK"));
		set(UNIX__PRELIM, ctrl);
	}
	if (flags & PAM_SILENT) {
		D(("SILENT"));
		set(UNIX__QUIET, ctrl);
	}

	/* preset encryption method with value from /etc/login.defs */
	val = pam_modutil_search_key(pamh, LOGIN_DEFS, "ENCRYPT_METHOD");
	if (val) {
	  for (j = 0; j < UNIX_CTRLS_; ++j) {
	    if (unix_args[j].token && unix_args[j].is_hash_algo
		&& !strncasecmp(val, unix_args[j].token, strlen(unix_args[j].token))) {
	      break;
	    }
	  }
	  if (j >= UNIX_CTRLS_) {
	    pam_syslog(pamh, LOG_WARNING, "unrecognized ENCRYPT_METHOD value [%s]", val);
	  } else {
	    ctrl &= unix_args[j].mask;	/* for turning things off */
	    ctrl |= unix_args[j].flag;	/* for turning things on  */
	  }
	  free (val);
	}

	/* now parse the arguments to this module */

	for (; argc-- > 0; ++argv) {
		const char *str = NULL;

		D(("pam_unix arg: %s", *argv));

		for (j = 0; j < UNIX_CTRLS_; ++j) {
			if (unix_args[j].token
			    && (str = pam_str_skip_prefix_len(*argv,
							      unix_args[j].token,
							      strlen(unix_args[j].token))) != NULL) {
				break;
			}
		}

		if (str == NULL) {
			pam_syslog(pamh, LOG_ERR,
			         "unrecognized option [%s]", *argv);
		} else {
			/* special cases */
			if (j == UNIX_REMEMBER_PASSWD) {
				if (remember == NULL) {
					pam_syslog(pamh, LOG_ERR,
					    "option remember not allowed for this module type");
					continue;
				}
				if (_unix_strtoi(str, -1, remember)) {
					pam_syslog(pamh, LOG_ERR,
					    "option remember invalid [%s]", str);
					continue;
				}
				if (*remember > 400)
					*remember = 400;
			} else if (j == UNIX_MIN_PASS_LEN) {
				if (pass_min_len == NULL) {
					pam_syslog(pamh, LOG_ERR,
					    "option minlen not allowed for this module type");
					continue;
				}
				if (_unix_strtoi(str, 0, pass_min_len)) {
					pam_syslog(pamh, LOG_ERR,
					    "option minlen invalid [%s]", str);
					continue;
				}
			} else if (j == UNIX_ALGO_ROUNDS) {
				if (rounds == NULL) {
					pam_syslog(pamh, LOG_ERR,
					    "option rounds not allowed for this module type");
					continue;
				}
				if (_unix_strtoi(str, 0, rounds)) {
					pam_syslog(pamh, LOG_ERR,
					    "option rounds invalid [%s]", str);
					continue;
				}
			}

			ctrl &= unix_args[j].mask;	/* for turning things off */
			ctrl |= unix_args[j].flag;	/* for turning things on  */
		}
	}

	if (UNIX_DES_CRYPT(ctrl)
	    && pass_min_len && *pass_min_len > 8)
	  {
	    pam_syslog (pamh, LOG_NOTICE, "Password minlen reset to 8 characters");
	    *pass_min_len = 8;
	  }

	if (flags & PAM_DISALLOW_NULL_AUTHTOK) {
		D(("DISALLOW_NULL_AUTHTOK"));
		set(UNIX__NONULL, ctrl);
	}

	/* Read number of rounds for sha256, sha512 and yescrypt */
	if (off(UNIX_ALGO_ROUNDS, ctrl) && rounds != NULL) {
		const char *key = NULL;
		if (on(UNIX_YESCRYPT_PASS, ctrl))
			key = "YESCRYPT_COST_FACTOR";
		else if (on(UNIX_SHA256_PASS, ctrl) || on(UNIX_SHA512_PASS, ctrl))
			key = "SHA_CRYPT_MAX_ROUNDS";
		else
			key = NULL;

		if (key != NULL) {
			val = pam_modutil_search_key(pamh, LOGIN_DEFS, key);
			if (val) {
				if (_unix_strtoi(val, 0, rounds))
					pam_syslog(pamh, LOG_ERR,
					    "option %s invalid [%s]", key, val);
				else
					set(UNIX_ALGO_ROUNDS, ctrl);
				free (val);
			}
		}
	}

	/* Set default rounds for blowfish, gost-yescrypt and yescrypt */
	if (off(UNIX_ALGO_ROUNDS, ctrl) && rounds != NULL) {
		if (on(UNIX_BLOWFISH_PASS, ctrl) ||
		    on(UNIX_GOST_YESCRYPT_PASS, ctrl) ||
		    on(UNIX_YESCRYPT_PASS, ctrl)) {
			*rounds = 5;
			set(UNIX_ALGO_ROUNDS, ctrl);
		}
	}

	/* Enforce sane "rounds" values */
	if (on(UNIX_ALGO_ROUNDS, ctrl)) {
		if (on(UNIX_GOST_YESCRYPT_PASS, ctrl) ||
		    on(UNIX_YESCRYPT_PASS, ctrl)) {
			if (*rounds < 3)
				*rounds = 3;
			else if (*rounds > 11)
				*rounds = 11;
		} else if (on(UNIX_BLOWFISH_PASS, ctrl)) {
			if (*rounds < 4)
				*rounds = 4;
			else if (*rounds > 31)
				*rounds = 31;
		} else if (on(UNIX_SHA256_PASS, ctrl) || on(UNIX_SHA512_PASS, ctrl)) {
			if ((*rounds < 1000) || (*rounds == INT_MAX)) {
				/* don't care about bogus values */
				*rounds = 0;
				unset(UNIX_ALGO_ROUNDS, ctrl);
			} else if (*rounds >= 10000000) {
				*rounds = 9999999;
			}
		}
	}

	/* auditing is a more sensitive version of debug */

	if (on(UNIX_AUDIT, ctrl)) {
		set(UNIX_DEBUG, ctrl);
	}
	/* return the set of flags */

	D(("done."));
	return ctrl;
}

/* ************************************************************** *
 * Useful non-trivial functions                                   *
 * ************************************************************** */

  /*
   * the following is used to keep track of the number of times a user fails
   * to authenticate themself.
   */

#define FAIL_PREFIX                   "-UN*X-FAIL-"
#define UNIX_MAX_RETRIES              3

struct _pam_failed_auth {
	char *user;		/* user that's failed to be authenticated */
	char *name;		/* attempt from user with name */
	int uid;		/* uid of calling user */
	int euid;		/* euid of calling process */
	int count;		/* number of failures so far */
};

#ifndef PAM_DATA_REPLACE
#error "Need to get an updated libpam 0.52 or better"
#endif

static void _cleanup_failures(pam_handle_t * pamh, void *fl, int err)
{
	int quiet;
	const void *service = NULL;
	const void *ruser = NULL;
	const void *rhost = NULL;
	const void *tty = NULL;
	struct _pam_failed_auth *failure;

	D(("called"));

	quiet = err & PAM_DATA_SILENT;	/* should we log something? */
	err &= PAM_DATA_REPLACE;	/* are we just replacing data? */
	failure = (struct _pam_failed_auth *) fl;

	if (failure != NULL) {

		if (!quiet && !err) {	/* under advisement from Sun,may go away */

			/* log the number of authentication failures */
			if (failure->count > 1) {
				(void) pam_get_item(pamh, PAM_SERVICE,
						    &service);
				(void) pam_get_item(pamh, PAM_RUSER,
						    &ruser);
				(void) pam_get_item(pamh, PAM_RHOST,
						    &rhost);
				(void) pam_get_item(pamh, PAM_TTY,
						    &tty);
				pam_syslog(pamh, LOG_NOTICE,
				         "%d more authentication failure%s; "
				         "logname=%s uid=%d euid=%d "
				         "tty=%s ruser=%s rhost=%s "
				         "%s%s",
				         failure->count - 1, failure->count == 2 ? "" : "s",
				         failure->name, failure->uid, failure->euid,
				         tty ? (const char *)tty : "", ruser ? (const char *)ruser : "",
				         rhost ? (const char *)rhost : "",
				         (failure->user && failure->user[0] != '\0')
				          ? " user=" : "",
					 failure->user ? failure->user : ""
				);

				if (failure->count > UNIX_MAX_RETRIES) {
					pam_syslog(pamh, LOG_NOTICE,
						 "service(%s) ignoring max retries; %d > %d",
						 service == NULL ? "**unknown**" : (const char *)service,
						 failure->count,
						 UNIX_MAX_RETRIES);
				}
			}
		}
		_pam_delete(failure->user);	/* tidy up */
		_pam_delete(failure->name);	/* tidy up */
		free(failure);
	}
}

/*
 * _unix_getpwnam() searches only /etc/passwd and NIS to find user information
 */
static void _unix_cleanup(pam_handle_t *pamh UNUSED, void *data, int error_status UNUSED)
{
	free(data);
}

int _unix_getpwnam(pam_handle_t *pamh, const char *name,
		   int files, int nis, struct passwd **ret)
{
	char *buf = NULL;
	int matched = 0;

	if (!matched && files && strchr(name, ':') == NULL) {
		FILE *passwd;

		passwd = fopen("/etc/passwd", "re");
		if (passwd != NULL) {
			size_t n = 0, userlen;
			ssize_t r;

			userlen = strlen(name);

			while ((r = getline(&buf, &n, passwd)) != -1) {
				if ((size_t)r > userlen && (buf[userlen] == ':') &&
				    (strncmp(name, buf, userlen) == 0)) {
					char *p;

					p = buf + strlen(buf) - 1;
					while (isspace((unsigned char)*p) && (p >= buf)) {
						*p-- = '\0';
					}
					matched = 1;
					break;
				}
			}
			if (!matched) {
				_pam_drop(buf);
			}
			fclose(passwd);
		}
	}

#if defined(HAVE_NIS) && defined(HAVE_YP_GET_DEFAULT_DOMAIN) && defined (HAVE_YP_BIND) && defined (HAVE_YP_MATCH) && defined (HAVE_YP_UNBIND)
	if (!matched && nis) {
		char *userinfo = NULL, *domain = NULL;
		int len = 0, i;
		len = yp_get_default_domain(&domain);
		if (len == YPERR_SUCCESS) {
			len = yp_bind(domain);
		}
		if (len == YPERR_SUCCESS) {
			i = yp_match(domain, "passwd.byname", name,
				     strlen(name), &userinfo, &len);
			yp_unbind(domain);
			if (i == YPERR_SUCCESS && (buf = strdup(userinfo)) != NULL) {
				matched = 1;
			}
		}
	}
#else
	/* we don't have NIS support, make compiler happy. */
	(void) nis;
#endif

	if (matched && (ret != NULL)) {
		char *slogin, *spasswd, *suid, *sgid, *sgecos, *shome, *sshell, *p;
		size_t retlen;

		*ret = NULL;

		slogin = buf;

		spasswd = strchr(slogin, ':');
		if (spasswd == NULL) {
			goto fail;
		}
		*spasswd++ = '\0';

		suid = strchr(spasswd, ':');
		if (suid == NULL) {
			goto fail;
		}
		*suid++ = '\0';

		sgid = strchr(suid, ':');
		if (sgid == NULL) {
			goto fail;
		}
		*sgid++ = '\0';

		sgecos = strchr(sgid, ':');
		if (sgecos == NULL) {
			goto fail;
		}
		*sgecos++ = '\0';

		shome = strchr(sgecos, ':');
		if (shome == NULL) {
			goto fail;
		}
		*shome++ = '\0';

		sshell = strchr(shome, ':');
		if (sshell == NULL) {
			goto fail;
		}
		*sshell++ = '\0';

		retlen = sizeof(struct passwd) +
			 strlen(slogin) + 1 +
			 strlen(spasswd) + 1 +
			 strlen(sgecos) + 1 +
			 strlen(shome) + 1 +
			 strlen(sshell) + 1;
		*ret = calloc(retlen, sizeof(char));
		if (*ret == NULL) {
			goto fail;
		}

		(*ret)->pw_uid = strtol(suid, &p, 10);
		if ((strlen(suid) == 0) || (*p != '\0')) {
			goto fail;
		}

		(*ret)->pw_gid = strtol(sgid, &p, 10);
		if ((strlen(sgid) == 0) || (*p != '\0')) {
			goto fail;
		}

		p = ((char*)(*ret)) + sizeof(struct passwd);
		(*ret)->pw_name = strcpy(p, slogin);
		p += strlen(p) + 1;
		(*ret)->pw_passwd = strcpy(p, spasswd);
		p += strlen(p) + 1;
		(*ret)->pw_gecos = strcpy(p, sgecos);
		p += strlen(p) + 1;
		(*ret)->pw_dir = strcpy(p, shome);
		p += strlen(p) + 1;
		(*ret)->pw_shell = strcpy(p, sshell);

		_pam_drop(buf);
		if ((buf = pam_asprintf("_pam_unix_getpwnam_%s", name)) == NULL) {
			goto fail;
		}

		if (pam_set_data(pamh, buf,
				 *ret, _unix_cleanup) != PAM_SUCCESS) {
			goto fail;
		}
	}

	_pam_drop(buf);
	return matched;
fail:
	_pam_drop(buf);
	_pam_drop(*ret);
	return matched;
}

/*
 * _unix_comsefromsource() is a quick check to see if information about a given
 * user comes from a particular source (just files and nis for now)
 *
 */
int _unix_comesfromsource(pam_handle_t *pamh,
			  const char *name, int files, int nis)
{
	return _unix_getpwnam(pamh, name, files, nis, NULL);
}

/*
 * verify the password of a user
 */

#include <sys/types.h>
#include <sys/wait.h>

static int _unix_run_helper_binary(pam_handle_t *pamh, const char *passwd,
				   unsigned long long ctrl, const char *user)
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
	const char *args[] = { NULL, NULL, NULL, NULL };

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

	/* must set the real uid to 0 so the helper will not error
	   out if pam is called from setuid binary (su, sudo...) */
	if (setuid(0) == -1) {
	   D(("setuid failed"));
	   if (geteuid() == 0) {
	      _exit(PAM_AUTHINFO_UNAVAIL);
	   }
	}

	/* exec binary helper */
	args[0] = CHKPWD_HELPER;
	args[1] = user;
	if (off(UNIX__NONULL, ctrl)) {	/* this means we've succeeded */
	  args[2]="nullok";
	} else {
	  args[2]="nonull";
	}

	DIAG_PUSH_IGNORE_CAST_QUAL;
	execve(CHKPWD_HELPER, (char *const *) args, envp);
	DIAG_POP_IGNORE_CAST_QUAL;

	/* should not get here: exit with error */
	D(("helper binary is not available"));
	_exit(PAM_AUTHINFO_UNAVAIL);
    } else if (child > 0) {
	/* wait for child */
	/* if the stored password is NULL */
        int rc=0;
	if (passwd != NULL) {            /* send the password to the child */
	    size_t len = strlen(passwd);

	    if (len > PAM_MAX_RESP_SIZE)
	      len = PAM_MAX_RESP_SIZE;
	    if (write(fds[1], passwd, len) == -1 ||
	        write(fds[1], "", 1) == -1) {
	      pam_syslog (pamh, LOG_ERR, "Cannot send password to helper: %m");
	      retval = PAM_AUTH_ERR;
	    }
	    passwd = NULL;
	} else {                         /* blank password */
	    if (write(fds[1], "", 1) == -1) {
	      pam_syslog (pamh, LOG_ERR, "Cannot send password to helper: %m");
	      retval = PAM_AUTH_ERR;
	    }
	}
	close(fds[0]);       /* close here to avoid possible SIGPIPE above */
	close(fds[1]);
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

    D(("returning %d", retval));
    return retval;
}

/*
 * _unix_blankpasswd() is a quick check for a blank password
 *
 * returns TRUE if user does not have a password
 * - to avoid prompting for one in such cases (CG)
 */

int
_unix_blankpasswd (pam_handle_t *pamh, unsigned long long ctrl, const char *name)
{
	struct passwd *pwd = NULL;
	char *salt = NULL;
	int daysleft;
	int retval;
	int blank = 0;
	int execloop;
	int nonexistent_check = 1;

	D(("called"));

	/*
	 * This function does not have to be too smart if something goes
	 * wrong, return FALSE and let this case be treated somewhere
	 * else (CG)
	 */

	if (on(UNIX_NULLRESETOK, ctrl)) {
	    retval = _unix_verify_user(pamh, ctrl, name, &daysleft);
	    if (retval == PAM_NEW_AUTHTOK_REQD) {
	        /* password reset is enforced, allow authentication with empty password */
	        pam_syslog(pamh, LOG_DEBUG, "user [%s] has expired blank password, enabling nullok", name);
	        set(UNIX__NULLOK, ctrl);
	    }
	}

	if (on(UNIX__NONULL, ctrl))
		return 0;	/* will fail but don't let on yet */

	/* UNIX passwords area */

	/*
	 * Execute this loop twice: one checking the password hash of an existing
	 * user and another one for a non-existing user. This way the runtimes
	 * are equal, making it more difficult to differentiate existing from
	 * non-existing users.
	 */
	for (execloop = 0; execloop < 2; ++execloop) {
		retval = get_pwd_hash(pamh, name, &pwd, &salt);

		if (retval == PAM_UNIX_RUN_HELPER) {
			if (_unix_run_helper_binary(pamh, NULL, ctrl, name) == PAM_SUCCESS)
				blank = nonexistent_check;
		} else if (retval == PAM_USER_UNKNOWN) {
			name = "root";
			nonexistent_check = 0;
			continue;
		} else if (salt != NULL) {
			if (strlen(salt) == 0)
				blank = nonexistent_check;
		}
		name = "pam_unix_non_existent:";
		/* non-existent user check will not affect the blank value */
	}

	/* tidy up */
	if (salt)
		_pam_delete(salt);

	return blank;
}

int _unix_verify_password(pam_handle_t * pamh, const char *name
			  ,const char *p, unsigned long long ctrl)
{
	struct passwd *pwd = NULL;
	char *salt = NULL;
	char *data_name;
	char pw[PAM_MAX_RESP_SIZE + 1];
	int retval;


	D(("called"));

	if (off(UNIX_NODELAY, ctrl)) {
		D(("setting delay"));
		(void) pam_fail_delay(pamh, 2000000);	/* 2 sec delay for on failure */
	}

	/* locate the entry for this user */

	D(("locating user's record"));

	retval = get_pwd_hash(pamh, name, &pwd, &salt);

	if ((data_name = pam_asprintf("%s%s", FAIL_PREFIX, name)) == NULL) {
		pam_syslog(pamh, LOG_CRIT, "no memory for data-name");
	}

	if (p != NULL && strlen(p) > PAM_MAX_RESP_SIZE) {
		memset(pw, 0, sizeof(pw));
		p = strncpy(pw, p, sizeof(pw) - 1);
	}

	if (retval != PAM_SUCCESS) {
		if (retval == PAM_UNIX_RUN_HELPER) {
			D(("running helper binary"));
			retval = _unix_run_helper_binary(pamh, p, ctrl, name);
		} else {
			D(("user's record unavailable"));
			p = NULL;
			if (on(UNIX_AUDIT, ctrl)) {
				/* this might be a typo and the user has given a password
				   instead of a username. Careful with this. */
				pam_syslog(pamh, LOG_NOTICE,
				         "check pass; user (%s) unknown", name);
			} else {
				name = NULL;
				if (on(UNIX_DEBUG, ctrl) || pwd == NULL) {
				    pam_syslog(pamh, LOG_NOTICE,
				            "check pass; user unknown");
				} else {
				    /* don't log failure as another pam module can succeed */
				    goto cleanup;
				}
			}
		}
	} else {
		retval = verify_pwd_hash(pamh, p, salt, off(UNIX__NONULL, ctrl));
	}

	if (retval == PAM_SUCCESS) {
		if (data_name)	/* reset failures */
			pam_set_data(pamh, data_name, NULL, _cleanup_failures);
	} else {
		if (data_name != NULL) {
			struct _pam_failed_auth *new = NULL;
			const struct _pam_failed_auth *old = NULL;

			/* get a failure recorder */

			new = (struct _pam_failed_auth *)
			    malloc(sizeof(struct _pam_failed_auth));

			if (new != NULL) {

			    const char *login_name;
			    const void *void_old;


			    login_name = pam_modutil_getlogin(pamh);
			    if (login_name == NULL) {
				login_name = "";
			    }

			        new->user = strdup(name ? name : "");
				new->uid = getuid();
				new->euid = geteuid();
				new->name = strdup(login_name);

				/* any previous failures for this user ? */
				if (pam_get_data(pamh, data_name, &void_old)
				    == PAM_SUCCESS)
				        old = void_old;
				else
				        old = NULL;

				if (old != NULL) {
					new->count = old->count + 1;
					if (new->count >= UNIX_MAX_RETRIES) {
						retval = PAM_MAXTRIES;
					}
				} else {
					const void *service=NULL;
					const void *ruser=NULL;
					const void *rhost=NULL;
					const void *tty=NULL;

					(void) pam_get_item(pamh, PAM_SERVICE,
							    &service);
					(void) pam_get_item(pamh, PAM_RUSER,
							    &ruser);
					(void) pam_get_item(pamh, PAM_RHOST,
							    &rhost);
					(void) pam_get_item(pamh, PAM_TTY,
							    &tty);

					pam_syslog(pamh, LOG_NOTICE,
					         "authentication failure; "
					         "logname=%s uid=%d euid=%d "
					         "tty=%s ruser=%s rhost=%s "
					         "%s%s",
					         new->name, new->uid, new->euid,
					         tty ? (const char *)tty : "",
					         ruser ? (const char *)ruser : "",
					         rhost ? (const char *)rhost : "",
					         (new->user && new->user[0] != '\0')
					          ? " user=" : "",
					         new->user ? new->user : ""
					);
					new->count = 1;
				}

				pam_set_data(pamh, data_name, new, _cleanup_failures);

			} else {
				pam_syslog(pamh, LOG_CRIT,
				         "no memory for failure recorder");
			}
		}
	}

cleanup:
	pam_overwrite_array(pw); /* clear memory of the password */
	if (data_name)
		_pam_delete(data_name);
	if (salt)
		_pam_delete(salt);

	D(("done [%d].", retval));

	return retval;
}

int
_unix_verify_user(pam_handle_t *pamh,
                  unsigned long long ctrl,
                  const char *name,
                  int *daysleft)
{
    int retval;
    struct spwd *spent;
    struct passwd *pwent;

    retval = get_account_info(pamh, name, &pwent, &spent);
    if (retval == PAM_USER_UNKNOWN) {
        pam_syslog(pamh, LOG_ERR,
             "could not identify user (from getpwnam(%s))",
             name);
        return retval;
    }

    if (retval == PAM_SUCCESS && spent == NULL)
        return PAM_SUCCESS;

    if (retval == PAM_UNIX_RUN_HELPER) {
        retval = _unix_run_verify_binary(pamh, ctrl, name, daysleft);
        if (retval == PAM_AUTHINFO_UNAVAIL &&
            on(UNIX_BROKEN_SHADOW, ctrl))
            return PAM_SUCCESS;
    } else if (retval != PAM_SUCCESS) {
        if (on(UNIX_BROKEN_SHADOW,ctrl))
            return PAM_SUCCESS;
        else
            return retval;
    } else
        retval = check_shadow_expiry(pamh, spent, daysleft);

    return retval;
}

/* ****************************************************************** *
 * Copyright (c) Jan Rękorajski 1999.
 * Copyright (c) Andrew G. Morgan 1996-8.
 * Copyright (c) Alex O. Yuriev, 1996.
 * Copyright (c) Cristian Gafton 1996.
 * Copyright (c) Red Hat, Inc. 2007.
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
