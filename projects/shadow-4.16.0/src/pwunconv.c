/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2012, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include "defines.h"
#include "getdef.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#include "pwio.h"
#include "shadowio.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"

/*
 * Global variables
 */
static const char Prog[] = "pwunconv";

static bool spw_locked = false;
static bool pw_locked = false;

/* local function prototypes */
static void fail_exit (int status);
static void usage (int status);
static void process_flags (int argc, char **argv);

static void fail_exit (int status)
{
	if (spw_locked) {
		if (spw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
			/* continue */
		}
	}
	if (pw_locked) {
		if (pw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
			/* continue */
		}
	}
	exit (status);
}

static void usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options]\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
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
	/*
	 * Parse the command line options.
	 */
	int c;
	static struct option long_options[] = {
		{"help", no_argument,       NULL, 'h'},
		{"root", required_argument, NULL, 'R'},
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv, "hR:",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
		default:
			usage (E_USAGE);
		}
	}

	if (optind != argc) {
		usage (E_USAGE);
	}
}

int main (int argc, char **argv)
{
	const struct passwd *pw;
	struct passwd pwent;
	const struct spwd *spwd;

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);

	OPENLOG (Prog);

	process_flags (argc, argv);

#ifdef WITH_TCB
	if (getdef_bool("USE_TCB")) {
		fprintf (stderr, _("%s: can't work with tcb enabled\n"), Prog);
		exit (1);
	}
#endif				/* WITH_TCB */

	if (!spw_file_present ()) {
		/* shadow not installed, do nothing */
		exit (0);
	}

	if (pw_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, pw_dbname ());
		fail_exit (5);
	}
	pw_locked = true;
	if (pw_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"),
		         Prog, pw_dbname ());
		fail_exit (1);
	}

	if (spw_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, spw_dbname ());
		fail_exit (5);
	}
	spw_locked = true;
	if (spw_open (O_RDONLY) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"),
		         Prog, spw_dbname ());
		fail_exit (1);
	}

	(void) pw_rewind ();
	while ((pw = pw_next ()) != NULL) {
		spwd = spw_locate (pw->pw_name);
		if (NULL == spwd) {
			continue;
		}

		pwent = *pw;

		/*
		 * Update password if non-shadow is "x".
		 */
		if (strcmp (pw->pw_passwd, SHADOW_PASSWD_STRING) == 0) {
			pwent.pw_passwd = spwd->sp_pwdp;
		}

		/*
		 * Password aging works differently in the two different
		 * systems. With shadow password files you apparently must
		 * have some aging information. The maxweeks or minweeks
		 * may not map exactly. In pwconv we set max == 10000,
		 * which is about 30 years. Here we have to undo that
		 * kludge. So, if maxdays == 10000, no aging information is
		 * put into the new file. Otherwise, the days are converted
		 * to weeks and so on.
		 */
		if (pw_update (&pwent) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, pw_dbname (), pwent.pw_name);
			fail_exit (3);
		}
	}

	(void) spw_close (); /* was only open O_RDONLY */

	if (pw_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (3);
	}

	if (unlink (SHADOW) != 0) {
		fprintf (stderr,
			 _("%s: cannot delete %s\n"), Prog, SHADOW);
		SYSLOG ((LOG_ERR, "cannot delete %s", SHADOW));
		fail_exit (3);
	}

	if (spw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
		/* continue */
	}
	if (pw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}

	nscd_flush_cache ("passwd");
	sssd_flush_cache (SSSD_DB_PASSWD);

	return 0;
}

