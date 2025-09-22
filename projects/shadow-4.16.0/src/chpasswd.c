/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2000 - 2006, Tomasz Kłoczko
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

#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
#include "atoi/str2i.h"
#include "defines.h"
#include "nscd.h"
#include "sssd.h"
#include "getdef.h"
#include "prototypes.h"
#include "pwio.h"
#include "shadowio.h"
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"


#define IS_CRYPT_METHOD(str) ((crypt_method != NULL && strcmp(crypt_method, str) == 0) ? true : false)

/*
 * Global variables
 */
static const char Prog[] = "chpasswd";
static bool eflg   = false;
static bool md5flg = false;
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
static bool sflg   = false;
#endif

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

static const char *prefix = "";

static bool is_shadow_pwd;
static bool pw_locked = false;
static bool spw_locked = false;

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
	(void) fputs (_("  -P, --prefix PREFIX_DIR       directory prefix\n"), usageout);
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
		{"prefix",       required_argument, NULL, 'P'},
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
		{"sha-rounds",   required_argument, NULL, 's'},
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv,
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	                         "c:ehmR:P:s:",
#else
	                         "c:ehmR:P:",
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
		case 'P': /* no-op, handled in process_prefix_flag () */
			break;
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
		case 's':
			sflg = true;
                        bad_s = 0;
#if defined(USE_SHA_CRYPT)
			if ((IS_CRYPT_METHOD("SHA256") || IS_CRYPT_METHOD("SHA512"))
			    && (-1 == str2sl(&sha_rounds, optarg))) {
                            bad_s = 1;
                        }
#endif				/* USE_SHA_CRYPT */
#if defined(USE_BCRYPT)
                        if (IS_CRYPT_METHOD("BCRYPT")
			    && (-1 == str2sl(&bcrypt_rounds, optarg))) {
                            bad_s = 1;
                        }
#endif				/* USE_BCRYPT */
#if defined(USE_YESCRYPT)
                        if (IS_CRYPT_METHOD("YESCRYPT")
			    && (-1 == str2sl(&yescrypt_cost, optarg))) {
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
		if ((!IS_CRYPT_METHOD("DES"))
		    &&(!IS_CRYPT_METHOD("MD5"))
		    &&(!IS_CRYPT_METHOD("NONE"))
#ifdef USE_SHA_CRYPT
		    &&(!IS_CRYPT_METHOD("SHA256"))
		    &&(!IS_CRYPT_METHOD("SHA512"))
#endif				/* USE_SHA_CRYPT */
#ifdef USE_BCRYPT
		    &&(!IS_CRYPT_METHOD("BCRYPT"))
#endif				/* USE_BCRYPT */
#ifdef USE_YESCRYPT
		    &&(!IS_CRYPT_METHOD("YESCRYPT"))
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
 *	With PAM support, the setuid bit can be set on chpasswd to allow
 *	non-root users to groups.
 *	Without PAM support, only users who can write in the group databases
 *	can add groups.
 *
 *	It will not return if the user is not allowed.
 */
static void check_perms (void)
{
#ifdef USE_PAM
#ifdef ACCT_TOOLS_SETUID
	/* If chpasswd uses PAM and is SUID, check the permissions,
	 * otherwise, the permissions are enforced by the access to the
	 * passwd and shadow files.
	 */
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
#endif				/* ACCT_TOOLS_SETUID */
#endif				/* USE_PAM */
}

/*
 * open_files - lock and open the password databases
 */
static void open_files (void)
{
	/*
	 * Lock the password file and open it for reading and writing. This
	 * will bring all of the entries into memory where they may be updated.
	 */
	if (pw_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, pw_dbname ());
		fail_exit (1);
	}
	pw_locked = true;
	if (pw_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"), Prog, pw_dbname ());
		fail_exit (1);
	}

	/* Do the same for the shadowed database, if it exist */
	if (is_shadow_pwd) {
		if (spw_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, spw_dbname ());
			fail_exit (1);
		}
		spw_locked = true;
		if (spw_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, spw_dbname ());
			fail_exit (1);
		}
	}
}

/*
 * close_files - close and unlock the password databases
 */
static void close_files (void)
{
	if (is_shadow_pwd) {
		if (spw_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", spw_dbname ()));
			fail_exit (1);
		}
		if (spw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
			/* continue */
		}
		spw_locked = false;
	}

	if (pw_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (1);
	}
	if (pw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}
	pw_locked = false;
}

static const char *get_salt(void)
{
	void *arg = NULL;

	if (eflg || IS_CRYPT_METHOD("NONE")) {
		return NULL;
	}

	if (md5flg) {
		crypt_method = "MD5";
	}
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	if (sflg) {
#if defined(USE_SHA_CRYPT)
		if (IS_CRYPT_METHOD("SHA256") || IS_CRYPT_METHOD("SHA512")) {
			arg = &sha_rounds;
		}
#endif				/* USE_SHA_CRYPT */
#if defined(USE_BCRYPT)
		if (IS_CRYPT_METHOD("BCRYPT")) {
			arg = &bcrypt_rounds;
		}
#endif				/* USE_BCRYPT */
#if defined(USE_YESCRYPT)
		if (IS_CRYPT_METHOD("YESCRYPT")) {
			arg = &yescrypt_cost;
		}
#endif				/* USE_YESCRYPT */
	}
#endif
	return crypt_make_salt (crypt_method, arg);
}

int main (int argc, char **argv)
{
	char buf[BUFSIZ];
	char *name;
	char *newpwd;
	char *cp;
	const char *salt;

#ifdef USE_PAM
	bool use_pam = true;
#endif				/* USE_PAM */

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

	process_flags (argc, argv);

	salt = get_salt();
	process_root_flag ("-R", argc, argv);
	prefix = process_prefix_flag ("-P", argc, argv);

#ifdef USE_PAM
	if (md5flg || eflg || cflg || prefix[0]) {
		use_pam = false;
	}
#endif				/* USE_PAM */

	OPENLOG (Prog);

	check_perms ();

#ifdef USE_PAM
	if (!use_pam)
#endif				/* USE_PAM */
	{
		is_shadow_pwd = spw_file_present ();

		open_files ();
	}

	/*
	 * Read each line, separating the user name from the password. The
	 * password entry for each user will be looked up in the appropriate
	 * file (shadow or passwd) and the password changed. For shadow
	 * files the last change date is set directly, for passwd files the
	 * last change date is set in the age only if aging information is
	 * present.
	 */
	while (fgets (buf, sizeof buf, stdin) != NULL) {
		line++;
		cp = strrchr (buf, '\n');
		if (NULL != cp) {
			*cp = '\0';
		} else {
			if (feof (stdin) == 0) {

				// Drop all remaining characters on this line.
				while (fgets (buf, sizeof buf, stdin) != NULL) {
					cp = strchr (buf, '\n');
					if (cp != NULL) {
						break;
					}
				}

				fprintf (stderr,
				         _("%s: line %d: line too long\n"),
				         Prog, line);
				errors++;
				continue;
			}
		}

		/*
		 * The username is the first field. It is separated from the
		 * password with a ":" character which is replaced with a
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

#ifdef USE_PAM
		if (use_pam) {
			if (do_pam_passwd_non_interactive (Prog, name, newpwd) != 0) {
				fprintf (stderr,
				         _("%s: (line %d, user %s) password not changed\n"),
				         Prog, line, name);
				errors++;
			}
		} else
#endif				/* USE_PAM */
		{
		const struct spwd *sp;
		struct spwd newsp;
		const struct passwd *pw;
		struct passwd newpw;

		if (salt) {
			cp = pw_encrypt (newpwd, salt);
			if (NULL == cp) {
				fprintf (stderr,
				         _("%s: failed to crypt password with salt '%s': %s\n"),
				         Prog, salt, strerror (errno));
				fail_exit (1);
			}
		}

		/*
		 * Get the password file entry for this user. The user must
		 * already exist.
		 */
		pw = pw_locate (name);
		if (NULL == pw) {
			fprintf (stderr,
			         _("%s: line %d: user '%s' does not exist\n"), Prog,
			         line, name);
			errors++;
			continue;
		}
		if (is_shadow_pwd) {
			/* The shadow entry should be updated if the
			 * passwd entry has a password set to 'x'.
			 * But on the other hand, if there is already both
			 * a passwd and a shadow password, it's preferable
			 * to update both.
			 */
			sp = spw_locate (name);

			if (   (NULL == sp)
			    && (strcmp (pw->pw_passwd,
			                SHADOW_PASSWD_STRING) == 0)) {
				/* If the password is set to 'x' in
				 * passwd, but there are no entries in
				 * shadow, create one.
				 */
				newsp.sp_namp  = name;
				/* newsp.sp_pwdp  = NULL; will be set later */
				/* newsp.sp_lstchg= 0;    will be set later */
				newsp.sp_min   = getdef_num ("PASS_MIN_DAYS", -1);
				newsp.sp_max   = getdef_num ("PASS_MAX_DAYS", -1);
				newsp.sp_warn  = getdef_num ("PASS_WARN_AGE", -1);
				newsp.sp_inact = -1;
				newsp.sp_expire= -1;
				newsp.sp_flag  = SHADOW_SP_FLAG_UNSET;
				sp = &newsp;
			}
		} else {
			sp = NULL;
		}

		/*
		 * The freshly encrypted new password is merged into the
		 * user's password file entry and the last password change
		 * date is set to the current date.
		 */
		if (NULL != sp) {
			newsp = *sp;
			newsp.sp_pwdp = cp;
			newsp.sp_lstchg = gettime () / DAY;
			if (0 == newsp.sp_lstchg) {
				/* Better disable aging than requiring a
				 * password change */
				newsp.sp_lstchg = -1;
			}
		}

		if (   (NULL == sp)
		    || (strcmp (pw->pw_passwd, SHADOW_PASSWD_STRING) != 0)) {
			newpw = *pw;
			newpw.pw_passwd = cp;
		}

		/*
		 * The updated password file entry is then put back and will
		 * be written to the password file later, after all the
		 * other entries have been updated as well.
		 */
		if (NULL != sp) {
			if (spw_update (&newsp) == 0) {
				fprintf (stderr,
				         _("%s: line %d: failed to prepare the new %s entry '%s'\n"),
				         Prog, line, spw_dbname (), newsp.sp_namp);
				errors++;
				continue;
			}
		}
		if (   (NULL == sp)
		    || (strcmp (pw->pw_passwd, SHADOW_PASSWD_STRING) != 0)) {
			if (pw_update (&newpw) == 0) {
				fprintf (stderr,
				         _("%s: line %d: failed to prepare the new %s entry '%s'\n"),
				         Prog, line, pw_dbname (), newpw.pw_name);
				errors++;
				continue;
			}
		}
		}
	}

	/*
	 * Any detected errors will cause the entire set of changes to be
	 * aborted. Unlocking the password file will cause all of the
	 * changes to be ignored. Otherwise the file is closed, causing the
	 * changes to be written out all at once, and then unlocked
	 * afterwards.
	 *
	 * With PAM, it is not possible to delay the update of the
	 * password database.
	 */
	if (0 != errors) {
#ifdef USE_PAM
		if (!use_pam)
#endif				/* USE_PAM */
		{
			fprintf (stderr,
			         _("%s: error detected, changes ignored\n"),
			         Prog);
		}
		fail_exit (1);
	}

#ifdef USE_PAM
	if (!use_pam)
#endif				/* USE_PAM */
	{
	/* Save the changes */
		close_files ();
	}

	nscd_flush_cache ("passwd");
	sssd_flush_cache (SSSD_DB_PASSWD);

	return (0);
}

