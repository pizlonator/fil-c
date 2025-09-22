/*
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2002 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2009 - 2012, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * pwconv - create or update /etc/shadow with information from
 * /etc/passwd.
 *
 * It is more like SysV pwconv, slightly different from the original Shadow
 * pwconv. Depends on "x" as password in /etc/passwd which means that the
 * password has already been moved to /etc/shadow. There is no need to move
 * /etc/npasswd to /etc/passwd, password files are updated using library
 * routines with proper locking.
 *
 * Can be used to update /etc/shadow after adding/deleting users by editing
 * /etc/passwd. There is no man page yet, but this program should be close
 * to pwconv(1M) on Solaris 2.x.
 *
 * Warning: make sure that all users have "x" as the password in /etc/passwd
 * before running this program for the first time on a system which already
 * has shadow passwords. Anything else (like "*" from old versions of the
 * shadow suite) will replace the user's encrypted password in /etc/shadow.
 *
 * Doesn't currently support pw_age information in /etc/passwd, and doesn't
 * support DBM files. Add it if you need it...
 *
 */

#include <config.h>

#ident "$Id$"

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

#include "defines.h"
#include "getdef.h"
#include "prototypes.h"
#include "pwio.h"
#include "shadowio.h"
#include "nscd.h"
#include "sssd.h"
#include "shadowlog.h"

/*
 * exit status values
 */
/*@-exitarg@*/
#define E_SUCCESS	0	/* success */
#define E_NOPERM	1	/* permission denied */
#define E_USAGE		2	/* invalid command syntax */
#define E_FAILURE	3	/* unexpected failure, nothing done */
#define E_MISSING	4	/* unexpected failure, passwd file missing */
#define E_PWDBUSY	5	/* passwd file(s) busy */
#define E_BADENTRY	6	/* bad shadow entry */
/*
 * Global variables
 */
static const char Prog[] = "pwconv";

static bool spw_locked = false;
static bool pw_locked = false;

/* local function prototypes */
static void fail_exit (int status);
static void usage (int status);
static void process_flags (int argc, char **argv);

static void fail_exit (int status)
{
	if (pw_locked) {
		if (pw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
			/* continue */
		}
	}

	if (spw_locked) {
		if (spw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
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
	const struct spwd *sp;
	struct spwd spent;

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
		exit (E_FAILURE);
	}
#endif				/* WITH_TCB */

	if (pw_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, pw_dbname ());
		fail_exit (E_PWDBUSY);
	}
	pw_locked = true;
	if (pw_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"), Prog, pw_dbname ());
		fail_exit (E_MISSING);
	}

	if (spw_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, spw_dbname ());
		fail_exit (E_PWDBUSY);
	}
	spw_locked = true;
	if (spw_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"), Prog, spw_dbname ());
		fail_exit (E_FAILURE);
	}

	/*
	 * Remove /etc/shadow entries for users not in /etc/passwd.
	 */
	(void) spw_rewind ();
	while ((sp = spw_next ()) != NULL) {
		if (pw_locate (sp->sp_namp) != NULL) {
			continue;
		}

		if (spw_remove (sp->sp_namp) == 0) {
			/*
			 * This shouldn't happen (the entry exists) but...
			 */
			fprintf (stderr,
			         _("%s: cannot remove entry '%s' from %s\n"),
			         Prog, sp->sp_namp, spw_dbname ());
			fail_exit (E_FAILURE);
		}
		(void) spw_rewind();
	}

	/*
	 * Update shadow entries which don't have "x" as pw_passwd. Add any
	 * missing shadow entries.
	 */
	(void) pw_rewind ();
	while ((pw = pw_next ()) != NULL) {
		sp = spw_locate (pw->pw_name);
		if (NULL != sp) {
			/* do we need to update this entry? */
			if (strcmp (pw->pw_passwd, SHADOW_PASSWD_STRING) == 0) {
				continue;
			}
			/* update existing shadow entry */
			spent = *sp;
		} else {
			/* add new shadow entry */
			bzero(&spent, sizeof spent);
			spent.sp_namp   = pw->pw_name;
			spent.sp_min    = getdef_num ("PASS_MIN_DAYS", -1);
			spent.sp_max    = getdef_num ("PASS_MAX_DAYS", -1);
			spent.sp_warn   = getdef_num ("PASS_WARN_AGE", -1);
			spent.sp_inact  = -1;
			spent.sp_expire = -1;
			spent.sp_flag   = SHADOW_SP_FLAG_UNSET;
		}
		spent.sp_pwdp = pw->pw_passwd;
		spent.sp_lstchg = gettime () / DAY;
		if (0 == spent.sp_lstchg) {
			/* Better disable aging than requiring a password
			 * change */
			spent.sp_lstchg = -1;
		}
		if (spw_update (&spent) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, spw_dbname (), spent.sp_namp);
			fail_exit (E_FAILURE);
		}

		/* remove password from /etc/passwd */
		pwent = *pw;
		pwent.pw_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */
		if (pw_update (&pwent) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, pw_dbname (), pwent.pw_name);
			fail_exit (E_FAILURE);
		}
	}

	if (spw_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, spw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", spw_dbname ()));
		fail_exit (E_FAILURE);
	}
	if (pw_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (E_FAILURE);
	}

	/* /etc/passwd- (backup file) */
	errno = 0;
	if ((chmod (PASSWD_FILE "-", 0600) != 0) && (errno != ENOENT)) {
		fprintf (stderr,
		         _("%s: failed to change the mode of %s to 0600\n"),
		         Prog, PASSWD_FILE "-");
		SYSLOG ((LOG_ERR, "failed to change the mode of %s to 0600", PASSWD_FILE "-"));
		/* continue */
	}

	if (pw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}

	if (spw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
		/* continue */
	}

	nscd_flush_cache ("passwd");
	sssd_flush_cache (SSSD_DB_PASSWD);

	return E_SUCCESS;
}

