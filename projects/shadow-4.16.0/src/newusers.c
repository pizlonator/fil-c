/*
 * SPDX-FileCopyrightText: 1990 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2000 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 *	newusers - create users from a batch file
 *
 *	newusers creates a collection of entries in /etc/passwd
 *	and related files by reading a passwd-format file and
 *	adding entries in the related directories.
 */

#include <config.h>

#ident "$Id$"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "alloc.h"
#include "atoi/str2i.h"
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
#include "prototypes.h"
#include "defines.h"
#include "getdef.h"
#include "groupio.h"
#include "nscd.h"
#include "sssd.h"
#include "pwio.h"
#include "sgroupio.h"
#include "shadowio.h"
#ifdef ENABLE_SUBIDS
#include "subordinateio.h"
#endif				/* ENABLE_SUBIDS */
#include "chkname.h"
#include "shadowlog.h"
#include "string/sprintf.h"


/*
 * Global variables
 */
static const char Prog[] = "newusers";

static bool rflg = false;	/* create a system account */
#ifndef USE_PAM
static /*@null@*//*@observer@*/char *crypt_method = NULL;
#define cflg (NULL != crypt_method)
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
static bool sflg = false;
#endif
#ifdef USE_SHA_CRYPT
static long sha_rounds = 5000;
#endif				/* USE_SHA_CRYPT */
#ifdef USE_BCRYPT
static long bcrypt_rounds = 13;
#endif				/* USE_BCRYPT */
#ifdef USE_YESCRYPT
static long yescrypt_cost = 5;
#endif				/* USE_YESCRYPT */
#endif				/* !USE_PAM */

static bool is_shadow;
#ifdef SHADOWGRP
static bool is_shadow_grp;
static bool sgr_locked = false;
#endif
static bool pw_locked = false;
static bool gr_locked = false;
static bool spw_locked = false;
#ifdef ENABLE_SUBIDS
static bool is_sub_uid = false;
static bool is_sub_gid = false;
static bool sub_uid_locked = false;
static bool sub_gid_locked = false;
#endif				/* ENABLE_SUBIDS */

/* local function prototypes */
NORETURN static void usage (int status);
NORETURN static void fail_exit (int);
static int add_group (const char *, const char *, gid_t *, gid_t);
static int get_user_id (const char *, uid_t *);
static int add_user (const char *, uid_t, gid_t);
#ifndef USE_PAM
static int update_passwd (struct passwd *, const char *);
#endif				/* !USE_PAM */
static int add_passwd (struct passwd *, const char *);
static void process_flags (int argc, char **argv);
static void check_flags (void);
static void check_perms (void);
static void open_files (void);
static void close_files (void);

extern int allow_bad_names;

/*
 * usage - display usage message and exit
 */
static void usage (int status)
{
	FILE *usageout = (EXIT_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options]\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -b, --badname                 allow bad names\n"), usageout);
#ifndef USE_PAM
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
#endif				/* !USE_PAM */
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -r, --system                  create system accounts\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
#ifndef USE_PAM
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	(void) fputs (_("  -s, --sha-rounds              number of rounds for the SHA, BCRYPT\n"
	                "                                or YESCRYPT crypt algorithms\n"),
	              usageout);
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
#endif				/* !USE_PAM */
	(void) fputs ("\n", usageout);

	exit (status);
}

/*
 * fail_exit - undo as much as possible
 */
static void fail_exit (int code)
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
#ifdef ENABLE_SUBIDS
	if (sub_uid_locked) {
		if (sub_uid_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sub_uid_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sub_uid_dbname ()));
			/* continue */
		}
	}
	if (sub_gid_locked) {
		if (sub_gid_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sub_gid_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sub_gid_dbname ()));
			/* continue */
		}
	}
#endif				/* ENABLE_SUBIDS */

	exit (code);
}

/*
 * add_group - create a new group or add a user to an existing group
 */
static int add_group (const char *name, const char *gid, gid_t *ngid, uid_t uid)
{
	const struct group *grp;
	struct group grent;
	char *members[1];
#ifdef SHADOWGRP
	const struct sgrp *sg;
#endif

	/*
	 * Start by seeing if the named group already exists. This will be
	 * very easy to deal with if it does.
	 */
	grp = getgrnam (gid);
	if (NULL == grp) {
		grp = gr_locate (gid);
	}
	if (NULL != grp) {
		/* The user will use this ID for her primary group */
		*ngid = grp->gr_gid;
		/* Don't check gshadow */
		return 0;
	}

	if (isdigit (gid[0])) {
		/*
		 * The GID is a number, which means either this is a brand
		 * new group, or an existing group.
		 */

		if (get_gid(gid, &grent.gr_gid) == -1) {
			fprintf (stderr,
			         _("%s: invalid group ID '%s'\n"),
			         Prog, gid);
			return -1;
		}

		/* Look in both the system database (getgrgid) and in the
		 * internal database (gr_locate_gid), which may contain
		 * uncommitted changes */
		if (   (getgrgid (grent.gr_gid) != NULL)
		    || (gr_locate_gid (grent.gr_gid) != NULL)) {
			/* The user will use this ID for her
			 * primary group */
			*ngid = grent.gr_gid;
			return 0;
		}

		/* Do not create groups with GID == (gid_t)-1 */
		if (grent.gr_gid == (gid_t)-1) {
			fprintf (stderr,
			         _("%s: invalid group ID '%s'\n"),
			         Prog, gid);
			return -1;
		}
	} else {
		/* The gid parameter can be "" or a name which is not
		 * already the name of an existing group.
		 * In both cases, figure out what group ID can be used.
		 */
		if (find_new_gid(rflg, &grent.gr_gid, &uid) < 0) {
			return -1;
		}
	}

	/*
	 * Now I have all of the fields required to create the new group.
	 */
	if (('\0' != gid[0]) && (!isdigit (gid[0]))) {
		grent.gr_name = xstrdup (gid);
	} else {
		grent.gr_name = xstrdup (name);
/* FIXME: check if the group exists */
	}

	/* Check if this is a valid group name */
	if (!is_valid_group_name (grent.gr_name)) {
		fprintf (stderr,
		         _("%s: invalid group name '%s'\n"),
		         Prog, grent.gr_name);
		free (grent.gr_name);
		return -1;
	}

	grent.gr_passwd = "*";	/* XXX warning: const */
	members[0] = NULL;
	grent.gr_mem = members;

	*ngid = grent.gr_gid;

#ifdef SHADOWGRP
	if (is_shadow_grp) {
		sg = sgr_locate (grent.gr_name);

		if (NULL != sg) {
			fprintf (stderr,
			         _("%s: group '%s' is a shadow group, but does not exist in /etc/group\n"),
			         Prog, grent.gr_name);
			return -1;
		}
	}
#endif

#ifdef SHADOWGRP
	if (is_shadow_grp) {
		struct sgrp sgrent;
		char *admins[1];
		sgrent.sg_name = grent.gr_name;
		sgrent.sg_passwd = "*";	/* XXX warning: const */
		grent.gr_passwd  = "x";	/* XXX warning: const */
		admins[0] = NULL;
		sgrent.sg_adm = admins;
		sgrent.sg_mem = members;

		if (sgr_update (&sgrent) == 0) {
			return -1;
		}
	}
#endif

	if (gr_update (&grent) == 0) {
		return -1;
	}

	return 0;
}

static int get_user_id (const char *uid, uid_t *nuid) {

	/*
	 * The first guess for the UID is either the numerical UID that the
	 * caller provided, or the next available UID.
	 */
	if (isdigit (uid[0])) {
		if ((get_uid(uid, nuid) == -1) || (*nuid == (uid_t)-1)) {
			fprintf (stderr,
			         _("%s: invalid user ID '%s'\n"),
			         Prog, uid);
			return -1;
		}
	} else {
		if ('\0' != uid[0]) {
			const struct passwd *pwd;
			/* local, no need for xgetpwnam */
			pwd = getpwnam (uid);
			if (pwd == NULL)
				pwd = pw_locate (uid);

			if (pwd == NULL) {
				fprintf (stderr,
				         _("%s: user '%s' does not exist\n"),
				         Prog, uid);
				return -1;
			}

			*nuid = pwd->pw_uid;
		} else {
			if (find_new_uid (rflg, nuid, NULL) < 0)
				return -1;
		}
	}

	return 0;
}

/*
 * add_user - create a new user ID
 */
static int add_user (const char *name, uid_t uid, gid_t gid)
{
	struct passwd pwent;

	/* Check if this is a valid user name */
	if (!is_valid_user_name (name)) {
		fprintf (stderr,
		         _("%s: invalid user name '%s': use --badname to ignore\n"),
		         Prog, name);
		return -1;
	}

	/*
	 * I don't want to fill in the entire password structure members
	 * JUST YET, since there is still more data to be added. So, I fill
	 * in the parts that I have.
	 */
	pwent.pw_name = xstrdup (name);
	pwent.pw_uid = uid;
	pwent.pw_passwd = "x";	/* XXX warning: const */
	pwent.pw_gid = gid;
	pwent.pw_gecos = "";	/* XXX warning: const */
	pwent.pw_dir = "";	/* XXX warning: const */
	pwent.pw_shell = "";	/* XXX warning: const */

	return (pw_update (&pwent) == 0) ? -1 : 0;
}

#ifndef USE_PAM
/*
 * update_passwd - update the password in the passwd entry
 *
 * Return 0 if successful.
 */
static int update_passwd (struct passwd *pwd, const char *password)
{
	void *crypt_arg = NULL;
	char *cp;
	if (NULL != crypt_method) {
#if defined(USE_SHA_CRYPT)
		if (sflg) {
			if (   (0 == strcmp (crypt_method, "SHA256"))
				|| (0 == strcmp (crypt_method, "SHA512"))) {
				crypt_arg = &sha_rounds;
			}
		}
#endif				/* USE_SHA_CRYPT */
#if defined(USE_BCRYPT)
		if (sflg) {
			if (0 == strcmp (crypt_method, "BCRYPT")) {
				crypt_arg = &bcrypt_rounds;
			}
		}
#endif				/* USE_BCRYPT */
#if defined(USE_YESCRYPT)
		if (sflg) {
			if (0 == strcmp (crypt_method, "YESCRYPT")) {
				crypt_arg = &yescrypt_cost;
			}
		}
#endif				/* USE_YESCRYPT */
	}

	if ((NULL != crypt_method) && (0 == strcmp(crypt_method, "NONE"))) {
		pwd->pw_passwd = (char *)password;
	} else {
		const char *salt = crypt_make_salt (crypt_method, crypt_arg);
		cp = pw_encrypt (password, salt);
		if (NULL == cp) {
			fprintf (stderr,
			         _("%s: failed to crypt password with salt '%s': %s\n"),
			         Prog, salt, strerror (errno));
			return 1;
		}
		pwd->pw_passwd = cp;
	}

	return 0;
}
#endif				/* !USE_PAM */

/*
 * add_passwd - add or update the encrypted password
 */
static int add_passwd (struct passwd *pwd, const char *password)
{
	const struct spwd *sp;
	struct spwd spent;
#ifndef USE_PAM
	char *cp;
#endif				/* !USE_PAM */

#ifndef USE_PAM
	void *crypt_arg = NULL;
	if (NULL != crypt_method) {
#if defined(USE_SHA_CRYPT)
		if (sflg) {
			if (   (0 == strcmp (crypt_method, "SHA256"))
				|| (0 == strcmp (crypt_method, "SHA512"))) {
				crypt_arg = &sha_rounds;
			}
		}
#endif				/* USE_SHA_CRYPT */
#if defined(USE_BCRYPT)
		if (sflg) {
			if (0 == strcmp (crypt_method, "BCRYPT")) {
				crypt_arg = &bcrypt_rounds;
			}
		}
#endif				/* USE_BCRYPT */
#if defined(USE_YESCRYPT)
		if (sflg) {
			if (0 == strcmp (crypt_method, "YESCRYPT")) {
				crypt_arg = &yescrypt_cost;
			}
		}
#endif				/* USE_PAM */
	}

	/*
	 * In the case of regular password files, this is real easy - pwd
	 * points to the entry in the password file. Shadow files are
	 * harder since there are zillions of things to do ...
	 */
	if (!is_shadow) {
		return update_passwd (pwd, password);
	}
#endif				/* USE_PAM */

	/*
	 * Do the first and easiest shadow file case. The user already
	 * exists in the shadow password file.
	 */
	sp = spw_locate (pwd->pw_name);
#ifndef USE_PAM
	if (NULL != sp) {
		spent = *sp;
		if (   (NULL != crypt_method)
		    && (0 == strcmp(crypt_method, "NONE"))) {
			spent.sp_pwdp = (char *)password;
		} else {
			const char *salt = crypt_make_salt (crypt_method,
			                                    crypt_arg);
			cp = pw_encrypt (password, salt);
			if (NULL == cp) {
				fprintf (stderr,
				         _("%s: failed to crypt password with salt '%s': %s\n"),
				         Prog, salt, strerror (errno));
				return 1;
			}
			spent.sp_pwdp = cp;
		}
		spent.sp_lstchg = gettime () / DAY;
		if (0 == spent.sp_lstchg) {
			/* Better disable aging than requiring a password
			 * change */
			spent.sp_lstchg = -1;
		}
		return (spw_update (&spent) == 0);
	}

	/*
	 * Pick the next easiest case - the user has an encrypted password
	 * which isn't equal to "x". The password was set to "x" earlier
	 * when the entry was created, so this user would have to have had
	 * the password set someplace else.
	 */
	if (strcmp (pwd->pw_passwd, "x") != 0) {
		return update_passwd (pwd, password);
	}
#else				/* USE_PAM */
	/*
	 * If there is already a shadow entry, do not touch it.
	 * If there is already a passwd entry with a password, do not
	 * touch it.
	 * The password will be updated later for all users using PAM.
	 */
	if (   (NULL != sp)
	    || (strcmp (pwd->pw_passwd, "x") != 0)) {
		return 0;
	}
#endif				/* USE_PAM */

	/*
	 * Now the really hard case - I need to create an entirely new
	 * shadow password file entry.
	 */
	spent.sp_namp = pwd->pw_name;
#ifndef USE_PAM
	if ((crypt_method != NULL) && (0 == strcmp(crypt_method, "NONE"))) {
		spent.sp_pwdp = (char *)password;
	} else {
		const char *salt = crypt_make_salt (crypt_method, crypt_arg);
		cp = pw_encrypt (password, salt);
		if (NULL == cp) {
			fprintf (stderr,
			         _("%s: failed to crypt password with salt '%s': %s\n"),
			         Prog, salt, strerror (errno));
			return 1;
		}
		spent.sp_pwdp = cp;
	}
#else
	/*
	 * Lock the password.
	 * The password will be updated later for all users using PAM.
	 */
	spent.sp_pwdp = "!";
#endif
	spent.sp_lstchg = gettime () / DAY;
	if (0 == spent.sp_lstchg) {
		/* Better disable aging than requiring a password change */
		spent.sp_lstchg = -1;
	}
	spent.sp_min    = getdef_num ("PASS_MIN_DAYS", 0);
	/* 10000 is infinity this week */
	spent.sp_max    = getdef_num ("PASS_MAX_DAYS", 10000);
	spent.sp_warn   = getdef_num ("PASS_WARN_AGE", -1);
	spent.sp_inact  = -1;
	spent.sp_expire = -1;
	spent.sp_flag   = SHADOW_SP_FLAG_UNSET;

	return (spw_update (&spent) == 0);
}

/*
 * process_flags - parse the command line options
 *
 *	It will not return if an error is encountered.
 */
static void process_flags (int argc, char **argv)
{
	int c;
#ifndef USE_PAM
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
        int bad_s;
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
#endif 				/* !USE_PAM */
	static struct option long_options[] = {
		{"badname",      no_argument,       NULL, 'b'},
#ifndef USE_PAM
		{"crypt-method", required_argument, NULL, 'c'},
#endif				/* !USE_PAM */
		{"help",         no_argument,       NULL, 'h'},
		{"system",       no_argument,       NULL, 'r'},
		{"root",         required_argument, NULL, 'R'},
#ifndef USE_PAM
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
		{"sha-rounds",   required_argument, NULL, 's'},
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
#endif				/* !USE_PAM */
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv,
#ifndef USE_PAM
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	                         "c:bhrs:",
#else				/* !USE_SHA_CRYPT && !USE_BCRYPT && !USE_YESCRYPT */
	                         "c:bhr",
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
#else				/* USE_PAM */
	                         "bhr",
#endif
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'b':
			allow_bad_names = true;
			break;
#ifndef USE_PAM
		case 'c':
			crypt_method = optarg;
			break;
#endif				/* !USE_PAM */
		case 'h':
			usage (EXIT_SUCCESS);
			break;
		case 'r':
			rflg = true;
			break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
#ifndef USE_PAM
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
		case 's':
			sflg = true;
                        bad_s = 0;

			if (!crypt_method){
				fprintf(stderr,
						_("%s: Provide '--crypt-method' before number of rounds\n"),
						Prog);
				usage (EXIT_FAILURE);
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
				usage (EXIT_FAILURE);
			}
			break;
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */
#endif				/* !USE_PAM */
		default:
			usage (EXIT_FAILURE);
			break;
		}
	}

	if (   (optind != argc)
	    && (optind + 1 != argc)) {
		usage (EXIT_FAILURE);
	}

	if (argv[optind] != NULL) {
		if (freopen (argv[optind], "r", stdin) == NULL) {
			char  buf[BUFSIZ];

			SNPRINTF(buf, "%s: %s", Prog, argv[1]);
			perror (buf);
			fail_exit (EXIT_FAILURE);
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
#ifndef USE_PAM
#if defined(USE_SHA_CRYPT) || defined(USE_BCRYPT) || defined(USE_YESCRYPT)
	if (sflg && !cflg) {
		fprintf (stderr,
		         _("%s: %s flag is only allowed with the %s flag\n"),
		         Prog, "-s", "-c");
		usage (EXIT_FAILURE);
	}
#endif				/* USE_SHA_CRYPT || USE_BCRYPT || USE_YESCRYPT */

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
			usage (EXIT_FAILURE);
		}
	}
#endif				/* !USE_PAM */
}

/*
 * check_perms - check if the caller is allowed to add a group
 *
 *	With PAM support, the setuid bit can be set on groupadd to allow
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
		fail_exit (EXIT_FAILURE);
	}

	retval = pam_start ("newusers", pampw->pw_name, &conv, &pamh);

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
		fail_exit (EXIT_FAILURE);
	}
	(void) pam_end (pamh, retval);
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
}

/*
 * open_files - lock and open the password, group and shadow databases
 */
static void open_files (void)
{
	/*
	 * Lock the password files and open them for update. This will bring
	 * all of the entries into memory where they may be searched for an
	 * modified, or new entries added. The password file is the key - if
	 * it gets locked, assume the others can be locked right away.
	 */
	if (pw_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, pw_dbname ());
		fail_exit (EXIT_FAILURE);
	}
	pw_locked = true;
	if (is_shadow) {
		if (spw_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, spw_dbname ());
			fail_exit (EXIT_FAILURE);
		}
		spw_locked = true;
	}
	if (gr_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, gr_dbname ());
		fail_exit (EXIT_FAILURE);
	}
	gr_locked = true;
#ifdef SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sgr_dbname ());
			fail_exit (EXIT_FAILURE);
		}
		sgr_locked = true;
	}
#endif
#ifdef ENABLE_SUBIDS
	if (is_sub_uid) {
		if (sub_uid_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sub_uid_dbname ());
			fail_exit (EXIT_FAILURE);
		}
		sub_uid_locked = true;
	}
	if (is_sub_gid) {
		if (sub_gid_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sub_gid_dbname ());
			fail_exit (EXIT_FAILURE);
		}
		sub_gid_locked = true;
	}
#endif				/* ENABLE_SUBIDS */

	if (pw_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, pw_dbname ());
		fail_exit (EXIT_FAILURE);
	}
	if (is_shadow && (spw_open (O_CREAT | O_RDWR) == 0)) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, spw_dbname ());
		fail_exit (EXIT_FAILURE);
	}
	if (gr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, gr_dbname ());
		fail_exit (EXIT_FAILURE);
	}
#ifdef SHADOWGRP
	if (is_shadow_grp && (sgr_open (O_CREAT | O_RDWR) == 0)) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, sgr_dbname ());
		fail_exit (EXIT_FAILURE);
	}
#endif
#ifdef ENABLE_SUBIDS
	if (is_sub_uid) {
		if (sub_uid_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sub_uid_dbname ());
			fail_exit (EXIT_FAILURE);
		}
	}
	if (is_sub_gid) {
		if (sub_gid_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sub_gid_dbname ());
			fail_exit (EXIT_FAILURE);
		}
	}
#endif				/* ENABLE_SUBIDS */
}

/*
 * close_files - close and unlock the password, group and shadow databases
 */
static void close_files (void)
{
	if (pw_close () == 0) {
		fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (EXIT_FAILURE);
	}
	if (pw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}
	pw_locked = false;

	if (is_shadow) {
		if (spw_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", spw_dbname ()));
			fail_exit (EXIT_FAILURE);
		}
		if (spw_unlock () == 0) {
			fprintf (stderr,
			         _("%s: failed to unlock %s\n"),
			         Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
			/* continue */
		}
		spw_locked = false;
	}

	if (gr_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", gr_dbname ()));
		fail_exit (EXIT_FAILURE);
	}
#ifdef ENABLE_SUBIDS
	if (is_sub_uid  && (sub_uid_close () == 0)) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"), Prog, sub_uid_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", sub_uid_dbname ()));
		fail_exit (EXIT_FAILURE);
	}
	if (is_sub_gid  && (sub_gid_close () == 0)) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"), Prog, sub_gid_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", sub_gid_dbname ()));
		fail_exit (EXIT_FAILURE);
	}
#endif				/* ENABLE_SUBIDS */

	if (gr_unlock () == 0) {
		fprintf (stderr,
		         _("%s: failed to unlock %s\n"),
		         Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
		/* continue */
	}
	gr_locked = false;

#ifdef SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", sgr_dbname ()));
			fail_exit (EXIT_FAILURE);
		}
		if (sgr_unlock () == 0) {
			fprintf (stderr,
			         _("%s: failed to unlock %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
			/* continue */
		}
		sgr_locked = false;
	}
#endif
#ifdef ENABLE_SUBIDS
	if (is_sub_uid) {
		if (sub_uid_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sub_uid_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sub_uid_dbname ()));
			/* continue */
		}
		sub_uid_locked = false;
	}
	if (is_sub_gid) {
		if (sub_gid_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sub_gid_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sub_gid_dbname ()));
			/* continue */
		}
		sub_gid_locked = false;
	}
#endif				/* ENABLE_SUBIDS */
}

static bool want_subuids(void)
{
	if (get_subid_nss_handle() != NULL)
		return false;
	if (getdef_ulong ("SUB_UID_COUNT", 65536) == 0)
		return false;
	return true;
}

static bool want_subgids(void)
{
	if (get_subid_nss_handle() != NULL)
		return false;
	if (getdef_ulong ("SUB_GID_COUNT", 65536) == 0)
		return false;
	return true;
}

int main (int argc, char **argv)
{
	char buf[BUFSIZ];
	char *fields[8];
	int nfields;
	char *cp;
	const struct passwd *pw;
	struct passwd newpw;
	int line = 0;
	uid_t uid;
	gid_t gid;
#ifdef USE_PAM
	int *lines = NULL;
	char **usernames = NULL;
	char **passwords = NULL;
	unsigned int nusers = 0;
#endif				/* USE_PAM */

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	/* FIXME: will not work with an input file */
	process_root_flag ("-R", argc, argv);

	OPENLOG ("newusers");

	process_flags (argc, argv);

	check_perms ();

	is_shadow = spw_file_present ();

#ifdef SHADOWGRP
	is_shadow_grp = sgr_file_present ();
#endif
#ifdef ENABLE_SUBIDS
	is_sub_uid = sub_uid_file_present () && !rflg;
	is_sub_gid = sub_gid_file_present () && !rflg;
#endif				/* ENABLE_SUBIDS */

	open_files ();

	/*
	 * Read each line. The line has the same format as a password file
	 * entry, except that certain fields are not constrained to be
	 * numerical values. If a group ID is entered which does not already
	 * exist, an attempt is made to allocate the same group ID as the
	 * numerical user ID. Should that fail, the next available group ID
	 * over 100 is allocated. The pw_gid field will be updated with that
	 * value.
	 */
	while (fgets (buf, sizeof buf, stdin) != NULL) {
		line++;
		cp = strrchr (buf, '\n');
		if (cp == NULL && feof (stdin) == 0) {
			fprintf (stderr, _("%s: line %d: line too long\n"),
				 Prog, line);
			fail_exit (EXIT_FAILURE);
		}
		if (cp != NULL) {
			*cp = '\0';
		}

		/*
		 * Break the string into fields and screw around with them.
		 * There MUST be 7 colon separated fields, although the
		 * values aren't that particular.
		 */
		for (cp = buf, nfields = 0; nfields < 7; nfields++) {
			fields[nfields] = cp;
			cp = strchr (cp, ':');
			if (cp == NULL)
				break;

			*cp = '\0';
			cp++;
		}
		if (nfields != 6) {
			fprintf (stderr, _("%s: line %d: invalid line\n"),
			         Prog, line);
			fail_exit (EXIT_FAILURE);
		}

		/*
		 * First check if we have to create or update a user
		 */
		pw = pw_locate (fields[0]);
		/* local, no need for xgetpwnam */
		if (NULL == pw && getpwnam(fields[0]) != NULL) {
			fprintf (stderr,
				 _("%s: cannot update the entry of user %s (not in the passwd database)\n"),
				 Prog, fields[0]);
			fail_exit (EXIT_FAILURE);
		}

		if (NULL == pw && get_user_id(fields[2], &uid) != 0) {
			fprintf (stderr,
			         _("%s: line %d: can't create user\n"),
			         Prog, line);
			fail_exit (EXIT_FAILURE);
		}

		/*
		 * Processed is the group name. A new group will be
		 * created if the group name is non-numeric and does not
		 * already exist. If the group name is a number (which is not
		 * an existing GID), a group with the same name as the user
		 * will be created, with the given GID. The given or created
		 * group will be the primary group of the user. If
		 * there is no named group to be a member of, the UID will
		 * be figured out and that value will be a candidate for a
		 * new group, if that group ID exists, a whole new group ID
		 * will be made up.
		 */
		if (   (NULL == pw)
		    && (add_group (fields[0], fields[3], &gid, uid) != 0)) {
			fprintf (stderr,
			         _("%s: line %d: can't create group\n"),
			         Prog, line);
			fail_exit (EXIT_FAILURE);
		}

		/*
		 * Now we work on the user ID. It has to be specified either
		 * as a numerical value, or left blank. If it is a numerical
		 * value, that value will be used, otherwise the next
		 * available user ID is computed and used. After this there
		 * will at least be a (struct passwd) for the user.
		 */
		if (   (NULL == pw)
		    && (add_user (fields[0], uid, gid) != 0)) {
			fprintf (stderr,
			         _("%s: line %d: can't create user\n"),
			         Prog, line);
			fail_exit (EXIT_FAILURE);
		}

		/*
		 * The password, gecos field, directory, and shell fields
		 * all come next.
		 */
		pw = pw_locate (fields[0]);
		if (NULL == pw) {
			fprintf (stderr,
			         _("%s: line %d: user '%s' does not exist in %s\n"),
			         Prog, line, fields[0], pw_dbname ());
			fail_exit (EXIT_FAILURE);
		}
		newpw = *pw;

#ifdef USE_PAM
		/* keep the list of user/password for later update by PAM */
		nusers++;
		lines     = REALLOCF(lines, nusers, int);
		usernames = REALLOCF(usernames, nusers, char *);
		passwords = REALLOCF(passwords, nusers, char *);
		if (lines == NULL || usernames == NULL || passwords == NULL) {
			fprintf (stderr,
			         _("%s: line %d: %s\n"),
			         Prog, line, strerror(errno));
			fail_exit (EXIT_FAILURE);
		}
		lines[nusers-1]     = line;
		usernames[nusers-1] = strdup (fields[0]);
		passwords[nusers-1] = strdup (fields[1]);
#endif				/* USE_PAM */
		if (add_passwd (&newpw, fields[1]) != 0) {
			fprintf (stderr,
			         _("%s: line %d: can't update password\n"),
			         Prog, line);
			fail_exit (EXIT_FAILURE);
		}
		if ('\0' != fields[4][0]) {
			newpw.pw_gecos = fields[4];
		}

		if ('\0' != fields[5][0]) {
			newpw.pw_dir = fields[5];
		}

		if ('\0' != fields[6][0]) {
			newpw.pw_shell = fields[6];
		}

		if (   ('\0' != fields[5][0])
		    && (access (newpw.pw_dir, F_OK) != 0)) {
/* FIXME: should check for directory */
			mode_t mode = getdef_num ("HOME_MODE",
			                          0777 & ~getdef_num ("UMASK", GETDEF_DEFAULT_UMASK));
			if (newpw.pw_dir[0] != '/') {
				fprintf(stderr,
					_("%s: line %d: homedir must be an absolute path\n"),
					Prog, line);
				fail_exit (EXIT_FAILURE);
			}
			if (mkdir (newpw.pw_dir, mode) != 0) {
				fprintf (stderr,
				         _("%s: line %d: mkdir %s failed: %s\n"),
				         Prog, line, newpw.pw_dir,
				         strerror (errno));
				if (errno != EEXIST) {
					fail_exit (EXIT_FAILURE);
				}
			}
			if (chown(newpw.pw_dir, newpw.pw_uid, newpw.pw_gid) != 0)
			{
				fprintf (stderr,
				         _("%s: line %d: chown %s failed: %s\n"),
				         Prog, line, newpw.pw_dir,
				         strerror (errno));
				fail_exit (EXIT_FAILURE);
			}
		}

		/*
		 * Update the password entry with the new changes made.
		 */
		if (pw_update (&newpw) == 0) {
			fprintf (stderr,
			         _("%s: line %d: can't update entry\n"),
			         Prog, line);
			fail_exit (EXIT_FAILURE);
		}

#ifdef ENABLE_SUBIDS
		/*
		 * Add subordinate uids if the user does not have them.
		 */
		if (is_sub_uid && want_subuids() && !local_sub_uid_assigned(fields[0])) {
			uid_t sub_uid_start = 0;
			unsigned long sub_uid_count = 0;
			if (find_new_sub_uids(&sub_uid_start, &sub_uid_count) != 0)
			{
				fprintf (stderr,
					_("%s: can't find subordinate user range\n"),
					Prog);
				fail_exit (EXIT_FAILURE);
			}
			if (sub_uid_add(fields[0], sub_uid_start, sub_uid_count) == 0)
			{
				fprintf (stderr,
					_("%s: failed to prepare new %s entry\n"),
					Prog, sub_uid_dbname ());
				fail_exit (EXIT_FAILURE);
			}
		}

		/*
		 * Add subordinate gids if the user does not have them.
		 */
		if (is_sub_gid && want_subgids() && !local_sub_gid_assigned(fields[0])) {
			gid_t sub_gid_start = 0;
			unsigned long sub_gid_count = 0;
			if (find_new_sub_gids(&sub_gid_start, &sub_gid_count) != 0) {
				fprintf (stderr,
					_("%s: can't find subordinate group range\n"),
					Prog);
				fail_exit (EXIT_FAILURE);
			}
			if (sub_gid_add(fields[0], sub_gid_start, sub_gid_count) == 0) {
				fprintf (stderr,
					_("%s: failed to prepare new %s entry\n"),
					Prog, sub_uid_dbname ());
				fail_exit (EXIT_FAILURE);
			}
		}
#endif				/* ENABLE_SUBIDS */
	}

	/*
	 * Any detected errors will cause the entire set of changes to be
	 * aborted. Unlocking the password file will cause all of the
	 * changes to be ignored. Otherwise the file is closed, causing the
	 * changes to be written out all at once, and then unlocked
	 * afterwards.
	 */
	close_files ();

	nscd_flush_cache ("passwd");
	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_PASSWD | SSSD_DB_GROUP);

#ifdef USE_PAM
	unsigned int i;
	/* Now update the passwords using PAM */
	for (i = 0; i < nusers; i++) {
		if (do_pam_passwd_non_interactive ("newusers", usernames[i], passwords[i]) != 0) {
			fprintf (stderr,
			         _("%s: (line %d, user %s) password not changed\n"),
			         Prog, lines[i], usernames[i]);
			exit (EXIT_FAILURE);
		}
	}
#endif				/* USE_PAM */

	exit (EXIT_SUCCESS);
}

