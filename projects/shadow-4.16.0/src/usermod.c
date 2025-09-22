/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2000 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#ifdef ENABLE_LASTLOG
#include <lastlog.h>
#endif /* ENABLE_LASTLOG */
#include <pwd.h>
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
#include <stdio.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "alloc.h"
#include "atoi/str2i.h"
#include "chkname.h"
#include "defines.h"
#include "faillog.h"
#include "getdef.h"
#include "groupio.h"
#include "memzero.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#include "pwauth.h"
#include "pwio.h"
#ifdef	SHADOWGRP
#include "sgroupio.h"
#endif
#include "shadowio.h"
#ifdef ENABLE_SUBIDS
#include "subordinateio.h"
#endif				/* ENABLE_SUBIDS */
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif				/* WITH_SELINUX */
#ifdef WITH_TCB
#include "tcbfuncs.h"
#endif
#include "shadowlog.h"
#include "string/sprintf.h"
#include "time/day_to_str.h"


/*
 * exit status values
 * for E_GRP_UPDATE and E_NOSPACE (not used yet), other update requests
 * will be implemented (as documented in the Solaris 2.x man page).
 */
/*@-exitarg@*/
#define E_SUCCESS	0	/* success */
#define E_PW_UPDATE	1	/* can't update password file */
#define E_USAGE		2	/* invalid command syntax */
#define E_BAD_ARG	3	/* invalid argument to option */
#define E_UID_IN_USE	4	/* UID already in use (and no -o) */
/* #define E_BAD_PWFILE	5	   passwd file contains errors */
#define E_NOTFOUND	6	/* specified user/group doesn't exist */
#define E_USER_BUSY	8	/* user to modify is logged in */
#define E_NAME_IN_USE	9	/* username or group name already in use */
#define E_GRP_UPDATE	10	/* can't update group file */
/* #define E_NOSPACE	11	   insufficient space to move home dir */
#define E_HOMEDIR	12	/* unable to complete home dir move */
#define E_SE_UPDATE	13	/* can't update SELinux user mapping */
#ifdef ENABLE_SUBIDS
#define E_SUB_UID_UPDATE 16	/* can't update the subordinate uid file */
#define E_SUB_GID_UPDATE 18	/* can't update the subordinate gid file */
#endif				/* ENABLE_SUBIDS */

#define	VALID(s)	(strcspn (s, ":\n") == strlen (s))

/*
 * Global variables
 */
static const char Prog[] = "usermod";

static char *user_name;
static char *user_newname;
static char *user_pass;
static uid_t user_id;
static uid_t user_newid;
static gid_t user_gid;
static gid_t user_newgid;
static char *user_comment;
static char *user_newcomment;
static char *user_home;
static char *user_newhome;
static char *user_shell;
#ifdef WITH_SELINUX
static const char *user_selinux = "";
static const char *user_selinux_range = NULL;
#endif				/* WITH_SELINUX */
static char *user_newshell;
static long user_expire;
static long user_newexpire;
static long user_inactive;
static long user_newinactive;
static long sys_ngroups;
static char **user_groups;	/* NULL-terminated list */

static const char* prefix = "";
static char* prefix_user_home = NULL;
static char* prefix_user_newhome = NULL;

static bool
    aflg = false,		/* append to existing secondary group set */
    cflg = false,		/* new comment (GECOS) field */
    dflg = false,		/* new home directory */
    eflg = false,		/* days since 1970-01-01 when account becomes expired */
    fflg = false,		/* days until account with expired password is locked */
    gflg = false,		/* new primary group ID */
    Gflg = false,		/* new secondary group set */
    Lflg = false,		/* lock the password */
    lflg = false,		/* new user name */
    mflg = false,		/* create user's home directory if it doesn't exist */
    oflg = false,		/* permit non-unique user ID to be specified with -u */
    pflg = false,		/* new encrypted password */
    rflg = false,		/* remove a user from a single group */
    sflg = false,		/* new shell program */
#ifdef WITH_SELINUX
    Zflg = false,		/* new selinux user */
#endif
#ifdef ENABLE_SUBIDS
    vflg = false,		/*    add subordinate uids */
    Vflg = false,		/* delete subordinate uids */
    wflg = false,		/*    add subordinate gids */
    Wflg = false,		/* delete subordinate gids */
#endif				/* ENABLE_SUBIDS */
    uflg = false,		/* specify new user ID */
    Uflg = false;		/* unlock the password */

static bool is_shadow_pwd;

#ifdef SHADOWGRP
static bool is_shadow_grp;
#endif

#ifdef ENABLE_SUBIDS
static bool is_sub_uid = false;
static bool is_sub_gid = false;
#endif				/* ENABLE_SUBIDS */

static bool pw_locked  = false;
static bool spw_locked = false;
static bool gr_locked  = false;
#ifdef SHADOWGRP
static bool sgr_locked = false;
#endif
#ifdef ENABLE_SUBIDS
static bool sub_uid_locked = false;
static bool sub_gid_locked = false;
#endif				/* ENABLE_SUBIDS */


/* local function prototypes */
static int get_groups (char *);
NORETURN static void usage (int status);
static void new_pwent (struct passwd *);
static void new_spent (struct spwd *);
NORETURN static void fail_exit (int);
static void update_group_file(void);
static void update_group(const struct group *grp);

#ifdef SHADOWGRP
static void update_gshadow_file(void);
static void update_gshadow(const struct sgrp *sgrp);
#endif
static void grp_update (void);

static void process_flags (int, char **);
static void close_files (void);
static void open_files (void);
static void usr_update (void);
static void move_home (void);
#ifdef ENABLE_LASTLOG
static void update_lastlog (void);
#endif /* ENABLE_LASTLOG */
static void update_faillog (void);

#ifndef NO_MOVE_MAILBOX
static void move_mailbox (void);
#endif

extern int allow_bad_names;

/*
 * get_groups - convert a list of group names to an array of group IDs
 *
 *	get_groups() takes a comma-separated list of group names and
 *	converts it to a NULL-terminated array. Any unknown group names are
 *	reported as errors.
 */
static int get_groups (char *list)
{
	char *cp;
	struct group *grp;
	int errors = 0;
	int ngroups = 0;

	/*
	 * Initialize the list to be empty
	 */
	user_groups[0] = NULL;

	if ('\0' == *list) {
		return 0;
	}

	/*
	 * So long as there is some data to be converted, strip off each
	 * name and look it up. A mix of numerical and string values for
	 * group identifiers is permitted.
	 */
	do {
		/*
		 * Strip off a single name from the list
		 */
		cp = strchr (list, ',');
		if (NULL != cp) {
			*cp = '\0';
			cp++;
		}

		/*
		 * Names starting with digits are treated as numerical GID
		 * values, otherwise the string is looked up as is.
		 */
		grp = prefix_getgr_nam_gid (list);

		/*
		 * There must be a match, either by GID value or by
		 * string name.
		 */
		if (NULL == grp) {
			fprintf (stderr, _("%s: group '%s' does not exist\n"),
			         Prog, list);
			errors++;
		}
		list = cp;

		/*
		 * If the group doesn't exist, don't dump core. Instead,
		 * try the next one.  --marekm
		 */
		if (NULL == grp) {
			continue;
		}

		if (ngroups == sys_ngroups) {
			fprintf (stderr,
			         _("%s: too many groups specified (max %d).\n"),
			         Prog, ngroups);
			gr_free (grp);
			break;
		}

		/*
		 * Add the group name to the user's list of groups.
		 */
		user_groups[ngroups++] = xstrdup (grp->gr_name);
		gr_free (grp);
	} while (NULL != list);

	user_groups[ngroups] = NULL;

	/*
	 * Any errors in finding group names are fatal
	 */
	if (0 != errors) {
		return -1;
	}

	return 0;
}

#ifdef ENABLE_SUBIDS
struct ulong_range
{
	unsigned long first;
	unsigned long last;
};

static struct ulong_range getulong_range(const char *str)
{
	struct ulong_range result = { .first = ULONG_MAX, .last = 0 };
	long long first, last;
	char *pos;

	errno = 0;
	first = strtoll(str, &pos, 10);
	if (('\0' == *str) || ('-' != *pos ) || (0 != errno) ||
	    (first != (unsigned long)first))
		goto out;

	errno = 0;
	last = strtoll(pos + 1, &pos, 10);
	if (('\0' != *pos ) || (0 != errno) ||
	    (last != (unsigned long)last))
		goto out;

	if (first > last)
		goto out;

	/*
	 * uid_t in linux is an unsigned int, anything over this is an invalid
	 * range will be later refused anyway by get_map_ranges().
	 */
	if (first > UINT_MAX || last > UINT_MAX)
		goto out;

	result.first = (unsigned long)first;
	result.last = (unsigned long)last;
out:
	return result;
}

struct ulong_range_list_entry {
	struct ulong_range_list_entry *next;
	struct ulong_range range;
};

static struct ulong_range_list_entry *add_sub_uids = NULL, *del_sub_uids = NULL;
static struct ulong_range_list_entry *add_sub_gids = NULL, *del_sub_gids = NULL;

static int prepend_range(const char *str, struct ulong_range_list_entry **head)
{
	struct ulong_range range;
	struct ulong_range_list_entry *entry;
	range = getulong_range(str);
	if (range.first > range.last)
		return 0;

	entry = MALLOC(1, struct ulong_range_list_entry);
	if (!entry) {
		fprintf (stderr,
			_("%s: failed to allocate memory: %s\n"),
			Prog, strerror (errno));
		return 0;
	}
	entry->next = *head;
	entry->range = range;
	*head = entry;
	return 1;
}
#endif				/* ENABLE_SUBIDS */

/*
 * usage - display usage message and exit
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
	(void) fputs (_("  -a, --append                  append the user to the supplemental GROUPS\n"
	                "                                mentioned by the -G option without removing\n"
	                "                                the user from other groups\n"), usageout);
	(void) fputs (_("  -b, --badname                 allow bad names\n"), usageout);
	(void) fputs (_("  -c, --comment COMMENT         new value of the GECOS field\n"), usageout);
	(void) fputs (_("  -d, --home HOME_DIR           new home directory for the user account\n"), usageout);
	(void) fputs (_("  -e, --expiredate EXPIRE_DATE  set account expiration date to EXPIRE_DATE\n"), usageout);
	(void) fputs (_("  -f, --inactive INACTIVE       set password inactive after expiration\n"
	                "                                to INACTIVE\n"), usageout);
	(void) fputs (_("  -g, --gid GROUP               force use GROUP as new primary group\n"), usageout);
	(void) fputs (_("  -G, --groups GROUPS           new list of supplementary GROUPS\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -l, --login NEW_LOGIN         new value of the login name\n"), usageout);
	(void) fputs (_("  -L, --lock                    lock the user account\n"), usageout);
	(void) fputs (_("  -m, --move-home               move contents of the home directory to the\n"
	                "                                new location (use only with -d)\n"), usageout);
	(void) fputs (_("  -o, --non-unique              allow using duplicate (non-unique) UID\n"), usageout);
	(void) fputs (_("  -p, --password PASSWORD       use encrypted password for the new password\n"), usageout);
	(void) fputs (_("  -P, --prefix PREFIX_DIR       prefix directory where are located the /etc/* files\n"), usageout);
	(void) fputs (_("  -r, --remove                  remove the user from only the supplemental GROUPS\n"
	                "                                mentioned by the -G option without removing\n"
	                "                                the user from other groups\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -s, --shell SHELL             new login shell for the user account\n"), usageout);
	(void) fputs (_("  -u, --uid UID                 new UID for the user account\n"), usageout);
	(void) fputs (_("  -U, --unlock                  unlock the user account\n"), usageout);
#ifdef ENABLE_SUBIDS
	(void) fputs (_("  -v, --add-subuids FIRST-LAST  add range of subordinate uids\n"), usageout);
	(void) fputs (_("  -V, --del-subuids FIRST-LAST  remove range of subordinate uids\n"), usageout);
	(void) fputs (_("  -w, --add-subgids FIRST-LAST  add range of subordinate gids\n"), usageout);
	(void) fputs (_("  -W, --del-subgids FIRST-LAST  remove range of subordinate gids\n"), usageout);
#endif				/* ENABLE_SUBIDS */
#ifdef WITH_SELINUX
	(void) fputs (_("  -Z, --selinux-user SEUSER     new SELinux user mapping for the user account\n"), usageout);
	(void) fputs (_("      --selinux-range SERANGE   new SELinux MLS range for the user account\n"), usageout);
#endif				/* WITH_SELINUX */
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * update encrypted password string (for both shadow and non-shadow
 * passwords)
 */
static char *new_pw_passwd (char *pw_pass)
{
	if (Lflg && ('!' != pw_pass[0])) {
		char *buf = XMALLOC(strlen(pw_pass) + 2, char);

#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "updating passwd", user_newname, user_newid, 0);
#endif
		SYSLOG ((LOG_INFO, "lock user '%s' password", user_newname));
		strcpy (buf, "!");
		strcat (buf, pw_pass);
		pw_pass = buf;
	} else if (Uflg && pw_pass[0] == '!') {
		if (pw_pass[1] == '\0') {
			fprintf (stderr,
			         _("%s: unlocking the user's password would result in a passwordless account.\n"
			           "You should set a password with usermod -p to unlock this user's password.\n"),
			         Prog);
			return pw_pass;
		}

#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "updating password", user_newname, user_newid, 0);
#endif
		SYSLOG ((LOG_INFO, "unlock user '%s' password", user_newname));
		memmove(pw_pass, pw_pass + 1, strlen(pw_pass));
	} else if (pflg) {
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing password", user_newname, user_newid, 1);
#endif
		SYSLOG ((LOG_INFO, "change user '%s' password", user_newname));
		pw_pass = xstrdup (user_pass);
	}
	return pw_pass;
}

/*
 * new_pwent - initialize the values in a password file entry
 *
 *	new_pwent() takes all of the values that have been entered and fills
 *	in a (struct passwd) with them.
 */
static void new_pwent (struct passwd *pwent)
{
	if (lflg) {
		if (pw_locate (user_newname) != NULL) {
			/* This should never happen.
			 * It was already checked that the user doesn't
			 * exist on the system.
			 */
			fprintf (stderr,
			         _("%s: user '%s' already exists in %s\n"),
			         Prog, user_newname, pw_dbname ());
			fail_exit (E_NAME_IN_USE);
		}
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing name", user_newname, user_newid, 1);
#endif
		SYSLOG ((LOG_INFO,
		         "change user name '%s' to '%s'",
		         pwent->pw_name, user_newname));
		pwent->pw_name = xstrdup (user_newname);
	}
	/* Update the password in passwd if there is no shadow file or if
	 * the password is currently in passwd (pw_passwd != "x").
	 * We do not force the usage of shadow passwords if they are not
	 * used for this account.
	 */
	if (   (!is_shadow_pwd)
	    || (strcmp (pwent->pw_passwd, SHADOW_PASSWD_STRING) != 0)) {
		pwent->pw_passwd = new_pw_passwd (pwent->pw_passwd);
	}

	if (uflg) {
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing uid", user_newname, user_newid, 1);
#endif
		SYSLOG ((LOG_INFO,
		         "change user '%s' UID from '%d' to '%d'",
		         pwent->pw_name, pwent->pw_uid, user_newid));
		pwent->pw_uid = user_newid;
	}
	if (gflg) {
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing primary group",
		              user_newname, user_newid, 1);
#endif
		SYSLOG ((LOG_INFO,
		         "change user '%s' GID from '%d' to '%d'",
		         pwent->pw_name, pwent->pw_gid, user_newgid));
		pwent->pw_gid = user_newgid;
	}
	if (cflg) {
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing comment", user_newname, user_newid, 1);
#endif
		pwent->pw_gecos = user_newcomment;
	}

	if (dflg) {
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing home directory",
		              user_newname, user_newid, 1);
#endif
		SYSLOG ((LOG_INFO,
		         "change user '%s' home from '%s' to '%s'",
		         pwent->pw_name, pwent->pw_dir, user_newhome));

		if (strlen(user_newhome) > 1
			&& '/' == user_newhome[strlen(user_newhome)-1]) {
			user_newhome[strlen(user_newhome)-1]='\0';
		}

		pwent->pw_dir = user_newhome;
	}
	if (sflg) {
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing user shell",
		              user_newname, user_newid, 1);
#endif
		SYSLOG ((LOG_INFO,
		         "change user '%s' shell from '%s' to '%s'",
		         pwent->pw_name, pwent->pw_shell, user_newshell));
		pwent->pw_shell = user_newshell;
	}
}

/*
 * new_spent - initialize the values in a shadow password file entry
 *
 *	new_spent() takes all of the values that have been entered and fills
 *	in a (struct spwd) with them.
 */
static void new_spent (struct spwd *spent)
{
	if (lflg) {
		if (spw_locate (user_newname) != NULL) {
			fprintf (stderr,
			         _("%s: user '%s' already exists in %s\n"),
			         Prog, user_newname, spw_dbname ());
			fail_exit (E_NAME_IN_USE);
		}
		spent->sp_namp = xstrdup (user_newname);
	}

	if (fflg) {
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing inactive days",
		              user_newname, user_newid, 1);
#endif
		SYSLOG ((LOG_INFO,
		         "change user '%s' inactive from '%ld' to '%ld'",
		         spent->sp_namp, spent->sp_inact, user_newinactive));
		spent->sp_inact = user_newinactive;
	}
	if (eflg) {
		/* log dates rather than numbers of days. */
		char new_exp[16], old_exp[16];

		DAY_TO_STR(new_exp, user_newexpire);
		DAY_TO_STR(old_exp, user_expire);
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              "changing expiration date",
		              user_newname, user_newid, 1);
#endif
		SYSLOG ((LOG_INFO,
		         "change user '%s' expiration from '%s' to '%s'",
		         spent->sp_namp, old_exp, new_exp));
		spent->sp_expire = user_newexpire;
	}

	/* Always update the shadowed password if there is a shadow entry
	 * (even if shadowed passwords might not be enabled for this
	 * account (pw_passwd != "x")).
	 * It seems better to update the password in both places in case a
	 * shadow and a non shadow entry exist.
	 * This might occur if:
	 *  + there were already both entries
	 *  + aging has been requested
	 */
	spent->sp_pwdp = new_pw_passwd (spent->sp_pwdp);

	if (pflg) {
		spent->sp_lstchg = gettime () / DAY;
		if (0 == spent->sp_lstchg) {
			/* Better disable aging than requiring a password
			 * change. */
			spent->sp_lstchg = -1;
		}
	}
}

/*
 * fail_exit - exit with an error code after unlocking files
 */
NORETURN
static void
fail_exit (int code)
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

#ifdef WITH_AUDIT
	audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
	              "modifying account",
	              user_name, AUDIT_NO_ID, 0);
#endif
	exit (code);
}


static void
update_group_file(void)
{
	const struct group  *grp;

	/*
	 * Scan through the entire group file looking for the groups that
	 * the user is a member of.
	 */
	while ((grp = gr_next()) != NULL)
		update_group(grp);
}


static void
update_group(const struct group *grp)
{
	bool          changed;
	bool          is_member;
	bool          was_member;
	struct group  *ngrp;

	changed = false;

	/*
	 * See if the user specified this group as one of their
	 * concurrent groups.
	 */
	was_member = is_on_list (grp->gr_mem, user_name);
	is_member = Gflg && (   (was_member && aflg)
			     || is_on_list (user_groups, grp->gr_name));

	if (!was_member && !is_member)
		return;

	/*
	* If rflg+Gflg  is passed in AKA -rG invert is_member flag, which removes
	* mentioned groups while leaving the others.
	*/
	if (Gflg && rflg) {
		is_member = !is_member;
	}

	ngrp = __gr_dup (grp);
	if (NULL == ngrp) {
		fprintf (stderr,
			 _("%s: Out of memory. Cannot update %s.\n"),
			 Prog, gr_dbname ());
		fail_exit (E_GRP_UPDATE);
	}

	if (was_member) {
		if ((!Gflg) || is_member) {
			/* User was a member and is still a member
			 * of this group.
			 * But the user might have been renamed.
			 */
			if (lflg) {
				ngrp->gr_mem = del_list (ngrp->gr_mem,
							 user_name);
				ngrp->gr_mem = add_list (ngrp->gr_mem,
							 user_newname);
				changed = true;
#ifdef WITH_AUDIT
				audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
					      "changing group member",
					      user_newname, AUDIT_NO_ID, 1);
#endif
				SYSLOG ((LOG_INFO,
					 "change '%s' to '%s' in group '%s'",
					 user_name, user_newname,
					 ngrp->gr_name));
			}
		} else {
			/* User was a member but is no more a
			 * member of this group.
			 */
			ngrp->gr_mem = del_list (ngrp->gr_mem, user_name);
			changed = true;
#ifdef WITH_AUDIT
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
				      "removing group member",
				      user_name, AUDIT_NO_ID, 1);
#endif
			SYSLOG ((LOG_INFO,
				 "delete '%s' from group '%s'",
				 user_name, ngrp->gr_name));
		}
	} else if (is_member) {
		/* User was not a member but is now a member this
		 * group.
		 */
		ngrp->gr_mem = add_list (ngrp->gr_mem, user_newname);
		changed = true;
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			      "adding user to group",
			      user_name, AUDIT_NO_ID, 1);
#endif
		SYSLOG ((LOG_INFO, "add '%s' to group '%s'",
			 user_newname, ngrp->gr_name));
	}
	if (!changed)
		goto free_ngrp;

	if (gr_update (ngrp) == 0) {
		fprintf (stderr,
			 _("%s: failed to prepare the new %s entry '%s'\n"),
			 Prog, gr_dbname (), ngrp->gr_name);
		SYSLOG ((LOG_WARN, "failed to prepare the new %s entry '%s'", gr_dbname (), ngrp->gr_name));
		fail_exit (E_GRP_UPDATE);
	}

free_ngrp:
	gr_free(ngrp);
}


#ifdef SHADOWGRP
static void
update_gshadow_file(void)
{
	const struct sgrp  *sgrp;

	/*
	 * Scan through the entire shadow group file looking for the groups
	 * that the user is a member of.
	 */
	while ((sgrp = sgr_next()) != NULL)
		update_gshadow(sgrp);
}
#endif				/* SHADOWGRP */


#ifdef SHADOWGRP
static void
update_gshadow(const struct sgrp *sgrp)
{
	bool         changed;
	bool         is_member;
	bool         was_member;
	bool         was_admin;
	struct sgrp  *nsgrp;

	changed = false;

	/*
	 * See if the user was a member of this group
	 */
	was_member = is_on_list (sgrp->sg_mem, user_name);

	/*
	 * See if the user was an administrator of this group
	 */
	was_admin = is_on_list (sgrp->sg_adm, user_name);

	/*
	 * See if the user specified this group as one of their
	 * concurrent groups.
	 */
	is_member = Gflg && (   (was_member && aflg)
			     || is_on_list (user_groups, sgrp->sg_name));

	if (!was_member && !was_admin && !is_member)
		return;

	/*
	* If rflg+Gflg  is passed in AKA -rG invert is_member, to remove targeted
	* groups while leaving the user apart of groups not mentioned
	*/
	if (Gflg && rflg) {
		is_member = !is_member;
	}

	nsgrp = __sgr_dup (sgrp);
	if (NULL == nsgrp) {
		fprintf (stderr,
			 _("%s: Out of memory. Cannot update %s.\n"),
			 Prog, sgr_dbname ());
		fail_exit (E_GRP_UPDATE);
	}

	if (was_admin && lflg) {
		/* User was an admin of this group but the user
		 * has been renamed.
		 */
		nsgrp->sg_adm = del_list (nsgrp->sg_adm, user_name);
		nsgrp->sg_adm = add_list (nsgrp->sg_adm, user_newname);
		changed = true;
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			      "changing admin name in shadow group",
			      user_name, AUDIT_NO_ID, 1);
#endif
		SYSLOG ((LOG_INFO,
			 "change admin '%s' to '%s' in shadow group '%s'",
			 user_name, user_newname, nsgrp->sg_name));
	}

	if (was_member) {
		if ((!Gflg) || is_member) {
			/* User was a member and is still a member
			 * of this group.
			 * But the user might have been renamed.
			 */
			if (lflg) {
				nsgrp->sg_mem = del_list (nsgrp->sg_mem,
							  user_name);
				nsgrp->sg_mem = add_list (nsgrp->sg_mem,
							  user_newname);
				changed = true;
#ifdef WITH_AUDIT
				audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
					      "changing member in shadow group",
					      user_name, AUDIT_NO_ID, 1);
#endif
				SYSLOG ((LOG_INFO,
					 "change '%s' to '%s' in shadow group '%s'",
					 user_name, user_newname,
					 nsgrp->sg_name));
			}
		} else {
			/* User was a member but is no more a
			 * member of this group.
			 */
			nsgrp->sg_mem = del_list (nsgrp->sg_mem, user_name);
			changed = true;
#ifdef WITH_AUDIT
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
				      "removing user from shadow group",
				      user_name, AUDIT_NO_ID, 1);
#endif
			SYSLOG ((LOG_INFO,
				 "delete '%s' from shadow group '%s'",
				 user_name, nsgrp->sg_name));
		}
	} else if (is_member) {
		/* User was not a member but is now a member this
		 * group.
		 */
		nsgrp->sg_mem = add_list (nsgrp->sg_mem, user_newname);
		changed = true;
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			      "adding user to shadow group",
			      user_newname, AUDIT_NO_ID, 1);
#endif
		SYSLOG ((LOG_INFO, "add '%s' to shadow group '%s'",
			 user_newname, nsgrp->sg_name));
	}
	if (!changed)
		goto free_nsgrp;

	/*
	 * Update the group entry to reflect the changes.
	 */
	if (sgr_update (nsgrp) == 0) {
		fprintf (stderr,
			 _("%s: failed to prepare the new %s entry '%s'\n"),
			 Prog, sgr_dbname (), nsgrp->sg_name);
		SYSLOG ((LOG_WARN, "failed to prepare the new %s entry '%s'",
			 sgr_dbname (), nsgrp->sg_name));
		fail_exit (E_GRP_UPDATE);
	}

free_nsgrp:
	free (nsgrp);
}
#endif				/* SHADOWGRP */


/*
 * grp_update - add user to secondary group set
 *
 *	grp_update() takes the secondary group set given in user_groups and
 *	adds the user to each group given by that set.
 */
static void grp_update (void)
{
	update_group_file();
#ifdef SHADOWGRP
	if (is_shadow_grp) {
		update_gshadow_file();
	}
#endif
}

/*
 * process_flags - perform command line argument setting
 *
 *	process_flags() interprets the command line arguments and sets the
 *	values that the user will be created with accordingly. The values
 *	are checked for sanity.
 */
static void process_flags (int argc, char **argv)
{
	struct stat st;
	bool anyflag = false;

	{
		/*
		 * Parse the command line options.
		 */
		int c;
		static struct option long_options[] = {
			{"append",       no_argument,       NULL, 'a'},
			{"badname",      no_argument,       NULL, 'b'},
			{"badnames",     no_argument,       NULL, 'b'},
			{"comment",      required_argument, NULL, 'c'},
			{"home",         required_argument, NULL, 'd'},
			{"expiredate",   required_argument, NULL, 'e'},
			{"inactive",     required_argument, NULL, 'f'},
			{"gid",          required_argument, NULL, 'g'},
			{"groups",       required_argument, NULL, 'G'},
			{"help",         no_argument,       NULL, 'h'},
			{"login",        required_argument, NULL, 'l'},
			{"lock",         no_argument,       NULL, 'L'},
			{"move-home",    no_argument,       NULL, 'm'},
			{"non-unique",   no_argument,       NULL, 'o'},
			{"password",     required_argument, NULL, 'p'},
			{"remove",       no_argument,       NULL, 'r'},
			{"root",         required_argument, NULL, 'R'},
			{"prefix",       required_argument, NULL, 'P'},
			{"shell",        required_argument, NULL, 's'},
			{"uid",          required_argument, NULL, 'u'},
			{"unlock",       no_argument,       NULL, 'U'},
#ifdef ENABLE_SUBIDS
			{"add-subuids",  required_argument, NULL, 'v'},
			{"del-subuids",  required_argument, NULL, 'V'},
			{"add-subgids",  required_argument, NULL, 'w'},
			{"del-subgids",  required_argument, NULL, 'W'},
#endif				/* ENABLE_SUBIDS */
#ifdef WITH_SELINUX
			{"selinux-user",  required_argument, NULL, 'Z'},
			{"selinux-range", required_argument, NULL, 202},
#endif				/* WITH_SELINUX */
			{NULL, 0, NULL, '\0'}
		};
		while ((c = getopt_long (argc, argv,
		                         "abc:d:e:f:g:G:hl:Lmop:rR:s:u:UP:"
#ifdef ENABLE_SUBIDS
		                         "v:w:V:W:"
#endif				/* ENABLE_SUBIDS */
#ifdef WITH_SELINUX
			                 "Z:"
#endif				/* WITH_SELINUX */
			                 , long_options, NULL)) != -1) {
			switch (c) {
			case 'a':
				aflg = true;
				break;
			case 'b':
				allow_bad_names = true;
				break;
			case 'c':
				if (!VALID (optarg)) {
					fprintf (stderr,
					         _("%s: invalid field '%s'\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				user_newcomment = optarg;
				cflg = true;
				break;
			case 'd':
				if (!VALID (optarg)) {
					fprintf (stderr,
					         _("%s: invalid field '%s'\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				dflg = true;
				user_newhome = optarg;
				if (user_newhome[0] != '/') {
					fprintf (stderr,
					         _("%s: homedir must be an absolute path\n"),
					         Prog);
					exit (E_BAD_ARG);
				}
				break;
			case 'e':
				user_newexpire = strtoday (optarg);
				if (user_newexpire < -1) {
					fprintf (stderr,
						 _("%s: invalid date '%s'\n"),
						 Prog, optarg);
					exit (E_BAD_ARG);
				}
				eflg = true;
				break;
			case 'f':
				if (   (str2sl(&user_newinactive, optarg) == -1)
				    || (user_newinactive < -1)) {
					fprintf (stderr,
					         _("%s: invalid numeric argument '%s'\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				fflg = true;
				break;
			case 'g':
			{
				struct group  *grp;

				grp = prefix_getgr_nam_gid (optarg);
				if (NULL == grp) {
					fprintf (stderr,
					         _("%s: group '%s' does not exist\n"),
					         Prog, optarg);
					exit (E_NOTFOUND);
				}
				user_newgid = grp->gr_gid;
				gflg = true;
				gr_free (grp);
				break;
			}
			case 'G':
				if (get_groups (optarg) != 0) {
					exit (E_NOTFOUND);
				}
				Gflg = true;
				break;
			case 'h':
				usage (E_SUCCESS);
				/*@notreached@*/break;
			case 'l':
				if (!is_valid_user_name (optarg)) {
					fprintf (stderr,
					         _("%s: invalid user name '%s': use --badname to ignore\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				lflg = true;
				user_newname = optarg;
				break;
			case 'L':
				Lflg = true;
				break;
			case 'm':
				mflg = true;
				break;
			case 'o':
				oflg = true;
				break;
			case 'p':
				user_pass = optarg;
				pflg = true;
				break;
			case 'r':
				rflg = true;
				break;
			case 'R': /* no-op, handled in process_root_flag () */
				break;
			case 'P': /* no-op, handled in process_prefix_flag () */
				break;
			case 's':
				if (   ( !VALID (optarg) )
				    || (   ('\0' != optarg[0])
				        && ('/'  != optarg[0])
				        && ('*'  != optarg[0]) )) {
					fprintf (stderr,
					         _("%s: invalid shell '%s'\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				if (    '\0' != optarg[0]
				     && '*'  != optarg[0]
				     && strcmp(optarg, "/sbin/nologin") != 0
				     && (   stat(optarg, &st) != 0
				         || S_ISDIR(st.st_mode)
				         || access(optarg, X_OK) != 0)) {
					fprintf (stderr,
					         _("%s: Warning: missing or non-executable shell '%s'\n"),
					         Prog, optarg);
				}
				user_newshell = optarg;
				sflg = true;
				break;
			case 'u':
				if (   (get_uid(optarg, &user_newid) == -1)
				    || (user_newid == (uid_t)-1)) {
					fprintf (stderr,
					         _("%s: invalid user ID '%s'\n"),
					         Prog, optarg);
					exit (E_BAD_ARG);
				}
				uflg = true;
				break;
			case 'U':
				Uflg = true;
				break;
#ifdef ENABLE_SUBIDS
			case 'v':
				if (prepend_range (optarg, &add_sub_uids) == 0) {
					fprintf (stderr,
						_("%s: invalid subordinate uid range '%s'\n"),
						Prog, optarg);
					exit(E_BAD_ARG);
				}
				vflg = true;
				break;
			case 'V':
				if (prepend_range (optarg, &del_sub_uids) == 0) {
					fprintf (stderr,
						_("%s: invalid subordinate uid range '%s'\n"),
						Prog, optarg);
					exit(E_BAD_ARG);
				}
				Vflg = true;
				break;
			case 'w':
				if (prepend_range (optarg, &add_sub_gids) == 0) {
					fprintf (stderr,
						_("%s: invalid subordinate gid range '%s'\n"),
						Prog, optarg);
					exit(E_BAD_ARG);
				}
				wflg = true;
                break;
			case 'W':
				if (prepend_range (optarg, &del_sub_gids) == 0) {
					fprintf (stderr,
						_("%s: invalid subordinate gid range '%s'\n"),
						Prog, optarg);
					exit(E_BAD_ARG);
				}
				Wflg = true;
				break;
#endif				/* ENABLE_SUBIDS */
#ifdef WITH_SELINUX
			case 'Z':
				if (prefix[0]) {
					fprintf (stderr,
					         _("%s: -Z cannot be used with --prefix\n"),
					         Prog);
					exit (E_BAD_ARG);
				}
				if (is_selinux_enabled () > 0) {
					user_selinux = optarg;
					Zflg = true;
				} else {
					fprintf (stderr,
					         _("%s: -Z requires SELinux enabled kernel\n"),
					         Prog);
					exit (E_BAD_ARG);
				}
				break;
			case 202:
				user_selinux_range = optarg;
				break;
#endif				/* WITH_SELINUX */
			default:
				usage (E_USAGE);
			}
			anyflag = true;
		}
	}

	if (optind != argc - 1) {
		usage (E_USAGE);
	}

	user_name = argv[argc - 1];

	{
		const struct passwd *pwd;
		/* local, no need for xgetpwnam */
		pwd = prefix_getpwnam (user_name);
		if (NULL == pwd) {
			fprintf (stderr,
			         _("%s: user '%s' does not exist\n"),
			         Prog, user_name);
			exit (E_NOTFOUND);
		}

		user_id = pwd->pw_uid;
		user_gid = pwd->pw_gid;
		user_comment = xstrdup (pwd->pw_gecos);
		user_home = xstrdup (pwd->pw_dir);
		user_shell = xstrdup (pwd->pw_shell);
	}

	/* user_newname, user_newid, user_newgid can be used even when the
	 * options where not specified. */
	if (!lflg) {
		user_newname = user_name;
	}
	if (!uflg) {
		user_newid = user_id;
	}
	if (!gflg) {
		user_newgid = user_gid;
	}
	if (prefix[0]) {
		xasprintf(&prefix_user_home, "%s/%s", prefix, user_home);
		if (user_newhome) {
			xasprintf(&prefix_user_newhome, "%s/%s",
			          prefix, user_newhome);
		}
	} else {
		prefix_user_home = user_home;
		prefix_user_newhome = user_newhome;
	}

	{
		const struct spwd *spwd = NULL;
		/* local, no need for xgetspnam */
		if (is_shadow_pwd && ((spwd = prefix_getspnam (user_name)) != NULL)) {
			user_expire = spwd->sp_expire;
			user_inactive = spwd->sp_inact;
		}
	}

	if (!anyflag) {
		fprintf (stderr, _("%s: no options\n"), Prog);
		usage (E_USAGE);
	}

	if (aflg && (!Gflg)) {
		fprintf (stderr,
		         _("%s: %s flag is only allowed with the %s flag\n"),
		         Prog, "-a", "-G");
		usage (E_USAGE);
	}

	if (rflg && (!Gflg)) {
		fprintf (stderr,
		         _("%s: %s flag is only allowed with the %s flag\n"),
		         Prog, "-r", "-G");
		usage (E_USAGE);
	}

	if (rflg && aflg) {
		fprintf (stderr,
		         _("%s: %s and %s are mutually exclusive flags\n"),
		         Prog, "-r", "-a");
		usage (E_USAGE);
	}

	if ((Lflg && (pflg || Uflg)) || (pflg && Uflg)) {
		fprintf (stderr,
		         _("%s: the -L, -p, and -U flags are exclusive\n"),
		         Prog);
		usage (E_USAGE);
	}

	if (oflg && !uflg) {
		fprintf (stderr,
		         _("%s: %s flag is only allowed with the %s flag\n"),
		         Prog, "-o", "-u");
		usage (E_USAGE);
	}

	if (mflg && !dflg) {
		fprintf (stderr,
		         _("%s: %s flag is only allowed with the %s flag\n"),
		         Prog, "-m", "-d");
		usage (E_USAGE);
	}

#ifdef WITH_SELINUX
	if (user_selinux_range && !Zflg) {
		fprintf (stderr,
		         _("%s: %s flag is only allowed with the %s flag\n"),
		         Prog, "--selinux-range", "--selinux-user");
		usage (E_USAGE);
	}
#endif				/* WITH_SELINUX */

	if (user_newid == user_id) {
		uflg = false;
		oflg = false;
	}
	if (user_newgid == user_gid) {
		gflg = false;
	}
	if (   (NULL != user_newshell)
	    && (strcmp (user_newshell, user_shell) == 0)) {
		sflg = false;
	}
	if (strcmp (user_newname, user_name) == 0) {
		lflg = false;
	}
	if (user_newinactive == user_inactive) {
		fflg = false;
	}
	if (user_newexpire == user_expire) {
		eflg = false;
	}
	if (   (NULL != user_newhome)
	    && (strcmp (user_newhome, user_home) == 0)) {
		dflg = false;
		mflg = false;
	}
	if (   (NULL != user_newcomment)
	    && (strcmp (user_newcomment, user_comment) == 0)) {
		cflg = false;
	}

	if (!(Uflg || uflg || sflg || pflg || mflg || Lflg ||
	      lflg || Gflg || gflg || fflg || eflg || dflg || cflg
#ifdef ENABLE_SUBIDS
	      || vflg || Vflg || wflg || Wflg
#endif				/* ENABLE_SUBIDS */
#ifdef WITH_SELINUX
	      || Zflg
#endif				/* WITH_SELINUX */
	)) {
		fprintf (stdout, _("%s: no changes\n"), Prog);
		exit (E_SUCCESS);
	}

	if (!is_shadow_pwd && (eflg || fflg)) {
		fprintf (stderr,
		         _("%s: shadow passwords required for -e and -f\n"),
		         Prog);
		exit (E_USAGE);
	}

	/* local, no need for xgetpwnam */
	if (lflg && (prefix_getpwnam (user_newname) != NULL)) {
		fprintf (stderr,
		         _("%s: user '%s' already exists\n"),
		         Prog, user_newname);
		exit (E_NAME_IN_USE);
	}

	/* local, no need for xgetpwuid */
	if (uflg && !oflg && (prefix_getpwuid (user_newid) != NULL)) {
		fprintf (stderr,
		         _("%s: UID '%lu' already exists\n"),
		         Prog, (unsigned long) user_newid);
		exit (E_UID_IN_USE);
	}

#ifdef ENABLE_SUBIDS
	if (   (vflg || Vflg)
	    && !is_sub_uid) {
		fprintf (stderr,
		         _("%s: %s does not exist, you cannot use the flags %s or %s\n"),
		         Prog, sub_uid_dbname (), "-v", "-V");
		exit (E_USAGE);
	}

	if (   (wflg || Wflg)
	    && !is_sub_gid) {
		fprintf (stderr,
		         _("%s: %s does not exist, you cannot use the flags %s or %s\n"),
		         Prog, sub_gid_dbname (), "-w", "-W");
		exit (E_USAGE);
	}
#endif				/* ENABLE_SUBIDS */
}

/*
 * close_files - close all of the files that were opened
 *
 *	close_files() closes all of the files that were opened for this new
 *	user. This causes any modified entries to be written out.
 */
static void close_files (void)
{
	if (pw_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (E_PW_UPDATE);
	}
	if (is_shadow_pwd && (spw_close () == 0)) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, spw_dbname ());
		SYSLOG ((LOG_ERR,
		         "failure while writing changes to %s",
		         spw_dbname ()));
		fail_exit (E_PW_UPDATE);
	}

	if (Gflg || lflg) {
		if (gr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, gr_dbname ());
			SYSLOG ((LOG_ERR,
			         "failure while writing changes to %s",
			         gr_dbname ()));
			fail_exit (E_GRP_UPDATE);
		}
#ifdef SHADOWGRP
		if (is_shadow_grp) {
			if (sgr_close () == 0) {
				fprintf (stderr,
				         _("%s: failure while writing changes to %s\n"),
				         Prog, sgr_dbname ());
				SYSLOG ((LOG_ERR,
				         "failure while writing changes to %s",
				         sgr_dbname ()));
				fail_exit (E_GRP_UPDATE);
			}
		}
#endif
#ifdef SHADOWGRP
		if (is_shadow_grp) {
			if (sgr_unlock () == 0) {
				fprintf (stderr,
				         _("%s: failed to unlock %s\n"),
				         Prog, sgr_dbname ());
				SYSLOG ((LOG_ERR,
				         "failed to unlock %s",
				         sgr_dbname ()));
				/* continue */
			}
		}
#endif
		if (gr_unlock () == 0) {
			fprintf (stderr,
			         _("%s: failed to unlock %s\n"),
			         Prog, gr_dbname ());
			SYSLOG ((LOG_ERR,
			         "failed to unlock %s",
			         gr_dbname ()));
			/* continue */
		}
	}

	if (is_shadow_pwd) {
		if (spw_unlock () == 0) {
			fprintf (stderr,
			         _("%s: failed to unlock %s\n"),
			         Prog, spw_dbname ());
			SYSLOG ((LOG_ERR,
			         "failed to unlock %s",
			         spw_dbname ()));
			/* continue */
		}
	}
	if (pw_unlock () == 0) {
		fprintf (stderr,
		         _("%s: failed to unlock %s\n"),
		         Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}

	pw_locked = false;
	spw_locked = false;
	gr_locked = false;
#ifdef	SHADOWGRP
	sgr_locked = false;
#endif

#ifdef ENABLE_SUBIDS
	if (vflg || Vflg) {
		if (sub_uid_close () == 0) {
			fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, sub_uid_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", sub_uid_dbname ()));
			fail_exit (E_SUB_UID_UPDATE);
		}
		if (sub_uid_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sub_uid_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sub_uid_dbname ()));
			/* continue */
		}
		sub_uid_locked = false;
	}
	if (wflg || Wflg) {
		if (sub_gid_close () == 0) {
			fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, sub_gid_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", sub_gid_dbname ()));
			fail_exit (E_SUB_GID_UPDATE);
		}
		if (sub_gid_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sub_gid_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sub_gid_dbname ()));
			/* continue */
		}
		sub_gid_locked = false;
	}
#endif				/* ENABLE_SUBIDS */

	/*
	 * Close the DBM and/or flat files
	 */
	endpwent ();
	endspent ();
	endgrent ();
#ifdef	SHADOWGRP
	endsgent ();
#endif
}

/*
 * open_files - lock and open the password files
 *
 *	open_files() opens the two password files.
 */
static void open_files (void)
{
	if (pw_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, pw_dbname ());
		fail_exit (E_PW_UPDATE);
	}
	pw_locked = true;
	if (pw_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"),
		         Prog, pw_dbname ());
		fail_exit (E_PW_UPDATE);
	}
	if (is_shadow_pwd && (spw_lock () == 0)) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, spw_dbname ());
		fail_exit (E_PW_UPDATE);
	}
	spw_locked = true;
	if (is_shadow_pwd && (spw_open (O_CREAT | O_RDWR) == 0)) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"),
		         Prog, spw_dbname ());
		fail_exit (E_PW_UPDATE);
	}

	if (Gflg || lflg) {
		/*
		 * Lock and open the group file. This will load all of the
		 * group entries.
		 */
		if (gr_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, gr_dbname ());
			fail_exit (E_GRP_UPDATE);
		}
		gr_locked = true;
		if (gr_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, gr_dbname ());
			fail_exit (E_GRP_UPDATE);
		}
#ifdef SHADOWGRP
		if (is_shadow_grp && (sgr_lock () == 0)) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sgr_dbname ());
			fail_exit (E_GRP_UPDATE);
		}
		sgr_locked = true;
		if (is_shadow_grp && (sgr_open (O_CREAT | O_RDWR) == 0)) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sgr_dbname ());
			fail_exit (E_GRP_UPDATE);
		}
#endif
	}
#ifdef ENABLE_SUBIDS
	if (vflg || Vflg) {
		if (sub_uid_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sub_uid_dbname ());
			fail_exit (E_SUB_UID_UPDATE);
		}
		sub_uid_locked = true;
		if (sub_uid_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sub_uid_dbname ());
			fail_exit (E_SUB_UID_UPDATE);
		}
	}
	if (wflg || Wflg) {
		if (sub_gid_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sub_gid_dbname ());
			fail_exit (E_SUB_GID_UPDATE);
		}
		sub_gid_locked = true;
		if (sub_gid_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sub_gid_dbname ());
			fail_exit (E_SUB_GID_UPDATE);
		}
	}
#endif				/* ENABLE_SUBIDS */
}

/*
 * usr_update - create the user entries
 *
 *	usr_update() creates the password file entries for this user and
 *	will update the group entries if required.
 */
static void usr_update (void)
{
	struct passwd pwent;
	const struct passwd *pwd;

	struct spwd spent;
	const struct spwd *spwd = NULL;

	/*
	 * Locate the entry in /etc/passwd, which MUST exist.
	 */
	pwd = pw_locate (user_name);
	if (NULL == pwd) {
		fprintf (stderr,
		         _("%s: user '%s' does not exist in %s\n"),
		         Prog, user_name, pw_dbname ());
		fail_exit (E_NOTFOUND);
	}
	pwent = *pwd;
	new_pwent (&pwent);


	/* If the shadow file does not exist, it won't be created */
	if (is_shadow_pwd) {
		spwd = spw_locate (user_name);
		if (NULL != spwd) {
			/* Update the shadow entry if it exists */
			spent = *spwd;
			new_spent (&spent);
		} else if (   (    pflg
		               && (strcmp (pwent.pw_passwd, SHADOW_PASSWD_STRING) == 0))
		           || eflg || fflg) {
			/* In some cases, we force the creation of a
			 * shadow entry:
			 *  + new password requested and passwd indicates
			 *    a shadowed password
			 *  + aging information is requested
			 */
			bzero(&spent, sizeof spent);
			spent.sp_namp   = user_name;

			/* The user explicitly asked for a shadow feature.
			 * Enable shadowed passwords for this new account.
			 */
			spent.sp_pwdp   = xstrdup (pwent.pw_passwd);
			pwent.pw_passwd = xstrdup (SHADOW_PASSWD_STRING);

			spent.sp_lstchg = gettime () / DAY;
			if (0 == spent.sp_lstchg) {
				/* Better disable aging than
				 * requiring a password change */
				spent.sp_lstchg = -1;
			}
			spent.sp_min    = getdef_num ("PASS_MIN_DAYS", -1);
			spent.sp_max    = getdef_num ("PASS_MAX_DAYS", -1);
			spent.sp_warn   = getdef_num ("PASS_WARN_AGE", -1);
			spent.sp_inact  = -1;
			spent.sp_expire = -1;
			spent.sp_flag   = SHADOW_SP_FLAG_UNSET;
			new_spent (&spent);
			spwd = &spent; /* entry needs to be committed */
		}
	}

	if (lflg || uflg || gflg || cflg || dflg || sflg || pflg
	    || Lflg || Uflg) {
		if (pw_update (&pwent) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, pw_dbname (), pwent.pw_name);
			fail_exit (E_PW_UPDATE);
		}
		if (lflg && (pw_remove (user_name) == 0)) {
			fprintf (stderr,
			         _("%s: cannot remove entry '%s' from %s\n"),
			         Prog, user_name, pw_dbname ());
			fail_exit (E_PW_UPDATE);
		}
	}
	if ((NULL != spwd) && (lflg || eflg || fflg || pflg || Lflg || Uflg)) {
		if (spw_update (&spent) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, spw_dbname (), spent.sp_namp);
			fail_exit (E_PW_UPDATE);
		}
		if (lflg && (spw_remove (user_name) == 0)) {
			fprintf (stderr,
			         _("%s: cannot remove entry '%s' from %s\n"),
			         Prog, user_name, spw_dbname ());
			fail_exit (E_PW_UPDATE);
		}
	}
}

/*
 * move_home - move the user's home directory
 *
 *	move_home() moves the user's home directory to a new location. The
 *	files will be copied if the directory cannot simply be renamed.
 */
static void move_home (void)
{
	struct stat sb;

	if (access (prefix_user_newhome, F_OK) == 0) {
		/*
		 * If the new home directory already exist, the user
		 * should not use -m.
		 */
		fprintf (stderr,
		         _("%s: directory %s exists\n"),
		         Prog, user_newhome);
		fail_exit (E_HOMEDIR);
	}

	if (stat (prefix_user_home, &sb) == 0) {
		/*
		 * Don't try to move it if it is not a directory
		 * (but /dev/null for example).  --marekm
		 */
		if (!S_ISDIR (sb.st_mode)) {
			fprintf (stderr,
			         _("%s: The previous home directory (%s) was "
			           "not a directory. It is not removed and no "
			           "home directories are created.\n"),
			         Prog, user_home);
			fail_exit (E_HOMEDIR);
		}

#ifdef WITH_AUDIT
		if (uflg || gflg) {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
				      "changing home directory owner",
				      user_newname, user_newid, 1);
		}
#endif

		if (rename (prefix_user_home, prefix_user_newhome) == 0) {
			/* FIXME: rename above may have broken symlinks
			 *        pointing to the user's home directory
			 *        with an absolute path. */
			if (chown_tree (prefix_user_newhome,
			                user_id,  uflg ? user_newid  : (uid_t)-1,
			                user_gid, gflg ? user_newgid : (gid_t)-1) != 0) {
				fprintf (stderr,
				         _("%s: Failed to change ownership of the home directory"),
				         Prog);
				fail_exit (E_HOMEDIR);
			}
#ifdef WITH_AUDIT
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "moving home directory",
			              user_newname, user_newid, 1);
#endif
			return;
		} else {
			if (EXDEV == errno) {
#ifdef WITH_BTRFS
				if (btrfs_is_subvolume (prefix_user_home) > 0) {
					fprintf (stderr,
					        _("%s: error: cannot move subvolume from %s to %s - different device\n"),
					        Prog, prefix_user_home, prefix_user_newhome);
					fail_exit (E_HOMEDIR);
				}
#endif

				if (copy_tree (prefix_user_home, prefix_user_newhome, true,
				               true,
				               user_id,
				               uflg ? user_newid : (uid_t)-1,
				               user_gid,
				               gflg ? user_newgid : (gid_t)-1) == 0) {
					if (remove_tree (prefix_user_home, true) != 0) {
						fprintf (stderr,
						         _("%s: warning: failed to completely remove old home directory %s"),
						         Prog, prefix_user_home);
					}
#ifdef WITH_AUDIT
					audit_logger (AUDIT_USER_CHAUTHTOK,
					              Prog,
					              "moving home directory",
					              user_newname,
					              user_newid,
					              1);
#endif
					return;
				}

				(void) remove_tree (prefix_user_newhome, true);
			}
			fprintf (stderr,
			         _("%s: cannot rename directory %s to %s\n"),
			         Prog, prefix_user_home, prefix_user_newhome);
			fail_exit (E_HOMEDIR);
		}
	} else {
		fprintf (stderr,
		         _("%s: The previous home directory (%s) does not "
		           "exist or is inaccessible. Move cannot be completed.\n"),
		         Prog, prefix_user_home);
	}
}

/*
 * update_lastlog - update the lastlog file
 *
 * Relocate the "lastlog" entries for the user. The old entry is
 * left alone in case the UID was shared. It doesn't hurt anything
 * to just leave it be.
 */
#ifdef ENABLE_LASTLOG
static void update_lastlog (void)
{
	struct lastlog ll;
	int fd;
	off_t off_uid = (off_t) user_id * sizeof ll;
	off_t off_newuid = (off_t) user_newid * sizeof ll;
	uid_t max_uid;

	if (access (LASTLOG_FILE, F_OK) != 0) {
		return;
	}

	max_uid = getdef_ulong ("LASTLOG_UID_MAX", 0xFFFFFFFFUL);
	if (user_newid > max_uid) {
		/* do not touch lastlog for large uids */
		return;
	}

	fd = open (LASTLOG_FILE, O_RDWR);

	if (-1 == fd) {
		fprintf (stderr,
		         _("%s: failed to copy the lastlog entry of user %lu to user %lu: %s\n"),
		         Prog, (unsigned long) user_id, (unsigned long) user_newid, strerror (errno));
		return;
	}

	if (   (lseek (fd, off_uid, SEEK_SET) == off_uid)
	    && (read (fd, &ll, sizeof ll) == (ssize_t) sizeof ll)) {
		/* Copy the old entry to its new location */
		if (   (lseek (fd, off_newuid, SEEK_SET) != off_newuid)
		    || (write_full(fd, &ll, sizeof ll) == -1)
		    || (fsync (fd) != 0)) {
			fprintf (stderr,
			         _("%s: failed to copy the lastlog entry of user %lu to user %lu: %s\n"),
			         Prog, (unsigned long) user_id, (unsigned long) user_newid, strerror (errno));
		}
	} else {
		/* Assume lseek or read failed because there is
		 * no entry for the old UID */

		/* Check if the new UID already has an entry */
		if (   (lseek (fd, off_newuid, SEEK_SET) == off_newuid)
		    && (read (fd, &ll, sizeof ll) == (ssize_t) sizeof ll)) {
			/* Reset the new uid's lastlog entry */
			memzero (&ll, sizeof (ll));
			if (   (lseek (fd, off_newuid, SEEK_SET) != off_newuid)
			    || (write_full(fd, &ll, sizeof ll) == -1)
			    || (fsync (fd) != 0)) {
				fprintf (stderr,
				         _("%s: failed to copy the lastlog entry of user %lu to user %lu: %s\n"),
				         Prog, (unsigned long) user_id, (unsigned long) user_newid, strerror (errno));
			}
		}
	}

	if (close (fd) != 0 && errno != EINTR) {
		fprintf (stderr,
		         _("%s: failed to copy the lastlog entry of user %ju to user %ju: %s\n"),
		         Prog, (uintmax_t) user_id, (uintmax_t) user_newid, strerror (errno));
	}
}
#endif /* ENABLE_LASTLOG */

/*
 * update_faillog - update the faillog file
 *
 * Relocate the "faillog" entries for the user. The old entry is
 * left alone in case the UID was shared. It doesn't hurt anything
 * to just leave it be.
 */
static void update_faillog (void)
{
	struct faillog fl;
	int fd;
	off_t off_uid = (off_t) user_id * sizeof fl;
	off_t off_newuid = (off_t) user_newid * sizeof fl;

	if (access (FAILLOG_FILE, F_OK) != 0) {
		return;
	}

	fd = open (FAILLOG_FILE, O_RDWR);

	if (-1 == fd) {
		fprintf (stderr,
		         _("%s: failed to copy the faillog entry of user %lu to user %lu: %s\n"),
		         Prog, (unsigned long) user_id, (unsigned long) user_newid, strerror (errno));
		return;
	}

	if (   (lseek (fd, off_uid, SEEK_SET) == off_uid)
	    && (read (fd, &fl, sizeof fl) == (ssize_t) sizeof fl)) {
		/* Copy the old entry to its new location */
		if (   (lseek (fd, off_newuid, SEEK_SET) != off_newuid)
		    || (write_full(fd, &fl, sizeof fl) == -1)
		    || (fsync (fd) != 0)) {
			fprintf (stderr,
			         _("%s: failed to copy the faillog entry of user %lu to user %lu: %s\n"),
			         Prog, (unsigned long) user_id, (unsigned long) user_newid, strerror (errno));
		}
	} else {
		/* Assume lseek or read failed because there is
		 * no entry for the old UID */

		/* Check if the new UID already has an entry */
		if (   (lseek (fd, off_newuid, SEEK_SET) == off_newuid)
		    && (read (fd, &fl, sizeof fl) == (ssize_t) sizeof fl)) {
			/* Reset the new uid's faillog entry */
			memzero (&fl, sizeof (fl));
			if (   (lseek (fd, off_newuid, SEEK_SET) != off_newuid)
			    || (write_full(fd, &fl, sizeof fl) == -1))
			{
				fprintf (stderr,
				         _("%s: failed to copy the faillog entry of user %lu to user %lu: %s\n"),
				         Prog, (unsigned long) user_id, (unsigned long) user_newid, strerror (errno));
			}
		}
	}

	if (close (fd) != 0 && errno != EINTR) {
		fprintf (stderr,
		         _("%s: failed to copy the faillog entry of user %ju to user %ju: %s\n"),
		         Prog, (uintmax_t) user_id, (uintmax_t) user_newid, strerror (errno));
	}
}

#ifndef NO_MOVE_MAILBOX
/*
 * This is the new and improved code to carefully chown/rename the user's
 * mailbox. Maybe I am too paranoid but the mail spool dir sometimes
 * happens to be mode 1777 (this makes mail user agents work without
 * being setgid mail, but is NOT recommended; they all should be fixed
 * to use movemail).  --marekm
 */
static void move_mailbox (void)
{
	int          fd;
	char         *mailfile;
	const char   *maildir;
	struct stat  st;

	maildir = getdef_str ("MAIL_DIR");
#ifdef MAIL_SPOOL_DIR
	if ((NULL == maildir) && (getdef_str ("MAIL_FILE") == NULL)) {
		maildir = MAIL_SPOOL_DIR;
	}
#endif
	if (NULL == maildir) {
		return;
	}

	/*
	 * O_NONBLOCK is to make sure open won't hang on mandatory locks.
	 * We do fstat/fchown to make sure there are no races (someone
	 * replacing /var/spool/mail/luser with a hard link to /etc/passwd
	 * between stat and chown).  --marekm
	 */
	if (prefix[0]) {
		xasprintf(&mailfile, "%s/%s/%s", prefix, maildir, user_name);
	} else {
		xasprintf(&mailfile, "%s/%s", maildir, user_name);
	}

	fd = open (mailfile, O_RDONLY | O_NONBLOCK, 0);
	if (fd < 0) {
		/* no need for warnings if the mailbox doesn't exist */
		if (errno != ENOENT) {
			perror (mailfile);
		}
		free(mailfile);
		return;
	}
	if (fstat (fd, &st) < 0) {
		perror ("fstat");
		(void) close (fd);
		free(mailfile);
		return;
	}
	if (st.st_uid != user_id) {
		/* better leave it alone */
		fprintf (stderr, _("%s: warning: %s not owned by %s\n"),
		         Prog, mailfile, user_name);
		(void) close (fd);
		free(mailfile);
		return;
	}
	if (uflg) {
		if (fchown (fd, user_newid, (gid_t) -1) < 0) {
			perror (_("failed to change mailbox owner"));
		}
#ifdef WITH_AUDIT
		else {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "changing mail file owner",
			              user_newname, user_newid, 1);
		}
#endif
	}

	(void) close (fd);

	if (lflg) {
		char  *newmailfile;

		if (prefix[0]) {
			xasprintf(&newmailfile, "%s/%s/%s",
			          prefix, maildir, user_newname);
		} else {
			xasprintf(&newmailfile, "%s/%s", maildir, user_newname);
		}
		if (   (link (mailfile, newmailfile) != 0)
		    || (unlink (mailfile) != 0)) {
			perror (_("failed to rename mailbox"));
		}
#ifdef WITH_AUDIT
		else {
			audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
			              "changing mail file name",
			              user_newname, user_newid, 1);
		}

		free(newmailfile);
#endif
	}

	free(mailfile);
}
#endif

/*
 * main - usermod command
 */
int main (int argc, char **argv)
{
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
	pam_handle_t *pamh = NULL;
	int retval;
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);
	prefix = process_prefix_flag ("-P", argc, argv);

	OPENLOG (Prog);
#ifdef WITH_AUDIT
	audit_help_open ();
#endif

	sys_ngroups = sysconf (_SC_NGROUPS_MAX);
	user_groups = XMALLOC(sys_ngroups + 1, char *);
	user_groups[0] = NULL;

	is_shadow_pwd = spw_file_present ();
#ifdef SHADOWGRP
	is_shadow_grp = sgr_file_present ();
#endif
#ifdef ENABLE_SUBIDS
	is_sub_uid = sub_uid_file_present ();
	is_sub_gid = sub_gid_file_present ();
#endif				/* ENABLE_SUBIDS */

	process_flags (argc, argv);

	/*
	 * The home directory, the username and the user's UID should not
	 * be changed while the user is logged in.
	 * Note: no need to check if a prefix is specified...
	 */
	if ( (prefix[0] == '\0') &&  (uflg || lflg || dflg
#ifdef ENABLE_SUBIDS
	        || Vflg || Wflg
#endif				/* ENABLE_SUBIDS */
	       )
	    && (user_busy (user_name, user_id) != 0)) {
		exit (E_USER_BUSY);
	}

#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
	{
		struct passwd *pampw;
		pampw = getpwuid (getuid ()); /* local, no need for xgetpwuid */
		if (pampw == NULL) {
			fprintf (stderr,
			         _("%s: Cannot determine your user name.\n"),
			         Prog);
			exit (1);
		}

		retval = pam_start (Prog, pampw->pw_name, &conv, &pamh);
	}

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

#ifdef WITH_TCB
	if (shadowtcb_set_user (user_name) == SHADOWTCB_FAILURE) {
		exit (E_PW_UPDATE);
	}
#endif

	/*
	 * Do the hard stuff - open the files, change the user entries,
	 * change the home directory, then close and update the files.
	 */
	open_files ();
	if (   cflg || dflg || eflg || fflg || gflg || Lflg || lflg || pflg
	    || sflg || uflg || Uflg) {
		usr_update ();
	}
	if (Gflg || lflg) {
		grp_update ();
	}
#ifdef ENABLE_SUBIDS
	if (Vflg) {
		struct ulong_range_list_entry *ptr;
		for (ptr = del_sub_uids; ptr != NULL; ptr = ptr->next) {
			unsigned long count = ptr->range.last - ptr->range.first + 1;
			if (sub_uid_remove(user_name, ptr->range.first, count) == 0) {
				fprintf (stderr,
					_("%s: failed to remove uid range %lu-%lu from '%s'\n"),
					Prog, ptr->range.first, ptr->range.last,
					sub_uid_dbname ());
				fail_exit (E_SUB_UID_UPDATE);
			}
		}
	}
	if (vflg) {
		struct ulong_range_list_entry *ptr;
		for (ptr = add_sub_uids; ptr != NULL; ptr = ptr->next) {
			unsigned long count = ptr->range.last - ptr->range.first + 1;
			if (sub_uid_add(user_name, ptr->range.first, count) == 0) {
				fprintf (stderr,
					_("%s: failed to add uid range %lu-%lu to '%s'\n"),
					Prog, ptr->range.first, ptr->range.last,
					sub_uid_dbname ());
				fail_exit (E_SUB_UID_UPDATE);
			}
		}
	}
	if (Wflg) {
		struct ulong_range_list_entry *ptr;
		for (ptr = del_sub_gids; ptr != NULL; ptr = ptr->next) {
			unsigned long count = ptr->range.last - ptr->range.first + 1;
			if (sub_gid_remove(user_name, ptr->range.first, count) == 0) {
				fprintf (stderr,
					_("%s: failed to remove gid range %lu-%lu from '%s'\n"),
					Prog, ptr->range.first, ptr->range.last,
					sub_gid_dbname ());
				fail_exit (E_SUB_GID_UPDATE);
			}
		}
	}
	if (wflg) {
		struct ulong_range_list_entry *ptr;
		for (ptr = add_sub_gids; ptr != NULL; ptr = ptr->next) {
			unsigned long count = ptr->range.last - ptr->range.first + 1;
			if (sub_gid_add(user_name, ptr->range.first, count) == 0) {
				fprintf (stderr,
					_("%s: failed to add gid range %lu-%lu to '%s'\n"),
					Prog, ptr->range.first, ptr->range.last,
					sub_gid_dbname ());
				fail_exit (E_SUB_GID_UPDATE);
			}
		}
	}
#endif				/* ENABLE_SUBIDS */
	close_files ();

#ifdef WITH_TCB
	if (   (lflg || uflg)
	    && (shadowtcb_move (user_newname, user_newid) == SHADOWTCB_FAILURE) ) {
		exit (E_PW_UPDATE);
	}
#endif

	nscd_flush_cache ("passwd");
	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_PASSWD | SSSD_DB_GROUP);

#ifdef WITH_SELINUX
	if (Zflg) {
		if ('\0' != *user_selinux) {
			if (set_seuser (user_name, user_selinux, user_selinux_range) != 0) {
				fprintf (stderr,
				         _("%s: warning: the user name %s to %s SELinux user mapping failed.\n"),
				         Prog, user_name, user_selinux);
#ifdef WITH_AUDIT
				audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
				              "modifying User mapping ",
				              user_name, user_id,
				              SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
				fail_exit (E_SE_UPDATE);
			}
		} else {
			if (del_seuser (user_name) != 0) {
				fprintf (stderr,
				         _("%s: warning: the user name %s to SELinux user mapping removal failed.\n"),
				         Prog, user_name);
#ifdef WITH_AUDIT
				audit_logger (AUDIT_ADD_USER, Prog,
				              "removing SELinux user mapping",
				              user_name, user_id,
				              SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
				fail_exit (E_SE_UPDATE);
			}
		}
	}
#endif				/* WITH_SELINUX */

	if (mflg) {
		move_home ();
	}

#ifndef NO_MOVE_MAILBOX
	if (lflg || uflg) {
		move_mailbox ();
	}
#endif				/* NO_MOVE_MAILBOX */

	if (uflg) {
#ifdef ENABLE_LASTLOG
		update_lastlog ();
#endif /* ENABLE_LASTLOG */
		update_faillog ();
	}

	if (!mflg && (uflg || gflg)) {
		struct stat sb;

		if (stat (dflg ? prefix_user_newhome : prefix_user_home, &sb) == 0 &&
			((uflg && sb.st_uid == user_newid) || sb.st_uid == user_id)) {
			/*
			 * Change the UID on all of the files owned by
			 * `user_id' to `user_newid' in the user's home
			 * directory.
			 *
			 * move_home() already takes care of changing the
			 * ownership.
			 *
			 */
#ifdef WITH_AUDIT
			if (uflg || gflg) {
				audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
					      "changing home directory owner",
					      user_newname, user_newid, 1);
			}
#endif
			if (chown_tree (dflg ? prefix_user_newhome : prefix_user_home,
			                user_id,
			                uflg ? user_newid  : (uid_t)-1,
			                user_gid,
			                gflg ? user_newgid : (gid_t)-1) != 0) {
				fprintf (stderr,
				         _("%s: Failed to change ownership of the home directory"),
				         Prog);
				fail_exit (E_HOMEDIR);
			}
		}
	}

	return E_SUCCESS;
}

