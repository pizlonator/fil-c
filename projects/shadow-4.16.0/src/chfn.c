/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>

#include "alloc.h"
#include "defines.h"
#include "getdef.h"
#include "nscd.h"
#include "sssd.h"
#ifdef USE_PAM
#include "pam_defs.h"
#endif
#include "prototypes.h"
#include "pwauth.h"
#include "pwio.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"
#include "string/sprintf.h"
#include "string/strtcpy.h"


/*
 * Global variables.
 */
static const char Prog[] = "chfn";
static char fullnm[BUFSIZ];
static char roomno[BUFSIZ];
static char workph[BUFSIZ];
static char homeph[BUFSIZ];
static char slop[BUFSIZ + 1 + 80];
static bool amroot;
/* Flags */
static bool fflg = false;		/* -f - set full name                */
static bool rflg = false;		/* -r - set room number              */
static bool wflg = false;		/* -w - set work phone number        */
static bool hflg = false;		/* -h - set home phone number        */
static bool oflg = false;		/* -o - set other information        */
static bool pw_locked = false;

/*
 * External identifiers
 */

/* local function prototypes */
NORETURN static void fail_exit (int code);
NORETURN static void usage (int status);
static bool may_change_field (int);
static void new_fields (void);
static char *copy_field (char *, char *, char *);
static void process_flags (int argc, char **argv);
static void check_perms (const struct passwd *pw);
static void update_gecos (const char *user, char *gecos);
static void get_old_fields (const char *gecos);

/*
 * fail_exit - exit with an error and do some cleanup
 */
static void fail_exit (int code)
{
	if (pw_locked) {
		if (pw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
			/* continue */
		}
	}
	pw_locked = false;

	closelog ();

	exit (code);
}

/*
 * usage - print command line syntax and exit
 */
NORETURN
static void
usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options] [LOGIN]\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -f, --full-name FULL_NAME     change user's full name\n"), usageout);
	(void) fputs (_("  -h, --home-phone HOME_PHONE   change user's home phone number\n"), usageout);
	(void) fputs (_("  -o, --other OTHER_INFO        change user's other GECOS information\n"), usageout);
	(void) fputs (_("  -r, --room ROOM_NUMBER        change user's room number\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -u, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -w, --work-phone WORK_PHONE   change user's office phone number\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * may_change_field - indicate if the user is allowed to change a given field
 *                    of her gecos information
 *
 *	root can change any field.
 *
 *	field should be one of 'f', 'r', 'w', 'h'
 *
 *	Return true if the user can change the field and false otherwise.
 */
static bool may_change_field (int field)
{
	const char *cp;

	/*
	 * CHFN_RESTRICT can now specify exactly which fields may be changed
	 * by regular users, by using any combination of the following
	 * letters:
	 *  f - full name
	 *  r - room number
	 *  w - work phone
	 *  h - home phone
	 *
	 * This makes it possible to disallow changing the room number
	 * information, for example - this feature was suggested by Maciej
	 * 'Tycoon' Majchrowski.
	 *
	 * For backward compatibility, "yes" is equivalent to "rwh",
	 * "no" is equivalent to "frwh". Only root can change anything
	 * if the string is empty or not defined at all.
	 */
	if (amroot) {
		return true;
	}

	cp = getdef_str ("CHFN_RESTRICT");
	if (NULL == cp) {
		cp = "";
	} else if (strcmp (cp, "yes") == 0) {
		cp = "rwh";
	} else if (strcmp (cp, "no") == 0) {
		cp = "frwh";
	}

	if (strchr (cp, field) != NULL) {
		return true;
	}

	return false;
}

/*
 * new_fields - change the user's GECOS information interactively
 *
 * prompt the user for each of the four fields and fill in the fields from
 * the user's response, or leave alone if nothing was entered.
 */
static void new_fields (void)
{
	puts (_("Enter the new value, or press ENTER for the default"));

	if (may_change_field ('f')) {
		change_field (fullnm, sizeof fullnm, _("Full Name"));
	} else {
		printf (_("\t%s: %s\n"), _("Full Name"), fullnm);
	}

	if (may_change_field ('r')) {
		change_field (roomno, sizeof roomno, _("Room Number"));
	} else {
		printf (_("\t%s: %s\n"), _("Room Number"), roomno);
	}

	if (may_change_field ('w')) {
		change_field (workph, sizeof workph, _("Work Phone"));
	} else {
		printf (_("\t%s: %s\n"), _("Work Phone"), workph);
	}

	if (may_change_field ('h')) {
		change_field (homeph, sizeof homeph, _("Home Phone"));
	} else {
		printf (_("\t%s: %s\n"), _("Home Phone"), homeph);
	}

	if (amroot) {
		change_field (slop, sizeof slop, _("Other"));
	}
}

/*
 * copy_field - get the next field from the gecos field
 *
 * copy_field copies the next field from the gecos field, returning a
 * pointer to the field which follows, or NULL if there are no more fields.
 *
 *	in - the current GECOS field
 *	out - where to copy the field to
 *	extra - fields with '=' get copied here
 */
static char *copy_field (char *in, char *out, char *extra)
{
	char *cp = NULL;

	while (NULL != in) {
		cp = strchr (in, ',');
		if (NULL != cp) {
			*cp++ = '\0';
		}

		if (strchr (in, '=') == NULL) {
			break;
		}

		if (NULL != extra) {
			if ('\0' != extra[0]) {
				strcat (extra, ",");
			}

			strcat (extra, in);
		}
		in = cp;
	}
	if ((NULL != in) && (NULL != out)) {
		strcpy (out, in);
	}

	return cp;
}

/*
 * process_flags - parse the command line options
 *
 *	It will not return if an error is encountered.
 */
static void process_flags (int argc, char **argv)
{
	int c;		/* flag currently being processed    */
	static struct option long_options[] = {
		{"full-name",  required_argument, NULL, 'f'},
		{"home-phone", required_argument, NULL, 'h'},
		{"other",      required_argument, NULL, 'o'},
		{"room",       required_argument, NULL, 'r'},
		{"root",       required_argument, NULL, 'R'},
		{"help",       no_argument,       NULL, 'u'},
		{"work-phone", required_argument, NULL, 'w'},
		{NULL, 0, NULL, '\0'}
	};

	/*
	 * The remaining arguments will be processed one by one and executed
	 * by this command. The name is the last argument if it does not
	 * begin with a "-", otherwise the name is determined from the
	 * environment and must agree with the real UID. Also, the UID will
	 * be checked for any commands which are restricted to root only.
	 */
	while ((c = getopt_long (argc, argv, "f:h:o:r:R:uw:",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'f':
			if (!may_change_field ('f')) {
				fprintf (stderr,
				         _("%s: Permission denied.\n"), Prog);
				exit (E_NOPERM);
			}
			fflg = true;
			STRTCPY(fullnm, optarg);
			break;
		case 'h':
			if (!may_change_field ('h')) {
				fprintf (stderr,
				         _("%s: Permission denied.\n"), Prog);
				exit (E_NOPERM);
			}
			hflg = true;
			STRTCPY(homeph, optarg);
			break;
		case 'o':
			if (!amroot) {
				fprintf (stderr,
				         _("%s: Permission denied.\n"), Prog);
				exit (E_NOPERM);
			}
			oflg = true;
			if (strlen (optarg) > (unsigned int) 80) {
				fprintf (stderr,
				         _("%s: fields too long\n"), Prog);
				exit (E_NOPERM);
			}
			STRTCPY(slop, optarg);
			break;
		case 'r':
			if (!may_change_field ('r')) {
				fprintf (stderr,
				         _("%s: Permission denied.\n"), Prog);
				exit (E_NOPERM);
			}
			rflg = true;
			STRTCPY(roomno, optarg);
			break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
		case 'u':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		case 'w':
			if (!may_change_field ('w')) {
				fprintf (stderr,
				         _("%s: Permission denied.\n"), Prog);
				exit (E_NOPERM);
			}
			wflg = true;
			STRTCPY(workph, optarg);
			break;
		default:
			usage (E_USAGE);
		}
	}
}

/*
 * check_perms - check if the caller is allowed to add a group
 *
 *	Non-root users are only allowed to change their gecos field.
 *	(see also may_change_field())
 *
 *	Non-root users must be authenticated.
 *
 *	It will not return if the user is not allowed.
 */
static void check_perms (const struct passwd *pw)
{
#ifdef USE_PAM
	pam_handle_t *pamh = NULL;
	int retval;
	struct passwd *pampw;
#endif

	/*
	 * Non-privileged users are only allowed to change the gecos field
	 * if the UID of the user matches the current real UID.
	 */
	if (!amroot && pw->pw_uid != getuid ()) {
		fprintf (stderr, _("%s: Permission denied.\n"), Prog);
		closelog ();
		exit (E_NOPERM);
	}
#ifdef WITH_SELINUX
	/*
	 * If the UID of the user does not match the current real UID,
	 * check if the change is allowed by SELinux policy.
	 */
	if ((pw->pw_uid != getuid ())
	    && (check_selinux_permit (Prog) != 0)) {
		fprintf (stderr, _("%s: Permission denied.\n"), Prog);
		closelog ();
		exit (E_NOPERM);
	}
#endif

#ifndef USE_PAM
	/*
	 * Non-privileged users are optionally authenticated (must enter the
	 * password of the user whose information is being changed) before
	 * any changes can be made. Idea from util-linux chfn/chsh.
	 * --marekm
	 */
	if (!amroot && getdef_bool ("CHFN_AUTH")) {
		passwd_check (pw->pw_name, pw->pw_passwd, Prog);
	}

#else				/* !USE_PAM */
	pampw = getpwuid (getuid ()); /* local, no need for xgetpwuid */
	if (NULL == pampw) {
		fprintf (stderr,
		         _("%s: Cannot determine your user name.\n"),
		         Prog);
		exit (E_NOPERM);
	}

	retval = pam_start (Prog, pampw->pw_name, &conv, &pamh);

	if (PAM_SUCCESS == retval) {
		retval = pam_authenticate (pamh, 0);
	}

	if (PAM_SUCCESS == retval) {
		retval = pam_acct_mgmt (pamh, 0);
	}

	if (PAM_SUCCESS != retval) {
		fprintf (stderr, _("%s: PAM: %s\n"),
		         Prog, pam_strerror (pamh, retval));
		SYSLOG((LOG_ERR, "%s", pam_strerror (pamh, retval)));
		if (NULL != pamh) {
			(void) pam_end (pamh, retval);
		}
		exit (E_NOPERM);
	}
	(void) pam_end (pamh, retval);
#endif				/* USE_PAM */
}

/*
 * update_gecos - update the gecos fields in the password database
 *
 *	Commit the user's entry after changing her gecos field.
 */
static void update_gecos (const char *user, char *gecos)
{
	const struct passwd *pw;	/* The user's password file entry */
	struct passwd pwent;		/* modified password file entry */

	/*
	 * Before going any further, raise the ulimit to prevent colliding
	 * into a lowered ulimit, and set the real UID to root to protect
	 * against unexpected signals. Any keyboard signals are set to be
	 * ignored.
	 */
	if (setuid (0) != 0) {
		fputs (_("Cannot change ID to root.\n"), stderr);
		SYSLOG ((LOG_ERR, "can't setuid(0)"));
		fail_exit (E_NOPERM);
	}
	pwd_init ();

	/*
	 * The passwd entry is now ready to be committed back to the
	 * password file. Get a lock on the file and open it.
	 */
	if (pw_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, pw_dbname ());
		fail_exit (E_NOPERM);
	}
	pw_locked = true;
	if (pw_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"), Prog, pw_dbname ());
		fail_exit (E_NOPERM);
	}

	/*
	 * Get the entry to update using pw_locate() - we want the real one
	 * from /etc/passwd, not the one from getpwnam() which could contain
	 * the shadow password if (despite the warnings) someone enables
	 * AUTOSHADOW (or SHADOW_COMPAT in libc).  --marekm
	 */
	pw = pw_locate (user);
	if (NULL == pw) {
		fprintf (stderr,
		         _("%s: user '%s' does not exist in %s\n"),
		         Prog, user, pw_dbname ());
		fail_exit (E_NOPERM);
	}

	/*
	 * Make a copy of the entry, then change the gecos field. The other
	 * fields remain unchanged.
	 */
	pwent = *pw;
	pwent.pw_gecos = gecos;

	/*
	 * Update the passwd file entry. If there is a DBM file, update that
	 * entry as well.
	 */
	if (pw_update (&pwent) == 0) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"),
		         Prog, pw_dbname (), pwent.pw_name);
		fail_exit (E_NOPERM);
	}

	/*
	 * Changes have all been made, so commit them and unlock the file.
	 */
	if (pw_close () == 0) {
		fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (E_NOPERM);
	}
	if (pw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}
	pw_locked = false;
}

/*
 * get_old_fields - parse the old gecos and use the old value for the fields
 *                  which are not set on the command line
 */
static void get_old_fields (const char *gecos)
{
	char *cp;		/* temporary character pointer       */
	char old_gecos[BUFSIZ];	/* buffer for old GECOS fields       */

	STRTCPY(old_gecos, gecos);

	/*
	 * Now get the full name. It is the first comma separated field in
	 * the GECOS field.
	 */
	cp = copy_field (old_gecos, fflg ? NULL : fullnm, slop);

	/*
	 * Now get the room number. It is the next comma separated field,
	 * if there is indeed one.
	 */
	if (NULL != cp) {
		cp = copy_field (cp, rflg ? NULL : roomno, slop);
	}

	/*
	 * Now get the work phone number. It is the third field.
	 */
	if (NULL != cp) {
		cp = copy_field (cp, wflg ? NULL : workph, slop);
	}

	/*
	 * Now get the home phone number. It is the fourth field.
	 */
	if (NULL != cp) {
		cp = copy_field (cp, hflg ? NULL : homeph, slop);
	}

	/*
	 * Anything left over is "slop".
	 */
	if ((NULL != cp) && !oflg) {
		if ('\0' != slop[0]) {
			strcat (slop, ",");
		}

		strcat (slop, cp);
	}
}

/*
 * check_fields - check all of the fields for valid information
 *
 *	It will not return if a field is not valid.
 */
static void check_fields (void)
{
	int err;
	err = valid_field (fullnm, ":,=\n");
	if (err > 0) {
		fprintf (stderr, _("%s: name with non-ASCII characters: '%s'\n"), Prog, fullnm);
	} else if (err < 0) {
		fprintf (stderr, _("%s: invalid name: '%s'\n"), Prog, fullnm);
		fail_exit (E_NOPERM);
	}
	err = valid_field (roomno, ":,=\n");
	if (err > 0) {
		fprintf (stderr, _("%s: room number with non-ASCII characters: '%s'\n"), Prog, roomno);
	} else if (err < 0) {
		fprintf (stderr, _("%s: invalid room number: '%s'\n"),
		         Prog, roomno);
		fail_exit (E_NOPERM);
	}
	if (valid_field (workph, ":,=\n") != 0) {
		fprintf (stderr, _("%s: invalid work phone: '%s'\n"),
		         Prog, workph);
		fail_exit (E_NOPERM);
	}
	if (valid_field (homeph, ":,=\n") != 0) {
		fprintf (stderr, _("%s: invalid home phone: '%s'\n"),
		         Prog, homeph);
		fail_exit (E_NOPERM);
	}
	err = valid_field (slop, ":\n");
	if (err > 0) {
		fprintf (stderr, _("%s: '%s' contains non-ASCII characters\n"), Prog, slop);
	} else if (err < 0) {
		fprintf (stderr,
		         _("%s: '%s' contains illegal characters\n"),
		         Prog, slop);
		fail_exit (E_NOPERM);
	}
}

/*
 * chfn - change a user's password file information
 *
 *	This command controls the GECOS field information in the password
 *	file entry.
 *
 *	The valid options are
 *
 *	-f	full name
 *	-r	room number
 *	-w	work phone number
 *	-h	home phone number
 *	-o	other information (*)
 *
 *	(*) requires root permission to execute.
 */
int main (int argc, char **argv)
{
	char                 new_gecos[BUFSIZ];
	char                 *user;
	const struct passwd  *pw;

	sanitize_env ();
	check_fds ();

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);

	/*
	 * This command behaves different for root and non-root
	 * users.
	 */
	amroot = (getuid () == 0);

	OPENLOG (Prog);

	/* parse the command line options */
	process_flags (argc, argv);

	/*
	 * Get the name of the user to check. It is either the command line
	 * name, or the name getlogin() returns.
	 */
	if (optind < argc) {
		user = argv[optind];
		pw = xgetpwnam (user);
		if (NULL == pw) {
			fprintf (stderr, _("%s: user '%s' does not exist\n"), Prog,
			         user);
			fail_exit (E_NOPERM);
		}
	} else {
		pw = get_my_pwent ();
		if (NULL == pw) {
			fprintf (stderr,
			         _("%s: Cannot determine your user name.\n"),
			         Prog);
			SYSLOG ((LOG_WARN, "Cannot determine the user name of the caller (UID %lu)",
			         (unsigned long) getuid ()));
			fail_exit (E_NOPERM);
		}
		user = xstrdup (pw->pw_name);
	}

	/* Check that the caller is allowed to change the gecos of the
	 * specified user */
	check_perms (pw);

	/* If some fields were not set on the command line, load the value from
	 * the old gecos fields. */
	get_old_fields (pw->pw_gecos);

	/*
	 * If none of the fields were changed from the command line, let the
	 * user interactively change them.
	 */
	if (!fflg && !rflg && !wflg && !hflg && !oflg) {
		printf (_("Changing the user information for %s\n"), user);
		new_fields ();
	}

	/*
	 * Check all of the fields for valid information
	 */
	check_fields ();

	/*
	 * Build the new GECOS field by plastering all the pieces together,
	 * if they will fit ...
	 */
	if ((strlen (fullnm) + strlen (roomno) + strlen (workph) +
	     strlen (homeph) + strlen (slop)) > (unsigned int) 80) {
		fprintf (stderr, _("%s: fields too long\n"), Prog);
		fail_exit (E_NOPERM);
	}
	SNPRINTF(new_gecos, "%s,%s,%s,%s%s%s",
	         fullnm, roomno, workph, homeph,
	         ('\0' != slop[0]) ? "," : "", slop);

	/* Rewrite the user's gecos in the passwd file */
	update_gecos (user, new_gecos);

	SYSLOG ((LOG_INFO, "changed user '%s' information", user));

	nscd_flush_cache ("passwd");
	sssd_flush_cache (SSSD_DB_PASSWD);

	closelog ();
	exit (E_SUCCESS);
}

