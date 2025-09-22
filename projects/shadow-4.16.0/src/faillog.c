/*
 * SPDX-FileCopyrightText: 1989 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2002 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <getopt.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>

#include "atoi/str2i.h"
#include "defines.h"
#include "faillog.h"
#include "memzero.h"
#include "prototypes.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"
#include "string/strftime.h"



/* local function prototypes */
NORETURN static void usage (int status);
static void print_one (/*@null@*/const struct passwd *pw, bool force);
static void set_locktime (long locktime);
static bool set_locktime_one (uid_t uid, long locktime);
static void setmax (short max);
static bool setmax_one (uid_t uid, short max);
static void print (void);
static bool reset_one (uid_t uid);
static void reset (void);

/*
 * Global variables
 */
static const char Prog[] = "faillog";	/* Program name */
static FILE *fail;		/* failure file stream */
static time_t seconds;		/* that number of days in seconds */
static unsigned long umin;	/* if uflg and has_umin, only display users with uid >= umin */
static bool has_umin = false;
static unsigned long umax;	/* if uflg and has_umax, only display users with uid <= umax */
static bool has_umax = false;
static bool errors = false;

static bool aflg = false;	/* set if all users are to be printed always */
static bool uflg = false;	/* set if user is a valid user id */
static bool tflg = false;	/* print is restricted to most recent days */
static bool lflg = false;	/* set the locktime */
static bool mflg = false;	/* set maximum failed login counters */
static bool rflg = false;	/* reset the counters of login failures */

static struct stat statbuf;	/* fstat buffer for file size */

#define	NOW	time(NULL)

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
	(void) fputs (_("  -a, --all                     display faillog records for all users\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -l, --lock-secs SEC           after failed login lock account for SEC seconds\n"), usageout);
	(void) fputs (_("  -m, --maximum MAX             set maximum failed login counters to MAX\n"), usageout);
	(void) fputs (_("  -r, --reset                   reset the counters of login failures\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -t, --time DAYS               display faillog records more recent than DAYS\n"), usageout);
	(void) fputs (_("  -u, --user LOGIN/RANGE        display faillog record or maintains failure\n"
	                "                                counters and limits (if used with -r, -m,\n"
	                "                                or -l) only for the specified LOGIN(s)\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * Looks up the offset in the faillog file for the given uid.
 * Returns -1 on error.
 */
static off_t lookup_faillog(struct faillog *fl, uid_t uid)
{
	off_t offset, size;

	/* Ensure multiplication does not overflow and retrieving a wrong entry */
	if (__builtin_mul_overflow(uid, sizeof(*fl), &offset)) {
		fprintf(stderr,
		        _("%s: Failed to get the entry for UID %lu\n"),
		        Prog, (unsigned long)uid);
		return -1;
	}

	if (!__builtin_add_overflow(offset, sizeof(*fl), &size)
	    && size <= statbuf.st_size) {
		/* fseeko errors are not really relevant for us. */
		int err = fseeko(fail, offset, SEEK_SET);
		assert(0 == err);
		/* faillog is a sparse file. Even if no entries were
		 * entered for this user, which should be able to get the
		 * empty entry in this case.
		 */
		if (fread(fl, sizeof(*fl), 1, fail) != 1) {
			fprintf(stderr,
			        _("%s: Failed to get the entry for UID %lu\n"),
			        Prog, (unsigned long)uid);
			return -1;
		}
	} else {
		/* Outsize of the faillog file.
		 * Behave as if there were a missing entry (same behavior
		 * as if we were reading an non existing entry in the
		 * sparse faillog file).
		 */
		memzero(fl, sizeof(*fl));
	}

	return offset;
}

static void print_one (/*@null@*/const struct passwd *pw, bool force)
{
	static bool once = false;
	struct tm *tm;
	struct faillog fl;
	time_t now;
	char *cp;
	char ptime[80];

	if (NULL == pw) {
		return;
	}

	if (lookup_faillog(&fl, pw->pw_uid) < 0) {
		return;
	}

	/* Nothing to report */
	if (!force && (0 == fl.fail_time)) {
		return;
	}

	(void) time(&now);

	/* Filter out entries that do not match with the -t option */
	if (tflg && ((now - fl.fail_time) > seconds)) {
		return;
	}

	/* Print the header only once */
	if (!once) {
		puts (_("Login       Failures Maximum Latest                   On\n"));
		once = true;
	}

	tm = localtime (&fl.fail_time);
	if (!tm) {
		fprintf (stderr, "Cannot read time from faillog.\n");
		return;
	}
	STRFTIME(ptime, "%D %H:%M:%S %z", tm);
	cp = ptime;

	printf ("%-9s   %5d    %5d   ",
	        pw->pw_name, fl.fail_cnt, fl.fail_max);
	printf ("%s  %s", cp, fl.fail_line);
	if (0 != fl.fail_locktime) {
		if (   ((fl.fail_time + fl.fail_locktime) > now)
		    && (0 != fl.fail_cnt)) {
			printf (_(" [%lus left]"),
			        (unsigned long) fl.fail_time + fl.fail_locktime - now);
		} else {
			printf (_(" [%lds lock]"),
			        fl.fail_locktime);
		}
	}
	putchar ('\n');
}

static void print (void)
{
	if (uflg && has_umin && has_umax && (umin==umax)) {
		print_one (getpwuid (umin), true);
	} else {
		/* We only print records for existing users.
		 * Loop based on the user database instead of reading the
		 * whole file. We will have to query the database anyway
		 * so except for very small ranges and large user
		 * database, this should not be a performance issue.
		 */
		struct passwd *pwent;

		setpwent ();
		while ( (pwent = getpwent ()) != NULL ) {
			if (   uflg
			    && (   (has_umin && (pwent->pw_uid < (uid_t)umin))
			        || (has_umax && (pwent->pw_uid > (uid_t)umax)))) {
				continue;
			}
			print_one (pwent, aflg);
		}
		endpwent ();
	}
}

/*
 * reset_one - Reset the fail count for one user
 *
 * This returns a boolean indicating if an error occurred.
 */
static bool reset_one (uid_t uid)
{
	off_t offset;
	struct faillog fl;

	offset = lookup_faillog(&fl, uid);
	if (offset < 0) {
		/* failure */
		return true;
	}

	if (0 == fl.fail_cnt) {
		/* If the count is already null, do not write in the file.
		 * This avoids writing 0 when no entries were present for
		 * the user.
		 */
		return false;
	}

	fl.fail_cnt = 0;

	if (   (fseeko (fail, offset, SEEK_SET) == 0)
	    && (fwrite (&fl, sizeof (fl), 1, fail) == 1)) {
		(void) fflush (fail);
		return false;
	}

	fprintf (stderr,
	         _("%s: Failed to reset fail count for UID %lu\n"),
	         Prog, (unsigned long)uid);
	return true;
}

static void reset (void)
{
	if (uflg && has_umin && has_umax && (umin==umax)) {
		if (reset_one (umin)) {
			errors = true;
		}
	} else {
		/* There is no need to reset outside of the faillog
		 * database.
		 */
		uid_t uidmax = statbuf.st_size / sizeof (struct faillog);
		if (uidmax > 1) {
			uidmax--;
		}
		if (has_umax && (uid_t)umax < uidmax) {
			uidmax = umax;
		}

		/* Reset all entries in the specified range.
		 * Non existing entries will not be touched.
		 */
		if (aflg) {
			/* Entries for non existing users are also reset.
			 */
			uid_t uid = 0;

			/* Make sure we stay in the umin-umax range if specified */
			if (has_umin) {
				uid = umin;
			}

			while (uid <= uidmax) {
				if (reset_one (uid)) {
					errors = true;
				}
				uid++;
			}
		} else {
			/* Only reset records for existing users.
			 */
			struct passwd *pwent;

			setpwent ();
			while ( (pwent = getpwent ()) != NULL ) {
				if (   uflg
				    && (   (has_umin && (pwent->pw_uid < (uid_t)umin))
				        || (pwent->pw_uid > uidmax))) {
					continue;
				}
				if (reset_one (pwent->pw_uid)) {
					errors = true;
				}
			}
			endpwent ();
		}
	}
}

/*
 * setmax_one - Set the maximum failed login counter for one user
 *
 * This returns a boolean indicating if an error occurred.
 */
static bool setmax_one (uid_t uid, short max)
{
	off_t offset;
	struct faillog fl;

	offset = lookup_faillog(&fl, uid);
	if (offset < 0) {
		return true;
	}

	if (max == fl.fail_max) {
		/* If the max is already set to the right value, do not
		 * write in the file.
		 * This avoids writing 0 when no entries were present for
		 * the user and the max argument is 0.
		 */
		return false;
	}

	fl.fail_max = max;

	if (   (fseeko (fail, offset, SEEK_SET) == 0)
	    && (fwrite (&fl, sizeof (fl), 1, fail) == 1)) {
		(void) fflush (fail);
		return false;
	}

	fprintf (stderr,
	         _("%s: Failed to set max for UID %lu\n"),
	         Prog, (unsigned long)uid);
	return true;
}

static void setmax (short max)
{
	if (uflg && has_umin && has_umax && (umin==umax)) {
		if (setmax_one (umin, max)) {
			errors = true;
		}
	} else {
		/* Set max for entries in the specified range.
		 * If max is unchanged for an entry, the entry is not touched.
		 * If max is null, and no entries exist for this user, no
		 * entries will be created.
		 */
		if (aflg) {
			/* Entries for non existing user are also taken into
			 * account (in order to define policy for future users).
			 */
			uid_t uid = 0;
			/* The default umax value is based on the size of the
			 * faillog database.
			 */
			uid_t uidmax = statbuf.st_size / sizeof (struct faillog);
			if (uidmax > 1) {
				uidmax--;
			}

			/* Make sure we stay in the umin-umax range if specified */
			if (has_umin) {
				uid = umin;
			}
			if (has_umax) {
				uidmax = umax;
			}

			while (uid <= uidmax) {
				if (setmax_one (uid, max)) {
					errors = true;
				}
				uid++;
			}
		} else {
			/* Only change records for existing users.
			 */
			struct passwd *pwent;

			setpwent ();
			while ( (pwent = getpwent ()) != NULL ) {
				if (   uflg
				    && (   (has_umin && (pwent->pw_uid < (uid_t)umin))
				        || (has_umax && (pwent->pw_uid > (uid_t)umax)))) {
					continue;
				}
				if (setmax_one (pwent->pw_uid, max)) {
					errors = true;
				}
			}
			endpwent ();
		}
	}
}

/*
 * set_locktime_one - Set the locktime for one user
 *
 * This returns a boolean indicating if an error occurred.
 */
static bool set_locktime_one (uid_t uid, long locktime)
{
	off_t offset;
	struct faillog fl;

	offset = lookup_faillog(&fl, uid);
	if (offset < 0) {
		return true;
	}

	if (locktime == fl.fail_locktime) {
		/* If the locktime is already set to the right value, do not
		 * write in the file.
		 * This avoids writing 0 when no entries were present for
		 * the user and the locktime argument is 0.
		 */
		return false;
	}

	fl.fail_locktime = locktime;

	if (   (fseeko (fail, offset, SEEK_SET) == 0)
	    && (fwrite (&fl, sizeof (fl), 1, fail) == 1)) {
		(void) fflush (fail);
		return false;
	}

	fprintf (stderr,
	         _("%s: Failed to set locktime for UID %lu\n"),
	         Prog, (unsigned long)uid);
	return true;
}

static void set_locktime (long locktime)
{
	if (uflg && has_umin && has_umax && (umin==umax)) {
		if (set_locktime_one (umin, locktime)) {
			errors = true;
		}
	} else {
		/* Set locktime for entries in the specified range.
		 * If locktime is unchanged for an entry, the entry is not touched.
		 * If locktime is null, and no entries exist for this user, no
		 * entries will be created.
		 */
		if (aflg) {
			/* Entries for non existing user are also taken into
			 * account (in order to define policy for future users).
			 */
			uid_t uid = 0;
			/* The default umax value is based on the size of the
			 * faillog database.
			 */
			uid_t uidmax = statbuf.st_size / sizeof (struct faillog);
			if (uidmax > 1) {
				uidmax--;
			}

			/* Make sure we stay in the umin-umax range if specified */
			if (has_umin) {
				uid = umin;
			}
			if (has_umax) {
				uidmax = umax;
			}

			while (uid <= uidmax) {
				if (set_locktime_one (uid, locktime)) {
					errors = true;
				}
				uid++;
			}
		} else {
			/* Only change records for existing users.
			 */
			struct passwd *pwent;

			setpwent ();
			while ( (pwent = getpwent ()) != NULL ) {
				if (   uflg
				    && (   (has_umin && (pwent->pw_uid < (uid_t)umin))
				        || (has_umax && (pwent->pw_uid > (uid_t)umax)))) {
					continue;
				}
				if (set_locktime_one (pwent->pw_uid, locktime)) {
					errors = true;
				}
			}
			endpwent ();
		}
	}
}

int main (int argc, char **argv)
{
	long fail_locktime = 0;
	short fail_max = 0; // initialize to silence compiler warning
	long days = 0;

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);

	{
		int c;
		static struct option long_options[] = {
			{"all",       no_argument,       NULL, 'a'},
			{"help",      no_argument,       NULL, 'h'},
			{"lock-secs", required_argument, NULL, 'l'},
			{"maximum",   required_argument, NULL, 'm'},
			{"reset",     no_argument,       NULL, 'r'},
			{"root",      required_argument, NULL, 'R'},
			{"time",      required_argument, NULL, 't'},
			{"user",      required_argument, NULL, 'u'},
			{NULL, 0, NULL, '\0'}
		};
		while ((c = getopt_long (argc, argv, "ahl:m:rR:t:u:",
		                         long_options, NULL)) != -1) {
			switch (c) {
			case 'a':
				aflg = true;
				break;
			case 'h':
				usage (E_SUCCESS);
				/*@notreached@*/break;
			case 'l':
				if (str2sl(&fail_locktime, optarg) == -1) {
					fprintf (stderr,
					         _("%s: invalid numeric argument '%s'\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				lflg = true;
				break;
			case 'm':
			{
				long  lmax;

				if (   (str2sl(&lmax, optarg) == -1)
				    || ((long)(short) lmax != lmax)) {
					fprintf (stderr,
					         _("%s: invalid numeric argument '%s'\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				fail_max = lmax;
				mflg = true;
				break;
			}
			case 'r':
				rflg = true;
				break;
			case 'R': /* no-op, handled in process_root_flag () */
				break;
			case 't':
				if (str2sl(&days, optarg) == -1) {
					fprintf (stderr,
					         _("%s: invalid numeric argument '%s'\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				seconds = (time_t) days * DAY;
				tflg = true;
				break;
			case 'u':
			{
				/*
				 * The user can be:
				 *  - a login name
				 *  - numerical
				 *  - a numerical login ID
				 *  - a range (-x, x-, x-y)
				 */
				struct passwd *pwent;

				uflg = true;
				/* local, no need for xgetpwnam */
				pwent = getpwnam (optarg);
				if (NULL != pwent) {
					umin = pwent->pw_uid;
					has_umin = true;
					umax = umin;
					has_umax = true;
				} else {
					if (getrange(optarg,
					             &umin, &has_umin,
					             &umax, &has_umax) == -1) {
						fprintf (stderr,
						         _("%s: Unknown user or range: %s\n"),
						         Prog, optarg);
						exit (E_BAD_ARG);
					}
				}

				break;
			}
			default:
				usage (E_USAGE);
			}
		}
		if (argc > optind) {
			fprintf (stderr,
			         _("%s: unexpected argument: %s\n"),
			         Prog, argv[optind]);
			usage (EXIT_FAILURE);
		}
	}

	if (tflg && (lflg || mflg || rflg)) {
		usage (E_USAGE);
	}

	/* Open the faillog database */
	if (lflg || mflg || rflg) {
		fail = fopen (FAILLOG_FILE, "r+");
	} else {
		fail = fopen (FAILLOG_FILE, "r");
	}
	if (NULL == fail) {
		fprintf (stderr,
		         _("%s: Cannot open %s: %s\n"),
		         Prog, FAILLOG_FILE, strerror (errno));
		exit (E_NOPERM);
	}

	/* Get the size of the faillog */
	if (fstat (fileno (fail), &statbuf) != 0) {
		fprintf (stderr,
		         _("%s: Cannot get the size of %s: %s\n"),
		         Prog, FAILLOG_FILE, strerror (errno));
		exit (E_NOPERM);
	}

	if (lflg) {
		set_locktime (fail_locktime);
	}

	if (mflg) {
		setmax (fail_max);
	}

	if (rflg) {
		reset ();
	}

	if (!(lflg || mflg || rflg)) {
		print ();
	}

	if (lflg || mflg || rflg) {
		if (   (ferror (fail) != 0)
		    || (fflush (fail) != 0)
		    || (fsync  (fileno (fail)) != 0)
		    || (fclose (fail) != 0)) {
			fprintf (stderr,
			         _("%s: Failed to write %s: %s\n"),
			         Prog, FAILLOG_FILE, strerror (errno));
			(void) fclose (fail);
			errors = true;
		}
	} else {
		(void) fclose (fail);
	}

	exit (errors ? E_NOPERM : E_SUCCESS);
}

