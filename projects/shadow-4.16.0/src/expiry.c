/*
 * SPDX-FileCopyrightText: 1994       , Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>

#include "attr.h"
#include "defines.h"
#include "prototypes.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"

/* Global variables */
static const char Prog[] = "expiry";
static bool cflg = false;

/* local function prototypes */
static void catch_signals (MAYBE_UNUSED int sig);
NORETURN static void usage (int status);
static void process_flags (int argc, char **argv);

/*
 * catch_signals - signal catcher
 */
static void catch_signals (MAYBE_UNUSED int sig)
{
	_exit (10);
}

/*
 * usage - print syntax message and exit
 */
NORETURN
static void
usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options]\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -c, --check                   check the user's password expiration\n"), usageout);
	(void) fputs (_("  -f, --force                   force password change if the user's password\n"
	                "                                is expired\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * process_flags - parse the command line options
 *
 *	It will not return if an error is encountered.
 */
static void process_flags (int argc, char **argv)
{
	bool fflg = false;
	int c;
	static struct option long_options[] = {
		{"check", no_argument, NULL, 'c'},
		{"force", no_argument, NULL, 'f'},
		{"help",  no_argument, NULL, 'h'},
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv, "cfh",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'c':
			cflg = true;
			break;
		case 'f':
			fflg = true;
			break;
		case 'h':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		default:
			usage (E_USAGE);
		}
	}

	if (! (cflg || fflg)) {
		usage (E_USAGE);
	}

	if (cflg && fflg) {
		fprintf (stderr,
		         _("%s: options %s and %s conflict\n"),
		         Prog, "-c", "-f");
		usage (E_USAGE);
	}

	if (argc != optind) {
		fprintf (stderr,
		         _("%s: unexpected argument: %s\n"),
		         Prog, argv[optind]);
		usage (E_USAGE);
	}
}

/*
 * expiry - check and enforce password expiration policy
 *
 *	expiry checks (-c) the current password expiration and forces (-f)
 *	changes when required. It is callable as a normal user command.
 */
int main (int argc, char **argv)
{
	struct passwd *pwd;
	struct spwd *spwd;

	sanitize_env ();
	check_fds ();

	log_set_progname(Prog);
	log_set_logfd(stderr);

	/*
	 * Start by disabling all of the keyboard signals.
	 */
	(void) signal (SIGHUP, catch_signals);
	(void) signal (SIGINT, catch_signals);
	(void) signal (SIGQUIT, catch_signals);
	(void) signal (SIGTSTP, catch_signals);

	/*
	 * expiry takes one of two arguments. The default action is to give
	 * the usage message.
	 */
	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	OPENLOG (Prog);

	process_flags (argc, argv);

	/*
	 * Get user entries for /etc/passwd and /etc/shadow
	 */
	pwd = get_my_pwent ();
	if (NULL == pwd) {
		fprintf (stderr, _("%s: Cannot determine your user name.\n"),
		         Prog);
		SYSLOG ((LOG_WARN, "Cannot determine the user name of the caller (UID %lu)",
		         (unsigned long) getuid ()));
		exit (10);
	}
	spwd = getspnam (pwd->pw_name); /* !USE_PAM, No need for xgetspnam */

	/*
	 * If checking accounts, use agecheck() function.
	 */
	if (cflg) {
		/*
		 * Print out number of days until expiration.
		 */
		agecheck (spwd);

		/*
		 * Exit with status indicating state of account.
		 */
		exit (isexpired (pwd, spwd));
	}

	/*
	 * Otherwise, force a password change with the expire() function.
	 * It will force the change or give a message indicating what to
	 * do.
	 * It won't return unless the account is unexpired.
	 */
	(void) expire (pwd, spwd);

	return E_SUCCESS;
}

