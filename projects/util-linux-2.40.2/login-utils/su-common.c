/*
 * su(1) for Linux.  Run a shell with substitute user and group IDs.
 *
 * Copyright (C) 1992-2006 Free Software Foundation, Inc.
 * Copyright (C) 2012 SUSE Linux Products GmbH, Nuernberg
 * Copyright (C) 2016-2017 Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.  You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 *
 *
 * Based on an implementation by David MacKenzie <djm@gnu.ai.mit.edu>.
 */
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <security/pam_appl.h>
#ifdef HAVE_SECURITY_PAM_MISC_H
# include <security/pam_misc.h>
#elif defined(HAVE_SECURITY_OPENPAM_H)
# include <security/openpam.h>
#endif
#include <signal.h>
#include <sys/wait.h>
#include <syslog.h>
#include <utmpx.h>
#include <sys/time.h>

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_PTY
# include <pty.h>
# include <poll.h>
# include <sys/signalfd.h>
# include "pty-session.h"
# define USE_PTY
#endif

#include "err.h"

#include <stdbool.h>

#include "c.h"
#include "xalloc.h"
#include "nls.h"
#include "pathnames.h"
#include "env.h"
#include "closestream.h"
#include "strv.h"
#include "strutils.h"
#include "ttyutils.h"
#include "pwdutils.h"
#include "optutils.h"

#include "logindefs.h"
#include "su-common.h"
#include "shells.h"

#include "debug.h"

UL_DEBUG_DEFINE_MASK(su);
UL_DEBUG_DEFINE_MASKNAMES(su) = UL_DEBUG_EMPTY_MASKNAMES;

#define SU_DEBUG_INIT		(1 << 1)
#define SU_DEBUG_PAM		(1 << 2)
#define SU_DEBUG_PARENT		(1 << 3)
#define SU_DEBUG_TTY		(1 << 4)
#define SU_DEBUG_LOG		(1 << 5)
#define SU_DEBUG_MISC		(1 << 6)
#define SU_DEBUG_SIG		(1 << 7)
#define SU_DEBUG_PTY		(1 << 8)
#define SU_DEBUG_ALL		0xFFFF

#define DBG(m, x)       __UL_DBG(su, SU_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(su, SU_DEBUG_, m, x)

/* name of the pam configuration files. separate configs for su and su -  */
#define PAM_SRVNAME_SU "su"
#define PAM_SRVNAME_SU_L "su-l"

#define PAM_SRVNAME_RUNUSER "runuser"
#define PAM_SRVNAME_RUNUSER_L "runuser-l"

#ifdef HAVE_LIBECONF
#define _PATH_LOGINDEFS_SU	"default/su"
#define _PATH_LOGINDEFS_RUNUSER "default/runuser"
#else
#define _PATH_LOGINDEFS_SU	"/etc/default/su"
#define _PATH_LOGINDEFS_RUNUSER "/etc/default/runuser"
#endif

#define is_pam_failure(_rc)	((_rc) != PAM_SUCCESS)

/* The shell to run if none is given in the user's passwd entry.  */
#define DEFAULT_SHELL "/bin/sh"

/* The user to become if none is specified.  */
#define DEFAULT_USER "root"

#ifndef HAVE_ENVIRON_DECL
extern char **environ;
#endif

enum {
	SIGTERM_IDX = 0,
	SIGINT_IDX,
	SIGQUIT_IDX,

	SIGNALS_IDX_COUNT
};

/*
 * su/runuser control struct
 */
struct su_context {
	pam_handle_t	*pamh;			/* PAM handler */
	struct pam_conv conv;			/* PAM conversation */

	struct passwd	*pwd;			/* new user info */
	char		*pwdbuf;		/* pwd strings */

	const char	*tty_path;		/* tty device path */
	const char	*tty_name;		/* tty_path without /dev prefix */
	const char	*tty_number;		/* end of the tty_path */

	char		*new_user;		/* wanted user */
	char		*old_user;		/* original user */

	pid_t		child;			/* fork() baby */
	int		childstatus;		/* wait() status */

	char		**env_whitelist_names;	/* environment whitelist */
	char		**env_whitelist_vals;

	struct sigaction oldact[SIGNALS_IDX_COUNT];	/* original sigactions indexed by SIG*_IDX */
#ifdef USE_PTY
	struct ul_pty	*pty;			/* pseudo terminal handler (for --pty) */
#endif
	unsigned int runuser :1,		/* flase=su, true=runuser */
		     runuser_uopt :1,		/* runuser -u specified */
		     isterm :1,			/* is stdin terminal? */
		     fast_startup :1,		/* pass the `-f' option to the subshell. */
		     simulate_login :1,		/* simulate a login instead of just starting a shell. */
		     change_environment :1,	/* change some environment vars to indicate the user su'd to.*/
		     same_session :1,		/* don't call setsid() with a command. */
		     suppress_pam_info:1,	/* don't print PAM info messages (Last login, etc.). */
		     pam_has_session :1,	/* PAM session opened */
		     pam_has_cred :1,		/* PAM cred established */
		     force_pty :1,		/* create pseudo-terminal */
		     restricted :1;		/* false for root user */
};


static sig_atomic_t volatile caught_signal = 0;

/* Signal handler for parent process.  */
static void
su_catch_sig(int sig)
{
	caught_signal = sig;
}

static void su_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(su, SU_DEBUG_, 0, SU_DEBUG);
}

static void init_tty(struct su_context *su)
{
	su->isterm = isatty(STDIN_FILENO) ? 1 : 0;
	DBG(TTY, ul_debug("initialize [is-term=%s]", su->isterm ? "true" : "false"));
	if (su->isterm)
		get_terminal_name(&su->tty_path, &su->tty_name, &su->tty_number);
}

/*
 * Note, this function has to be possible call more than once. If the child is
 * already dead than it returns saved result from the previous call.
 */
static int wait_for_child(struct su_context *su)
{
	pid_t pid = (pid_t) -1;
	int status = 0;

	if (su->child == (pid_t) -1)
		return su->childstatus;

	if (su->child != (pid_t) -1) {
		/*
		 * The "su" parent process spends all time here in waitpid(),
		 * but "su --pty" uses pty_proxy_master() and waitpid() is only
		 * called to pick up child status or to react to SIGSTOP.
		 */
		DBG(SIG, ul_debug("waiting for child [%d]...", su->child));
		for (;;) {
			pid = waitpid(su->child, &status, WUNTRACED);

			if (pid != (pid_t) - 1 && WIFSTOPPED(status)) {
				DBG(SIG, ul_debug(" child got SIGSTOP -- stop all session"));
				kill(getpid(), SIGSTOP);
				/* once we get here, we must have resumed */
				kill(pid, SIGCONT);
				DBG(SIG, ul_debug(" session resumed -- continue"));
#ifdef USE_PTY
				/* Let's go back to pty_proxy_master() */
				if (su->force_pty && ul_pty_is_running(su->pty)) {
					DBG(SIG, ul_debug(" leaving on child SIGSTOP"));
					return 0;
				}
#endif
			} else
				break;
		}
	}
	if (pid != (pid_t) -1) {
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s%s\n",
				strsignal(WTERMSIG(status)),
				WCOREDUMP(status) ? _(" (core dumped)")
				: "");
			status = WTERMSIG(status) + 128;
		} else
			status = WEXITSTATUS(status);

		DBG(SIG, ul_debug("child %d is dead", su->child));
		su->child = (pid_t) -1;	/* Don't use the PID anymore! */
		su->childstatus = status;
#ifdef USE_PTY
		/* inform pty suff that we have no child anymore */
		if (su->force_pty)
			ul_pty_set_child(su->pty, (pid_t) -1);
#endif
	} else if (caught_signal)
		status = caught_signal + 128;
	else
		status = 1;

	DBG(SIG, ul_debug("child status=%d", status));
	return status;
}

#ifdef USE_PTY
static void wait_for_child_cb(
			void *data,
			pid_t child __attribute__((__unused__)))
{
	wait_for_child((struct su_context *) data);
}

static void chownmod_pty(struct su_context *su)
{
	gid_t gid = su->pwd->pw_gid;
	mode_t mode = (mode_t) getlogindefs_num("TTYPERM", TTY_MODE);
	const char *grname = getlogindefs_str("TTYGROUP", TTYGRPNAME);

	if (grname && *grname) {
		struct group *gr = getgrnam(grname);
		if (gr)	/* group by name */
			gid = gr->gr_gid;
		else	/* group by ID */
			gid = (gid_t) getlogindefs_num("TTYGROUP", gid);
	}

	if (ul_pty_chownmod_slave(su->pty,
				  su->pwd->pw_uid,
				  gid, mode))
		warn(_("change owner or mode for pseudo-terminal failed"));
}
#endif

/* Log the fact that someone has run su to the user given by PW;
   if SUCCESSFUL is true, they gave the correct password, etc.  */

static void log_syslog(struct su_context *su, bool successful)
{
	DBG(LOG, ul_debug("syslog logging"));

	openlog(su->runuser ? "runuser" : "su", LOG_PID, LOG_AUTH);
	syslog(LOG_NOTICE, "%s(to %s) %s on %s",
	       successful ? "" :
	       su->runuser ? "FAILED RUNUSER " : "FAILED SU ",
	       su->new_user, su->old_user ? : "",
	       su->tty_name ? : "none");
	closelog();
}

/*
 * Log failed login attempts in _PATH_BTMP if that exists.
 */
static void log_btmp(struct su_context *su)
{
	struct utmpx ut;
	struct timeval tv;

	DBG(LOG, ul_debug("btmp logging"));

	memset(&ut, 0, sizeof(ut));
	str2memcpy(ut.ut_user,
		su->pwd && su->pwd->pw_name ? su->pwd->pw_name : "(unknown)",
		sizeof(ut.ut_user));

	if (su->tty_number)
		str2memcpy(ut.ut_id, su->tty_number, sizeof(ut.ut_id));
	if (su->tty_name)
		str2memcpy(ut.ut_line, su->tty_name, sizeof(ut.ut_line));

	gettimeofday(&tv, NULL);
	ut.ut_tv.tv_sec = tv.tv_sec;
	ut.ut_tv.tv_usec = tv.tv_usec;
	ut.ut_type = LOGIN_PROCESS;	/* XXX doesn't matter */
	ut.ut_pid = getpid();

	updwtmpx(_PATH_BTMP, &ut);
}

static int supam_conv(	int num_msg,
			const struct pam_message **msg,
			struct pam_response **resp,
			void *data)
{
	struct su_context *su = (struct su_context *) data;

	if (su->suppress_pam_info
	    && num_msg == 1
	    && msg && msg[0]->msg_style == PAM_TEXT_INFO)
		return PAM_SUCCESS;

#ifdef HAVE_SECURITY_PAM_MISC_H
	return misc_conv(num_msg, msg, resp, data);
#elif defined(HAVE_SECURITY_OPENPAM_H)
	return openpam_ttyconv(num_msg, msg, resp, data);
#else
	return PAM_CONV_ERR;
#endif
}

static void supam_cleanup(struct su_context *su, int retcode)
{
	const int errsv = errno;

	DBG(PAM, ul_debug("cleanup"));

	if (su->pam_has_session)
		pam_close_session(su->pamh, 0);
	if (su->pam_has_cred)
		pam_setcred(su->pamh, PAM_DELETE_CRED | PAM_SILENT);
	pam_end(su->pamh, retcode);
	errno = errsv;
}


static void supam_export_environment(struct su_context *su)
{
	char **env;

	DBG(PAM, ul_debug("init environ[]"));

	/* This is a copy but don't care to free as we exec later anyways.  */
	env = pam_getenvlist(su->pamh);

	while (env && *env) {
		if (putenv(*env) != 0)
			err(EXIT_FAILURE, _("failed to modify environment"));
		env++;
	}
}

static void supam_authenticate(struct su_context *su)
{
	const char *srvname = NULL;
	int rc;

	srvname = su->runuser ?
		   (su->simulate_login ? PAM_SRVNAME_RUNUSER_L : PAM_SRVNAME_RUNUSER) :
		   (su->simulate_login ? PAM_SRVNAME_SU_L : PAM_SRVNAME_SU);

	DBG(PAM, ul_debug("start [name: %s]", srvname));

	rc = pam_start(srvname, su->pwd->pw_name, &su->conv, &su->pamh);
	if (is_pam_failure(rc))
		goto done;

	if (su->tty_path) {
		rc = pam_set_item(su->pamh, PAM_TTY, su->tty_path);
		if (is_pam_failure(rc))
			goto done;
	}
	if (su->old_user) {
		rc = pam_set_item(su->pamh, PAM_RUSER, (const void *) su->old_user);
		if (is_pam_failure(rc))
			goto done;
	}
	if (su->runuser) {
		/*
		 * This is the only difference between runuser(1) and su(1). The command
		 * runuser(1) does not required authentication, because user is root.
		 */
		if (su->restricted)
			errx(EXIT_FAILURE, _("may not be used by non-root users"));
		return;
	}

	rc = pam_authenticate(su->pamh, 0);
	if (is_pam_failure(rc))
		goto done;

	/* Check password expiration and offer option to change it.  */
	rc = pam_acct_mgmt(su->pamh, 0);
	if (rc == PAM_NEW_AUTHTOK_REQD)
		rc = pam_chauthtok(su->pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
 done:
	log_syslog(su, !is_pam_failure(rc));

	if (is_pam_failure(rc)) {
		const char *msg;

		DBG(PAM, ul_debug("authentication failed"));
		log_btmp(su);

		msg = pam_strerror(su->pamh, rc);
		pam_end(su->pamh, rc);
		sleep(getlogindefs_num("FAIL_DELAY", 1));
		errx(EXIT_FAILURE, "%s", msg ? msg : _("authentication failed"));
	}
}

static void supam_open_session(struct su_context *su)
{
	int rc;

	DBG(PAM, ul_debug("opening session"));

	rc = pam_open_session(su->pamh, 0);
	if (is_pam_failure(rc)) {
		const char *msg = pam_strerror(su->pamh, rc);

		supam_cleanup(su, rc);
		errx(EXIT_FAILURE, _("cannot open session: %s"), msg);
	} else
		su->pam_has_session = 1;
}

static void parent_setup_signals(struct su_context *su)
{
	sigset_t ourset;

	/*
	 * Signals setup
	 *
	 * 1) block all signals
	 */
	DBG(SIG, ul_debug("initialize signals"));

	sigfillset(&ourset);
	if (sigprocmask(SIG_BLOCK, &ourset, NULL)) {
		warn(_("cannot block signals"));
		caught_signal = true;
	}

	if (!caught_signal) {
		struct sigaction action;
		action.sa_handler = su_catch_sig;
		sigemptyset(&action.sa_mask);
		action.sa_flags = 0;

		sigemptyset(&ourset);

		/* 2a) add wanted signals to the mask (for session) */
		if (!su->same_session
		    && (sigaddset(&ourset, SIGINT)
		       || sigaddset(&ourset, SIGQUIT))) {

			warn(_("cannot initialize signal mask for session"));
			caught_signal = true;
		}
		/* 2b) add wanted generic signals to the mask */
		if (!caught_signal
		    && (sigaddset(&ourset, SIGTERM)
		       || sigaddset(&ourset, SIGALRM))) {

			warn(_("cannot initialize signal mask"));
			caught_signal = true;
		}

		/* 3a) set signal handlers (for session) */
		if (!caught_signal
		    && !su->same_session
		    && (sigaction(SIGINT, &action, &su->oldact[SIGINT_IDX])
		       || sigaction(SIGQUIT, &action, &su->oldact[SIGQUIT_IDX]))) {

			warn(_("cannot set signal handler for session"));
			caught_signal = true;
		}

		/* 3b) set signal handlers */
		if (!caught_signal
		     && sigaction(SIGTERM, &action, &su->oldact[SIGTERM_IDX])) {

			warn(_("cannot set signal handler"));
			caught_signal = true;
		}

		/* 4) unblock wanted signals */
		if (!caught_signal
		    && sigprocmask(SIG_UNBLOCK, &ourset, NULL)) {

			warn(_("cannot set signal mask"));
			caught_signal = true;
		}
	}
}

static void create_watching_parent(struct su_context *su)
{
	struct sigaction action;
	int status;

	DBG(MISC, ul_debug("forking..."));
#ifdef USE_PTY
	if (su->force_pty) {
		struct ul_pty_callbacks *cb;

		/* set callbacks */
		ul_pty_set_callback_data(su->pty, (void *) su);

		cb = ul_pty_get_callbacks(su->pty);
		cb->child_wait    = wait_for_child_cb;
		cb->child_sigstop = wait_for_child_cb;

		ul_pty_slave_echo(su->pty, 1);

		/* create pty */
		if (ul_pty_setup(su->pty))
			err(EXIT_FAILURE, _("failed to create pseudo-terminal"));
		if (ul_pty_signals_setup(su->pty))
			err(EXIT_FAILURE, _("failed to initialize signals handler"));
	}
#endif
	fflush(stdout);			/* ??? */

	/* set default handler for SIGCHLD */
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	action.sa_handler = SIG_DFL;
	if (sigaction(SIGCHLD, &action, NULL)) {
		supam_cleanup(su, PAM_ABORT);
#ifdef USE_PTY
		if (su->force_pty)
			ul_pty_cleanup(su->pty);
#endif
		err(EXIT_FAILURE, _("cannot set child signal handler"));
	}

	switch ((int) (su->child = fork())) {
	case -1: /* error */
		supam_cleanup(su, PAM_ABORT);
#ifdef USE_PTY
		if (su->force_pty)
			ul_pty_cleanup(su->pty);
#endif
		err(EXIT_FAILURE, _("cannot create child process"));
		break;

	case 0: /* child */
		return;

	default: /* parent */
		DBG(MISC, ul_debug("child [pid=%d]", (int) su->child));
		break;
	}

	/* free unnecessary stuff */
	free_getlogindefs_data();

	/* In the parent watch the child.  */

	/* su without pam support does not have a helper that keeps
	   sitting on any directory so let's go to /.  */
	if (chdir("/") != 0)
		warn(_("cannot change directory to %s"), "/");
#ifdef USE_PTY
	if (su->force_pty) {
		ul_pty_set_child(su->pty, su->child);

		ul_pty_proxy_master(su->pty);

		/* ul_pty_proxy_master() keeps classic signal handler are out of game */
		caught_signal = ul_pty_get_delivered_signal(su->pty);

		ul_pty_cleanup(su->pty);
	} else
#endif
		parent_setup_signals(su);

	/*
	 * Wait for child
	 */
	if (!caught_signal)
		status = wait_for_child(su);
	else
		status = 1;

	DBG(SIG, ul_debug("final child status=%d", status));

	if (caught_signal && su->child != (pid_t)-1) {
		fprintf(stderr, _("\nSession terminated, killing shell..."));
		kill(su->child, SIGTERM);
	}

	supam_cleanup(su, PAM_SUCCESS);

	if (caught_signal) {
		if (su->child != (pid_t)-1) {
			DBG(SIG, ul_debug("killing child"));
			sleep(2);
			kill(su->child, SIGKILL);
			fprintf(stderr, _(" ...killed.\n"));
		}

		/* Let's terminate itself with the received signal.
		 *
		 * It seems that shells use WIFSIGNALED() rather than our exit status
		 * value to detect situations when is necessary to cleanup (reset)
		 * terminal settings (kzak -- Jun 2013).
		 */
		DBG(SIG, ul_debug("restore signals setting"));
		switch (caught_signal) {
		case SIGTERM:
			sigaction(SIGTERM, &su->oldact[SIGTERM_IDX], NULL);
			break;
		case SIGINT:
			sigaction(SIGINT, &su->oldact[SIGINT_IDX], NULL);
			break;
		case SIGQUIT:
			sigaction(SIGQUIT, &su->oldact[SIGQUIT_IDX], NULL);
			break;
		default:
			/* just in case that signal stuff initialization failed and
			 * caught_signal = true */
			caught_signal = SIGKILL;
			break;
		}
		DBG(SIG, ul_debug("self-send %d signal", caught_signal));
		kill(getpid(), caught_signal);
	}

	DBG(MISC, ul_debug("exiting [rc=%d]", status));
	exit(status);
}

/* Adds @name from the current environment to the whitelist. If @name is not
 * set then nothing is added to the whitelist and returns 1.
 */
static int env_whitelist_add(struct su_context *su, const char *name)
{
	const char *env = getenv(name);

	if (!env)
		return 1;
	if (strv_extend(&su->env_whitelist_names, name))
                err_oom();
	if (strv_extend(&su->env_whitelist_vals, env))
                err_oom();
	return 0;
}

static int env_whitelist_setenv(struct su_context *su, int overwrite)
{
	char **one;
	size_t i = 0;
	int rc;

	STRV_FOREACH(one, su->env_whitelist_names) {
		rc = setenv(*one, su->env_whitelist_vals[i], overwrite);
		if (rc)
			return rc;
		i++;
	}

	return 0;
}

/* Creates (add to) whitelist from comma delimited string */
static int env_whitelist_from_string(struct su_context *su, const char *str)
{
	char **all = strv_split(str, ",");
	char **one;

	if (!all) {
		if (errno == ENOMEM)
			err_oom();
		return -EINVAL;
	}

	STRV_FOREACH(one, all)
		env_whitelist_add(su, *one);
	strv_free(all);
	return 0;
}

static void setenv_path(const struct passwd *pw)
{
	int rc;

	DBG(MISC, ul_debug("setting PATH"));

	if (pw->pw_uid)
		rc = logindefs_setenv("PATH", "ENV_PATH", _PATH_DEFPATH);

	else if ((rc = logindefs_setenv("PATH", "ENV_SUPATH", NULL)) != 0)
		rc = logindefs_setenv("PATH", "ENV_ROOTPATH", _PATH_DEFPATH_ROOT);

	if (rc)
		err(EXIT_FAILURE, _("failed to set the PATH environment variable"));
}

static void modify_environment(struct su_context *su, const char *shell)
{
	const struct passwd *pw = su->pwd;


	DBG(MISC, ul_debug("modify environ[]"));

	/* Leave TERM unchanged.  Set HOME, SHELL, USER, LOGNAME, PATH.
	 *
	 * Unset all other environment variables, but follow
	 * --whitelist-environment if specified.
	 */
	if (su->simulate_login) {
		/* leave TERM unchanged */
		env_whitelist_add(su, "TERM");

		/* Note that original su(1) has allocated environ[] by malloc
		 * to the number of expected variables. This seems unnecessary
		 * optimization as libc later re-alloc(current_size+2) and for
		 * empty environ[] the curren_size is zero. It seems better to
		 * keep all logic around environment in glibc's hands.
		 *                                           --kzak [Aug 2018]
		 */
#ifdef HAVE_CLEARENV
		clearenv();
#else
		environ = NULL;
#endif
		/* always reset */
		if (shell)
			xsetenv("SHELL", shell, 1);

		setenv_path(pw);

		xsetenv("HOME", pw->pw_dir, 1);
		xsetenv("USER", pw->pw_name, 1);
		xsetenv("LOGNAME", pw->pw_name, 1);

		/* apply all from whitelist, but no overwrite */
		env_whitelist_setenv(su, 0);

	/* Set HOME, SHELL, and (if not becoming a superuser) USER and LOGNAME.
	 */
	} else if (su->change_environment) {
		xsetenv("HOME", pw->pw_dir, 1);
		if (shell)
			xsetenv("SHELL", shell, 1);

		if (getlogindefs_bool("ALWAYS_SET_PATH", 0))
			setenv_path(pw);

		if (pw->pw_uid) {
			xsetenv("USER", pw->pw_name, 1);
			xsetenv("LOGNAME", pw->pw_name, 1);
		}
	}

	supam_export_environment(su);
}

static void init_groups(struct su_context *su, gid_t *groups, size_t ngroups)
{
	int rc;

	DBG(MISC, ul_debug("initialize groups"));

	errno = 0;
	if (ngroups)
		rc = setgroups(ngroups, groups);
	else
		rc = initgroups(su->pwd->pw_name, su->pwd->pw_gid);

	if (rc == -1) {
		supam_cleanup(su, PAM_ABORT);
		err(EXIT_FAILURE, _("cannot set groups"));
	}
	endgrent();

	rc = pam_setcred(su->pamh, PAM_ESTABLISH_CRED);
	if (is_pam_failure(rc))
		errx(EXIT_FAILURE, _("failed to establish user credentials: %s"),
					pam_strerror(su->pamh, rc));
	su->pam_has_cred = 1;
}

static void change_identity(const struct passwd *pw)
{
	DBG(MISC, ul_debug("changing identity [GID=%d, UID=%d]", pw->pw_gid, pw->pw_uid));

	if (setgid(pw->pw_gid))
		err(EXIT_FAILURE, _("cannot set group id"));
	if (setuid(pw->pw_uid))
		err(EXIT_FAILURE, _("cannot set user id"));
}

/* Run SHELL, if COMMAND is nonzero, pass it to the shell with the -c option.
 * Pass ADDITIONAL_ARGS to the shell as more arguments; there are
 * N_ADDITIONAL_ARGS extra arguments.
 */
static void run_shell(
		struct su_context *su,
		char const *shell, char const *command, char **additional_args,
		size_t n_additional_args)
{
	size_t n_args = 1 + su->fast_startup + 2 * ! !command + n_additional_args + 1;
	const char **args = xcalloc(n_args, sizeof *args);
	size_t argno = 1;
	char *tmp;

	DBG(MISC, ul_debug("starting shell [shell=%s, command=\"%s\"%s%s]",
				shell, command,
				su->simulate_login ? " login" : "",
				su->fast_startup ? " fast-start" : ""));
	tmp = xstrdup(shell);

	if (su->simulate_login) {
		char *arg0;
		char *shell_basename;

		shell_basename = basename(tmp);
		arg0 = xmalloc(strlen(shell_basename) + 2);
		arg0[0] = '-';
		strcpy(arg0 + 1, shell_basename);
		args[0] = arg0;
	} else
		args[0] = basename(tmp);

	if (su->fast_startup)
		args[argno++] = "-f";
	if (command) {
		args[argno++] = "-c";
		args[argno++] = command;
	}

	memcpy(args + argno, additional_args, n_additional_args * sizeof *args);
	args[argno + n_additional_args] = NULL;
	execv(shell, (char **)args);
	errexec(shell);
}

/* Return true if SHELL is a restricted shell (one not returned by
 * getusershell), else false, meaning it is a standard shell.
 */
static bool is_restricted_shell(const char *shell)
{
	if (is_known_shell(shell)) {
		return false;
	}
#ifdef USE_VENDORDIR
	DBG(MISC, ul_debug("%s is restricted shell (not in e.g. vendor shells file, /etc/shells, ...)", shell));
#else
	DBG(MISC, ul_debug("%s is restricted shell (not in /etc/shells)", shell));
#endif
	return true;
}

static void usage_common(void)
{
	fputs(_(" -m, -p, --preserve-environment      do not reset environment variables\n"), stdout);
	fputs(_(" -w, --whitelist-environment <list>  don't reset specified variables\n"), stdout);
	fputs(USAGE_SEPARATOR, stdout);

	fputs(_(" -g, --group <group>             specify the primary group\n"), stdout);
	fputs(_(" -G, --supp-group <group>        specify a supplemental group\n"), stdout);
	fputs(USAGE_SEPARATOR, stdout);

	fputs(_(" -, -l, --login                  make the shell a login shell\n"), stdout);
	fputs(_(" -c, --command <command>         pass a single command to the shell with -c\n"), stdout);
	fputs(_(" --session-command <command>     pass a single command to the shell with -c\n"
	        "                                   and do not create a new session\n"), stdout);
	fputs(_(" -f, --fast                      pass -f to the shell (for csh or tcsh)\n"), stdout);
	fputs(_(" -s, --shell <shell>             run <shell> if /etc/shells allows it\n"), stdout);
	fputs(_(" -P, --pty                       create a new pseudo-terminal\n"), stdout);
	fputs(_(" -T, --no-pty                    do not create a new pseudo-terminal (bad security!)\n"), stdout);

	fputs(USAGE_SEPARATOR, stdout);
	printf(USAGE_HELP_OPTIONS(33));
}

static void usage_runuser(void)
{
	fputs(USAGE_HEADER, stdout);
	fprintf(stdout,
		_(" %1$s [options] -u <user> [[--] <command>]\n"
	          " %1$s [options] [-] [<user> [<argument>...]]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	fputs(_("Run <command> with the effective user ID and group ID of <user>.  If -u is\n"
	       "not given, fall back to su(1)-compatible semantics and execute standard shell.\n"
	       "The options -c, -f, -l, and -s are mutually exclusive with -u.\n"), stdout);

	fputs(USAGE_OPTIONS, stdout);
	fputs(_(" -u, --user <user>               username\n"), stdout);
	usage_common();
	fputs(USAGE_SEPARATOR, stdout);

	fprintf(stdout, USAGE_MAN_TAIL("runuser(1)"));
}

static void usage_su(void)
{
	fputs(USAGE_HEADER, stdout);
	fprintf(stdout,
		_(" %s [options] [-] [<user> [<argument>...]]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	fputs(_("Change the effective user ID and group ID to that of <user>.\n"
		"A mere - implies -l.  If <user> is not given, root is assumed.\n"), stdout);

	fputs(USAGE_OPTIONS, stdout);
	usage_common();

	fprintf(stdout, USAGE_MAN_TAIL("su(1)"));
}

static void __attribute__((__noreturn__)) usage(int mode)
{
	if (mode == SU_MODE)
		usage_su();
	else
		usage_runuser();

	exit(EXIT_SUCCESS);
}

static void load_config(void *data)
{
	struct su_context *su = (struct su_context *) data;

	DBG(MISC, ul_debug("loading logindefs"));
#ifndef HAVE_LIBECONF
	logindefs_load_file(_PATH_LOGINDEFS);
#endif
	logindefs_load_file(su->runuser ? _PATH_LOGINDEFS_RUNUSER : _PATH_LOGINDEFS_SU);
}

/*
 * Returns 1 if the current user is not root
 */
static int is_not_root(void)
{
	const uid_t ruid = getuid();
	const uid_t euid = geteuid();

	/* if we're really root and aren't running setuid */
	return (uid_t) 0 == ruid && ruid == euid ? 0 : 1;
}

/* Don't rely on PAM and reset the most important limits. */
static void sanitize_prlimits(void)
{
#ifdef HAVE_SYS_RESOURCE_H
	struct rlimit lm = { .rlim_cur = 0, .rlim_max = 0 };

	/* reset to zero */
#ifdef RLIMIT_NICE
	setrlimit(RLIMIT_NICE, &lm);
#endif
#ifdef RLIMIT_RTPRIO
	setrlimit(RLIMIT_RTPRIO, &lm);
#endif

	/* reset to unlimited */
	lm.rlim_cur = RLIM_INFINITY;
	lm.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_FSIZE, &lm);
	setrlimit(RLIMIT_AS, &lm);

	/* reset soft limit only */
	getrlimit(RLIMIT_NOFILE, &lm);
	if (lm.rlim_cur != FD_SETSIZE) {
		lm.rlim_cur = FD_SETSIZE;
		setrlimit(RLIMIT_NOFILE, &lm);
	}
#endif
}

static gid_t add_supp_group(const char *name, gid_t **groups, size_t *ngroups)
{
	struct group *gr;

	if (*ngroups >= NGROUPS_MAX)
		errx(EXIT_FAILURE,
		     P_("specifying more than %d supplemental group is not possible",
		        "specifying more than %d supplemental groups is not possible",
			NGROUPS_MAX - 1), NGROUPS_MAX - 1);

	gr = getgrnam(name);
	if (!gr)
		errx(EXIT_FAILURE, _("group %s does not exist"), name);

	DBG(MISC, ul_debug("add %s group [name=%s, GID=%d]", name, gr->gr_name, (int) gr->gr_gid));

	*groups = xreallocarray(*groups, *ngroups + 1, sizeof(gid_t));
	(*groups)[*ngroups] = gr->gr_gid;
	(*ngroups)++;

	return gr->gr_gid;
}

int su_main(int argc, char **argv, int mode)
{
	struct su_context _su = {
		.conv			= { supam_conv, NULL },
		.runuser		= (mode == RUNUSER_MODE ? 1 : 0),
		.change_environment	= 1,
		.new_user		= DEFAULT_USER
	}, *su = &_su;

	int optc;
	char *command = NULL;
	int request_same_session = 0;
	char *shell = NULL;

	gid_t *groups = NULL;
	size_t ngroups = 0;
	bool use_supp = false;
	bool use_gid = false;
	gid_t gid = 0;

	static const struct option longopts[] = {
		{"command", required_argument, NULL, 'c'},
		{"session-command", required_argument, NULL, 'C'},
		{"fast", no_argument, NULL, 'f'},
		{"login", no_argument, NULL, 'l'},
		{"preserve-environment", no_argument, NULL, 'p'},
		{"pty", no_argument, NULL, 'P'},
		{"no-pty", no_argument, NULL, 'T'},
		{"shell", required_argument, NULL, 's'},
		{"group", required_argument, NULL, 'g'},
		{"supp-group", required_argument, NULL, 'G'},
		{"user", required_argument, NULL, 'u'},	/* runuser only */
		{"whitelist-environment", required_argument, NULL, 'w'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'V'},
		{NULL, 0, NULL, 0}
	};
	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'm', 'w' },			/* preserve-environment, whitelist-environment */
		{ 'p', 'w' },			/* preserve-environment, whitelist-environment */
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	su_init_debug();
	su->conv.appdata_ptr = (void *) su;

	while ((optc =
		getopt_long(argc, argv, "c:fg:G:lmpPTs:u:hVw:", longopts,
			    NULL)) != -1) {

		err_exclusive_options(optc, longopts, excl, excl_st);

		switch (optc) {
		case 'c':
			command = optarg;
			break;

		case 'C':
			command = optarg;
			request_same_session = 1;
			break;

		case 'f':
			su->fast_startup = true;
			break;

		case 'g':
			use_gid = true;
			gid = add_supp_group(optarg, &groups, &ngroups);
			break;

		case 'G':
			use_supp = true;
			add_supp_group(optarg, &groups, &ngroups);
			break;

		case 'l':
			su->simulate_login = true;
			break;

		case 'm':
		case 'p':
			su->change_environment = false;
			break;

		case 'w':
			env_whitelist_from_string(su, optarg);
			break;

		case 'P':
#ifdef USE_PTY
			su->force_pty = 1;
#else
			errx(EXIT_FAILURE, _("--pty is not supported for your system"));
#endif
			break;

		case 'T':
			su->force_pty = 0;
			break;

		case 's':
			shell = optarg;
			break;

		case 'u':
			if (!su->runuser)
				errtryhelp(EXIT_FAILURE);
			su->runuser_uopt = 1;
			su->new_user = optarg;
			break;

		case 'h':
			usage(mode);

		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	su->restricted = is_not_root();

	if (optind < argc && !strcmp(argv[optind], "-")) {
		su->simulate_login = true;
		++optind;
	}

	if (su->simulate_login && !su->change_environment) {
		warnx(_
		      ("ignoring --preserve-environment, it's mutually exclusive with --login"));
		su->change_environment = true;
	}

	switch (mode) {
	case RUNUSER_MODE:
		/* runuser -u <user> <command>
		 *
		 * If -u <user> is not specified, then follow traditional su(1) behavior and
		 * fallthrough
		 */
		if (su->runuser_uopt) {
			if (shell || su->fast_startup || command || su->simulate_login)
				errx(EXIT_FAILURE,
				     _("options --{shell,fast,command,session-command,login} and "
				      "--user are mutually exclusive"));
			if (optind == argc)
				errx(EXIT_FAILURE, _("no command was specified"));
			break;
		}
		/* fallthrough */
	case SU_MODE:
		if (optind < argc)
			su->new_user = argv[optind++];
		break;
	}

	if ((use_supp || use_gid) && su->restricted)
		errx(EXIT_FAILURE,
		     _("only root can specify alternative groups"));

	logindefs_set_loader(load_config, (void *) su);
	init_tty(su);

	su->pwd = xgetpwnam(su->new_user, &su->pwdbuf);
	if (!su->pwd
	    || !su->pwd->pw_passwd
	    || !su->pwd->pw_name || !*su->pwd->pw_name
	    || !su->pwd->pw_dir  || !*su->pwd->pw_dir)
		errx(EXIT_FAILURE,
		     _("user %s does not exist or the user entry does not "
		       "contain all the required fields"), su->new_user);

	su->new_user = su->pwd->pw_name;
	su->old_user = xgetlogin();

	if (!su->pwd->pw_shell || !*su->pwd->pw_shell)
		su->pwd->pw_shell = DEFAULT_SHELL;

	if (use_supp && !use_gid)
		su->pwd->pw_gid = groups[0];
	else if (use_gid)
		su->pwd->pw_gid = gid;

	supam_authenticate(su);

	if (request_same_session || !command || !su->pwd->pw_uid)
		su->same_session = 1;

	/* initialize shell variable only if "-u <user>" not specified */
	if (su->runuser_uopt) {
		shell = NULL;
	} else {
		if (!shell && !su->change_environment)
			shell = getenv("SHELL");

		if (shell
		    && strcmp(shell, su->pwd->pw_shell) != 0
		    && getuid() != 0
		    && is_restricted_shell(su->pwd->pw_shell)) {
			/* The user being su'd to has a nonstandard shell, and
			 * so is probably a uucp account or has restricted
			 * access.  Don't compromise the account by allowing
			 * access with a standard shell.
			 */
			warnx(_("using restricted shell %s"), su->pwd->pw_shell);
			shell = NULL;
		}
		shell = xstrdup(shell ? shell : su->pwd->pw_shell);
	}

	init_groups(su, groups, ngroups);

	if (!su->simulate_login || command)
		su->suppress_pam_info = 1;	/* don't print PAM info messages */

	sanitize_prlimits();

	supam_open_session(su);

#ifdef USE_PTY
	if (su->force_pty) {
		ON_DBG(PTY, ul_pty_init_debug(0xffff));

		su->pty = ul_new_pty(su->isterm);
		if (!su->pty)
			err(EXIT_FAILURE, _("failed to allocate pty handler"));
	}
#endif
	create_watching_parent(su);
	/* Now we're in the child.  */

#ifdef USE_PTY
	if (su->force_pty)
		chownmod_pty(su);
#endif
	change_identity(su->pwd);
	if (!su->same_session) {
		/* note that on --pty we call setsid() in ul_pty_init_slave() */
		DBG(MISC, ul_debug("call setsid()"));
		setsid();
	}
#ifdef USE_PTY
	if (su->force_pty)
		ul_pty_init_slave(su->pty);
#endif
	/* Set environment after pam_open_session, which may put KRB5CCNAME
	   into the pam_env, etc.  */

	modify_environment(su, shell);

	if (su->simulate_login && chdir(su->pwd->pw_dir) != 0)
		warn(_("warning: cannot change directory to %s"), su->pwd->pw_dir);

	/* http://www.linux-pam.org/Linux-PAM-html/adg-interface-by-app-expected.html#adg-pam_end */
	(void) pam_end(su->pamh, PAM_SUCCESS|PAM_DATA_SILENT);

	if (shell)
		run_shell(su, shell, command, argv + optind, max(0, argc - optind));

	execvp(argv[optind], &argv[optind]);
	err(EXIT_FAILURE, _("failed to execute %s"), argv[optind]);
}
