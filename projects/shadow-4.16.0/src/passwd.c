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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

#include "agetpass.h"
#include "alloc.h"
#include "atoi/str2i.h"
#include "defines.h"
#include "getdef.h"
#include "memzero.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#include "pwauth.h"
#include "pwio.h"
#include "shadowio.h"
#include "shadowlog.h"
#include "string/strtcpy.h"
#include "time/day_to_str.h"



/*
 * exit status values
 */
/*@-exitarg@*/
#define E_SUCCESS	0	/* success */
#define E_NOPERM	1	/* permission denied */
#define E_USAGE		2	/* invalid combination of options */
#define E_FAILURE	3	/* unexpected failure, nothing done */
#define E_MISSING	4	/* unexpected failure, passwd file missing */
#define E_PWDBUSY	5	/* passwd file busy, try again later */
#define E_BAD_ARG	6	/* invalid argument to option */
/*
 * Global variables
 */
static const char Prog[] = "passwd";	/* Program name */

static char *name;		/* The name of user whose password is being changed */
static char *myname;		/* The current user's name */
static bool amroot;		/* The caller's real UID was 0 */

static const char *prefix = "";

static bool
    aflg = false,			/* -a - show status for all users */
    dflg = false,			/* -d - delete password */
    eflg = false,			/* -e - force password change */
    iflg = false,			/* -i - set inactive days */
    kflg = false,			/* -k - change only if expired */
    lflg = false,			/* -l - lock the user's password */
    nflg = false,			/* -n - set minimum days */
    qflg = false,			/* -q - quiet mode */
    Sflg = false,			/* -S - show password status */
    uflg = false,			/* -u - unlock the user's password */
    wflg = false,			/* -w - set warning days */
    xflg = false,			/* -x - set maximum days */
    sflg = false;			/* -s - read passwd from stdin */

/*
 * set to 1 if there are any flags which require root privileges,
 * and require username to be specified
 */
static bool anyflag = false;

static long age_min = 0;	/* Minimum days before change   */
static long age_max = 0;	/* Maximum days until change     */
static long warn = 0;		/* Warning days before change   */
static long inact = 0;		/* Days without change before locked */

static bool do_update_age = false;
#ifdef USE_PAM
static bool use_pam = true;
#else
static bool use_pam = false;
#endif				/* USE_PAM */

static bool pw_locked = false;
static bool spw_locked = false;

/*
 * Size of the biggest passwd:
 *   $6$	3
 *   rounds=	7
 *   999999999	9
 *   $		1
 *   salt	16
 *   $		1
 *   SHA512	123
 *   nul	1
 *
 *   total	161
 */
static char crypt_passwd[256];
static bool do_update_pwd = false;

/*
 * External identifiers
 */

/* local function prototypes */
NORETURN static void usage (int);

static int new_password (const struct passwd *);

static void check_password (const struct passwd *, const struct spwd *);
static /*@observer@*/const char *pw_status (const char *);
static void print_status (const struct passwd *);
NORETURN static void fail_exit (int);
NORETURN static void oom (void);
static char *update_crypt_pw (char *);
static void update_noshadow (void);

static void update_shadow (void);

/*
 * usage - print command usage and exit
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
	(void) fputs (_("  -a, --all                     report password status on all accounts\n"), usageout);
	(void) fputs (_("  -d, --delete                  delete the password for the named account\n"), usageout);
	(void) fputs (_("  -e, --expire                  force expire the password for the named account\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -k, --keep-tokens             change password only if expired\n"), usageout);
	(void) fputs (_("  -i, --inactive INACTIVE       set password inactive after expiration\n"
	                "                                to INACTIVE\n"), usageout);
	(void) fputs (_("  -l, --lock                    lock the password of the named account\n"), usageout);
	(void) fputs (_("  -n, --mindays MIN_DAYS        set minimum number of days before password\n"
	                "                                change to MIN_DAYS\n"), usageout);
	(void) fputs (_("  -q, --quiet                   quiet mode\n"), usageout);
	(void) fputs (_("  -r, --repository REPOSITORY   change password in REPOSITORY repository\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -P, --prefix PREFIX_DIR       directory prefix\n"), usageout);
	(void) fputs (_("  -S, --status                  report password status on the named account\n"), usageout);
	(void) fputs (_("  -u, --unlock                  unlock the password of the named account\n"), usageout);
	(void) fputs (_("  -w, --warndays WARN_DAYS      set expiration warning days to WARN_DAYS\n"), usageout);
	(void) fputs (_("  -x, --maxdays MAX_DAYS        set maximum number of days before password\n"
	                "                                change to MAX_DAYS\n"), usageout);
	(void) fputs (_("  -s, --stdin                   read new token from stdin\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}


/*
 * new_password - validate old password and replace with new (both old and
 * new in global "char crypt_passwd[128]")
 */
static int new_password (const struct passwd *pw)
{
	char *clear;		/* Pointer to clear text */
	char *cipher;		/* Pointer to cipher text */
	const char *salt;	/* Pointer to new salt */
	char *cp;		/* Pointer to agetpass() response */
	char orig[PASS_MAX + 1];	/* Original password */
	char pass[PASS_MAX + 1];	/* New password */
	int i;			/* Counter for retries */
	int ret;
	bool warned;
	int pass_max_len = -1;
	const char *method;

	/*
	 * Authenticate the user. The user will be prompted for their own
	 * password.
	 */

	if (!amroot && ('\0' != crypt_passwd[0])) {
		clear = agetpass (_("Old password: "));
		if (NULL == clear) {
			return -1;
		}

		cipher = pw_encrypt (clear, crypt_passwd);

		if (NULL == cipher) {
			erase_pass (clear);
			fprintf (stderr,
			         _("%s: failed to crypt password with previous salt: %s\n"),
			         Prog, strerror (errno));
			SYSLOG ((LOG_INFO,
			         "Failed to crypt password with previous salt of user '%s'",
			         pw->pw_name));
			return -1;
		}

		if (strcmp (cipher, crypt_passwd) != 0) {
			erase_pass (clear);
			strzero (cipher);
			SYSLOG ((LOG_WARN, "incorrect password for %s",
			         pw->pw_name));
			(void) sleep (1);
			(void) fprintf (stderr,
			                _("Incorrect password for %s.\n"),
			                pw->pw_name);
			return -1;
		}
		STRTCPY(orig, clear);
		erase_pass (clear);
		strzero (cipher);
	} else {
		orig[0] = '\0';
	}

	/*
	 * Get the new password. The user is prompted for the new password
	 * and has five tries to get it right. The password will be tested
	 * for strength, unless it is the root user. This provides an escape
	 * for initial login passwords.
	 */
	method = getdef_str ("ENCRYPT_METHOD");
	if (NULL == method) {
		if (!getdef_bool ("MD5_CRYPT_ENAB")) {
			pass_max_len = getdef_num ("PASS_MAX_LEN", 8);
		}
	} else {
		if (   (strcmp (method, "MD5")    == 0)
#ifdef USE_SHA_CRYPT
		    || (strcmp (method, "SHA256") == 0)
		    || (strcmp (method, "SHA512") == 0)
#endif /* USE_SHA_CRYPT */
#ifdef USE_BCRYPT
		    || (strcmp (method, "BCRYPT") == 0)
#endif /* USE_BCRYPT*/
#ifdef USE_YESCRYPT
		    || (strcmp (method, "YESCRYPT") == 0)
#endif /* USE_YESCRYPT*/

		    ) {
			pass_max_len = -1;
		} else {
			pass_max_len = getdef_num ("PASS_MAX_LEN", 8);
		}
	}
	if (!qflg && !sflg) {
		if (pass_max_len == -1) {
			(void) printf (_(
"Enter the new password (minimum of %d characters)\n"
"Please use a combination of upper and lower case letters and numbers.\n"),
				getdef_num ("PASS_MIN_LEN", 5));
		} else {
			(void) printf (_(
"Enter the new password (minimum of %d, maximum of %d characters)\n"
"Please use a combination of upper and lower case letters and numbers.\n"),
				getdef_num ("PASS_MIN_LEN", 5), pass_max_len);
		}
	}

	if (sflg) {
		/*
		 * root is setting the passphrase from stdin
		 */
		cp = agetpass_stdin ();
		if (NULL == cp) {
			return -1;
		}
		ret = STRTCPY (pass, cp);
		erase_pass (cp);
		if (ret == -1) {
			(void) fputs (_("Password is too long.\n"), stderr);
			MEMZERO(pass);
			return -1;
		}
	} else {
		warned = false;
		for (i = getdef_num ("PASS_CHANGE_TRIES", 5); i > 0; i--) {
			cp = agetpass (_("New password: "));
			if (NULL == cp) {
				MEMZERO(orig);
				MEMZERO(pass);
				return -1;
			}
			if (warned && (strcmp (pass, cp) != 0)) {
				warned = false;
			}
			ret = STRTCPY (pass, cp);
			erase_pass (cp);
			if (ret == -1) {
				(void) fputs (_("Password is too long.\n"), stderr);
				MEMZERO(orig);
				MEMZERO(pass);
				return -1;
			}

			if (!amroot && !obscure(orig, pass, pw)) {
				(void) puts (_("Try again."));
				continue;
			}

			/*
			 * If enabled, warn about weak passwords even if you are
			 * root (enter this password again to use it anyway).
			 * --marekm
			 */
			if (amroot && !warned && getdef_bool ("PASS_ALWAYS_WARN")
				&& !obscure(orig, pass, pw)) {
				(void) puts (_("\nWarning: weak password (enter it again to use it anyway)."));
				warned = true;
				continue;
			}
			cp = agetpass (_("Re-enter new password: "));
			if (NULL == cp) {
				MEMZERO(orig);
				MEMZERO(pass);
				return -1;
			}
			if (strcmp (cp, pass) != 0) {
				erase_pass (cp);
				(void) fputs (_("They don't match; try again.\n"), stderr);
			} else {
				erase_pass (cp);
				break;
			}
		}
		MEMZERO(orig);

		if (i == 0) {
			MEMZERO(pass);
			return -1;
		}
	}


	/*
	 * Encrypt the password, then wipe the cleartext password.
	 */
	salt = crypt_make_salt (NULL, NULL);
	cp = pw_encrypt (pass, salt);
	MEMZERO(pass);

	if (NULL == cp) {
		fprintf (stderr,
		         _("%s: failed to crypt password with salt '%s': %s\n"),
		         Prog, salt, strerror (errno));
		return -1;
	}

	STRTCPY(crypt_passwd, cp);
	return 0;
}

/*
 * check_password - test a password to see if it can be changed
 *
 *	check_password() sees if the invoker has permission to change the
 *	password for the given user.
 */
static void check_password (const struct passwd *pw, const struct spwd *sp)
{
	int exp_status;

	exp_status = isexpired (pw, sp);

	/*
	 * If not expired and the "change only if expired" option (idea from
	 * PAM) was specified, do nothing. --marekm
	 */
	if (kflg && (0 == exp_status)) {
		exit (E_SUCCESS);
	}

	/*
	 * Root can change any password any time.
	 */
	if (amroot) {
		return;
	}

	/*
	 * Expired accounts cannot be changed ever. Passwords which are
	 * locked may not be changed. Passwords where min > max may not be
	 * changed. Passwords which have been inactive too long cannot be
	 * changed.
	 */
	if (   (sp->sp_pwdp[0] == '!')
	    || (exp_status > 1)
	    || (   (sp->sp_max >= 0)
	        && (sp->sp_min > sp->sp_max))) {
		(void) fprintf (stderr,
		                _("The password for %s cannot be changed.\n"),
		                sp->sp_namp);
		SYSLOG ((LOG_WARN, "password locked for '%s'", sp->sp_namp));
		closelog ();
		exit (E_NOPERM);
	}

	/*
	 * Passwords may only be changed after sp_min time is up.
	 */
	if (sp->sp_lstchg > 0) {
		long now, ok;
		now = time(NULL) / DAY;
		ok = sp->sp_lstchg;
		if (   (sp->sp_min > 0)
		    && __builtin_add_overflow(ok, sp->sp_min, &ok)) {
			ok = LONG_MAX;
		}

		if (now < ok) {
			(void) fprintf (stderr,
			                _("The password for %s cannot be changed yet.\n"),
			                pw->pw_name);
			SYSLOG ((LOG_WARN, "now < minimum age for '%s'", pw->pw_name));
			closelog ();
			exit (E_NOPERM);
		}
	}
}

static /*@observer@*/const char *pw_status (const char *pass)
{
	if (*pass == '*' || *pass == '!') {
		return "L";
	}
	if (*pass == '\0') {
		return "NP";
	}
	return "P";
}

/*
 * print_status - print current password status
 */
static void print_status (const struct passwd *pw)
{
	char         date[80];
	struct spwd *sp;

	sp = prefix_getspnam (pw->pw_name); /* local, no need for xprefix_getspnam */
	if (NULL != sp) {
		DAY_TO_STR(date, sp->sp_lstchg);
		(void) printf ("%s %s %s %ld %ld %ld %ld\n",
		               pw->pw_name,
		               pw_status (sp->sp_pwdp),
		               date,
		               sp->sp_min,
		               sp->sp_max,
		               sp->sp_warn,
		               sp->sp_inact);
	} else if (NULL != pw->pw_passwd) {
		(void) printf ("%s %s\n",
		               pw->pw_name, pw_status (pw->pw_passwd));
	} else {
		(void) fprintf(stderr, _("%s: malformed password data obtained for user %s\n"),
		               Prog, pw->pw_name);
	}
}


NORETURN
static void
fail_exit (int status)
{
	if (pw_locked) {
		if (pw_unlock () == 0) {
			(void) fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
			/* continue */
		}
	}

	if (spw_locked) {
		if (spw_unlock () == 0) {
			(void) fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
			/* continue */
		}
	}

	exit (status);
}

NORETURN
static void
oom (void)
{
	(void) fprintf (stderr, _("%s: out of memory\n"), Prog);
	fail_exit (E_FAILURE);
}

static char *update_crypt_pw (char *cp)
{
	if (!use_pam)
	{
		if (do_update_pwd) {
			cp = xstrdup (crypt_passwd);
		}
	}

	if (dflg) {
		*cp = '\0';
	}

	if (uflg && *cp == '!') {
		if (cp[1] == '\0') {
			(void) fprintf (stderr,
			                _("%s: unlocking the password would result in a passwordless account.\n"
			                  "You should set a password with usermod -p to unlock the password of this account.\n"),
			                Prog);
			fail_exit (E_FAILURE);
		} else {
			cp++;
		}
	}

	if (lflg && *cp != '!') {
		char *newpw = XMALLOC(strlen(cp) + 2, char);

		strcpy (newpw, "!");
		strcat (newpw, cp);
		if (!use_pam)
		{
			if (do_update_pwd) {
				free (cp);
			}
		}
		cp = newpw;
	}
	return cp;
}


static void update_noshadow (void)
{
	const struct passwd *pw;
	struct passwd *npw;

	if (pw_lock () == 0) {
		(void) fprintf (stderr,
		                _("%s: cannot lock %s; try again later.\n"),
		                Prog, pw_dbname ());
		exit (E_PWDBUSY);
	}
	pw_locked = true;
	if (pw_open (O_CREAT | O_RDWR) == 0) {
		(void) fprintf (stderr,
		                _("%s: cannot open %s\n"),
		                Prog, pw_dbname ());
		SYSLOG ((LOG_WARN, "cannot open %s", pw_dbname ()));
		fail_exit (E_MISSING);
	}
	pw = pw_locate (name);
	if (NULL == pw) {
		(void) fprintf (stderr,
		                _("%s: user '%s' does not exist in %s\n"),
		                Prog, name, pw_dbname ());
		fail_exit (E_NOPERM);
	}
	npw = __pw_dup (pw);
	if (NULL == npw) {
		oom ();
	}
	npw->pw_passwd = update_crypt_pw (npw->pw_passwd);
	if (pw_update (npw) == 0) {
		(void) fprintf (stderr,
		                _("%s: failed to prepare the new %s entry '%s'\n"),
		                Prog, pw_dbname (), npw->pw_name);
		fail_exit (E_FAILURE);
	}
	if (pw_close () == 0) {
		(void) fprintf (stderr,
		                _("%s: failure while writing changes to %s\n"),
		                Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (E_FAILURE);
	}
	if (pw_unlock () == 0) {
		(void) fprintf (stderr,
		                _("%s: failed to unlock %s\n"),
		                Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}
	pw_locked = false;
}

static void update_shadow (void)
{
	const struct spwd *sp;
	struct spwd *nsp;

	if (spw_lock () == 0) {
		(void) fprintf (stderr,
		                _("%s: cannot lock %s; try again later.\n"),
		                Prog, spw_dbname ());
		exit (E_PWDBUSY);
	}
	spw_locked = true;
	if (spw_open (O_CREAT | O_RDWR) == 0) {
		(void) fprintf (stderr,
		                _("%s: cannot open %s\n"),
		                Prog, spw_dbname ());
		SYSLOG ((LOG_WARN, "cannot open %s", spw_dbname ()));
		fail_exit (E_FAILURE);
	}
	sp = spw_locate (name);
	if (NULL == sp) {
		/* Try to update the password in /etc/passwd instead. */
		(void) spw_close ();
		update_noshadow ();
		if (spw_unlock () == 0) {
			(void) fprintf (stderr,
			                _("%s: failed to unlock %s\n"),
			                Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
			/* continue */
		}
		spw_locked = false;
		return;
	}
	nsp = __spw_dup (sp);
	if (NULL == nsp) {
		oom ();
	}
	nsp->sp_pwdp = update_crypt_pw (nsp->sp_pwdp);
	if (xflg) {
		nsp->sp_max = age_max;
	}
	if (nflg) {
		nsp->sp_min = age_min;
	}
	if (wflg) {
		nsp->sp_warn = warn;
	}
	if (iflg) {
		nsp->sp_inact = inact;
	}
	if (!use_pam)
	{
		if (do_update_age) {
			nsp->sp_lstchg = gettime () / DAY;
			if (0 == nsp->sp_lstchg) {
				/* Better disable aging than requiring a password
				 * change */
				nsp->sp_lstchg = -1;
			}
		}
	}

	/*
	 * Force change on next login, like SunOS 4.x passwd -e or Solaris
	 * 2.x passwd -f. Solaris 2.x seems to do the same thing (set
	 * sp_lstchg to 0).
	 */
	if (eflg) {
		nsp->sp_lstchg = 0;
	}

	if (spw_update (nsp) == 0) {
		(void) fprintf (stderr,
		                _("%s: failed to prepare the new %s entry '%s'\n"),
		                Prog, spw_dbname (), nsp->sp_namp);
		fail_exit (E_FAILURE);
	}
	if (spw_close () == 0) {
		(void) fprintf (stderr,
		                _("%s: failure while writing changes to %s\n"),
		                Prog, spw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", spw_dbname ()));
		fail_exit (E_FAILURE);
	}
	if (spw_unlock () == 0) {
		(void) fprintf (stderr,
		                _("%s: failed to unlock %s\n"),
		                Prog, spw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
		/* continue */
	}
	spw_locked = false;
}

/*
 * passwd - change a user's password file information
 *
 *	This command controls the password file and commands which are used
 * 	to modify it.
 *
 *	The valid options are
 *
 *	-d	delete the password for the named account (*)
 *	-e	expire the password for the named account (*)
 *	-i #	set sp_inact to # days (*)
 *	-k	change password only if expired
 *	-l	lock the password of the named account (*)
 *	-n #	set sp_min to # days (*)
 *	-r #	change password in # repository
 *	-S	show password status of named account
 *	-u	unlock the password of the named account (*)
 *	-w #	set sp_warn to # days (*)
 *	-x #	set sp_max to # days (*)
 *	-s	read password from stdin (*)
 *
 *	(*) requires root permission to execute.
 *
 *	All of the time fields are entered in days and converted to the
 * 	appropriate internal format. For finer resolute the chage
 *	command must be used.
 */
int main (int argc, char **argv)
{
	const struct passwd *pw;	/* Password file entry for user      */

	char *cp;		/* Miscellaneous character pointing  */

	const struct spwd *sp;	/* Shadow file entry for user   */

	sanitize_env ();
	check_fds ();

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);
	prefix = process_prefix_flag ("-P", argc, argv);

	if (prefix[0]) {
		use_pam = false;
		do_update_age = true;
	}

	/*
	 * The program behaves differently when executed by root than when
	 * executed by a normal user.
	 */
	amroot = (getuid () == 0);

	OPENLOG (Prog);

	{
		/*
		 * Parse the command line options.
		 */
		int c;
		static struct option long_options[] = {
			{"all",         no_argument,       NULL, 'a'},
			{"delete",      no_argument,       NULL, 'd'},
			{"expire",      no_argument,       NULL, 'e'},
			{"help",        no_argument,       NULL, 'h'},
			{"inactive",    required_argument, NULL, 'i'},
			{"keep-tokens", no_argument,       NULL, 'k'},
			{"lock",        no_argument,       NULL, 'l'},
			{"mindays",     required_argument, NULL, 'n'},
			{"quiet",       no_argument,       NULL, 'q'},
			{"repository",  required_argument, NULL, 'r'},
			{"root",        required_argument, NULL, 'R'},
			{"prefix",      required_argument, NULL, 'P'},
			{"status",      no_argument,       NULL, 'S'},
			{"unlock",      no_argument,       NULL, 'u'},
			{"warndays",    required_argument, NULL, 'w'},
			{"maxdays",     required_argument, NULL, 'x'},
			{"stdin",       no_argument,       NULL, 's'},
			{NULL, 0, NULL, '\0'}
		};

		while ((c = getopt_long (argc, argv, "adehi:kln:qr:R:P:Suw:x:s",
		                         long_options, NULL)) != -1) {
			switch (c) {
			case 'a':
				aflg = true;
				break;
			case 'd':
				dflg = true;
				anyflag = true;
				break;
			case 'e':
				eflg = true;
				anyflag = true;
				break;
			case 'h':
				usage (E_SUCCESS);
				/*@notreached@*/break;
			case 'i':
				if (   (str2sl(&inact, optarg) == -1)
				    || (inact < -1)) {
					fprintf (stderr,
					         _("%s: invalid numeric argument '%s'\n"),
					         Prog, optarg);
					usage (E_BAD_ARG);
				}
				iflg = true;
				anyflag = true;
				break;
			case 'k':
				/* change only if expired, like Linux-PAM passwd -k. */
				kflg = true;	/* ok for users */
				break;
			case 'l':
				lflg = true;
				anyflag = true;
				break;
			case 'n':
				if (   (str2sl(&age_min, optarg) == -1)
				    || (age_min < -1)) {
					fprintf (stderr,
					         _("%s: invalid numeric argument '%s'\n"),
					         Prog, optarg);
					usage (E_BAD_ARG);
				}
				nflg = true;
				anyflag = true;
				break;
			case 'q':
				qflg = true;	/* ok for users */
				break;
			case 'r':
				/* -r repository (files|nis|nisplus) */
				/* only "files" supported for now */
				if (strcmp (optarg, "files") != 0) {
					fprintf (stderr,
					         _("%s: repository %s not supported\n"),
						 Prog, optarg);
					exit (E_BAD_ARG);
				}
				break;
			case 'R': /* no-op, handled in process_root_flag () */
				break;
			case 'P': /* no-op, handled in process_prefix_flag () */
				break;
			case 'S':
				Sflg = true;	/* ok for users */
				break;
			case 'u':
				uflg = true;
				anyflag = true;
				break;
			case 'w':
				if (   (str2sl(&warn, optarg) == -1)
				    || (warn < -1)) {
					(void) fprintf (stderr,
					                _("%s: invalid numeric argument '%s'\n"),
					                Prog, optarg);
					usage (E_BAD_ARG);
				}
				wflg = true;
				anyflag = true;
				break;
			case 'x':
				if (   (str2sl(&age_max, optarg) == -1)
				    || (age_max < -1)) {
					(void) fprintf (stderr,
					                _("%s: invalid numeric argument '%s'\n"),
					                Prog, optarg);
					usage (E_BAD_ARG);
				}
				xflg = true;
				anyflag = true;
				break;
			case 's':
				if (!amroot) {
					(void) fprintf (stderr,
					                _("%s: only root can use --stdin/-s option\n"),
					                Prog);
					usage (E_BAD_ARG);
				}
				sflg = true;
				break;
			default:
				usage (E_BAD_ARG);
			}
		}
	}

	/*
	 * Now I have to get the user name. The name will be gotten from the
	 * command line if possible. Otherwise it is figured out from the
	 * environment.
	 */
	pw = get_my_pwent ();
	if (NULL == pw) {
		(void) fprintf (stderr,
		                _("%s: Cannot determine your user name.\n"),
		                Prog);
		SYSLOG ((LOG_WARN, "Cannot determine the user name of the caller (UID %lu)",
		         (unsigned long) getuid ()));
		exit (E_NOPERM);
	}
	myname = xstrdup (pw->pw_name);
	if (optind < argc) {
		name = argv[optind];
	} else {
		name = myname;
	}

	/*
	 * Make sure that at most one username was specified.
	 */
	if (argc > (optind+1)) {
		usage (E_USAGE);
	}

	/*
	 * The -a flag requires -S, no other flags, no username, and
	 * you must be root.  --marekm
	 */
	if (aflg) {
		if (anyflag || !Sflg || (optind < argc)) {
			usage (E_USAGE);
		}
		if (!amroot) {
			(void) fprintf (stderr,
			                _("%s: Permission denied.\n"),
			                Prog);
			exit (E_NOPERM);
		}
		prefix_setpwent ();
		while ( (pw = prefix_getpwent ()) != NULL ) {
			print_status (pw);
		}
		prefix_endpwent ();
		exit (E_SUCCESS);
	}
#if 0
	/*
	 * Allow certain users (administrators) to change passwords of
	 * certain users. Not implemented yet. --marekm
	 */
	if (may_change_passwd (myname, name))
		amroot = 1;
#endif

	/*
	 * If any of the flags were given, a user name must be supplied on
	 * the command line. Only an unadorned command line doesn't require
	 * the user's name be given. Also, -x, -n, -w, -i, -e, -d,
	 * -l, -u may appear with each other. -S, -k must appear alone.
	 */

	/*
	 * -S now ok for normal users (check status of my own account), and
	 * doesn't require username.  --marekm
	 */
	if (anyflag && optind >= argc) {
		usage (E_USAGE);
	}

	if (   (Sflg && kflg)
	    || (anyflag && (Sflg || kflg))) {
		usage (E_USAGE);
	}

	if (anyflag && !amroot) {
		(void) fprintf (stderr, _("%s: Permission denied.\n"), Prog);
		exit (E_NOPERM);
	}

	pw = xprefix_getpwnam (name);
	if (NULL == pw) {
		(void) fprintf (stderr,
		                _("%s: user '%s' does not exist\n"),
		                Prog, name);
		exit (E_NOPERM);
	}
#ifdef WITH_SELINUX
	/* only do this check when getuid()==0 because it's a pre-condition for
	   changing a password without entering the old one */
	if (amroot && (check_selinux_permit (Prog) != 0)) {
		SYSLOG ((LOG_ALERT,
		         "root is not authorized by SELinux to change the password of %s",
		         name));
		(void) fprintf(stderr,
		               _("%s: root is not authorized by SELinux to change the password of %s\n"),
		               Prog, name);
		exit (E_NOPERM);
	}
#endif				/* WITH_SELINUX */

	/*
	 * If the UID of the user does not match the current real UID,
	 * check if I'm root.
	 */
	if (!amroot && (pw->pw_uid != getuid ())) {
		(void) fprintf (stderr,
		                _("%s: You may not view or modify password information for %s.\n"),
		                Prog, name);
		SYSLOG ((LOG_WARN,
		         "can't view or modify password information for %s",
		         name));
		closelog ();
		exit (E_NOPERM);
	}

	if (Sflg) {
		print_status (pw);
		exit (E_SUCCESS);
	}
	if (!use_pam)
	{
		/*
		 * The user name is valid, so let's get the shadow file entry.
		 */
		sp = prefix_getspnam (name); /* !use_pam, no need for xprefix_getspnam */
		if (NULL == sp) {
			if (errno == EACCES) {
				(void) fprintf (stderr,
				                _("%s: Permission denied.\n"),
				                Prog);
				exit (E_NOPERM);
			}
			sp = pwd_to_spwd (pw);
		}

		cp = sp->sp_pwdp;

		/*
		 * If there are no other flags, just change the password.
		 */
		if (!anyflag) {
			STRTCPY(crypt_passwd, cp);

			/*
			 * See if the user is permitted to change the password.
			 * Otherwise, go ahead and set a new password.
			 */
			check_password (pw, sp);

			/*
			 * Let the user know whose password is being changed.
			 */
			if (!qflg) {
				(void) printf (_("Changing password for %s\n"), name);
			}

			if (new_password (pw) != 0) {
				(void) fprintf (stderr,
				                _("The password for %s is unchanged.\n"),
				                name);
				closelog ();
				exit (E_NOPERM);
			}
			do_update_pwd = true;
			do_update_age = true;
		}
	}

	/*
	 * Before going any further, raise the ulimit to prevent colliding
	 * into a lowered ulimit, and set the real UID to root to protect
	 * against unexpected signals. Any keyboard signals are set to be
	 * ignored.
	 */
	pwd_init ();

#ifdef USE_PAM
	/*
	 * Don't set the real UID for PAM...
	 */
	if (!anyflag && use_pam) {
		if (sflg) {
			cp = agetpass_stdin ();
			if (cp == NULL) {
				exit (E_FAILURE);
			}
			do_pam_passwd_non_interactive ("passwd", name, cp);
			erase_pass (cp);
		} else {
			do_pam_passwd (name, qflg, kflg);
		}
		exit (E_SUCCESS);
	}
#endif				/* USE_PAM */
	if (setuid (0) != 0) {
		(void) fputs (_("Cannot change ID to root.\n"), stderr);
		SYSLOG ((LOG_ERR, "can't setuid(0)"));
		closelog ();
		exit (E_NOPERM);
	}
	if (spw_file_present ()) {
		update_shadow ();
	} else {
		update_noshadow ();
	}

	nscd_flush_cache ("passwd");
	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_PASSWD | SSSD_DB_GROUP);

	SYSLOG ((LOG_INFO, "password for '%s' changed by '%s'", name, myname));
	closelog ();
	if (!qflg) {
		if (!anyflag) {
#ifndef USE_PAM
			(void) printf (_("%s: password changed.\n"), Prog);
#endif				/* USE_PAM */
		} else {
			(void) printf (_("%s: password changed.\n"), Prog);
		}
	}

	return E_SUCCESS;
}
