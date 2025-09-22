/*
 * SPDX-FileCopyrightText: 1992 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001       , Michał Moskal
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <getopt.h>
#include "chkname.h"
#include "commonio.h"
#include "defines.h"
#include "prototypes.h"
#include "pwio.h"
#include "shadowio.h"
#include "getdef.h"
#include "nscd.h"
#include "sssd.h"
#ifdef WITH_TCB
#include "tcbfuncs.h"
#endif				/* WITH_TCB */
#include "shadowlog.h"

/*
 * Exit codes
 */
/*@-exitarg@*/
#define	E_OKAY		0
#define	E_SUCCESS	0
#define	E_USAGE		1
#define	E_BADENTRY	2
#define	E_CANTOPEN	3
#define	E_CANTLOCK	4
#define	E_CANTUPDATE	5
#define	E_CANTSORT	6

/*
 * Global variables
 */
static const char Prog[] = "pwck";

static bool use_system_pw_file = true;
static bool use_system_spw_file = true;

static bool is_shadow = false;

static bool spw_opened = false;

static bool pw_locked  = false;
static bool spw_locked = false;

/* Options */
static bool read_only = false;
static bool sort_mode = false;
static bool quiet = false;		/* don't report warnings, only errors */

/* local function prototypes */
static void fail_exit (int code);
NORETURN static void usage (int status);
static void process_flags (int argc, char **argv);
static void open_files (void);
static void close_files (bool changed);
static void check_pw_file (int *errors, bool *changed);
static void check_spw_file (int *errors, bool *changed);

extern int allow_bad_names;

/*
 * fail_exit - do some cleanup and exit with the given error code
 */
static void fail_exit (int code)
{
	if (spw_locked) {
		if (spw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
			if (use_system_spw_file) {
				SYSLOG ((LOG_ERR, "failed to unlock %s",
				         spw_dbname ()));
			}
			/* continue */
		}
	}

	if (pw_locked) {
		if (pw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
			if (use_system_pw_file) {
				SYSLOG ((LOG_ERR, "failed to unlock %s",
				         pw_dbname ()));
			}
			/* continue */
		}
	}

	closelog ();

	exit (code);
}
/*
 * usage - print syntax message and exit
 */
NORETURN
static void
usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
#ifdef WITH_TCB
	if (getdef_bool ("USE_TCB")) {
		(void) fprintf (usageout,
		                _("Usage: %s [options] [passwd]\n"
		                  "\n"
		                  "Options:\n"),
		                Prog);
	} else
#endif				/* WITH_TCB */
	{
		(void) fprintf (usageout,
		                _("Usage: %s [options] [passwd [shadow]]\n"
		                  "\n"
		                  "Options:\n"),
		                Prog);
	}
	(void) fputs (_("  -b, --badname                 allow bad names\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -q, --quiet                   report errors only\n"), usageout);
	(void) fputs (_("  -r, --read-only               display errors and warnings\n"
	                "                                but do not change files\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
#ifdef WITH_TCB
	if (!getdef_bool ("USE_TCB"))
#endif				/* !WITH_TCB */
	{
		(void) fputs (_("  -s, --sort                    sort entries by UID\n"), usageout);
	}
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
	int c;
	static struct option long_options[] = {
		{"badname",   no_argument,       NULL, 'b'},
		{"help",      no_argument,       NULL, 'h'},
		{"quiet",     no_argument,       NULL, 'q'},
		{"read-only", no_argument,       NULL, 'r'},
		{"root",      required_argument, NULL, 'R'},
		{"sort",      no_argument,       NULL, 's'},
		{NULL, 0, NULL, '\0'}
	};

	/*
	 * Parse the command line arguments
	 */
	while ((c = getopt_long (argc, argv, "behqrR:s",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'b':
			allow_bad_names = true;
			break;
		case 'h':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		case 'e':	/* added for Debian shadow-961025-2 compatibility */
		case 'q':
			quiet = true;
			break;
		case 'r':
			read_only = true;
			break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
		case 's':
			sort_mode = true;
			break;
		default:
			usage (E_USAGE);
		}
	}

	if (sort_mode && read_only) {
		fprintf (stderr, _("%s: -s and -r are incompatible\n"), Prog);
		exit (E_USAGE);
	}

	/*
	 * Make certain we have the right number of arguments
	 */
	if (argc > (optind + 2)) {
		usage (E_USAGE);
	}

	/*
	 * If there are two left over filenames, use those as the password
	 * and shadow password filenames.
	 */
	if (optind != argc) {
		pw_setdbname (argv[optind]);
		use_system_pw_file = false;
	}
	if ((optind + 2) == argc) {
#ifdef WITH_TCB
		if (getdef_bool ("USE_TCB")) {
			fprintf (stderr,
			         _("%s: no alternative shadow file allowed when USE_TCB is enabled.\n"),
			         Prog);
			usage (E_USAGE);
		}
#endif				/* WITH_TCB */
		spw_setdbname (argv[optind + 1]);
		is_shadow = true;
		use_system_spw_file = false;
	} else if (optind == argc) {
		is_shadow = spw_file_present ();
	}
}

/*
 * open_files - open the shadow database
 *
 *	In read-only mode, the databases are not locked and are opened
 *	only for reading.
 */
static void open_files (void)
{
	bool use_tcb = false;
#ifdef WITH_TCB
	use_tcb = getdef_bool ("USE_TCB");
#endif				/* WITH_TCB */

	/*
	 * Lock the files if we aren't in "read-only" mode
	 */
	if (!read_only) {
		if (pw_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, pw_dbname ());
			fail_exit (E_CANTLOCK);
		}
		pw_locked = true;
		if (is_shadow && !use_tcb) {
			if (spw_lock () == 0) {
				fprintf (stderr,
				         _("%s: cannot lock %s; try again later.\n"),
				         Prog, spw_dbname ());
				fail_exit (E_CANTLOCK);
			}
			spw_locked = true;
		}
	}

	/*
	 * Open the files. Use O_RDONLY if we are in read_only mode, O_RDWR
	 * otherwise.
	 */
	if (pw_open (read_only ? O_RDONLY : O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"),
		         Prog, pw_dbname ());
		if (use_system_pw_file) {
			SYSLOG ((LOG_WARN, "cannot open %s", pw_dbname ()));
		}
		fail_exit (E_CANTOPEN);
	}
	if (is_shadow && !use_tcb) {
		if (spw_open (read_only ? O_RDONLY : O_RDWR) == 0) {
			fprintf (stderr, _("%s: cannot open %s\n"),
			         Prog, spw_dbname ());
			if (use_system_spw_file) {
				SYSLOG ((LOG_WARN, "cannot open %s",
				         spw_dbname ()));
			}
			fail_exit (E_CANTOPEN);
		}
		spw_opened = true;
	}
}

/*
 * close_files - close and unlock the password/shadow databases
 *
 *	If changed is not set, the databases are not closed, and no
 *	changes are committed in the databases. The databases are
 *	unlocked anyway.
 */
static void close_files (bool changed)
{
	/*
	 * All done. If there were no change we can just abandon any
	 * changes to the files.
	 */
	if (changed) {
		if (pw_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, pw_dbname ());
			if (use_system_pw_file) {
				SYSLOG ((LOG_ERR,
				         "failure while writing changes to %s",
				         pw_dbname ()));
			}
			fail_exit (E_CANTUPDATE);
		}
		if (spw_opened && (spw_close () == 0)) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, spw_dbname ());
			if (use_system_spw_file) {
				SYSLOG ((LOG_ERR,
				         "failure while writing changes to %s",
				         spw_dbname ()));
			}
			fail_exit (E_CANTUPDATE);
		}
		spw_opened = false;
	}

	/*
	 * Don't be anti-social - unlock the files when you're done.
	 */
	if (spw_locked) {
		if (spw_unlock () == 0) {
			fprintf (stderr,
			         _("%s: failed to unlock %s\n"),
			         Prog, spw_dbname ());
			if (use_system_spw_file) {
				SYSLOG ((LOG_ERR, "failed to unlock %s",
				         spw_dbname ()));
			}
			/* continue */
		}
	}
	spw_locked = false;
	if (pw_locked) {
		if (pw_unlock () == 0) {
			fprintf (stderr,
			         _("%s: failed to unlock %s\n"),
			         Prog, pw_dbname ());
			if (use_system_pw_file) {
				SYSLOG ((LOG_ERR, "failed to unlock %s",
				         pw_dbname ()));
			}
			/* continue */
		}
	}
	pw_locked = false;
}

/*
 * check_pw_file - check the content of the passwd file
 */
static void check_pw_file (int *errors, bool *changed)
{
	struct commonio_entry *pfe, *tpfe;
	struct passwd *pwd;
	const struct spwd *spw;
	uid_t min_sys_id = getdef_ulong ("SYS_UID_MIN", 101UL);
	uid_t max_sys_id = getdef_ulong ("SYS_UID_MAX", 999UL);

	/*
	 * Loop through the entire password file.
	 */
	for (pfe = __pw_get_head (); NULL != pfe; pfe = pfe->next) {
		/*
		 * If this is a NIS line, skip it. You can't "know" what NIS
		 * is going to do without directly asking NIS ...
		 */
		if (('+' == pfe->line[0]) || ('-' == pfe->line[0])) {
			continue;
		}

		/*
		 * Start with the entries that are completely corrupt.  They
		 * have no (struct passwd) entry because they couldn't be
		 * parsed properly.
		 */
		if (NULL == pfe->eptr) {
			/*
			 * Tell the user this entire line is bogus and ask
			 * them to delete it.
			 */
			puts (_("invalid password file entry"));
			printf (_("delete line '%s'? "), pfe->line);
			*errors += 1;

			/*
			 * prompt the user to delete the entry or not
			 */
			if (!yes_or_no (read_only)) {
				continue;
			}

			/*
			 * All password file deletions wind up here. This
			 * code removes the current entry from the linked
			 * list. When done, it skips back to the top of the
			 * loop to try out the next list element.
			 */
		      delete_pw:
			if (use_system_pw_file) {
				SYSLOG ((LOG_INFO, "delete passwd line '%s'",
				         pfe->line));
			}
			*changed = true;

			__pw_del_entry (pfe);
			continue;
		}

		/*
		 * Password structure is good, start using it.
		 */
		pwd = pfe->eptr;

		/*
		 * Make sure this entry has a unique name.
		 */
		for (tpfe = __pw_get_head (); NULL != tpfe; tpfe = tpfe->next) {
			const struct passwd *ent = tpfe->eptr;

			/*
			 * Don't check this entry
			 */
			if (tpfe == pfe) {
				continue;
			}

			/*
			 * Don't check invalid entries.
			 */
			if (NULL == ent) {
				continue;
			}

			if (strcmp (pwd->pw_name, ent->pw_name) != 0) {
				continue;
			}

			/*
			 * Tell the user this entry is a duplicate of
			 * another and ask them to delete it.
			 */
			puts (_("duplicate password entry"));
			printf (_("delete line '%s'? "), pfe->line);
			*errors += 1;

			/*
			 * prompt the user to delete the entry or not
			 */
			if (yes_or_no (read_only)) {
				goto delete_pw;
			}
		}

		/*
		 * Check for invalid usernames.  --marekm
		 */

		if (!is_valid_user_name (pwd->pw_name)) {
			printf (_("invalid user name '%s': use --badname to ignore\n"),
					pwd->pw_name);
			*errors += 1;
		}

		/*
		 * Check for invalid user ID.
		 */
		if (pwd->pw_uid == (uid_t)-1) {
			printf (_("invalid user ID '%lu'\n"), (long unsigned int)pwd->pw_uid);
			*errors += 1;
		}

		/*
		 * Make sure the primary group exists
		 */
		/* local, no need for xgetgrgid */
		if (!quiet && (NULL == getgrgid (pwd->pw_gid))) {

			/*
			 * No primary group, just give a warning
			 */

			printf (_("user '%s': no group %lu\n"),
			        pwd->pw_name, (unsigned long) pwd->pw_gid);
			*errors += 1;
		}

		/*
		 * If uid is not system and has a home directory, then check
		 */
		if (!(pwd->pw_uid >= min_sys_id && pwd->pw_uid <= max_sys_id ) && pwd->pw_dir && pwd->pw_dir[0]) {
			/*
			 * Make sure the home directory exists
			 */
			if (!quiet && (access (pwd->pw_dir, F_OK) != 0)) {
				const char *nonexistent = getdef_str("NONEXISTENT");

				/*
				 * Home directory does not exist, give a warning (unless intentional)
				 */
				if (NULL == nonexistent || strcmp (pwd->pw_dir, nonexistent) != 0) {
					printf (_("user '%s': directory '%s' does not exist\n"),
							pwd->pw_name, pwd->pw_dir);
					*errors += 1;
				}
			}
		}

		/*
		 * Make sure the login shell is executable
		 */
		if (   !quiet
		    && ('\0' != pwd->pw_shell[0])
		    && (access (pwd->pw_shell, F_OK) != 0)) {

			/*
			 * Login shell doesn't exist, give a warning
			 */
			printf (_("user '%s': program '%s' does not exist\n"),
			        pwd->pw_name, pwd->pw_shell);
			*errors += 1;
		}

		/*
		 * Make sure this entry exists in the /etc/shadow file.
		 */

		if (is_shadow) {
#ifdef WITH_TCB
			if (getdef_bool ("USE_TCB")) {
				if (shadowtcb_set_user (pwd->pw_name) == SHADOWTCB_FAILURE) {
					printf (_("no tcb directory for %s\n"),
					        pwd->pw_name);
					printf (_("create tcb directory for %s?"),
					        pwd->pw_name);
					*errors += 1;
					if (yes_or_no (read_only)) {
						if (shadowtcb_create (pwd->pw_name, pwd->pw_uid) == SHADOWTCB_FAILURE) {
							*errors += 1;
							printf (_("failed to create tcb directory for %s\n"), pwd->pw_name);
							continue;
						}
					} else {
						continue;
					}
				}
				if (spw_lock () == 0) {
					*errors += 1;
					fprintf (stderr,
					         _("%s: cannot lock %s.\n"),
					         Prog, spw_dbname ());
					continue;
				}
				spw_locked = true;
				if (spw_open (read_only ? O_RDONLY : O_RDWR) == 0) {
					fprintf (stderr,
					         _("%s: cannot open %s\n"),
					         Prog, spw_dbname ());
					*errors += 1;
					if (spw_unlock () == 0) {
						fprintf (stderr,
						         _("%s: failed to unlock %s\n"),
						         Prog, spw_dbname ());
						if (use_system_spw_file) {
							SYSLOG ((LOG_ERR,
							         "failed to unlock %s",
							         spw_dbname ()));
						}
					}
					continue;
				}
				spw_opened = true;
			}
#endif				/* WITH_TCB */
			spw = spw_locate (pwd->pw_name);
			if (NULL == spw) {
				printf (_("no matching password file entry in %s\n"),
				        spw_dbname ());
				printf (_("add user '%s' in %s? "),
				        pwd->pw_name, spw_dbname ());
				*errors += 1;
				if (yes_or_no (read_only)) {
					struct spwd sp;
					struct passwd pw;

					sp.sp_namp   = pwd->pw_name;
					sp.sp_pwdp   = pwd->pw_passwd;
					sp.sp_min    =
					    getdef_num ("PASS_MIN_DAYS", -1);
					sp.sp_max    =
					    getdef_num ("PASS_MAX_DAYS", -1);
					sp.sp_warn   =
					    getdef_num ("PASS_WARN_AGE", -1);
					sp.sp_inact  = -1;
					sp.sp_expire = -1;
					sp.sp_flag   = SHADOW_SP_FLAG_UNSET;
					sp.sp_lstchg = gettime () / DAY;
					if (0 == sp.sp_lstchg) {
						/* Better disable aging than
						 * requiring a password change
						 */
						sp.sp_lstchg = -1;
					}
					*changed = true;

					if (spw_update (&sp) == 0) {
						fprintf (stderr,
						         _("%s: failed to prepare the new %s entry '%s'\n"),
						         Prog, spw_dbname (), sp.sp_namp);
						fail_exit (E_CANTUPDATE);
					}
					/* remove password from /etc/passwd */
					pw = *pwd;
					pw.pw_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */
					if (pw_update (&pw) == 0) {
						fprintf (stderr,
						         _("%s: failed to prepare the new %s entry '%s'\n"),
						         Prog, pw_dbname (), pw.pw_name);
						fail_exit (E_CANTUPDATE);
					}
				}
			} else {
				/* The passwd entry has a shadow counterpart.
				 * Make sure no passwords are in passwd.
				 */
				if (   !quiet
				    && (strcmp (pwd->pw_passwd,
				                SHADOW_PASSWD_STRING) != 0)) {
					printf (_("user %s has an entry in %s, but its password field in %s is not set to 'x'\n"),
					        pwd->pw_name, spw_dbname (), pw_dbname ());
					*errors += 1;
				}
			}
		}
#ifdef WITH_TCB
		if (getdef_bool ("USE_TCB") && spw_locked) {
			if (spw_opened && (spw_close () == 0)) {
				fprintf (stderr,
				         _("%s: failure while writing changes to %s\n"),
				         Prog, spw_dbname ());
				if (use_system_spw_file) {
					SYSLOG ((LOG_ERR,
					         "failure while writing changes to %s",
					         spw_dbname ()));
				}
			} else {
				spw_opened = false;
			}
			if (spw_unlock () == 0) {
				fprintf (stderr,
				         _("%s: failed to unlock %s\n"),
				         Prog, spw_dbname ());
				if (use_system_spw_file) {
					SYSLOG ((LOG_ERR, "failed to unlock %s",
					         spw_dbname ()));
				}
			} else {
				spw_locked = false;
			}
		}
#endif				/* WITH_TCB */
	}
}

/*
 * check_spw_file - check the content of the shadowed password file (shadow)
 */
static void check_spw_file (int *errors, bool *changed)
{
	struct commonio_entry *spe, *tspe;
	struct spwd *spw;

	/*
	 * Loop through the entire shadow password file.
	 */
	for (spe = __spw_get_head (); NULL != spe; spe = spe->next) {
		/*
		 * Do not treat lines which were missing in shadow
		 * and were added earlier.
		 */
		if (NULL == spe->line) {
			continue;
		}

		/*
		 * If this is a NIS line, skip it. You can't "know" what NIS
		 * is going to do without directly asking NIS ...
		 */
		if (('+' == spe->line[0]) || ('-' == spe->line[0])) {
			continue;
		}

		/*
		 * Start with the entries that are completely corrupt. They
		 * have no (struct spwd) entry because they couldn't be
		 * parsed properly.
		 */
		if (NULL == spe->eptr) {
			/*
			 * Tell the user this entire line is bogus and ask
			 * them to delete it.
			 */
			puts (_("invalid shadow password file entry"));
			printf (_("delete line '%s'? "), spe->line);
			*errors += 1;

			/*
			 * prompt the user to delete the entry or not
			 */
			if (!yes_or_no (read_only)) {
				continue;
			}

			/*
			 * All shadow file deletions wind up here. This code
			 * removes the current entry from the linked list.
			 * When done, it skips back to the top of the loop
			 * to try out the next list element.
			 */
		      delete_spw:
			if (use_system_spw_file) {
				SYSLOG ((LOG_INFO, "delete shadow line '%s'",
				         spe->line));
			}
			*changed = true;

			__spw_del_entry (spe);
			continue;
		}

		/*
		 * Shadow password structure is good, start using it.
		 */
		spw = spe->eptr;

		/*
		 * Make sure this entry has a unique name.
		 */
		for (tspe = __spw_get_head (); NULL != tspe; tspe = tspe->next) {
			const struct spwd *ent = tspe->eptr;

			/*
			 * Don't check this entry
			 */
			if (tspe == spe) {
				continue;
			}

			/*
			 * Don't check invalid entries.
			 */
			if (NULL == ent) {
				continue;
			}

			if (strcmp (spw->sp_namp, ent->sp_namp) != 0) {
				continue;
			}

			/*
			 * Tell the user this entry is a duplicate of
			 * another and ask them to delete it.
			 */
			puts (_("duplicate shadow password entry"));
			printf (_("delete line '%s'? "), spe->line);
			*errors += 1;

			/*
			 * prompt the user to delete the entry or not
			 */
			if (yes_or_no (read_only)) {
				goto delete_spw;
			}
		}

		/*
		 * Make sure this entry exists in the /etc/passwd
		 * file.
		 */
		if (pw_locate (spw->sp_namp) == NULL) {
			/*
			 * Tell the user this entry has no matching
			 * /etc/passwd entry and ask them to delete it.
			 */
			printf (_("no matching password file entry in %s\n"),
			        pw_dbname ());
			printf (_("delete line '%s'? "), spe->line);
			*errors += 1;

			/*
			 * prompt the user to delete the entry or not
			 */
			if (yes_or_no (read_only)) {
				goto delete_spw;
			}
		}

		/*
		 * Warn if last password change in the future.  --marekm
		 */
		if (!quiet) {
			time_t t = time (NULL);
			if (   (t != 0)
			    && (spw->sp_lstchg > t / DAY)) {
				printf (_("user %s: last password change in the future\n"),
			                spw->sp_namp);
				*errors += 1;
			}
		}
	}
}

/*
 * pwck - verify password file integrity
 */
int main (int argc, char **argv)
{
	int errors = 0;
	bool changed = false;

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);

	OPENLOG (Prog);

	/* Parse the command line arguments */
	process_flags (argc, argv);

	open_files ();

	if (sort_mode) {
		if (pw_sort () != 0) {
			fprintf (stderr,
			         _("%s: cannot sort entries in %s\n"),
			         Prog, pw_dbname ());
			fail_exit (E_CANTSORT);
		}
		if (is_shadow) {
			if (spw_sort () != 0) {
				fprintf (stderr,
				         _("%s: cannot sort entries in %s\n"),
				         Prog, spw_dbname ());
				fail_exit (E_CANTSORT);
			}
		}
		changed = true;
	} else {
		check_pw_file (&errors, &changed);

		if (is_shadow) {
			check_spw_file (&errors, &changed);
		}
	}

	close_files (changed);

	if (!read_only) {
		nscd_flush_cache ("passwd");
		sssd_flush_cache (SSSD_DB_PASSWD);
	}

	/*
	 * Tell the user what we did and exit.
	 */
	if (0 != errors) {
		printf (changed ?
		        _("%s: the files have been updated\n") :
		        _("%s: no changes\n"), Prog);
	}

	closelog ();
	return ((0 != errors) ? E_BADENTRY : E_OKAY);
}

