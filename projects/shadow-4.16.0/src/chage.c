/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2000 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
#include <pwd.h>

#include "alloc.h"
#include "atoi/str2i.h"
#include "defines.h"
#include "memzero.h"
#include "prototypes.h"
#include "pwio.h"
#include "shadowio.h"
#include "shadowlog.h"
#include "string/sprintf.h"
#include "string/strftime.h"
#include "string/strtcpy.h"
#include "time/day_to_str.h"
/*@-exitarg@*/
#include "exitcodes.h"

#ifdef WITH_TCB
#include "tcbfuncs.h"
#endif


/*
 * Global variables
 */
static const char Prog[] = "chage";

static bool
    dflg = false,		/* set last password change date */
    Eflg = false,		/* set account expiration date */
    iflg = false,		/* set iso8601 date formatting */
    Iflg = false,		/* set password inactive after expiration */
    lflg = false,		/* show account aging information */
    mflg = false,		/* set minimum number of days before password change */
    Mflg = false,		/* set maximum number of days before password change */
    Wflg = false;		/* set expiration warning days */
static bool amroot = false;

static const char *prefix = "";

static bool pw_locked  = false;	/* Indicate if the password file is locked */
static bool spw_locked = false;	/* Indicate if the shadow file is locked */
/* The name and UID of the user being worked on */
static char user_name[BUFSIZ] = "";
static uid_t user_uid = -1;

static long mindays;
static long maxdays;
static long lstchgdate;
static long warndays;
static long inactdays;
static long expdate;

/* local function prototypes */
NORETURN static void usage (int status);
static int new_fields (void);
static void print_day_as_date (long day);
static void list_fields (void);
static void process_flags (int argc, char **argv);
static void check_flags (int argc, int opt_index);
static void check_perms (void);
static void open_files (bool readonly);
static void close_files (void);
NORETURN static void fail_exit (int code);

/*
 * fail_exit - do some cleanup and exit with the given error code
 */
NORETURN
static void
fail_exit (int code)
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
	closelog ();

#ifdef WITH_AUDIT
	if (E_SUCCESS != code) {
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "change age", user_name, user_uid, 0);
	}
#endif

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
	                _("Usage: %s [options] LOGIN\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -d, --lastday LAST_DAY        set date of last password change to LAST_DAY\n"), usageout);
	(void) fputs (_("  -E, --expiredate EXPIRE_DATE  set account expiration date to EXPIRE_DATE\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -i, --iso8601                 use YYYY-MM-DD when printing dates\n"), usageout);
	(void) fputs (_("  -I, --inactive INACTIVE       set password inactive after expiration\n"
	                "                                to INACTIVE\n"), usageout);
	(void) fputs (_("  -l, --list                    show account aging information\n"), usageout);
	(void) fputs (_("  -m, --mindays MIN_DAYS        set minimum number of days before password\n"
	                "                                change to MIN_DAYS\n"), usageout);
	(void) fputs (_("  -M, --maxdays MAX_DAYS        set maximum number of days before password\n"
	                "                                change to MAX_DAYS\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -P, --prefix PREFIX_DIR       directory prefix\n"), usageout);
	(void) fputs (_("  -W, --warndays WARN_DAYS      set expiration warning days to WARN_DAYS\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * new_fields - change the user's password aging information interactively.
 *
 * prompt the user for all of the password age values. set the fields
 * from the user's response, or leave alone if nothing was entered. The
 * value (-1) is used to indicate the field should be removed if possible.
 * any other negative value is an error. very large positive values will
 * be handled elsewhere.
 */
static int new_fields (void)
{
	char  buf[200];

	(void) puts (_("Enter the new value, or press ENTER for the default"));
	(void) puts ("");

	SNPRINTF(buf, "%ld", mindays);
	change_field (buf, sizeof buf, _("Minimum Password Age"));
	if (   (str2sl(&mindays, buf) == -1)
	    || (mindays < -1)) {
		return 0;
	}

	SNPRINTF(buf, "%ld", maxdays);
	change_field (buf, sizeof buf, _("Maximum Password Age"));
	if (   (str2sl(&maxdays, buf) == -1)
	    || (maxdays < -1)) {
		return 0;
	}

	if (-1 == lstchgdate || lstchgdate > LONG_MAX / DAY)
		strcpy(buf, "-1");
	else
		DAY_TO_STR(buf, lstchgdate);

	change_field (buf, sizeof buf, _("Last Password Change (YYYY-MM-DD)"));

	if (strcmp (buf, "-1") == 0) {
		lstchgdate = -1;
	} else {
		lstchgdate = strtoday (buf);
		if (lstchgdate <= -1) {
			return 0;
		}
	}

	SNPRINTF(buf, "%ld", warndays);
	change_field (buf, sizeof buf, _("Password Expiration Warning"));
	if (   (str2sl(&warndays, buf) == -1)
	    || (warndays < -1)) {
		return 0;
	}

	SNPRINTF(buf, "%ld", inactdays);
	change_field (buf, sizeof buf, _("Password Inactive"));
	if (   (str2sl(&inactdays, buf) == -1)
	    || (inactdays < -1)) {
		return 0;
	}

	if (-1 == expdate || LONG_MAX / DAY < expdate)
		strcpy(buf, "-1");
	else
		DAY_TO_STR(buf, expdate);

	change_field (buf, sizeof buf,
	              _("Account Expiration Date (YYYY-MM-DD)"));

	if (strcmp (buf, "-1") == 0) {
		expdate = -1;
	} else {
		expdate = strtoday (buf);
		if (expdate <= -1) {
			return 0;
		}
	}

	return 1;
}


static void
print_day_as_date(long day)
{
	char       buf[80];
	time_t     date;
	struct tm  tm;

	if (day < 0) {
		puts(_("never"));
		return;
	}
	if (__builtin_mul_overflow(day, DAY, &date)) {
		puts(_("future"));
		return;
	}

	if (gmtime_r(&date, &tm) == NULL) {
		(void) printf ("time_t: %lu\n", (unsigned long)date);
		return;
	}

	STRFTIME(buf, iflg ? "%Y-%m-%d" : "%b %d, %Y", &tm);
	(void) puts (buf);
}


/*
 * list_fields - display the current values of the expiration fields
 *
 * display the password age information from the password fields. Date
 * values will be displayed as a calendar date, or the word "never" if
 * the date is 1/1/70, which is day number 0.
 */
static void list_fields (void)
{
	/*
	 * The "last change" date is either "never" or the date the password
	 * was last modified. The date is the number of days since 1/1/1970.
	 */
	(void) fputs (_("Last password change\t\t\t\t\t: "), stdout);
	if (lstchgdate == 0) {
		(void) puts (_("password must be changed"));
	} else {
		print_day_as_date(lstchgdate);
	}

	/*
	 * The password expiration date is determined from the last change
	 * date plus the number of days the password is valid for.
	 */
	(void) fputs (_("Password expires\t\t\t\t\t: "), stdout);
	if (lstchgdate == 0) {
		(void) puts (_("password must be changed"));
	} else if (   (lstchgdate < 0)
	           || (maxdays >= 10000)
	           || (maxdays < 0)
	           || (LONG_MAX - lstchgdate < maxdays))
	{
		(void) puts (_("never"));
	} else {
		print_day_as_date(lstchgdate + maxdays);
	}

	/*
	 * The account becomes inactive if the password is expired for more
	 * than "inactdays". The expiration date is calculated and the
	 * number of inactive days is added. The resulting date is when the
	 * active will be disabled.
	 */
	(void) fputs (_("Password inactive\t\t\t\t\t: "), stdout);
	if (lstchgdate == 0) {
		(void) puts (_("password must be changed"));
	} else if (   (lstchgdate < 0)
	           || (inactdays < 0)
	           || (maxdays >= 10000)
	           || (maxdays < 0)
	           || (LONG_MAX - inactdays < maxdays)
	           || (LONG_MAX - lstchgdate < maxdays + inactdays))
	{
		(void) puts (_("never"));
	} else {
		print_day_as_date(lstchgdate + maxdays + inactdays);
	}

	/*
	 * The account will expire on the given date regardless of the
	 * password expiring or not.
	 */
	(void) fputs (_("Account expires\t\t\t\t\t\t: "), stdout);
	print_day_as_date(expdate);

	/*
	 * Start with the easy numbers - the number of days before the
	 * password can be changed, the number of days after which the
	 * password must be changed, the number of days before the password
	 * expires that the user is told, and the number of days after the
	 * password expires that the account becomes unusable.
	 */
	printf (_("Minimum number of days between password change\t\t: %ld\n"),
	        mindays);
	printf (_("Maximum number of days between password change\t\t: %ld\n"),
	        maxdays);
	printf (_("Number of days of warning before password expires\t: %ld\n"),
	        warndays);
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
		{"lastday",    required_argument, NULL, 'd'},
		{"expiredate", required_argument, NULL, 'E'},
		{"help",       no_argument,       NULL, 'h'},
		{"inactive",   required_argument, NULL, 'I'},
		{"list",       no_argument,       NULL, 'l'},
		{"mindays",    required_argument, NULL, 'm'},
		{"maxdays",    required_argument, NULL, 'M'},
		{"root",       required_argument, NULL, 'R'},
		{"prefix",     required_argument, NULL, 'P'},
		{"warndays",   required_argument, NULL, 'W'},
		{"iso8601",    no_argument,       NULL, 'i'},
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv, "d:E:hiI:lm:M:R:P:W:",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'd':
			dflg = true;
			lstchgdate = strtoday (optarg);
			if (lstchgdate < -1) {
				fprintf (stderr,
				         _("%s: invalid date '%s'\n"),
				         Prog, optarg);
				usage (E_USAGE);
			}
			break;
		case 'E':
			Eflg = true;
			expdate = strtoday (optarg);
			if (expdate < -1) {
				fprintf (stderr,
				         _("%s: invalid date '%s'\n"),
				         Prog, optarg);
				usage (E_USAGE);
			}
			break;
		case 'h':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		case 'i':
			iflg = true;
			break;
		case 'I':
			Iflg = true;
			if (   (str2sl(&inactdays, optarg) == -1)
			    || (inactdays < -1)) {
				fprintf (stderr,
				         _("%s: invalid numeric argument '%s'\n"),
				         Prog, optarg);
				usage (E_USAGE);
			}
			break;
		case 'l':
			lflg = true;
			break;
		case 'm':
			mflg = true;
			if (   (str2sl(&mindays, optarg) == -1)
			    || (mindays < -1)) {
				fprintf (stderr,
				         _("%s: invalid numeric argument '%s'\n"),
				         Prog, optarg);
				usage (E_USAGE);
			}
			break;
		case 'M':
			Mflg = true;
			if (   (str2sl(&maxdays, optarg) == -1)
			    || (maxdays < -1)) {
				fprintf (stderr,
				         _("%s: invalid numeric argument '%s'\n"),
				         Prog, optarg);
				usage (E_USAGE);
			}
			break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
		case 'P': /* no-op, handled in process_prefix_flag () */
			break;
		case 'W':
			Wflg = true;
			if (   (str2sl(&warndays, optarg) == -1)
			    || (warndays < -1)) {
				fprintf (stderr,
				         _("%s: invalid numeric argument '%s'\n"),
				         Prog, optarg);
				usage (E_USAGE);
			}
			break;
		default:
			usage (E_USAGE);
		}
	}

	check_flags (argc, optind);
}

/*
 * check_flags - check flags and parameters consistency
 *
 *	It will not return if an error is encountered.
 */
static void check_flags (int argc, int opt_index)
{
	/*
	 * Make certain the flags do not conflict and that there is a user
	 * name on the command line.
	 */

	if (argc != opt_index + 1) {
		usage (E_USAGE);
	}

	if (lflg && (mflg || Mflg || dflg || Wflg || Iflg || Eflg)) {
		fprintf (stderr,
		         _("%s: do not include \"l\" with other flags\n"),
		         Prog);
		usage (E_USAGE);
	}
}

/*
 * check_perms - check if the caller is allowed to add a group
 *
 *	Non-root users are only allowed to display their aging information.
 *	(we will later make sure that the user is only listing her aging
 *	information)
 *
 *	With PAM support, the setuid bit can be set on chage to allow
 *	non-root users to groups.
 *	Without PAM support, only users who can write in the group databases
 *	can add groups.
 *
 *	It will not return if the user is not allowed.
 */
static void check_perms (void)
{
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
	pam_handle_t *pamh = NULL;
	struct passwd *pampw;
	int retval;
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */

	/*
	 * An unprivileged user can ask for their own aging information, but
	 * only root can change it, or list another user's aging
	 * information.
	 */

	if (!amroot && !lflg) {
		fprintf (stderr, _("%s: Permission denied.\n"), Prog);
		fail_exit (E_NOPERM);
	}

#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
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
		fail_exit (E_NOPERM);
	}
	(void) pam_end (pamh, retval);
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
}

/*
 * open_files - open the shadow database
 *
 *	In read-only mode, the databases are not locked and are opened
 *	only for reading.
 */
static void open_files (bool readonly)
{
	/*
	 * Lock and open the password file. This loads all of the password
	 * file entries into memory. Then we get a pointer to the password
	 * file entry for the requested user.
	 */
	if (!readonly) {
		if (pw_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, pw_dbname ());
			fail_exit (E_NOPERM);
		}
		pw_locked = true;
	}
	if (pw_open (readonly ? O_RDONLY: O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_WARN, "cannot open %s", pw_dbname ()));
		fail_exit (E_NOPERM);
	}

	/*
	 * For shadow password files we have to lock the file and read in
	 * the entries as was done for the password file. The user entries
	 * does not have to exist in this case; a new entry will be created
	 * for this user if one does not exist already.
	 */
	if (!readonly) {
		if (spw_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, spw_dbname ());
			fail_exit (E_NOPERM);
		}
		spw_locked = true;
	}
	if (spw_open (readonly ? O_RDONLY: O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"), Prog, spw_dbname ());
		SYSLOG ((LOG_WARN, "cannot open %s", spw_dbname ()));
		fail_exit (E_NOPERM);
	}
}

/*
 * close_files - close and unlock the password/shadow databases
 */
static void close_files (void)
{
	/*
	 * Now close the shadow password file, which will cause all of the
	 * entries to be re-written.
	 */
	if (spw_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"), Prog, spw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", spw_dbname ()));
		fail_exit (E_NOPERM);
	}

	/*
	 * Close the password file. If any entries were modified, the file
	 * will be re-written.
	 */
	if (pw_close () == 0) {
		fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (E_NOPERM);
	}
	if (spw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
		/* continue */
	}
	spw_locked = false;
	if (pw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}
	pw_locked = false;
}

/*
 * update_age - update the aging information in the database
 *
 *	It will not return in case of error
 */
static void update_age (/*@null@*/const struct spwd *sp,
                        /*@notnull@*/const struct passwd *pw)
{
	struct spwd spwent;

	/*
	 * There was no shadow entry. The new entry will have the encrypted
	 * password transferred from the normal password file along with the
	 * aging information.
	 */
	if (NULL == sp) {
		struct passwd pwent = *pw;

		memzero (&spwent, sizeof spwent);
		spwent.sp_namp = xstrdup (pwent.pw_name);
		spwent.sp_pwdp = xstrdup (pwent.pw_passwd);
		spwent.sp_flag = SHADOW_SP_FLAG_UNSET;

		pwent.pw_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */
		if (pw_update (&pwent) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"), Prog, pw_dbname (), pwent.pw_name);
			fail_exit (E_NOPERM);
		}
	} else {
		spwent.sp_namp = xstrdup (sp->sp_namp);
		spwent.sp_pwdp = xstrdup (sp->sp_pwdp);
		spwent.sp_flag = sp->sp_flag;
	}

	/*
	 * Copy the fields back to the shadow file entry and write the
	 * modified entry back to the shadow file. Closing the shadow and
	 * password files will commit any changes that have been made.
	 */
	spwent.sp_max = maxdays;
	spwent.sp_min = mindays;
	spwent.sp_lstchg = lstchgdate;
	spwent.sp_warn = warndays;
	spwent.sp_inact = inactdays;
	spwent.sp_expire = expdate;

	if (spw_update (&spwent) == 0) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"), Prog, spw_dbname (), spwent.sp_namp);
		fail_exit (E_NOPERM);
	}

}

/*
 * get_defaults - get the value of the fields not set from the command line
 */
static void get_defaults (/*@null@*/const struct spwd *sp)
{
	/*
	 * Set the fields that aren't being set from the command line from
	 * the password file.
	 */
	if (NULL != sp) {
		if (!Mflg) {
			maxdays = sp->sp_max;
		}
		if (!mflg) {
			mindays = sp->sp_min;
		}
		if (!dflg) {
			lstchgdate = sp->sp_lstchg;
		}
		if (!Wflg) {
			warndays = sp->sp_warn;
		}
		if (!Iflg) {
			inactdays = sp->sp_inact;
		}
		if (!Eflg) {
			expdate = sp->sp_expire;
		}
	} else {
		/*
		 * Use default values that will not change the behavior of the
		 * account.
		 */
		if (!Mflg) {
			maxdays = -1;
		}
		if (!mflg) {
			mindays = -1;
		}
		if (!dflg) {
			lstchgdate = -1;
		}
		if (!Wflg) {
			warndays = -1;
		}
		if (!Iflg) {
			inactdays = -1;
		}
		if (!Eflg) {
			expdate = -1;
		}
	}
}

/*
 * chage - change a user's password aging information
 *
 *	This command controls the password aging information.
 *
 *	The valid options are
 *
 *	-d	set last password change date (*)
 *	-E	set account expiration date (*)
 *	-I	set password inactive after expiration (*)
 *	-l	show account aging information
 *	-M	set maximum number of days before password change (*)
 *	-m	set minimum number of days before password change (*)
 *	-W	set expiration warning days (*)
 *
 *	(*) requires root permission to execute.
 *
 *	All of the time fields are entered in the internal format which is
 *	either seconds or days.
 */

int main (int argc, char **argv)
{
	const struct spwd *sp;
	uid_t ruid;
	gid_t rgid;
	const struct passwd *pw;

	sanitize_env ();
	check_fds ();

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);
	prefix = process_prefix_flag ("-P", argc, argv);

#ifdef WITH_AUDIT
	audit_help_open ();
#endif
	OPENLOG (Prog);

	ruid = getuid ();
	rgid = getgid ();
	amroot = (ruid == 0);
#ifdef WITH_SELINUX
	if (amroot) {
		amroot = (check_selinux_permit ("rootok") == 0);
	}
#endif

	process_flags (argc, argv);

	check_perms ();

	if (!spw_file_present ()) {
		fprintf (stderr,
		         _("%s: the shadow password file is not present\n"),
		         Prog);
		SYSLOG ((LOG_WARN, "can't find the shadow password file"));
		closelog ();
		exit (E_SHADOW_NOTFOUND);
	}

	open_files (lflg);
	/* Drop privileges */
	if (lflg && (   (setregid (rgid, rgid) != 0)
	             || (setreuid (ruid, ruid) != 0))) {
		fprintf (stderr, _("%s: failed to drop privileges (%s)\n"),
		         Prog, strerror (errno));
		fail_exit (E_NOPERM);
	}

	pw = pw_locate (argv[optind]);
	if (NULL == pw) {
		fprintf (stderr, _("%s: user '%s' does not exist in %s\n"),
		         Prog, argv[optind], pw_dbname ());
		closelog ();
		fail_exit (E_NOPERM);
	}

	STRTCPY(user_name, pw->pw_name);
#ifdef WITH_TCB
	if (shadowtcb_set_user (pw->pw_name) == SHADOWTCB_FAILURE) {
		fail_exit (E_NOPERM);
	}
#endif
	user_uid = pw->pw_uid;

	sp = spw_locate (argv[optind]);
	get_defaults (sp);

	/*
	 * Print out the expiration fields if the user has requested the
	 * list option.
	 */
	if (lflg) {
		if (!amroot && (ruid != user_uid)) {
			fprintf (stderr, _("%s: Permission denied.\n"), Prog);
			fail_exit (E_NOPERM);
		}
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "display aging info", user_name, user_uid, 1);
#endif
		list_fields ();
		fail_exit (E_SUCCESS);
	}

	/*
	 * If none of the fields were changed from the command line, let the
	 * user interactively change them.
	 */
	if (!mflg && !Mflg && !dflg && !Wflg && !Iflg && !Eflg) {
		printf (_("Changing the aging information for %s\n"),
		        user_name);
		if (new_fields () == 0) {
			fprintf (stderr, _("%s: error changing fields\n"),
			         Prog);
			fail_exit (E_NOPERM);
		}
#ifdef WITH_AUDIT
		else {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "change all aging information",
			              user_name, user_uid, 1);
		}
#endif
	} else {
#ifdef WITH_AUDIT
		if (Mflg) {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "change max age", user_name, user_uid, 1);
		}
		if (mflg) {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "change min age", user_name, user_uid, 1);
		}
		if (dflg) {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "change last change date",
			              user_name, user_uid, 1);
		}
		if (Wflg) {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "change passwd warning",
			              user_name, user_uid, 1);
		}
		if (Iflg) {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "change inactive days",
			              user_name, user_uid, 1);
		}
		if (Eflg) {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "change passwd expiration",
			              user_name, user_uid, 1);
		}
#endif
	}

	update_age (sp, pw);

	close_files ();

	SYSLOG ((LOG_INFO, "changed password expiry for %s", user_name));

	closelog ();
	exit (E_SUCCESS);
}

