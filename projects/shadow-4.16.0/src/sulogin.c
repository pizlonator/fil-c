/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2002 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "agetpass.h"
#include "alloc.h"
#include "attr.h"
#include "defines.h"
#include "getdef.h"
#include "prototypes.h"
#include "pwauth.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"


/*
 * Global variables
 */
static const char Prog[] = "sulogin";


extern char **newenvp;

#ifndef	ALARM
#define	ALARM	60
#endif


static void catch_signals (int);
static int pw_entry(const char *name, struct passwd *pwent);


static void catch_signals (MAYBE_UNUSED int sig)
{
	_exit (1);
}


int
main(int argc, char *argv[])
{
	int            err = 0;
	char           **envp = environ;
	TERMIO         termio;
	struct passwd  pwent = {};
	bool           done;
#ifndef USE_PAM
	const char     *env;
#endif


	tcgetattr (0, &termio);
	termio.c_iflag |= (ICRNL | IXON);
	termio.c_oflag |= (CREAD);
	termio.c_lflag |= (ECHO | ECHOE | ECHOK | ICANON | ISIG);
	tcsetattr (0, TCSANOW, &termio);

	log_set_progname(Prog);
	log_set_logfd(stderr);
	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	initenv();
	if (argc > 1) {
		close(0);
		close(1);
		close(2);

		if (open(argv[1], O_RDWR) == -1)
			exit(1);
		dup(0);
		dup(0);
	}
	if (access (PASSWD_FILE, F_OK) == -1) {	/* must be a password file! */
		(void) puts (_("No password file"));
		exit (1);
	}
#if !defined(DEBUG) && defined(SULOGIN_ONLY_INIT)
	if (getppid () != 1) {	/* parent must be INIT */
		exit (1);
	}
#endif
	if ((isatty (0) == 0) || (isatty (1) == 0) || (isatty (2) == 0)) {
		exit (1);	/* must be a terminal */
	}
	/* If we were init, we need to start a new session */
	if (getppid() == 1) {
		setsid();
		if (ioctl(0, TIOCSCTTY, 1) != 0) {
			(void) fputs (_("TIOCSCTTY failed"), stderr);
		}
	}
	while (NULL != *envp) {		/* add inherited environment, */
		addenv (*envp, NULL);	/* some variables change later */
		envp++;
	}

#ifndef USE_PAM
	env = getdef_str ("ENV_TZ");
	if (NULL != env) {
		addenv (('/' == *env) ? tz (env) : env, NULL);
	}
	env = getdef_str ("ENV_HZ");
	if (NULL != env) {
		addenv (env, NULL);	/* set the default $HZ, if one */
	}
#endif				/* !USE_PAM */

	(void) signal (SIGALRM, catch_signals);	/* exit if the timer expires */
	(void) alarm (ALARM);		/* only wait so long ... */

	do {			/* repeatedly get login/password pairs */
		char *pass;
		if (pw_entry("root", &pwent) == -1) {	/* get entry from password file */
			/*
			 * Fail secure
			 */
			(void) puts(_("No password entry for 'root'"));
			exit(1);
		}

		/*
		 * Here we prompt for the root password, or if no password
		 * is given we just exit.
		 */

		/* get a password for root */
		pass = agetpass (_(
"\n"
"Type control-d to proceed with normal startup,\n"
"(or give root password for system maintenance):"));
		/*
		 * XXX - can't enter single user mode if root password is
		 * empty.  I think this doesn't happen very often :-). But
		 * it will work with standard getpass() (no NULL on EOF).
		 * --marekm
		 */
		if ((NULL == pass) || ('\0' == *pass)) {
			erase_pass (pass);
			(void) puts ("");
#ifdef	TELINIT
			execl (PATH_TELINIT, "telinit", RUNLEVEL, (char *) NULL);
#endif
			exit (0);
		}

		done = valid(pass, &pwent);
		erase_pass (pass);

		if (!done) {	/* check encrypted passwords ... */
			/* ... encrypted passwords did not match */
			sleep (2);
			(void) puts (_("Login incorrect"));
		}
	} while (!done);
	(void) alarm (0);
	(void) signal (SIGALRM, SIG_DFL);
	environ = newenvp;	/* make new environment active */

	(void) puts (_("Entering System Maintenance Mode"));

	/* exec the shell finally. */
	err = shell (pwent.pw_shell, NULL, environ);

	return ((err == ENOENT) ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
}


static int
pw_entry(const char *name, struct passwd *pwent)
{
	struct spwd    *spwd;
	struct passwd  *passwd;

	if (!(passwd = getpwnam(name)))  /* local, no need for xgetpwnam */
		return -1;

	free(pwent->pw_name);
	pwent->pw_name = xstrdup(passwd->pw_name);
	pwent->pw_uid = passwd->pw_uid;
	pwent->pw_gid = passwd->pw_gid;
	free(pwent->pw_gecos);
	pwent->pw_gecos = xstrdup(passwd->pw_gecos);
	free(pwent->pw_dir);
	pwent->pw_dir = xstrdup(passwd->pw_dir);
	free(pwent->pw_shell);
	pwent->pw_shell = xstrdup(passwd->pw_shell);
#if !defined(AUTOSHADOW)
	/* local, no need for xgetspnam */
	if ((spwd = getspnam(name))) {
		free(pwent->pw_passwd);
		pwent->pw_passwd = xstrdup(spwd->sp_pwdp);
		return 0;
	}
#endif
	free(pwent->pw_passwd);
	pwent->pw_passwd = xstrdup(passwd->pw_passwd);
	return 0;
}
