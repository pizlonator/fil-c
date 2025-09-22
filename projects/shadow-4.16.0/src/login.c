/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2001, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2012, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <errno.h>
#include <grp.h>
#ifndef USE_PAM
#ifdef ENABLE_LASTLOG
#include <lastlog.h>
#endif 				/* ENABLE_LASTLOG */
#endif				/* !USE_PAM */
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "alloc.h"
#include "attr.h"
#include "chkname.h"
#include "defines.h"
#include "faillog.h"
#include "failure.h"
#include "getdef.h"
#include "memzero.h"
#include "prototypes.h"
#include "pwauth.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"
#include "string/sprintf.h"
#include "string/strftime.h"
#include "string/strtcpy.h"


#ifdef USE_PAM
#include "pam_defs.h"

static pam_handle_t *pamh = NULL;

#define PAM_FAIL_CHECK if (retcode != PAM_SUCCESS) { \
	fprintf(stderr,"\n%s\n",pam_strerror(pamh, retcode)); \
	SYSLOG((LOG_ERR,"%s",pam_strerror(pamh, retcode))); \
	(void) pam_end(pamh, retcode); \
	exit(1); \
   }
#define PAM_END { retcode = pam_close_session(pamh,0); \
		(void) pam_end(pamh,retcode); }

#endif				/* USE_PAM */

#ifndef USE_PAM
/*
 * Needed for MkLinux DR1/2/2.1 - J.
 */
#ifndef LASTLOG_FILE
#define LASTLOG_FILE "/var/log/lastlog"
#endif
#endif				/* !USE_PAM */

/*
 * Global variables
 */
static const char Prog[] = "login";

static const char *hostname = "";
static /*@null@*/ /*@only@*/char *username = NULL;
static int reason = PW_LOGIN;

#ifndef USE_PAM
#ifdef ENABLE_LASTLOG
static struct lastlog ll;
#endif				/* ENABLE_LASTLOG */
#endif				/* !USE_PAM */
static bool pflg = false;
static bool fflg = false;

static bool hflg = false;
static bool preauth_flag = false;

static bool amroot;
static char tmsg[256];

/*
 * External identifiers.
 */

extern char **newenvp;
extern size_t newenvc;

#ifndef	ALARM
#define	ALARM	60
#endif

#ifndef	RETRIES
#define	RETRIES	3
#endif

/* local function prototypes */
static void usage (void);
static void setup_tty (void);
static void process_flags (int argc, char *const *argv);
static /*@observer@*/const char *get_failent_user (/*@returned@*/const char *user);

#ifndef USE_PAM
static struct faillog faillog;

static void bad_time_notify (void);
static void check_nologin (bool login_to_root);
#else
static void get_pam_user (char **ptr_pam_user);
#endif

static void init_env (void);
static void alarm_handler (int);
static void exit_handler (int);

/*
 * usage - print login command usage and exit
 *
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */
static void usage (void)
{
	fprintf (stderr, _("Usage: %s [-p] [name]\n"), Prog);
	if (!amroot) {
		exit (1);
	}
	fprintf (stderr, _("       %s [-p] [-h host] [-f name]\n"), Prog);
	exit (1);
}

static void setup_tty (void)
{
	TERMIO termio;

	if (GTTY (0, &termio) == 0) {	/* get terminal characteristics */
		int erasechar;
		int killchar;

		/*
		 * Add your favorite terminal modes here ...
		 */
		termio.c_lflag |= ISIG | ICANON | ECHO | ECHOE;
		termio.c_iflag |= ICRNL;

#if defined(ECHOKE) && defined(ECHOCTL)
		termio.c_lflag |= ECHOKE | ECHOCTL;
#endif
#if defined(ECHOPRT) && defined(NOFLSH) && defined(TOSTOP)
		termio.c_lflag &= ~(ECHOPRT | NOFLSH | TOSTOP);
#endif
#ifdef ONLCR
		termio.c_oflag |= ONLCR;
#endif

		/* leave these values unchanged if not specified in login.defs */
		erasechar = getdef_num ("ERASECHAR", termio.c_cc[VERASE]);
		killchar = getdef_num ("KILLCHAR", termio.c_cc[VKILL]);
		termio.c_cc[VERASE] = erasechar;
		termio.c_cc[VKILL] = killchar;
		/* Make sure the values were valid.
		 * getdef_num cannot validate this.
		 */
		if (erasechar != (int) termio.c_cc[VERASE]) {
			fprintf (stderr,
			         _("configuration error - cannot parse %s value: '%d'"),
			         "ERASECHAR", erasechar);
			exit (1);
		}
		if (killchar != (int) termio.c_cc[VKILL]) {
			fprintf (stderr,
			         _("configuration error - cannot parse %s value: '%d'"),
			         "KILLCHAR", killchar);
			exit (1);
		}

		/*
		 * ttymon invocation prefers this, but these settings
		 * won't come into effect after the first username login
		 */
		(void) STTY (0, &termio);
	}
}


#ifndef USE_PAM
/*
 * Tell the user that this is not the right time to login at this tty
 */
static void bad_time_notify (void)
{
	(void) puts (_("Invalid login time"));
	(void) fflush (stdout);
}

static void check_nologin (bool login_to_root)
{
	const char *fname;

	/*
	 * Check to see if system is turned off for non-root users.
	 * This would be useful to prevent users from logging in
	 * during system maintenance. We make sure the message comes
	 * out for root so she knows to remove the file if she's
	 * forgotten about it ...
	 */
	fname = getdef_str ("NOLOGINS_FILE");
	if ((NULL != fname) && (access (fname, F_OK) == 0)) {
		FILE *nlfp;

		/*
		 * Cat the file if it can be opened, otherwise just
		 * print a default message
		 */
		nlfp = fopen (fname, "r");
		if (NULL != nlfp) {
			int c;
			while ((c = getc (nlfp)) != EOF) {
				if (c == '\n') {
					(void) putchar ('\r');
				}

				(void) putchar (c);
			}
			(void) fflush (stdout);
			(void) fclose (nlfp);
		} else {
			(void) puts (_("\nSystem closed for routine maintenance"));
		}
		/*
		 * Non-root users must exit. Root gets the message, but
		 * gets to login.
		 */

		if (!login_to_root) {
			closelog ();
			exit (0);
		}
		(void) puts (_("\n[Disconnect bypassed -- root login allowed.]"));
	}
}
#endif				/* !USE_PAM */

static void process_flags (int argc, char *const *argv)
{
	int arg;
	int flag;

	/*
	 * Check the flags for proper form. Every argument starting with
	 * "-" must be exactly two characters long. This closes all the
	 * clever telnet, and getty holes.
	 */
	for (arg = 1; arg < argc; arg++) {
		if (argv[arg][0] == '-' && strlen (argv[arg]) > 2) {
			usage ();
		}
		if (strcmp(argv[arg], "--") == 0) {
			break; /* stop checking on a "--" */
		}
	}

	/*
	 * Process options.
	 */
	while ((flag = getopt (argc, argv, "d:fh:pr:")) != EOF) {
		switch (flag) {
		case 'd':
			/* "-d device" ignored for compatibility */
			break;
		case 'f':
			fflg = true;
			break;
		case 'h':
			hflg = true;
			hostname = optarg;
			reason = PW_TELNET;
			break;
		case 'p':
			pflg = true;
			break;
		default:
			usage ();
		}
	}

	/*
	 * Allow authentication bypass only if real UID is zero.
	 */

	if ((fflg || hflg) && !amroot) {
		fprintf (stderr, _("%s: Permission denied.\n"), Prog);
		exit (1);
	}

	/*
	 *  Get the user name.
	 */
	if (optind < argc) {
		assert (NULL == username);
		username = xstrdup (argv[optind]);
		strzero (argv[optind]);
		++optind;
	}

	if (fflg && (NULL == username)) {
		usage ();
	}

}


static void init_env (void)
{
#ifndef USE_PAM
	const char *cp;
#endif
	char *tmp;

	tmp = getenv ("LANG");
	if (NULL != tmp) {
		addenv ("LANG", tmp);
	}

	/*
	 * Add the timezone environmental variable so that time functions
	 * work correctly.
	 */
	tmp = getenv ("TZ");
	if (NULL != tmp) {
		addenv ("TZ", tmp);
	}
#ifndef USE_PAM
	else {
		cp = getdef_str ("ENV_TZ");
		if (NULL != cp) {
			addenv (('/' == *cp) ? tz (cp) : cp, NULL);
		}
	}
#endif				/* !USE_PAM */
	/*
	 * Add the clock frequency so that profiling commands work
	 * correctly.
	 */
	tmp = getenv ("HZ");
	if (NULL != tmp) {
		addenv ("HZ", tmp);
	}
#ifndef USE_PAM
	else {
		cp = getdef_str ("ENV_HZ");
		if (NULL != cp) {
			addenv (cp, NULL);
		}
	}
#endif				/* !USE_PAM */
}

static void exit_handler (MAYBE_UNUSED int sig)
{
	_exit (0);
}

static void alarm_handler (MAYBE_UNUSED int sig)
{
	write_full(STDERR_FILENO, tmsg, strlen(tmsg));
	signal(SIGALRM, exit_handler);
	alarm(2);
}

#ifdef USE_PAM
/*
 * get_pam_user - Get the username according to PAM
 *
 * ptr_pam_user shall point to a malloc'ed string (or NULL).
 */
static void get_pam_user (char **ptr_pam_user)
{
	int         retcode;
	const void  *ptr_user;

	assert (NULL != ptr_pam_user);

	retcode = pam_get_item (pamh, PAM_USER, &ptr_user);
	PAM_FAIL_CHECK;

	free (*ptr_pam_user);
	if (NULL != ptr_user) {
		*ptr_pam_user = xstrdup (ptr_user);
	} else {
		*ptr_pam_user = NULL;
	}
}
#endif

/*
 * get_failent_user - Return a string that can be used to log failure
 *                    from a user.
 *
 * This will be either the user argument, or "UNKNOWN".
 *
 * It is quite common to mistyped the password for username, and passwords
 * should not be logged.
 */
static /*@observer@*/const char *get_failent_user (/*@returned@*/const char *user)
{
	const char *failent_user = "UNKNOWN";
	bool log_unkfail_enab = getdef_bool("LOG_UNKFAIL_ENAB");

	if ((NULL != user) && ('\0' != user[0])) {
		if (   log_unkfail_enab
		    || (getpwnam (user) != NULL)) {
			failent_user = user;
		}
	}

	return failent_user;
}

/*
 * login - create a new login session for a user
 *
 *	login is typically called by getty as the second step of a
 *	new user session. getty is responsible for setting the line
 *	characteristics to a reasonable set of values and getting
 *	the name of the user to be logged in. login may also be
 *	called to create a new user session on a pty for a variety
 *	of reasons, such as X servers or network logins.
 *
 *	the flags which login supports are
 *
 *	-p - preserve the environment
 *	-f - do not perform authentication, user is preauthenticated
 *	-h - the name of the remote host
 */
int main (int argc, char **argv)
{
	int            err;
	bool           subroot = false;
	char           **envp = environ;
	char           *host = NULL;
	char           tty[BUFSIZ];
	char           fromhost[512];
	const char     *failent_user;
	const char     *tmptty;
	const char     *cp;
	const char     *tmp;
	unsigned int   delay;
	unsigned int   retries;
	unsigned int   timeout;
	struct passwd  *pwd = NULL;

#if defined(USE_PAM)
	int            retcode;
	char           *pam_user = NULL;
	pid_t          child;
#else
	bool is_console;
	struct spwd *spwd = NULL;
# if defined(ENABLE_LASTLOG)
	char           ptime[80];
# endif
#endif

	/*
	 * Some quick initialization.
	 */

	sanitize_env ();

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	initenv ();

	amroot = (getuid () == 0);
	log_set_progname(Prog);
	log_set_logfd(stderr);

	if (geteuid() != 0) {
		fprintf (stderr, _("%s: Cannot possibly work without effective root\n"), Prog);
		exit (1);
	}

	process_flags (argc, argv);

	if ((isatty (0) == 0) || (isatty (1) == 0) || (isatty (2) == 0)) {
		exit (1);	/* must be a terminal */
	}

	err = get_session_host(&host);
	/*
	 * Be picky if run by normal users (possible if installed setuid
	 * root), but not if run by root.
	 */
	if (!amroot && (err != 0)) {
		SYSLOG ((LOG_ERR,
				 "No session entry, error %d.  You must exec \"login\" from the lowest level \"sh\"",
				 err));
		exit (1);
	}

	tmptty = ttyname (0);
	if (NULL == tmptty) {
		tmptty = "UNKNOWN";
	}
	STRTCPY(tty, tmptty);

#ifndef USE_PAM
	is_console = console (tty);
#endif

	if (hflg) {
		/*
		 * Add remote hostname to the environment. I think
		 * (not sure) I saw it once on Irix.  --marekm
		 */
		addenv ("REMOTEHOST", hostname);
	}
	if (fflg) {
		preauth_flag = true;
	}
	if (hflg) {
		reason = PW_RLOGIN;
	}

	OPENLOG (Prog);

	setup_tty ();

#ifndef USE_PAM
	(void) umask (getdef_num ("UMASK", GETDEF_DEFAULT_UMASK));

	{
		/*
		 * Use the ULIMIT in the login.defs file, and if
		 * there isn't one, use the default value. The
		 * user may have one for themselves, but otherwise,
		 * just take what you get.
		 */
		long limit = getdef_long ("ULIMIT", -1L);

		if (limit != -1) {
			set_filesize_limit (limit);
		}
	}

#endif
	/*
	 * The entire environment will be preserved if the -p flag
	 * is used.
	 */
	if (pflg) {
		while (NULL != *envp) {	/* add inherited environment, */
			addenv (*envp, NULL); /* some variables change later */
			envp++;
		}
	}

	/* preserve TERM from getty */
	if (!pflg) {
		tmp = getenv ("TERM");
		if (NULL != tmp) {
			addenv ("TERM", tmp);
		}
	}

	init_env ();

	if (optind < argc) {	/* now set command line variables */
		set_env (argc - optind, &argv[optind]);
	}

	if (hflg) {
		cp = hostname;
	} else if ((host != NULL) && (host[0] != '\0')) {
		cp = host;
	} else {
		cp = "";
	}

	if ('\0' != *cp) {
		SNPRINTF(fromhost, " on '%.100s' from '%.200s'", tty, cp);
	} else {
		SNPRINTF(fromhost, " on '%.100s'", tty);
	}
	free(host);

      top:
	/* only allow ALARM sec. for login */
	timeout = getdef_unum ("LOGIN_TIMEOUT", ALARM);
	SNPRINTF(tmsg, _("\nLogin timed out after %u seconds.\n"), timeout);
	(void) signal (SIGALRM, alarm_handler);
	if (timeout > 0) {
		(void) alarm (timeout);
	}

	environ = newenvp;	/* make new environment active */
	delay   = getdef_unum ("FAIL_DELAY", 1);
	retries = getdef_unum ("LOGIN_RETRIES", RETRIES);

#ifdef USE_PAM
	retcode = pam_start (Prog, username, &conv, &pamh);
	if (retcode != PAM_SUCCESS) {
		fprintf (stderr,
		         _("login: PAM Failure, aborting: %s\n"),
		         pam_strerror (pamh, retcode));
		SYSLOG ((LOG_ERR, "Couldn't initialize PAM: %s",
		         pam_strerror (pamh, retcode)));
		exit (99);
	}

	/*
	 * hostname & tty are either set to NULL or their correct values,
	 * depending on how much we know. We also set PAM's fail delay to
	 * ours.
	 *
	 * PAM_RHOST and PAM_TTY are used for authentication, only use
	 * information coming from login or from the caller (e.g. no utmp)
	 */
	retcode = pam_set_item (pamh, PAM_RHOST, hostname);
	PAM_FAIL_CHECK;
	retcode = pam_set_item (pamh, PAM_TTY, tty);
	PAM_FAIL_CHECK;
#ifdef HAS_PAM_FAIL_DELAY
	retcode = pam_fail_delay (pamh, 1000000 * delay);
	PAM_FAIL_CHECK;
#endif
	/* if fflg, then the user has already been authenticated */
	if (!fflg) {
		char          hostn[256];
		char          loginprompt[256]; //That's one hell of a prompt :)
		unsigned int  failcount = 0;

		/* Make the login prompt look like we want it */
		if (gethostname (hostn, sizeof (hostn)) == 0) {
			SNPRINTF(loginprompt, _("%s login: "), hostn);
		} else {
			STRTCPY(loginprompt, _("login: "));
		}

		retcode = pam_set_item (pamh, PAM_USER_PROMPT, loginprompt);
		PAM_FAIL_CHECK;

		/* if we didn't get a user on the command line,
		   set it to NULL */
		get_pam_user (&pam_user);
		if ((NULL != pam_user) && ('\0' == pam_user[0])) {
			retcode = pam_set_item (pamh, PAM_USER, NULL);
			PAM_FAIL_CHECK;
		}

		/*
		 * There may be better ways to deal with some of
		 * these conditions, but at least this way I don't
		 * think we'll be giving away information. Perhaps
		 * someday we can trust that all PAM modules will
		 * pay attention to failure count and get rid of
		 * MAX_LOGIN_TRIES?
		 */
		failcount = 0;
		while (true) {
			bool failed = false;

			failcount++;
#ifdef HAS_PAM_FAIL_DELAY
			if (delay > 0) {
				retcode = pam_fail_delay(pamh, 1000000*delay);
				PAM_FAIL_CHECK;
			}
#endif

			retcode = pam_authenticate (pamh, 0);

			get_pam_user (&pam_user);
			failent_user = get_failent_user (pam_user);

			if (retcode == PAM_MAXTRIES) {
				SYSLOG ((LOG_NOTICE,
				         "TOO MANY LOGIN TRIES (%u)%s FOR '%s'",
				         failcount, fromhost, failent_user));
				fprintf (stderr,
				         _("Maximum number of tries exceeded (%u)\n"),
				         failcount);
				PAM_END;
				exit(0);
			} else if (retcode == PAM_ABORT) {
				/* Serious problems, quit now */
				(void) fputs (_("login: abort requested by PAM\n"), stderr);
				SYSLOG ((LOG_ERR,"PAM_ABORT returned from pam_authenticate()"));
				PAM_END;
				exit(99);
			} else if (retcode != PAM_SUCCESS) {
				SYSLOG ((LOG_NOTICE,"FAILED LOGIN (%u)%s FOR '%s', %s",
				         failcount, fromhost, failent_user,
				         pam_strerror (pamh, retcode)));
				failed = true;
			}

			if (!failed) {
				break;
			}

#ifdef WITH_AUDIT
			audit_fd = audit_open ();
			audit_log_acct_message (audit_fd,
			                        AUDIT_USER_LOGIN,
			                        NULL,    /* Prog. name */
			                        "login",
			                        failent_user,
			                        AUDIT_NO_ID,
			                        hostname,
			                        NULL,    /* addr */
			                        tty,
			                        0);      /* result */
			close (audit_fd);
#endif				/* WITH_AUDIT */

			(void) puts ("");
			(void) puts (_("Login incorrect"));

			if (failcount >= retries) {
				SYSLOG ((LOG_NOTICE,
				         "TOO MANY LOGIN TRIES (%u)%s FOR '%s'",
				         failcount, fromhost, failent_user));
				fprintf (stderr,
				         _("Maximum number of tries exceeded (%u)\n"),
				         failcount);
				PAM_END;
				exit(0);
			}

			/*
			 * Let's give it another go around.
			 * Even if a username was given on the command
			 * line, prompt again for the username.
			 */
			retcode = pam_set_item (pamh, PAM_USER, NULL);
			PAM_FAIL_CHECK;
		}

		/* We don't get here unless they were authenticated above */
		(void) alarm (0);
	}

	/* Check the account validity */
	retcode = pam_acct_mgmt (pamh, 0);
	if (retcode == PAM_NEW_AUTHTOK_REQD) {
		retcode = pam_chauthtok (pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
	}
	PAM_FAIL_CHECK;

	/* Open the PAM session */
	get_pam_user (&pam_user);
	retcode = pam_open_session (pamh, hushed (pam_user) ? PAM_SILENT : 0);
	PAM_FAIL_CHECK;

	/* Grab the user information out of the password file for future usage
	 * First get the username that we are actually using, though.
	 *
	 * From now on, we will discard changes of the user (PAM_USER) by
	 * PAM APIs.
	 */
	get_pam_user (&pam_user);
	free (username);
	username = xstrdup (pam_user);
	failent_user = get_failent_user (username);

	pwd = xgetpwnam (username);
	if (NULL == pwd) {
		SYSLOG ((LOG_ERR, "cannot find user %s", failent_user));
		fprintf (stderr,
		         _("Cannot find user (%s)\n"),
		         username);
		exit (1);
	}

	/* This set up the process credential (group) and initialize the
	 * supplementary group access list.
	 * This has to be done before pam_setcred
	 */
	if (setup_groups (pwd) != 0) {
		exit (1);
	}

	retcode = pam_setcred (pamh, PAM_ESTABLISH_CRED);
	PAM_FAIL_CHECK;
	/* NOTE: If pam_setcred changes PAM_USER, this will not be taken
	 * into account.
	 */

#else				/* ! USE_PAM */
	while (true) {	/* repeatedly get login/password pairs */
		bool failed;
		/* user_passwd is always a pointer to this constant string
		 * or a passwd or shadow password that will be memzero by
		 * pw_free / spw_free.
		 * Do not free() user_passwd. */
		const char *user_passwd = "!";

		/* Do some cleanup to avoid keeping entries we do not need
		 * anymore. */
		if (NULL != pwd) {
			pw_free (pwd);
			pwd = NULL;
		}
		if (NULL != spwd) {
			spw_free (spwd);
			spwd = NULL;
		}

		failed = false;	/* haven't failed authentication yet */
		if (NULL == username) {	/* need to get a login id */
			size_t  max_size;

			max_size = login_name_max_size();
			if (subroot) {
				closelog ();
				exit (1);
			}
			preauth_flag = false;
			username = XMALLOC(max_size, char);
			username[max_size - 1] = '\0';
			login_prompt(username, max_size);

			if ('\0' == username[0]) {
				/* Prompt for a new login */
				free (username);
				username = NULL;
				continue;
			}
		}
		/* Get the username to be used to log failures */
		failent_user = get_failent_user (username);

		pwd = xgetpwnam (username);
		if (NULL == pwd) {
			preauth_flag = false;
			failed = true;
		} else {
			user_passwd = pwd->pw_passwd;
			/*
			 * If the encrypted password begins with a "!",
			 * the account is locked and the user cannot
			 * login, even if they have been
			 * "pre-authenticated."
			 */
			if (   ('!' == user_passwd[0])
			    || ('*' == user_passwd[0])) {
				failed = true;
			}

			if (strcmp (user_passwd, "") == 0) {
				const char *prevent_no_auth = getdef_str("PREVENT_NO_AUTH");

				if (prevent_no_auth == NULL) {
					prevent_no_auth = "superuser";
				}
				if (strcmp(prevent_no_auth, "yes") == 0) {
					failed = true;
				} else if ((pwd->pw_uid == 0)
					&& (strcmp(prevent_no_auth, "superuser") == 0)) {
					failed = true;
				}
			}
		}

		if (strcmp (user_passwd, SHADOW_PASSWD_STRING) == 0) {
			spwd = xgetspnam (username);
			if (NULL != spwd) {
				user_passwd = spwd->sp_pwdp;
			} else {
				/* The user exists in passwd, but not in
				 * shadow. SHADOW_PASSWD_STRING indicates
				 * that the password shall be in shadow.
				 */
				SYSLOG ((LOG_WARN,
				         "no shadow password for '%s'%s",
				         username, fromhost));
			}
		}

		/*
		 * The -f flag provides a name which has already
		 * been authenticated by some server.
		 */
		if (preauth_flag) {
			goto auth_ok;
		}

		if (pw_auth (user_passwd, username, reason, NULL) == 0) {
			goto auth_ok;
		}

		SYSLOG ((LOG_WARN, "invalid password for '%s' %s",
		         failent_user, fromhost));
		failed = true;

	      auth_ok:
		/*
		 * This is the point where all authenticated users wind up.
		 * If you reach this far, your password has been
		 * authenticated and so on.
		 */
		if (   !failed
		    && (NULL != pwd)
		    && (0 == pwd->pw_uid)
		    && !is_console) {
			SYSLOG ((LOG_CRIT, "ILLEGAL ROOT LOGIN %s", fromhost));
			failed = true;
		}
		if (   !failed
		    && !login_access (username, ('\0' != *hostname) ? hostname : tty)) {
			SYSLOG ((LOG_WARN, "LOGIN '%s' REFUSED %s",
			         username, fromhost));
			failed = true;
		}
		if (   (NULL != pwd)
		    && getdef_bool ("FAILLOG_ENAB")
		    && !failcheck (pwd->pw_uid, &faillog, failed)) {
			SYSLOG ((LOG_CRIT,
			         "exceeded failure limit for '%s' %s",
			         username, fromhost));
			failed = true;
		}
		if (!failed) {
			break;
		}

		/* don't log non-existent users */
		if ((NULL != pwd) && getdef_bool ("FAILLOG_ENAB")) {
			failure (pwd->pw_uid, tty, &faillog);
		}
#ifndef ENABLE_LOGIND
		record_failure(failent_user, tty, hostname);
#endif /* ENABLE_LOGIND */

		retries--;
		if (retries <= 0) {
			SYSLOG ((LOG_CRIT, "REPEATED login failures%s",
			         fromhost));
		}

		/*
		 * If this was a passwordless account and we get here, login
		 * was denied (securetty, faillog, etc.). There was no
		 * password prompt, so do it now (will always fail - the bad
		 * guys won't see that the passwordless account exists at
		 * all).  --marekm
		 */
		if (user_passwd[0] == '\0') {
			pw_auth ("!", username, reason, NULL);
		}

		/*
		 * Authentication of this user failed.
		 * The username must be confirmed in the next try.
		 */
		free (username);
		username = NULL;

		/*
		 * Wait a while (a la SVR4 /usr/bin/login) before attempting
		 * to login the user again. If the earlier alarm occurs
		 * before the sleep() below completes, login will exit.
		 */
		if (delay > 0) {
			(void) sleep (delay);
		}

		(void) puts (_("Login incorrect"));

		/* allow only one attempt with -f */
		if (fflg || (retries <= 0)) {
			closelog ();
			exit (1);
		}
	}			/* while (true) */
#endif				/* ! USE_PAM */
	assert (NULL != username);
	assert (NULL != pwd);

	(void) alarm (0);		/* turn off alarm clock */

#ifndef USE_PAM			/* PAM does this */
	/*
	 * porttime checks moved here, after the user has been
	 * authenticated. now prints a message, as suggested
	 * by Ivan Nejgebauer <ian@unsux.ns.ac.yu>.  --marekm
	 */
	if (   getdef_bool ("PORTTIME_CHECKS_ENAB")
	    && !isttytime (username, tty, time (NULL))) {
		SYSLOG ((LOG_WARN, "invalid login time for '%s'%s",
		         username, fromhost));
		closelog ();
		bad_time_notify ();
		exit (1);
	}

	check_nologin (pwd->pw_uid == 0);
#endif

	if (getenv ("IFS")) {	/* don't export user IFS ... */
		addenv ("IFS= \t\n", NULL);	/* ... instead, set a safe IFS */
	}

	if (pwd->pw_shell[0] == '*') {	/* subsystem root */
		pwd->pw_shell++;	/* skip the '*' */
		subsystem (pwd);	/* figure out what to execute */
		subroot = true;	/* say I was here again */
		endpwent ();	/* close all of the file which were */
		endgrent ();	/* open in the original rooted file */
		endspent ();	/* system. they will be re-opened */
#ifdef	SHADOWGRP
		endsgent ();	/* in the new rooted file system */
#endif
		goto top;	/* go do all this all over again */
	}

#ifdef WITH_AUDIT
	audit_fd = audit_open ();
	audit_log_acct_message (audit_fd,
	                        AUDIT_USER_LOGIN,
	                        NULL,    /* Prog. name */
	                        "login",
	                        username,
	                        AUDIT_NO_ID,
	                        hostname,
	                        NULL,    /* addr */
	                        tty,
	                        1);      /* result */
	close (audit_fd);
#endif				/* WITH_AUDIT */

#ifndef USE_PAM			/* pam_lastlog handles this */
#ifdef ENABLE_LASTLOG
	if (   getdef_bool ("LASTLOG_ENAB")
	    && pwd->pw_uid <= (uid_t) getdef_ulong ("LASTLOG_UID_MAX", 0xFFFFFFFFUL)) {
		/* give last login and log this one */
		dolastlog (&ll, pwd, tty, hostname);
	}
#endif /* ENABLE_LASTLOG */
#endif

#ifndef USE_PAM			/* PAM handles this as well */
	/*
	 * Have to do this while we still have root privileges, otherwise we
	 * don't have access to /etc/shadow.
	 */
	if (NULL != spwd) {		/* check for age of password */
		if (expire (pwd, spwd)) {
			/* The user updated her password, get the new
			 * entries.
			 * Use the x variants because we need to keep the
			 * entry for a long time, and there might be other
			 * getxxyyy in between.
			 */
			pw_free (pwd);
			pwd = xgetpwnam (username);
			if (NULL == pwd) {
				SYSLOG ((LOG_ERR,
				         "cannot find user %s after update of expired password",
				         username));
				exit (1);
			}
			spw_free (spwd);
			spwd = xgetspnam (username);
		}
	}
	setup_limits (pwd);	/* nice, ulimit etc. */
#endif				/* ! USE_PAM */
	chown_tty (pwd);

#ifdef USE_PAM
	/*
	 * We must fork before setuid() because we need to call
	 * pam_close_session() as root.
	 */
	(void) signal (SIGINT, SIG_IGN);
	child = fork ();
	if (child < 0) {
		/* error in fork() */
		fprintf (stderr, _("%s: failure forking: %s"),
		         Prog, strerror (errno));
		PAM_END;
		exit (0);
	} else if (child != 0) {
		/*
		 * parent - wait for child to finish, then cleanup
		 * session
		 */
		wait (NULL);
		PAM_END;
		exit (0);
	}
	/* child */
#endif

	/* If we were init, we need to start a new session */
	if (getppid() == 1) {
		setsid();
		if (ioctl(0, TIOCSCTTY, 1) != 0) {
			fprintf (stderr, _("TIOCSCTTY failed on %s"), tty);
		}
	}

#ifndef ENABLE_LOGIND
	/*
	 * The utmp entry needs to be updated to indicate the new status
	 * of the session, the new PID and SID.
	 */
	err = update_utmp (username, tty, hostname);
	if (err != 0) {
		SYSLOG ((LOG_WARN, "Unable to update utmp entry for %s", username));
	}
#endif /* ENABLE_LOGIND */

	/* The pwd and spwd entries for the user have been copied.
	 *
	 * Close all the files so that unauthorized access won't occur.
	 */
	endpwent ();		/* stop access to password file */
	endgrent ();		/* stop access to group file */
	endspent ();		/* stop access to shadow passwd file */
#ifdef	SHADOWGRP
	endsgent ();		/* stop access to shadow group file */
#endif

	/* Drop root privileges */
#ifndef USE_PAM
	if (setup_uid_gid (pwd, is_console))
#else
	/* The group privileges were already dropped.
	 * See setup_groups() above.
	 */
	if (change_uid (pwd))
#endif
	{
		exit (1);
	}

	setup_env (pwd);	/* set env vars, cd to the home dir */

#ifdef USE_PAM
	{
		const char *const *env;

		env = (const char *const *) pam_getenvlist (pamh);
		while ((NULL != env) && (NULL != *env)) {
			addenv (*env, NULL);
			env++;
		}
	}
	(void) pam_end (pamh, PAM_SUCCESS | PAM_DATA_SILENT);
#endif

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	if (!hushed (username)) {
		addenv ("HUSHLOGIN=FALSE", NULL);
		/*
		 * pam_unix, pam_mail and pam_lastlog should take care of
		 * this
		 */
#ifndef USE_PAM
		motd ();	/* print the message of the day */
		if (   getdef_bool ("FAILLOG_ENAB")
		    && (0 != faillog.fail_cnt)) {
			failprint (&faillog);
			/* Reset the lockout times if logged in */
			if (   (0 != faillog.fail_max)
			    && (faillog.fail_cnt >= faillog.fail_max)) {
				(void) puts (_("Warning: login re-enabled after temporary lockout."));
				SYSLOG ((LOG_WARN,
				         "login '%s' re-enabled after temporary lockout (%d failures)",
				         username, (int) faillog.fail_cnt));
			}
		}
#ifdef ENABLE_LASTLOG
		if (   getdef_bool ("LASTLOG_ENAB")
		    && pwd->pw_uid <= (uid_t) getdef_ulong ("LASTLOG_UID_MAX", 0xFFFFFFFFUL)
		    && (ll.ll_time != 0))
		{
			time_t     ll_time = ll.ll_time;
			struct tm  tm;

			localtime_r(&ll_time, &tm);
			STRFTIME(ptime, "%a %b %e %H:%M:%S %z %Y", &tm);
			printf (_("Last login: %s on %s"),
			        ptime, ll.ll_line);
#ifdef HAVE_LL_HOST		/* __linux__ || SUN4 */
			if ('\0' != ll.ll_host[0]) {
				printf (_(" from %.*s"),
				        (int) sizeof ll.ll_host, ll.ll_host);
			}
#endif
			printf (".\n");
		}
#endif /* ENABLE_LASTLOG */
		agecheck (spwd);

		mailcheck ();	/* report on the status of mail */
#endif				/* !USE_PAM */
	} else {
		addenv ("HUSHLOGIN=TRUE", NULL);
	}

	ttytype (tty);

	(void) signal (SIGQUIT, SIG_DFL);	/* default quit signal */
	(void) signal (SIGTERM, SIG_DFL);	/* default terminate signal */
	(void) signal (SIGALRM, SIG_DFL);	/* default alarm signal */
	(void) signal (SIGHUP, SIG_DFL);	/* added this.  --marekm */
	(void) signal (SIGINT, SIG_DFL);	/* default interrupt signal */

	if (0 == pwd->pw_uid) {
		SYSLOG ((LOG_NOTICE, "ROOT LOGIN %s", fromhost));
	} else if (getdef_bool ("LOG_OK_LOGINS")) {
		SYSLOG ((LOG_INFO, "'%s' logged in %s", username, fromhost));
	}
	closelog ();
	tmp = getdef_str ("FAKE_SHELL");
	if (NULL != tmp) {
		err = shell (tmp, pwd->pw_shell, newenvp); /* fake shell */
	} else {
		/* exec the shell finally */
		err = shell (pwd->pw_shell, NULL, newenvp);
	}

	return ((err == ENOENT) ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
}

