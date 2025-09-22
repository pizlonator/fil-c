/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 2006       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2006       , Jonas Meurer
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <fcntl.h>
#include <getopt.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
#include "atoi/str2i.h"
#include "defines.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#include "groupio.h"
#ifdef	SHADOWGRP
#include "sgroupio.h"
#endif
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"


/*
 * Global variables
 */
static const char Prog[] = "chgpasswd";
static bool eflg   = false;
static bool md5flg = false;
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
static bool sflg   = false;
#endif /* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */

static /*@null@*//*@observer@*/const char *crypt_method = NULL;
#define cflg (NULL != crypt_method)
#ifdef USE_SHA_CRYPT
static long sha_rounds = 5000;
#endif
#ifdef USE_BCRYPT
static long bcrypt_rounds = 13;
#endif
#ifdef USE_YESCRYPT
static long yescrypt_cost = 5;
#endif

#ifdef SHADOWGRP
static bool is_shadow_grp;
static bool sgr_locked = false;
#endif
static bool gr_locked = false;

/* local function prototypes */
NORETURN static void fail_exit (int code);
NORETURN static void usage (int status);
static void process_flags (int argc, char **argv);
static void check_flags (void);
static void check_perms (void);
static void open_files (void);
static void close_files (void);

/*
 * fail_exit - exit with a failure code after unlocking the files
 */
static void fail_exit (int code)
{
	if (gr_locked) {
		if (gr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, gr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
			/* continue */
		}
	}

#ifdef	SHADOWGRP
	if (sgr_locked) {
		if (sgr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
			/* continue */
		}
	}
#endif

	exit (code);
}

/*
 * usage - display usage message and exit
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
	(void) fprintf (usageout,
	                _("  -c, --crypt-method METHOD     the crypt method (one of %s)\n"),
	                "NONE DES MD5"
#if defined(USE_SHA_CRYPT)
	                " SHA256 SHA512"
#endif
#if defined(USE_BCRYPT)
	                " BCRYPT"
#endif
#if defined(USE_YESCRYPT)
	                " YESCRYPT"
#endif
	               );
	(void) fputs (_("  -e, --encrypted               supplied passwords are encrypted\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -m, --md5                     encrypt the clear text password using\n"
	                "                                the MD5 algorithm\n"),
	              usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	(void) fputs (_("  -s, --sha-rounds              number of rounds for the SHA, BCRYPT\n"
	                "                                or YESCRYPT crypt algorithms\n"),
	              usageout);
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
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
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	int bad_s;
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
	static struct option long_options[] = {
		{"crypt-method", required_argument, NULL, 'c'},
		{"encrypted",    no_argument,       NULL, 'e'},
		{"help",         no_argument,       NULL, 'h'},
		{"md5",          no_argument,       NULL, 'm'},
		{"root",         required_argument, NULL, 'R'},
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
		{"sha-rounds",   required_argument, NULL, 's'},
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
		{NULL, 0, NULL, '\0'}
	};
	while ((c = getopt_long (argc, argv,
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	                         "c:ehmR:s:",
#else
	                         "c:ehmR:",
#endif
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'c':
			crypt_method = optarg;
			break;
		case 'e':
			eflg = true;
			break;
		case 'h':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		case 'm':
			md5flg = true;
			break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
		case 's':
			sflg = true;
                        bad_s = 0;

			if (!crypt_method) {
				fprintf (stderr,
				         _("%s: no crypt method defined\n"),
				         Prog);
				usage (E_USAGE);
			}
#if defined(USE_SHA_CRYPT)
			if (  (   ((0 == strcmp (crypt_method, "SHA256")) || (0 == strcmp (crypt_method, "SHA512")))
			       && (-1 == str2sl(&sha_rounds, optarg)))) {
                            bad_s = 1;
                        }
#endif				/* USE_SHA_CRYPT */
#if defined(USE_BCRYPT)
                        if ((   (0 == strcmp (crypt_method, "BCRYPT"))
			       && (-1 == str2sl(&bcrypt_rounds, optarg)))) {
                            bad_s = 1;
                        }
#endif				/* USE_BCRYPT */
#if defined(USE_YESCRYPT)
                        if ((   (0 == strcmp (crypt_method, "YESCRYPT"))
			       && (-1 == str2sl(&yescrypt_cost, optarg)))) {
                            bad_s = 1;
                        }
#endif				/* USE_YESCRYPT */
                        if (bad_s != 0) {
				fprintf (stderr,
				         _("%s: invalid numeric argument '%s'\n"),
				         Prog, optarg);
				usage (E_USAGE);
			}
			break;
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */

		default:
			usage (E_USAGE);
			/*@notreached@*/break;
		}
	}

	/* validate options */
	check_flags ();
}

/*
 * check_flags - check flags and parameters consistency
 *
 *	It will not return if an error is encountered.
 */
static void check_flags (void)
{
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	if (sflg && !cflg) {
		fprintf (stderr,
		         _("%s: %s flag is only allowed with the %s flag\n"),
		         Prog, "-s", "-c");
		usage (E_USAGE);
	}
#endif

	if ((eflg && (md5flg || cflg)) ||
	    (md5flg && cflg)) {
		fprintf (stderr,
		         _("%s: the -c, -e, and -m flags are exclusive\n"),
		         Prog);
		usage (E_USAGE);
	}

	if (cflg) {
		if (   (0 != strcmp (crypt_method, "DES"))
		    && (0 != strcmp (crypt_method, "MD5"))
		    && (0 != strcmp (crypt_method, "NONE"))
#ifdef USE_SHA_CRYPT
		    && (0 != strcmp (crypt_method, "SHA256"))
		    && (0 != strcmp (crypt_method, "SHA512"))
#endif				/* USE_SHA_CRYPT */
#ifdef USE_BCRYPT
		    && (0 != strcmp (crypt_method, "BCRYPT"))
#endif				/* USE_BCRYPT */
#ifdef USE_YESCRYPT
		    && (0 != strcmp (crypt_method, "YESCRYPT"))
#endif				/* USE_YESCRYPT */
		    ) {
			fprintf (stderr,
			         _("%s: unsupported crypt method: %s\n"),
			         Prog, crypt_method);
			usage (E_USAGE);
		}
	}
}

/*
 * check_perms - check if the caller is allowed to add a group
 *
 *	With PAM support, the setuid bit can be set on chgpasswd to allow
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
	int retval;
	struct passwd *pampw;

	pampw = getpwuid (getuid ()); /* local, no need for xgetpwuid */
	if (NULL == pampw) {
		fprintf (stderr,
		         _("%s: Cannot determine your user name.\n"),
		         Prog);
		exit (1);
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
		exit (1);
	}
	(void) pam_end (pamh, retval);
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
}

/*
 * open_files - lock and open the group databases
 */
static void open_files (void)
{
	/*
	 * Lock the group file and open it for reading and writing. This will
	 * bring all of the entries into memory where they may be updated.
	 */
	if (gr_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, gr_dbname ());
		fail_exit (1);
	}
	gr_locked = true;
	if (gr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"), Prog, gr_dbname ());
		fail_exit (1);
	}

#ifdef SHADOWGRP
	/* Do the same for the shadowed database, if it exist */
	if (is_shadow_grp) {
		if (sgr_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sgr_dbname ());
			fail_exit (1);
		}
		sgr_locked = true;
		if (sgr_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr, _("%s: cannot open %s\n"),
			         Prog, sgr_dbname ());
			fail_exit (1);
		}
	}
#endif
}

/*
 * close_files - close and unlock the group databases
 */
static void close_files (void)
{
#ifdef SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", sgr_dbname ()));
			fail_exit (1);
		}
		if (sgr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
			/* continue */
		}
		sgr_locked = false;
	}
#endif

	if (gr_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", gr_dbname ()));
		fail_exit (1);
	}
	if (gr_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
		/* continue */
	}
	gr_locked = false;
}

int main (int argc, char **argv)
{
	char buf[BUFSIZ];
	char *name;
	char *newpwd;
	char *cp;

#ifdef	SHADOWGRP
	const struct sgrp *sg;
	struct sgrp newsg;
#endif

	const struct group *gr;
	struct group newgr;
	int errors = 0;
	int line = 0;

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

#ifdef WITH_SELINUX
	if (check_selinux_permit ("passwd") != 0) {
		return (E_NOPERM);
	}
#endif				/* WITH_SELINUX */

	process_root_flag ("-R", argc, argv);

	process_flags (argc, argv);

	OPENLOG (Prog);

	check_perms ();

#ifdef SHADOWGRP
	is_shadow_grp = sgr_file_present ();
#endif

	open_files ();

	/*
	 * Read each line, separating the group name from the password. The
	 * group entry for each group will be looked up in the appropriate
	 * file (gshadow or group) and the password changed.
	 */
	while (fgets (buf, (int) sizeof buf, stdin) != NULL) {
		line++;
		cp = strrchr (buf, '\n');
		if (NULL != cp) {
			*cp = '\0';
		} else {
			fprintf (stderr, _("%s: line %d: line too long\n"),
			         Prog, line);
			errors++;
			continue;
		}

		/*
		 * The group's name is the first field. It is separated from
		 * the password with a ":" character which is replaced with a
		 * NUL to give the new password. The new password will then
		 * be encrypted in the normal fashion with a new salt
		 * generated, unless the '-e' is given, in which case it is
		 * assumed to already be encrypted.
		 */

		name = buf;
		cp = strchr (name, ':');
		if (NULL != cp) {
			*cp = '\0';
			cp++;
		} else {
			fprintf (stderr,
			         _("%s: line %d: missing new password\n"),
			         Prog, line);
			errors++;
			continue;
		}
		newpwd = cp;
		if (   (!eflg)
		    && (   (NULL == crypt_method)
		        || (0 != strcmp (crypt_method, "NONE")))) {
			void *arg = NULL;
			const char *salt;
			if (md5flg) {
				crypt_method = "MD5";
			}
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
			if (sflg) {
#if defined(USE_SHA_CRYPT)
				if (   (0 == strcmp (crypt_method, "SHA256"))
					|| (0 == strcmp (crypt_method, "SHA512"))) {
					arg = &sha_rounds;
				}
#endif				/* USE_SHA_CRYPT */
#if defined(USE_BCRYPT)
				if (0 == strcmp (crypt_method, "BCRYPT")) {
					arg = &bcrypt_rounds;
				}
#endif				/* USE_BCRYPT */
#if defined(USE_YESCRYPT)
				if (0 == strcmp (crypt_method, "YESCRYPT")) {
					arg = &yescrypt_cost;
				}
#endif				/* USE_YESCRYPT */
			}
#endif
			salt = crypt_make_salt (crypt_method, arg);
			cp = pw_encrypt (newpwd, salt);
			if (NULL == cp) {
				fprintf (stderr,
				         _("%s: failed to crypt password with salt '%s': %s\n"),
				         Prog, salt, strerror (errno));
				fail_exit (1);
			}
		}

		/*
		 * Get the group file entry for this group. The group must
		 * already exist.
		 */
		gr = gr_locate (name);
		if (NULL == gr) {
			fprintf (stderr,
			         _("%s: line %d: group '%s' does not exist\n"), Prog,
			         line, name);
			errors++;
			continue;
		}
#ifdef SHADOWGRP
		if (is_shadow_grp) {
			/* The gshadow entry should be updated if the
			 * group entry has a password set to 'x'.
			 * But on the other hand, if there is already both
			 * a group and a gshadow password, it's preferable
			 * to update both.
			 */
			sg = sgr_locate (name);

			if (   (NULL == sg)
			    && (strcmp (gr->gr_passwd,
			                SHADOW_PASSWD_STRING) == 0)) {
				static char *empty = NULL;
				/* If the password is set to 'x' in
				 * group, but there are no entries in
				 * gshadow, create one.
				 */
				newsg.sg_name   = name;
				/* newsg.sg_passwd = NULL; will be set later */
				newsg.sg_adm    = &empty;
				newsg.sg_mem    = dup_list (gr->gr_mem);
				sg = &newsg;
			}
		} else {
			sg = NULL;
		}
#endif

		/*
		 * The freshly encrypted new password is merged into the
		 * group's entry.
		 */
#ifdef SHADOWGRP
		if (NULL != sg) {
			newsg = *sg;
			newsg.sg_passwd = cp;
		}
		if (   (NULL == sg)
		    || (strcmp (gr->gr_passwd, SHADOW_PASSWD_STRING) != 0))
#endif
		{
			newgr = *gr;
			newgr.gr_passwd = cp;
		}

		/*
		 * The updated group file entry is then put back and will
		 * be written to the group file later, after all the
		 * other entries have been updated as well.
		 */
#ifdef SHADOWGRP
		if (NULL != sg) {
			if (sgr_update (&newsg) == 0) {
				fprintf (stderr,
				         _("%s: line %d: failed to prepare the new %s entry '%s'\n"),
				         Prog, line, sgr_dbname (), newsg.sg_name);
				errors++;
				continue;
			}
		}
		if (   (NULL == sg)
		    || (strcmp (gr->gr_passwd, SHADOW_PASSWD_STRING) != 0))
#endif
		{
			if (gr_update (&newgr) == 0) {
				fprintf (stderr,
				         _("%s: line %d: failed to prepare the new %s entry '%s'\n"),
				         Prog, line, gr_dbname (), newgr.gr_name);
				errors++;
				continue;
			}
		}
	}

	/*
	 * Any detected errors will cause the entire set of changes to be
	 * aborted. Unlocking the group file will cause all of the
	 * changes to be ignored. Otherwise the file is closed, causing the
	 * changes to be written out all at once, and then unlocked
	 * afterwards.
	 */
	if (0 != errors) {
		fprintf (stderr,
		         _("%s: error detected, changes ignored\n"), Prog);
		fail_exit (1);
	}

	close_files ();

	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_GROUP);

	return (0);
}

