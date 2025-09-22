/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2000 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2013, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Some parts substantially derived from an ancestor of:
   su for GNU.  Run a shell with substitute user and group IDs.

   Copyright (C) 1992-2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */


#include <config.h>

#ident "$Id$"

#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef USE_PAM
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif				/* !USE_PAM */

#include "alloc.h"
#include "attr.h"
#include "cast.h"
#include "prototypes.h"
#include "defines.h"
#include "pwauth.h"
#include "getdef.h"
#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"
#include "string/sprintf.h"
#include "string/strtcpy.h"


/*
 * Global variables
 */
static const char Prog[] = "su";
static /*@observer@*/const char *caller_tty = NULL;	/* Name of tty SU is run from */
static bool caller_is_root = false;
static uid_t caller_uid;
#ifndef USE_PAM
static bool caller_on_console = false;
#ifdef SU_ACCESS
static /*@only@*/char *caller_pass;
#endif
#endif				/* !USE_PAM */
static bool doshell = false;
static bool fakelogin = false;
static /*@observer@*/const char *shellstr;
static /*@null@*/char *command = NULL;


/* not needed by sulog.c anymore */
static char name[BUFSIZ];
static char caller_name[BUFSIZ];

/* If nonzero, change some environment vars to indicate the user su'd to. */
static bool change_environment = true;

#ifdef USE_PAM
static char kill_msg[256];
static char wait_msg[256];
static pam_handle_t *pamh = NULL;
static int caught = 0;
/* PID of the child, in case it needs to be killed */
static pid_t pid_child = 0;
#endif

/*
 * External identifiers
 */

extern char **newenvp; /* lib/env.c */
extern size_t newenvc; /* lib/env.c */

/* local function prototypes */

static void execve_shell (const char *shellname,
                          char *args[],
                          char *const envp[]);
#ifdef USE_PAM
static void kill_child (MAYBE_UNUSED int s);
static void prepare_pam_close_session (void);
#else				/* !USE_PAM */
static void die (int);
static bool iswheel (const char *);
#endif				/* !USE_PAM */
static bool restricted_shell (const char *shellname);
NORETURN static void su_failure (const char *tty, bool su_to_root);
static /*@only@*/struct passwd * check_perms (void);
static /*@only@*/struct passwd * do_check_perms (void);
#ifdef USE_PAM
static void check_perms_pam (const struct passwd *pw);
#else				/* !USE_PAM */
static void check_perms_nopam (const struct passwd *pw);
#endif				/* !USE_PAM */
static void save_caller_context (char **argv);
static void process_flags (int argc, char **argv);
static void set_environment (struct passwd *pw);

#ifndef USE_PAM
/*
 * die - set or reset termio modes.
 *
 *	die() is called before processing begins. signal() is then called
 *	with die() as the signal handler. If signal later calls die() with a
 *	signal number, the terminal modes are then reset.
 */
static void die (int killed)
{
	static TERMIO sgtty;

	if (killed != 0) {
		STTY (0, &sgtty);
	} else {
		GTTY (0, &sgtty);
	}

	if (killed != 0) {
		_exit (128+killed);
	}
}

static bool iswheel (const char *username)
{
	struct group *grp;

	grp = getgrnam ("wheel"); /* !USE_PAM, no need for xgetgrnam */
	if (   (NULL ==grp)
	    || (NULL == grp->gr_mem)) {
		return false;
	}
	return is_on_list (grp->gr_mem, username);
}
#else				/* USE_PAM */
static void kill_child (MAYBE_UNUSED int s)
{
	if (0 != pid_child) {
		(void) kill (-pid_child, SIGKILL);
		(void) write_full(STDERR_FILENO, kill_msg, strlen(kill_msg));
	} else {
		(void) write_full(STDERR_FILENO, wait_msg, strlen(wait_msg));
	}
	_exit (255);
}
#endif				/* USE_PAM */

/* borrowed from GNU sh-utils' "su.c" */
static bool restricted_shell (const char *shellname)
{
	/*@observer@*/const char *line;

	setusershell ();
	while ((line = getusershell ()) != NULL) {
		if (('#' != *line) && (strcmp (line, shellname) == 0)) {
			endusershell ();
			return false;
		}
	}
	endusershell ();
	return true;
}

NORETURN
static void
su_failure (const char *tty, bool su_to_root)
{
	sulog (tty, false, caller_name, name);	/* log failed attempt */
	if (getdef_bool ("SYSLOG_SU_ENAB")) {
		SYSLOG ((su_to_root ? LOG_NOTICE : LOG_INFO,
		         "- %s %s:%s", tty,
		         ('\0' != caller_name[0]) ? caller_name : "???",
		         ('\0' != name[0]) ? name : "???"));
	}
	closelog ();

#ifdef WITH_AUDIT
	audit_fd = audit_open ();
	audit_log_acct_message (audit_fd,
				AUDIT_USER_ROLE_CHANGE,
				NULL,    /* Prog. name */
				"su",
				('\0' != caller_name[0]) ? caller_name : "???",
				AUDIT_NO_ID,
				"localhost",
				NULL,    /* addr */
				tty,
				0);      /* result */
	close (audit_fd);
#endif				/* WITH_AUDIT */

	exit (1);
}

/*
 * execve_shell - Execute a shell with execve, or interpret it with
 * /bin/sh
 */
static void execve_shell (const char *shellname,
                          char *args[],
                          char *const envp[])
{
	int err;
	(void) execve (shellname, args, envp);
	err = errno;

	if (access (shellname, R_OK|X_OK) == 0) {
		/*
		 * Assume this is a shell script (with no shebang).
		 * Interpret it with /bin/sh
		 */
		size_t n_args = 0;
		char **targs;
		while (NULL != args[n_args]) {
			n_args++;
		}
		targs = XMALLOC(n_args + 3, char *);
		targs[0] = "sh";
		targs[1] = "-";
		targs[2] = xstrdup (shellname);
		targs[n_args+2] = NULL;
		while (1 != n_args) {
			targs[n_args+1] = args[n_args - 1];
			n_args--;
		}

		(void) execve (SHELL, targs, envp);
	} else {
		errno = err;
	}
}

#ifdef USE_PAM
/* Signal handler for parent process later */
static void catch_signals (int sig)
{
	caught = sig;
}

/*
 * prepare_pam_close_session - Fork and wait for the child to close the session
 *
 *	Only the child returns. The parent will wait for the child to
 *	terminate and exit.
 */
static void prepare_pam_close_session (void)
{
	sigset_t ourset;
	int status;
	int ret;
	struct sigaction action;

	/* reset SIGCHLD handling to default */
	action.sa_handler = SIG_DFL;
	sigemptyset (&action.sa_mask);
	action.sa_flags = 0;
	if (0 == caught && sigaction (SIGCHLD, &action, NULL) != 0) {
		fprintf (stderr,
		         _("%s: signal masking malfunction\n"),
		         Prog);
		SYSLOG ((LOG_WARN, "Will not execute %s", shellstr));
		closelog ();
		exit (1);
		/* Only the child returns. See above. */
	}

	pid_child = fork ();
	if (pid_child == 0) {	/* child shell */
		return; /* Only the child will return from pam_create_session */
	} else if ((pid_t)-1 == pid_child) {
		(void) fprintf (stderr,
		                _("%s: Cannot fork user shell\n"),
		                Prog);
		SYSLOG ((LOG_WARN, "Cannot execute %s", shellstr));
		closelog ();
		exit (1);
		/* Only the child returns. See above. */
	}

	/* parent only */
	sigfillset (&ourset);
	if (sigprocmask (SIG_BLOCK, &ourset, NULL) != 0) {
		(void) fprintf (stderr,
		                _("%s: signal malfunction\n"),
		                Prog);
		caught = SIGTERM;
	}
	if (0 == caught) {
		action.sa_handler = catch_signals;
		sigemptyset (&ourset);

		if (   (sigaddset (&ourset, SIGTERM) != 0)
		    || (sigaddset (&ourset, SIGALRM) != 0)
		    || (sigaction (SIGTERM, &action, NULL) != 0)
		    || (   !doshell /* handle SIGINT (Ctrl-C), SIGQUIT
		                     * (Ctrl-\), and SIGTSTP (Ctrl-Z)
		                     * since the child will not control
		                     * the tty.
		                     */
		        && (   (sigaddset (&ourset, SIGINT)  != 0)
		            || (sigaddset (&ourset, SIGQUIT) != 0)
		            || (sigaddset (&ourset, SIGTSTP) != 0)
		            || (sigaction (SIGINT,  &action, NULL) != 0)
		            || (sigaction (SIGQUIT, &action, NULL) != 0)
		            || (sigaction (SIGTSTP,  &action, NULL) != 0)))
		    || (sigprocmask (SIG_UNBLOCK, &ourset, NULL) != 0)
		    ) {
			fprintf (stderr,
			         _("%s: signal masking malfunction\n"),
			         Prog);
			caught = SIGTERM;
		}
	}

	if (0 == caught) {
		bool stop = true;

		do {
			pid_t pid;
			stop = true;

			do {
				pid = waitpid (-1, &status, WUNTRACED);
			} while (pid != -1 && pid != pid_child);

			/* When interrupted by signal, the signal will be
			 * forwarded to the child, and termination will be
			 * forced later.
			 */
			if (   ((pid_t)-1 == pid)
			    && (EINTR == errno)
			    && (SIGTSTP == caught)) {
				caught = 0;
				/* Except for SIGTSTP, which request to
				 * stop the child.
				 * We will SIGSTOP ourself on the next
				 * waitpid round.
				 */
				kill (pid_child, SIGSTOP);
				stop = false;
			} else if (   ((pid_t)-1 != pid)
			           && (0 != WIFSTOPPED (status))) {
				/* The child (shell) was suspended.
				 * Suspend su. */
				kill (getpid (), SIGSTOP);
				/* wake child when resumed */
				kill (pid, SIGCONT);
				stop = false;
			} else if (   (pid_t)-1 != pid) {
				pid_child = 0;
			}
		} while (!stop);
	}

	if (0 != caught && 0 != pid_child) {
		(void) fputs ("\n", stderr);
		(void) fputs (_("Session terminated, terminating shell..."),
		              stderr);
		(void) kill (-pid_child, caught);

		SNPRINTF(kill_msg, _(" ...killed.\n"));
		SNPRINTF(wait_msg, _(" ...waiting for child to terminate.\n"));

		/* Any signals other than SIGCHLD and SIGALRM will no longer have any effect,
		 * so it's time to block all of them. */
		sigfillset (&ourset);
		if (sigprocmask (SIG_BLOCK, &ourset, NULL) != 0) {
			fprintf (stderr, _("%s: signal masking malfunction\n"), Prog);
			kill_child (0);
			/* Never reach (_exit called). */
		}

		/* Send SIGKILL to the child if it doesn't
		 * exit within 2 seconds (after SIGTERM) */
		(void) signal (SIGALRM, kill_child);
		(void) signal (SIGCHLD, catch_signals);
		(void) alarm (2);

		(void) sigdelset (&ourset, SIGALRM);
		(void) sigdelset (&ourset, SIGCHLD);

		while (0 == waitpid (pid_child, &status, WNOHANG)) {
			sigsuspend (&ourset);
		}
		pid_child = 0;

		(void) fputs (_(" ...terminated.\n"), stderr);
	}

	ret = pam_close_session (pamh, 0);
	if (PAM_SUCCESS != ret) {
		SYSLOG ((LOG_ERR, "pam_close_session: %s",
		         pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
	}

	(void) pam_setcred (pamh, PAM_DELETE_CRED);
	(void) pam_end (pamh, PAM_SUCCESS);

	exit ((0 != WIFEXITED (status)) ? WEXITSTATUS (status)
	                                : WTERMSIG (status) + 128);
	/* Only the child returns. See above. */
}
#endif				/* USE_PAM */

/*
 * usage - print command line syntax and exit
 */
NORETURN
static void usage (int status)
{
	(void)
	fputs (_("Usage: su [options] [-] [username [args]]\n"
	         "\n"
	         "Options:\n"
	         "  -c, --command COMMAND         pass COMMAND to the invoked shell\n"
	         "  -h, --help                    display this help message and exit\n"
	         "  -, -l, --login                make the shell a login shell\n"
	         "  -m, -p,\n"
	         "  --preserve-environment        do not reset environment variables, and\n"
	         "                                keep the same shell\n"
	         "  -s, --shell SHELL             use SHELL instead of the default in passwd\n"
	         "\n"
	         "If no username is given, assume root.\n"), (E_SUCCESS != status) ? stderr : stdout);
	exit (status);
}

#ifdef USE_PAM
static void check_perms_pam (const struct passwd *pw)
{
	int ret;
	ret = pam_authenticate (pamh, 0);
	if (PAM_SUCCESS != ret) {
		SYSLOG (((pw->pw_uid != 0)? LOG_NOTICE : LOG_WARN, "pam_authenticate: %s",
		         pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
		(void) pam_end (pamh, ret);
		su_failure (caller_tty, 0 == pw->pw_uid);
	}

	ret = pam_acct_mgmt (pamh, 0);
	if (PAM_SUCCESS != ret) {
		if (caller_is_root) {
			fprintf (stderr,
			         _("%s: %s\n(Ignored)\n"),
			         Prog, pam_strerror (pamh, ret));
		} else if (PAM_NEW_AUTHTOK_REQD == ret) {
			ret = pam_chauthtok (pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
			if (PAM_SUCCESS != ret) {
				SYSLOG ((LOG_ERR, "pam_chauthtok: %s",
				         pam_strerror (pamh, ret)));
				fprintf (stderr,
				         _("%s: %s\n"),
				         Prog, pam_strerror (pamh, ret));
				(void) pam_end (pamh, ret);
				su_failure (caller_tty, 0 == pw->pw_uid);
			}
		} else {
			SYSLOG ((LOG_ERR, "pam_acct_mgmt: %s",
			         pam_strerror (pamh, ret)));
			fprintf (stderr,
			         _("%s: %s\n"),
			         Prog, pam_strerror (pamh, ret));
			(void) pam_end (pamh, ret);
			su_failure (caller_tty, 0 == pw->pw_uid);
		}
	}
}
#else				/* !USE_PAM */
static void check_perms_nopam (const struct passwd *pw)
{
	/*@observer@*/const struct spwd *spwd = NULL;
	/*@observer@*/const char *password = pw->pw_passwd;
	sighandler_t oldsig;

	if (caller_is_root) {
		return;
	}

	if (strcmp (pw->pw_passwd, "") == 0) {
		const char  *prevent_no_auth = getdef_str("PREVENT_NO_AUTH");

		if (prevent_no_auth == NULL) {
			prevent_no_auth = "superuser";
		}
		if (strcmp(prevent_no_auth, "yes") == 0) {
			fprintf(stderr, _("Password field is empty, this is forbidden for all accounts.\n"));
			exit(1);
		} else if ((pw->pw_uid == 0)
				&& (strcmp(prevent_no_auth, "superuser") == 0)) {
			fprintf(stderr, _("Password field is empty, this is forbidden for super-user.\n"));
			exit(1);
		}
	}

	/*
	 * BSD systems only allow "wheel" to SU to root. USG systems don't,
	 * so we make this a configurable option.
	 */

	/* The original Shadow 3.3.2 did this differently. Do it like BSD:
	 *
	 * - check for UID 0 instead of name "root" - there are systems with
	 *   several root accounts under different names,
	 *
	 * - check the contents of /etc/group instead of the current group
	 *   set (you must be listed as a member, GID 0 is not sufficient).
	 *
	 * In addition to this traditional feature, we now have complete su
	 * access control (allow, deny, no password, own password).  Thanks
	 * to Chris Evans <lady0110@sable.ox.ac.uk>.
	 */

	if (   (0 == pw->pw_uid)
	    && getdef_bool ("SU_WHEEL_ONLY")
	    && !iswheel (caller_name)) {
		fprintf (stderr,
		         _("You are not authorized to su %s\n"),
		         name);
		exit (1);
	}
	spwd = getspnam (name); /* !USE_PAM, no need for xgetspnam */
#ifdef SU_ACCESS
	if (strcmp (pw->pw_passwd, SHADOW_PASSWD_STRING) == 0) {
		if (NULL != spwd) {
			password = spwd->sp_pwdp;
		}
	}

	switch (check_su_auth (caller_name, name, 0 == pw->pw_uid)) {
	case 0:	/* normal su, require target user's password */
		break;
	case 1:	/* require no password */
		password = "";	/* XXX warning: const */
		break;
	case 2:	/* require own password */
		(void) puts (_("(Enter your own password)"));
		password = caller_pass;
		break;
	default:	/* access denied (-1) or unexpected value */
		fprintf (stderr,
		         _("You are not authorized to su %s\n"),
		         name);
		exit (1);
	}
#endif				/* SU_ACCESS */
	/*
	 * Set up a signal handler in case the user types QUIT.
	 */
	die (0);
	oldsig = signal (SIGQUIT, die);

	/*
	 * See if the system defined authentication method is being used.
	 * The first character of an administrator defined method is an '@'
	 * character.
	 */
	if (pw_auth (password, name, PW_SU, NULL) != 0) {
		SYSLOG (((pw->pw_uid != 0)? LOG_NOTICE : LOG_WARN,
		         "Authentication failed for %s", name));
		fprintf(stderr, _("%s: Authentication failure\n"), Prog);
		su_failure (caller_tty, 0 == pw->pw_uid);
	}
	(void) signal (SIGQUIT, oldsig);

	/*
	 * Check to see if the account is expired. root gets to ignore any
	 * expired accounts, but normal users can't become a user with an
	 * expired password.
	 */
	if (NULL != spwd) {
		(void) expire (pw, spwd);
	}

	/*
	 * Check to see if the account permits "su". root gets to ignore any
	 * restricted accounts, but normal users can't become a user if
	 * there is a "SU" entry in the /etc/porttime file denying access to
	 * the account.
	 */
	if (!isttytime (name, "SU", time (NULL))) {
		SYSLOG (((0 != pw->pw_uid) ? LOG_WARN : LOG_CRIT,
		         "SU by %s to restricted account %s",
		         caller_name, name));
		fprintf (stderr,
		         _("%s: You are not authorized to su at that time\n"),
		         Prog);
		su_failure (caller_tty, 0 == pw->pw_uid);
	}
}
#endif				/* !USE_PAM */

/*
 * check_perms - check permissions to switch to the user 'name'
 *
 *	The user is authenticated in all subsystems iterately.
 */
static /*@only@*/struct passwd * check_perms (void)
{
	struct passwd *pw = NULL;

	while (pw == NULL)
		pw = do_check_perms();
	return pw;
}

/*
 * do_check_perms - check permissions to switch to the user 'name'
 *
 *	The user is authenticated in current subsystem, if any. Returns
 *	NULL if permissions have to be checked in next subsystem, in
 *	which case the subsystem has already been entered.
 */
static /*@only@*/struct passwd * do_check_perms (void)
{
#ifdef USE_PAM
	int         ret;
	const char  *tmp_name;
	const void  *item;
#endif				/* !USE_PAM */
	/*
	 * The password file entries for the user is gotten and the account
	 * validated.
	 */
	struct passwd *pw = xgetpwnam (name);
	if (NULL == pw) {
		(void) fprintf (stderr,
		                _("No passwd entry for user '%s'\n"), name);
		SYSLOG ((LOG_NOTICE, "No passwd entry for user '%s'", name));
		su_failure (caller_tty, true);
	}

	(void) signal (SIGINT, SIG_IGN);
	(void) signal (SIGQUIT, SIG_IGN);

#ifdef USE_PAM
	check_perms_pam (pw);
	/* PAM authentication can request a change of account */
	ret = pam_get_item(pamh, PAM_USER, &item);
	if (ret != PAM_SUCCESS) {
		SYSLOG((LOG_ERR, "pam_get_item: internal PAM error\n"));
		(void) fprintf (stderr,
		                "%s: Internal PAM error retrieving username\n",
		                Prog);
		(void) pam_end (pamh, ret);
		su_failure (caller_tty, 0 == pw->pw_uid);
	}
	tmp_name = item;
	if (strcmp (name, tmp_name) != 0) {
		SYSLOG ((LOG_INFO,
		         "Change user from '%s' to '%s' as requested by PAM",
		         name, tmp_name));
		if (STRTCPY(name, tmp_name) == -1) {
			fprintf (stderr, _("Overlong user name '%s'\n"),
			         tmp_name);
			SYSLOG ((LOG_NOTICE, "Overlong user name '%s'",
			         tmp_name));
			su_failure (caller_tty, true);
		}
		pw = xgetpwnam (name);
		if (NULL == pw) {
			(void) fprintf (stderr,
			                _("No passwd entry for user '%s'\n"),
			                name);
			SYSLOG ((LOG_NOTICE,
			         "No passwd entry for user '%s'", name));
			su_failure (caller_tty, true);
		}
	}
#else				/* !USE_PAM */
	check_perms_nopam (pw);
#endif				/* !USE_PAM */

	(void) signal (SIGINT, SIG_DFL);
	(void) signal (SIGQUIT, SIG_DFL);

	/*
	 * Even if --shell is specified, the subsystem login test is based on
	 * the shell specified in /etc/passwd (not the one specified with
	 * --shell, which will be the one executed in the chroot later).
	 */
	if ('*' == pw->pw_shell[0]) {	/* subsystem root required */
		subsystem (pw);	/* change to the subsystem root */
		endpwent ();		/* close the old password databases */
		endspent ();
		pw_free (pw);
		return NULL;	/* authenticate in the subsystem */
	}

	return pw;
}

/*
 * save_caller_context - save information from the call context
 *
 *	Save the program's name (Prog), caller's UID (caller_uid /
 *	caller_is_root), name (caller_name), and password (caller_pass),
 *	the TTY (ttyp), and whether su was called from a console
 *	(is_console) for further processing and before they might change.
 */
static void save_caller_context (char **argv)
{
	struct passwd *pw = NULL;
#ifndef USE_PAM
#ifdef SU_ACCESS
	const char *password = NULL;
#endif				/* SU_ACCESS */
#endif				/* !USE_PAM */
	log_set_progname(Prog);
	log_set_logfd(stderr);

	caller_uid = getuid ();
	caller_is_root = (caller_uid == 0);

	/*
	 * Get the tty name. Entries will be logged indicating that the user
	 * tried to change to the named new user from the current terminal.
	 */
	caller_tty = ttyname (0);
	if ((isatty (0) != 0) && (NULL != caller_tty)) {
#ifndef USE_PAM
		caller_on_console = console (caller_tty);
#endif				/* !USE_PAM */
	} else {
		/*
		 * Be more paranoid, like su from SimplePAMApps.  --marekm
		 */
		if (!caller_is_root) {
			fprintf (stderr,
			         _("%s: must be run from a terminal\n"),
			         Prog);
			exit (1);
		}
		caller_tty = "???";
	}

	/*
	 * Get the user's real name. The current UID is used to determine
	 * who has executed su. That user ID must exist.
	 */
	pw = get_my_pwent ();
	if (NULL == pw) {
		fprintf (stderr,
		         _("%s: Cannot determine your user name.\n"),
		         Prog);
		SYSLOG ((LOG_WARN, "Cannot determine the user name of the caller (UID %lu)",
		         (unsigned long) caller_uid));
		su_failure (caller_tty, true); /* unknown target UID*/
	}
	STRTCPY(caller_name, pw->pw_name);

#ifndef USE_PAM
#ifdef SU_ACCESS
	/*
	 * Sort out the password of user calling su, in case needed later
	 * -- chris
	 */
	password = pw->pw_passwd;
	if (strcmp (pw->pw_passwd, SHADOW_PASSWD_STRING) == 0) {
		const struct spwd *spwd = getspnam (caller_name);
		if (NULL != spwd) {
			password = spwd->sp_pwdp;
		}
	}
	free (caller_pass);
	caller_pass = xstrdup (password);
#endif				/* SU_ACCESS */
#endif				/* !USE_PAM */
	pw_free (pw);
}

/*
 * process_flags - Process the command line arguments
 *
 *	process_flags() interprets the command line arguments and sets
 *	the values that the user will be created with accordingly. The
 *	values are checked for sanity.
 */
static void process_flags (int argc, char **argv)
{
	int c;
	static struct option long_options[] = {
		{"command",              required_argument, NULL, 'c'},
		{"help",                 no_argument,       NULL, 'h'},
		{"login",                no_argument,       NULL, 'l'},
		{"preserve-environment", no_argument,       NULL, 'p'},
		{"shell",                required_argument, NULL, 's'},
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv, "c:hlmps:",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'c':
			command = optarg;
			break;
		case 'h':
			usage (E_SUCCESS);
			break;
		case 'l':
			fakelogin = true;
			break;
		case 'm':
		case 'p':
			/* This will only have an effect if the target
			 * user does not have a restricted shell, or if
			 * su is called by root.
			 */
			change_environment = false;
			break;
		case 's':
			shellstr = optarg;
			break;
		default:
			usage (E_USAGE);	/* NOT REACHED */
		}
	}

	if ((optind < argc) && (strcmp (argv[optind], "-") == 0)) {
		fakelogin = true;
		optind++;
	}

	if (optind < argc) {
		STRTCPY(name, argv[optind++]);	/* use this login id */
	}
	if ('\0' == name[0]) {		/* use default user */
		struct passwd *root_pw = getpwnam ("root");
		if ((NULL != root_pw) && (0 == root_pw->pw_uid)) {
			(void) strcpy (name, "root");
		} else {
			root_pw = getpwuid (0);
			if (NULL == root_pw) {
				SYSLOG ((LOG_CRIT, "There is no UID 0 user."));
				su_failure (caller_tty, true);
			}
			(void) strcpy (name, root_pw->pw_name);
		}
	}

	doshell = (argc == optind);	/* any arguments remaining? */
	if (NULL != command) {
		doshell = false;
	}
}

static void set_environment (struct passwd *pw)
{
	const char *cp;
	/*
	 * If a new login is being set up, the old environment will be
	 * ignored and a new one created later on.
	 */
	if (change_environment && fakelogin) {
		/*
		 * The terminal type will be left alone if it is present in
		 * the environment already.
		 */
		cp = getenv ("TERM");
		if (NULL != cp) {
			addenv ("TERM", cp);
		}

		/*
		 * For some terminals COLORTERM seems to be the only way
		 * for checking for that specific terminal. For instance,
		 * gnome-terminal sets its TERM as "xterm" but its
		 * COLORTERM as "gnome-terminal". The COLORTERM variable
		 * is also of use when running GNU screen since it sets
		 * TERM to "screen" but doesn't touch COLORTERM.
		 */
		cp = getenv ("COLORTERM");
		if (NULL != cp) {
			addenv ("COLORTERM", cp);
		}

#ifndef USE_PAM
		cp = getdef_str ("ENV_TZ");
		if (NULL != cp) {
			addenv (('/' == *cp) ? tz (cp) : cp, NULL);
		}

		/*
		 * The clock frequency will be reset to the login value if required
		 */
		cp = getdef_str ("ENV_HZ");
		if (NULL != cp) {
			addenv (cp, NULL);	/* set the default $HZ, if one */
		}
#endif				/* !USE_PAM */

		/*
		 * Also leave DISPLAY and XAUTHORITY if present, else
		 * pam_xauth will not work.
		 */
		cp = getenv ("DISPLAY");
		if (NULL != cp) {
			addenv ("DISPLAY", cp);
		}
		cp = getenv ("XAUTHORITY");
		if (NULL != cp) {
			addenv ("XAUTHORITY", cp);
		}
	} else {
		char **envp = environ;
		while (NULL != *envp) {
			addenv (*envp, NULL);
			envp++;
		}
	}

	cp = getdef_str ((pw->pw_uid == 0) ? "ENV_SUPATH" : "ENV_PATH");
	if (NULL == cp) {
		addenv ((pw->pw_uid == 0) ? "PATH=/sbin:/bin:/usr/sbin:/usr/bin" : "PATH=/bin:/usr/bin", NULL);
	} else if (strchr (cp, '=') != NULL) {
		addenv (cp, NULL);
	} else {
		addenv ("PATH", cp);
	}

	if (getenv ("IFS") != NULL) {	/* don't export user IFS ... */
		addenv ("IFS= \t\n", NULL);	/* ... instead, set a safe IFS */
	}

	environ = newenvp;	/* make new environment active */

	if (change_environment) {
		if (fakelogin) {
			if (shellstr != pw->pw_shell) {
				free (pw->pw_shell);
				pw->pw_shell = xstrdup (shellstr);
			}
			setup_env (pw);
		} else {
			addenv ("HOME", pw->pw_dir);
			addenv ("USER", pw->pw_name);
			addenv ("LOGNAME", pw->pw_name);
			addenv ("SHELL", shellstr);
		}

#ifdef USE_PAM
	        /* we need to setup the environment *after* pam_open_session(),
                 * else the UID is changed before stuff like pam_xauth could
                 * run, and we cannot access /etc/shadow and co
                 */
		/* update environment with all pam set variables */
		char **envcp = pam_getenvlist (pamh);
		if (NULL != envcp) {
			while (NULL != *envcp) {
				addenv (*envcp, NULL);
				envcp++;
			}
		}
#endif				/* !USE_PAM */
	}

}

/*
 * su - switch user id
 *
 *	su changes the user's ids to the values for the specified user.  if
 *	no new user name is specified, "root" or UID 0 is used by default.
 *
 *	Any additional arguments are passed to the user's shell. In
 *	particular, the argument "-c" will cause the next argument to be
 *	interpreted as a command by the common shell programs.
 */
int main (int argc, char **argv)
{
	const char *cp;
	struct passwd *pw = NULL;

#ifdef USE_PAM
	int ret;
#endif				/* USE_PAM */

	check_fds ();

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	save_caller_context (argv);

	OPENLOG (Prog);

	process_flags (argc, argv);

	initenv ();

#ifdef USE_PAM
	ret = pam_start (Prog, name, &conv, &pamh);
	if (PAM_SUCCESS != ret) {
		SYSLOG ((LOG_ERR, "pam_start: error %d", ret);
		fprintf (stderr,
		         _("%s: pam_start: error %d\n"),
		         Prog, ret));
		exit (1);
	}

	ret = pam_set_item (pamh, PAM_TTY, caller_tty);
	if (PAM_SUCCESS == ret) {
		ret = pam_set_item (pamh, PAM_RUSER, caller_name);
	}
	if (PAM_SUCCESS != ret) {
		SYSLOG ((LOG_ERR, "pam_set_item: %s",
		         pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
		pam_end (pamh, ret);
		exit (1);
	}
#endif				/* USE_PAM */

	pw = check_perms ();

	/* If the user does not want to change the environment,
	 * use the current SHELL.
	 * (unless another shell is required by the command line)
	 */
	if ((NULL == shellstr) && !change_environment) {
		shellstr = getenv ("SHELL");
	}

	/* If su is not called by root, and the target user has a
	 * restricted shell, the environment must be changed and the shell
	 * must be the one specified in /etc/passwd.
	 */
	if (   !caller_is_root
	    && restricted_shell (pw->pw_shell)) {
		shellstr = NULL;
		change_environment = true;
	}

	/* If the shell is not set at this time, use the shell specified
	 * in /etc/passwd.
	 */
	if (NULL == shellstr) {
		shellstr = pw->pw_shell;
	}

	/*
	 * Set the default shell.
	 */
	if ((NULL == shellstr) || ('\0' == shellstr[0])) {
		shellstr = SHELL;
	}

	sulog (caller_tty, true, caller_name, name);	/* save SU information */
	if (getdef_bool ("SYSLOG_SU_ENAB")) {
		SYSLOG ((LOG_INFO, "+ %s %s:%s", caller_tty,
		         ('\0' != caller_name[0]) ? caller_name : "???",
		         ('\0' != name[0]) ? name : "???"));
	}

#ifdef USE_PAM
	/* set primary group id and supplementary groups */
	if (setup_groups (pw) != 0) {
		pam_end (pamh, PAM_ABORT);
		exit (1);
	}

	/*
	 * pam_setcred() may do things like resource limits, console groups,
	 * and much more, depending on the configured modules
	 */
	ret = pam_setcred (pamh, PAM_ESTABLISH_CRED);
	if (PAM_SUCCESS != ret) {
		SYSLOG ((LOG_ERR, "pam_setcred: %s", pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
		(void) pam_end (pamh, ret);
		exit (1);
	}

	ret = pam_open_session (pamh, 0);
	if (PAM_SUCCESS != ret) {
		SYSLOG ((LOG_ERR, "pam_open_session: %s",
		         pam_strerror (pamh, ret)));
		fprintf (stderr, _("%s: %s\n"), Prog, pam_strerror (pamh, ret));
		pam_setcred (pamh, PAM_DELETE_CRED);
		(void) pam_end (pamh, ret);
		exit (1);
	}

	prepare_pam_close_session ();

	/* become the new user */
	if (change_uid (pw) != 0) {
		exit (1);
	}
#else				/* !USE_PAM */
	/* no limits if su from root (unless su must fake login's behavior) */
	if (!caller_is_root || fakelogin) {
		setup_limits (pw);
	}

	if (setup_uid_gid (pw, caller_on_console) != 0) {
		exit (1);
	}
#endif				/* !USE_PAM */

#ifdef WITH_AUDIT
	audit_fd = audit_open ();
	audit_log_acct_message (audit_fd,
				AUDIT_USER_ROLE_CHANGE,
				NULL,    /* Prog. name */
				"su",
				('\0' != caller_name[0]) ? caller_name : "???",
				AUDIT_NO_ID,
				"localhost",
				NULL,    /* addr */
				caller_tty,
				1);      /* result */
	close (audit_fd);
#endif				/* WITH_AUDIT */

	set_environment (pw);

	if (!doshell) {
		/* There is no need for a controlling terminal.
		 * This avoids the callee to inject commands on
		 * the caller's tty. */
		int err = -1;

#ifdef USE_PAM
		/* When PAM is used, we are on the child */
		err = setsid ();
#else
		/* Otherwise, we cannot use setsid */
		int fd = open ("/dev/tty", O_RDWR);

		if (fd >= 0) {
			err = ioctl (fd, TIOCNOTTY, (char *) NULL);
			(void) close (fd);
		} else if (ENXIO == errno) {
			/* There are no controlling terminal already */
			err = 0;
		}
#endif				/* USE_PAM */

		if (-1 == err) {
			(void) fprintf (stderr,
			                _("%s: Cannot drop the controlling terminal\n"),
			                Prog);
			exit (1);
		}
	}

#ifdef USE_PAM
	(void) pam_end (pamh, PAM_SUCCESS | PAM_DATA_SILENT);
#endif

	endpwent ();
	endspent ();
	/*
	 * This is a workaround for Linux libc bug/feature (?) - the
	 * /dev/log file descriptor is open without the close-on-exec flag
	 * and used to be passed to the new shell. There is "fcntl(LogFile,
	 * F_SETFD, 1)" in libc/misc/syslog.c, but it is commented out (at
	 * least in 5.4.33). Why?  --marekm
	 */
	closelog ();

	/*
	 * See if the user has extra arguments on the command line. In that
	 * case they will be provided to the new user's shell as arguments.
	 */
	if (fakelogin) {
		char *arg0;

		cp = getdef_str ("SU_NAME");
		if (NULL == cp) {
			cp = Basename (shellstr);
		}

		arg0 = XMALLOC(strlen(cp) + 2, char);
		arg0[0] = '-';
		strcpy (arg0 + 1, cp);
		cp = arg0;
	} else {
		cp = Basename (shellstr);
	}

	if (!doshell) {
		int err;
		/* Position argv to the remaining arguments */
		argv += optind;
		if (NULL != command) {
			argv -= 2;
			argv[0] = "-c";
			argv[1] = command;
		}
		/*
		 * Use the shell and create an argv
		 * with the rest of the command line included.
		 */
		argv[-1] = const_cast(char *, cp);
		execve_shell (shellstr, &argv[-1], environ);
		err = errno;
		(void) fprintf (stderr,
		                _("Cannot execute %s\n"), shellstr);
		errno = err;
	} else {
		(void) shell (shellstr, cp, environ);
	}

	pw_free (pw);

	return (errno == ENOENT ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
}

