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

#include <getopt.h>
#include <lastlog.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#ifdef HAVE_LL_HOST
#include <net/if.h>
#endif

#include "atoi/str2i.h"
#include "defines.h"
#include "prototypes.h"
#include "getdef.h"
#include "memzero.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"
#include "string/strftime.h"



/*
 * Needed for MkLinux DR1/2/2.1 - J.
 */
#ifndef LASTLOG_FILE
#define LASTLOG_FILE "/var/log/lastlog"
#endif

/*
 * Global variables
 */
static const char Prog[] = "lastlog";	/* Program name */
static FILE *lastlogfile;	/* lastlog file stream */
static unsigned long umin;	/* if uflg and has_umin, only display users with uid >= umin */
static bool has_umin = false;
static unsigned long umax;	/* if uflg and has_umax, only display users with uid <= umax */
static bool has_umax = false;
static time_t seconds;		/* that number of days in seconds */
static time_t inverse_seconds;	/* that number of days in seconds */
static struct stat statbuf;	/* fstat buffer for file size */


static bool uflg = false;	/* print only a user of range of users */
static bool tflg = false;	/* print is restricted to most recent days */
static bool bflg = false;	/* print excludes most recent days */
static bool Cflg = false;	/* clear record for user */
static bool Sflg = false;	/* set record for user */

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
	(void) fputs (_("  -b, --before DAYS             print only lastlog records older than DAYS\n"), usageout);
	(void) fputs (_("  -C, --clear                   clear lastlog record of a user (usable only with -u)\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -S, --set                     set lastlog record to current time (usable only with -u)\n"), usageout);
	(void) fputs (_("  -t, --time DAYS               print only lastlog records more recent than DAYS\n"), usageout);
	(void) fputs (_("  -u, --user LOGIN              print lastlog record of the specified LOGIN\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

static void print_one (/*@null@*/const struct passwd *pw)
{
	static bool once = false;
	char *cp;
	struct tm *tm;
	time_t ll_time;
	off_t offset;
	struct lastlog ll;
	char ptime[80];

#ifdef HAVE_LL_HOST
	/*
	 * ll_host is in minimized form, thus the maximum IPv6 address possible is
	 * 8*4+7 = 39 characters.
	 * RFC 4291 2.5.6 states that for LL-addresses fe80+only the interface ID is set,
	 * thus having a maximum size of 25+1+IFNAMSIZ.
	 * POSIX says IFNAMSIZ should be 16 characters long including the null byte, thus
	 * 25+1+IFNAMSIZ >= 42 > 39
	 */
	/* Link-Local address + % + Interfacename */
	const int maxIPv6Addrlen = 25+1+IFNAMSIZ;
#endif

	if (NULL == pw) {
		return;
	}


	offset = (off_t) pw->pw_uid * sizeof (ll);
	if (offset + sizeof (ll) <= statbuf.st_size) {
		/* fseeko errors are not really relevant for us. */
		int err = fseeko (lastlogfile, offset, SEEK_SET);
		assert (0 == err);
		/* lastlog is a sparse file. Even if no entries were
		 * entered for this user, which should be able to get the
		 * empty entry in this case.
		 */
		if (fread (&ll, sizeof (ll), 1, lastlogfile) != 1) {
			fprintf (stderr,
			         _("%s: Failed to get the entry for UID %lu\n"),
			         Prog, (unsigned long)pw->pw_uid);
			exit (EXIT_FAILURE);
		}
	} else {
		/* Outsize of the lastlog file.
		 * Behave as if there were a missing entry (same behavior
		 * as if we were reading an non existing entry in the
		 * sparse lastlog file).
		 */
		memzero (&ll, sizeof (ll));
	}

	/* Filter out entries that do not match with the -t or -b options */
	if (tflg && ((NOW - ll.ll_time) > seconds)) {
		return;
	}

	if (bflg && ((NOW - ll.ll_time) < inverse_seconds)) {
		return;
	}

	/* Print the header only once */
	if (!once) {
#ifdef HAVE_LL_HOST
		printf (_("Username         Port     From%*sLatest\n"), maxIPv6Addrlen-4, " ");
#else
		puts (_("Username                Port     Latest"));
#endif
		once = true;
	}

	ll_time = ll.ll_time;
	tm = localtime (&ll_time);
	if (tm == NULL) {
		cp = "(unknown)";
	} else {
		STRFTIME(ptime, "%a %b %e %H:%M:%S %z %Y", tm);
		cp = ptime;
	}
	if (ll.ll_time == (time_t) 0) {
		cp = _("**Never logged in**\0");
	}

#ifdef HAVE_LL_HOST
	printf ("%-16s %-8.8s %*s%s\n",
	        pw->pw_name, ll.ll_line, -maxIPv6Addrlen, ll.ll_host, cp);
#else
	printf ("%-16s\t%-8.8s %s\n",
	        pw->pw_name, ll.ll_line, cp);
#endif
}

static void print (void)
{
	const struct passwd *pwent;
	unsigned long lastlog_uid_max;

	lastlog_uid_max = getdef_ulong ("LASTLOG_UID_MAX", 0xFFFFFFFFUL);
	if (   (has_umin && umin > lastlog_uid_max)
	    || (has_umax && umax > lastlog_uid_max)) {
		fprintf (stderr, _("%s: Selected uid(s) are higher than LASTLOG_UID_MAX (%lu),\n"
				   "\tthe output might be incorrect.\n"), Prog, lastlog_uid_max);
	}

	if (uflg && has_umin && has_umax && (umin == umax)) {
		print_one (getpwuid (umin));
	} else {
		setpwent ();
		while ( (pwent = getpwent ()) != NULL ) {
			if (   uflg
			    && (   (has_umin && (pwent->pw_uid < (uid_t)umin))
			        || (has_umax && (pwent->pw_uid > (uid_t)umax)))) {
				continue;
			} else if ( !uflg && pwent->pw_uid > (uid_t) lastlog_uid_max) {
				continue;
			}
			print_one (pwent);
		}
		endpwent ();
	}
}

static void update_one (/*@null@*/const struct passwd *pw)
{
	off_t offset;
	struct lastlog ll;
	int err;

	if (NULL == pw) {
		return;
	}

	offset = (off_t) pw->pw_uid * sizeof (ll);
	/* fseeko errors are not really relevant for us. */
	err = fseeko (lastlogfile, offset, SEEK_SET);
	assert (0 == err);

	memzero (&ll, sizeof (ll));

	if (Sflg) {
		ll.ll_time = NOW;
#ifdef HAVE_LL_HOST
		strcpy (ll.ll_host, "localhost");
#endif
		strcpy (ll.ll_line, "lastlog");
#ifdef WITH_AUDIT
		audit_logger (AUDIT_ACCT_UNLOCK, Prog,
			"clearing-lastlog",
			pw->pw_name, pw->pw_uid, SHADOW_AUDIT_SUCCESS);
#endif
	}
#ifdef WITH_AUDIT
	else {
		audit_logger (AUDIT_ACCT_UNLOCK, Prog,
			"refreshing-lastlog",
			pw->pw_name, pw->pw_uid, SHADOW_AUDIT_SUCCESS);
	}
#endif

	if (fwrite (&ll, sizeof(ll), 1, lastlogfile) != 1) {
			fprintf (stderr,
			         _("%s: Failed to update the entry for UID %lu\n"),
			         Prog, (unsigned long)pw->pw_uid);
			exit (EXIT_FAILURE);
	}
}

static void update (void)
{
	const struct passwd *pwent;
	unsigned long lastlog_uid_max;

	if (!uflg) /* safety measure */
		return;

	lastlog_uid_max = getdef_ulong ("LASTLOG_UID_MAX", 0xFFFFFFFFUL);
	if (   (has_umin && umin > lastlog_uid_max)
	    || (has_umax && umax > lastlog_uid_max)) {
		fprintf (stderr, _("%s: Selected uid(s) are higher than LASTLOG_UID_MAX (%lu),\n"
				   "\tthey will not be updated.\n"), Prog, lastlog_uid_max);
		return;
	}

	if (has_umin && has_umax && (umin == umax)) {
		update_one (getpwuid (umin));
	} else {
		setpwent ();
		while ( (pwent = getpwent ()) != NULL ) {
			if ((has_umin && (pwent->pw_uid < (uid_t)umin))
				|| (has_umax && (pwent->pw_uid > (uid_t)umax))) {
				continue;
			}
			update_one (pwent);
		}
		endpwent ();
	}

	if (fflush (lastlogfile) != 0 || fsync (fileno (lastlogfile)) != 0) {
			fprintf (stderr,
			         _("%s: Failed to update the lastlog file\n"),
			         Prog);
			exit (EXIT_FAILURE);
	}
}

int main (int argc, char **argv)
{
	/*
	 * Get the program name. The program name is used as a prefix to
	 * most error messages.
	 */
	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);

#ifdef WITH_AUDIT
	audit_help_open ();
#endif

	{
		int c;
		static struct option const longopts[] = {
			{"before", required_argument, NULL, 'b'},
			{"clear",  no_argument,       NULL, 'C'},
			{"help",   no_argument,       NULL, 'h'},
			{"root",   required_argument, NULL, 'R'},
			{"set",    no_argument,       NULL, 'S'},
			{"time",   required_argument, NULL, 't'},
			{"user",   required_argument, NULL, 'u'},
			{NULL, 0, NULL, '\0'}
		};

		while ((c = getopt_long (argc, argv, "b:ChR:St:u:", longopts,
		                         NULL)) != -1) {
			switch (c) {
			case 'b':
			{
				unsigned long inverse_days;
				if (str2ul(&inverse_days, optarg) == -1) {
					fprintf (stderr,
					         _("%s: invalid numeric argument '%s'\n"),
					         Prog, optarg);
					exit (EXIT_FAILURE);
				}
				inverse_seconds = (time_t) inverse_days * DAY;
				bflg = true;
				break;
			}
			case 'C':
			{
				Cflg = true;
				break;
			}
			case 'h':
				usage (EXIT_SUCCESS);
				/*@notreached@*/break;
			case 'R': /* no-op, handled in process_root_flag () */
				break;
			case 'S':
			{
				Sflg = true;
				break;
			}
			case 't':
			{
				unsigned long days;
				if (str2ul(&days, optarg) == -1) {
					fprintf (stderr,
					         _("%s: invalid numeric argument '%s'\n"),
					         Prog, optarg);
					exit (EXIT_FAILURE);
				}
				seconds = (time_t) days * DAY;
				tflg = true;
				break;
			}
			case 'u':
			{
				const struct passwd *pwent;
				/*
				 * The user can be:
				 *  - a login name
				 *  - numerical
				 *  - a numerical login ID
				 *  - a range (-x, x-, x-y)
				 */
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
						exit (EXIT_FAILURE);
					}
				}
				break;
			}
			default:
				usage (EXIT_FAILURE);
				/*@notreached@*/break;
			}
		}
		if (argc > optind) {
			fprintf (stderr,
			         _("%s: unexpected argument: %s\n"),
			         Prog, argv[optind]);
			usage (EXIT_FAILURE);
		}
		if (Cflg && Sflg) {
			fprintf (stderr,
			         _("%s: Option -C cannot be used together with option -S\n"),
			         Prog);
			usage (EXIT_FAILURE);
		}
		if ((Cflg || Sflg) && !uflg) {
			fprintf (stderr,
			         _("%s: Options -C and -S require option -u to specify the user\n"),
			         Prog);
			usage (EXIT_FAILURE);
		}
	}

	lastlogfile = fopen (LASTLOG_FILE, (Cflg || Sflg)?"r+":"r");
	if (NULL == lastlogfile) {
		perror (LASTLOG_FILE);
		exit (EXIT_FAILURE);
	}

	/* Get the lastlog size */
	if (fstat (fileno (lastlogfile), &statbuf) != 0) {
		fprintf (stderr,
		         _("%s: Cannot get the size of %s: %s\n"),
		         Prog, LASTLOG_FILE, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (Cflg || Sflg)
		update ();
	else
		print ();

	(void) fclose (lastlogfile);

	return EXIT_SUCCESS;
}

