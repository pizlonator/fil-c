/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2000 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2012, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "alloc.h"
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
#include "defines.h"
#include "getdef.h"
#include "groupio.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#include "pwauth.h"
#include "pwio.h"
#include "shadowio.h"
#ifdef	SHADOWGRP
#include "sgroupio.h"
#endif				/* SHADOWGRP */
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif				/* WITH_SELINUX */
#ifdef WITH_TCB
#include <tcb.h>
#include "tcbfuncs.h"
#endif				/* WITH_TCB */
#include "run_part.h"
/*@-exitarg@*/
#include "exitcodes.h"
#ifdef ENABLE_SUBIDS
#include "subordinateio.h"
#endif				/* ENABLE_SUBIDS */
#include "shadowlog.h"
#include "string/sprintf.h"


/*
 * exit status values
 */
#define E_PW_UPDATE	1	/* can't update password file */
#define E_NOTFOUND	6	/* specified user doesn't exist */
#define E_USER_BUSY	8	/* user currently logged in */
#define E_GRP_UPDATE	10	/* can't update group file */
#define E_HOMEDIR	12	/* can't remove home directory */
#define E_SE_UPDATE	14	/* can't update SELinux user mapping */
#ifdef ENABLE_SUBIDS
#define E_SUB_UID_UPDATE 16	/* can't update the subordinate uid file */
#define E_SUB_GID_UPDATE 18	/* can't update the subordinate gid file */
#endif				/* ENABLE_SUBIDS */

/*
 * Global variables
 */
static const char Prog[] = "userdel";

static char *user_name;
static uid_t user_id;
static gid_t user_gid;
static char *user_home;

static bool fflg = false;
static bool rflg = false;
#ifdef WITH_SELINUX
static bool Zflg = false;
#endif
static bool Rflg = false;

static bool is_shadow_pwd;

#ifdef SHADOWGRP
static bool is_shadow_grp;
static bool sgr_locked = false;
#endif				/* SHADOWGRP */
static bool pw_locked  = false;
static bool gr_locked   = false;
static bool spw_locked  = false;
#ifdef ENABLE_SUBIDS
static bool is_sub_uid;
static bool is_sub_gid;
static bool sub_uid_locked = false;
static bool sub_gid_locked = false;
#endif				/* ENABLE_SUBIDS */

static const char* prefix = "";

/* local function prototypes */
static void usage (int status);
static void update_groups (void);
static void remove_usergroup (void);
static void close_files (void);
static void fail_exit (int);
static void open_files (void);
static void update_user (void);
static void user_cancel (const char *);

#ifdef EXTRA_CHECK_HOME_DIR
static bool path_prefix (const char *, const char *);
#endif				/* EXTRA_CHECK_HOME_DIR */
static int is_owner (uid_t, const char *);
static int remove_mailbox (void);
#ifdef WITH_TCB
static int remove_tcbdir (const char *user_name, uid_t user_id);
#endif				/* WITH_TCB */

/*
 * usage - display usage message and exit
 */
static void usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options] LOGIN\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -f, --force                   force some actions that would fail otherwise\n"
	                "                                e.g. removal of user still logged in\n"
	                "                                or files, even if not owned by the user\n"),
	              usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -r, --remove                  remove home directory and mail spool\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -P, --prefix PREFIX_DIR       prefix directory where are located the /etc/* files\n"), usageout);
#ifdef WITH_SELINUX
	(void) fputs (_("  -Z, --selinux-user            remove any SELinux user mapping for the user\n"), usageout);
#endif				/* WITH_SELINUX */
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * update_groups - delete user from secondary group set
 *
 *	update_groups() takes the user name that was given and searches
 *	the group files for membership in any group.
 *
 *	we also check to see if they have any groups they own (the same
 *	name is their user name) and delete them too (only if USERGROUPS_ENAB
 *	is enabled).
 */
static void update_groups (void)
{
	const struct group *grp;
	struct group *ngrp;

#ifdef	SHADOWGRP
	const struct sgrp *sgrp;
	struct sgrp *nsgrp;
#endif				/* SHADOWGRP */

	/*
	 * Scan through the entire group file looking for the groups that
	 * the user is a member of.
	 */
	for (gr_rewind (), grp = gr_next (); NULL != grp; grp = gr_next ()) {

		/*
		 * See if the user specified this group as one of their
		 * concurrent groups.
		 */
		if (!is_on_list (grp->gr_mem, user_name)) {
			continue;
		}

		/*
		 * Delete the username from the list of group members and
		 * update the group entry to reflect the change.
		 */
		ngrp = __gr_dup (grp);
		if (NULL == ngrp) {
			fprintf (stderr,
			         _("%s: Out of memory. Cannot update %s.\n"),
			         Prog, gr_dbname ());
			exit (13);	/* XXX */
		}
		ngrp->gr_mem = del_list (ngrp->gr_mem, user_name);
		if (gr_update (ngrp) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, gr_dbname (), ngrp->gr_name);
			exit (E_GRP_UPDATE);
		}

		/*
		 * Update the DBM group file with the new entry as well.
		 */
#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_USER, Prog,
		              "deleting user from group",
		              user_name, user_id, SHADOW_AUDIT_SUCCESS);
#endif				/* WITH_AUDIT */
		SYSLOG ((LOG_INFO, "delete '%s' from group '%s'\n",
			 user_name, ngrp->gr_name));
	}

	if (getdef_bool ("USERGROUPS_ENAB")) {
		remove_usergroup ();
	}

#ifdef	SHADOWGRP
	if (!is_shadow_grp) {
		return;
	}

	/*
	 * Scan through the entire shadow group file looking for the groups
	 * that the user is a member of. Both the administrative list and
	 * the ordinary membership list is checked.
	 */
	for (sgr_rewind (), sgrp = sgr_next ();
	     NULL != sgrp;
	     sgrp = sgr_next ()) {
		bool was_member, was_admin;

		/*
		 * See if the user specified this group as one of their
		 * concurrent groups.
		 */
		was_member = is_on_list (sgrp->sg_mem, user_name);
		was_admin = is_on_list (sgrp->sg_adm, user_name);

		if (!was_member && !was_admin) {
			continue;
		}

		nsgrp = __sgr_dup (sgrp);
		if (NULL == nsgrp) {
			fprintf (stderr,
			         _("%s: Out of memory. Cannot update %s.\n"),
			         Prog, sgr_dbname ());
			exit (13);	/* XXX */
		}

		if (was_member) {
			nsgrp->sg_mem = del_list (nsgrp->sg_mem, user_name);
		}

		if (was_admin) {
			nsgrp->sg_adm = del_list (nsgrp->sg_adm, user_name);
		}

		if (sgr_update (nsgrp) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, sgr_dbname (), nsgrp->sg_name);
			exit (E_GRP_UPDATE);
		}
#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_USER, Prog,
		              "deleting user from shadow group",
		              user_name, user_id, SHADOW_AUDIT_SUCCESS);
#endif				/* WITH_AUDIT */
		SYSLOG ((LOG_INFO, "delete '%s' from shadow group '%s'\n",
		         user_name, nsgrp->sg_name));
	}
#endif				/* SHADOWGRP */
}

/*
 * remove_usergroup - delete the user's group if it is a usergroup
 *
 *	An usergroup is removed if
 *	  + it has the same name as the user
 *	  + it is the primary group of the user
 *	  + it has no other members
 *	  + it is not the primary group of any other user
 */
static void remove_usergroup (void)
{
	const struct group *grp;
	const struct passwd *pwd = NULL;

	grp = gr_locate (user_name);
	if (NULL == grp) {
		/* This user has no usergroup. */
		return;
	}

	if (grp->gr_gid != user_gid) {
		fprintf (stderr,
		         _("%s: group %s not removed because it is not the primary group of user %s.\n"),
		         Prog, grp->gr_name, user_name);
		return;
	}

	if (NULL != grp->gr_mem[0]) {
		/* The usergroup has other members. */
		fprintf (stderr,
		         _("%s: group %s not removed because it has other members.\n"),
		         Prog, grp->gr_name);
		return;
	}

	if (!fflg) {
		/*
		 * Scan the passwd file to check if this group is still
		 * used as a primary group.
		 */
		prefix_setpwent ();
		while ((pwd = prefix_getpwent ()) != NULL) {
			if (strcmp (pwd->pw_name, user_name) == 0) {
				continue;
			}
			if (pwd->pw_gid == grp->gr_gid) {
				fprintf (stderr,
				         _("%s: group %s is the primary group of another user and is not removed.\n"),
				         Prog, grp->gr_name);
				break;
			}
		}
		prefix_endpwent ();
	}

	if (NULL == pwd) {
		/*
		 * We can remove this group, it is not the primary
		 * group of any remaining user.
		 */
		if (gr_remove (user_name) == 0) {
			fprintf (stderr,
			         _("%s: cannot remove entry '%s' from %s\n"),
			         Prog, user_name, gr_dbname ());
			fail_exit (E_GRP_UPDATE);
		}

#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_GROUP, Prog,
		              "deleting group",
		              user_name, AUDIT_NO_ID,
		              SHADOW_AUDIT_SUCCESS);
#endif				/* WITH_AUDIT */
		SYSLOG ((LOG_INFO,
		         "removed group '%s' owned by '%s'\n",
		         user_name, user_name));

#ifdef	SHADOWGRP
		if (sgr_locate (user_name) != NULL) {
			if (sgr_remove (user_name) == 0) {
				fprintf (stderr,
				         _("%s: cannot remove entry '%s' from %s\n"),
				         Prog, user_name, sgr_dbname ());
				fail_exit (E_GRP_UPDATE);
			}
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_GROUP, Prog,
			              "deleting shadow group",
			              user_name, AUDIT_NO_ID,
			              SHADOW_AUDIT_SUCCESS);
#endif				/* WITH_AUDIT */
			SYSLOG ((LOG_INFO,
			         "removed shadow group '%s' owned by '%s'\n",
			         user_name, user_name));

		}
#endif				/* SHADOWGRP */
	}
}

/*
 * close_files - close all of the files that were opened
 *
 *	close_files() closes all of the files that were opened for this
 *	new user. This causes any modified entries to be written out.
 */
static void close_files (void)
{
	if (pw_close () == 0) {
		fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", pw_dbname ()));
		fail_exit (E_PW_UPDATE);
	}
	if (pw_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
		/* continue */
	}
	pw_locked = false;

	if (is_shadow_pwd) {
		if (spw_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"), Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", spw_dbname ()));
			fail_exit (E_PW_UPDATE);
		}
		if (spw_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, spw_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
			/* continue */
		}
		spw_locked = false;
	}

	if (gr_close () == 0) {
		fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", gr_dbname ()));
		fail_exit (E_GRP_UPDATE);
	}
	if (gr_unlock () == 0) {
		fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
		/* continue */
	}
	gr_locked = false;

#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"), Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", sgr_dbname ()));
			fail_exit (E_GRP_UPDATE);
		}

		if (sgr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
			/* continue */
		}
		sgr_locked = false;
	}
#endif				/* SHADOWGRP */

#ifdef ENABLE_SUBIDS
	if (is_sub_uid) {
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

	if (is_sub_gid) {
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
}

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
	if (gr_locked) {
		if (gr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, gr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
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
#ifdef	SHADOWGRP
	if (sgr_locked) {
		if (sgr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
			/* continue */
		}
	}
#endif				/* SHADOWGRP */
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
	audit_logger (AUDIT_DEL_USER, Prog,
	              "deleting user",
	              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */

	exit (code);
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
#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_USER, Prog,
		              "locking password file",
		              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
		fail_exit (E_PW_UPDATE);
	}
	pw_locked = true;
	if (pw_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"), Prog, pw_dbname ());
#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_USER, Prog,
		              "opening password file",
		              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
		fail_exit (E_PW_UPDATE);
	}
	if (is_shadow_pwd) {
		if (spw_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, spw_dbname ());
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
			              "locking shadow password file",
			              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_PW_UPDATE);
		}
		spw_locked = true;
		if (spw_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, spw_dbname ());
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
			              "opening shadow password file",
			              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_PW_UPDATE);
		}
	}
	if (gr_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, gr_dbname ());
#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_USER, Prog,
		              "locking group file",
		              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
		fail_exit (E_GRP_UPDATE);
	}
	gr_locked = true;
	if (gr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, gr_dbname ());
#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_USER, Prog,
		              "opening group file",
		              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
		fail_exit (E_GRP_UPDATE);
	}
#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sgr_dbname ());
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
			              "locking shadow group file",
			              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_GRP_UPDATE);
		}
		sgr_locked= true;
		if (sgr_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr, _("%s: cannot open %s\n"),
			         Prog, sgr_dbname ());
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
			              "opening shadow group file",
			              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_GRP_UPDATE);
		}
	}
#endif				/* SHADOWGRP */
#ifdef ENABLE_SUBIDS
	if (is_sub_uid) {
		if (sub_uid_lock () == 0) {
			fprintf (stderr,
				_("%s: cannot lock %s; try again later.\n"),
				Prog, sub_uid_dbname ());
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
				"locking subordinate user file",
				user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_SUB_UID_UPDATE);
		}
		sub_uid_locked = true;
		if (sub_uid_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
				_("%s: cannot open %s\n"), Prog, sub_uid_dbname ());
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
				"opening subordinate user file",
				user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_SUB_UID_UPDATE);
		}
	}
	if (is_sub_gid) {
		if (sub_gid_lock () == 0) {
			fprintf (stderr,
				_("%s: cannot lock %s; try again later.\n"),
				Prog, sub_gid_dbname ());
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
				"locking subordinate group file",
				user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_SUB_GID_UPDATE);
		}
		sub_gid_locked = true;
		if (sub_gid_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
				_("%s: cannot open %s\n"), Prog, sub_gid_dbname ());
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
				"opening subordinate group file",
				user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_SUB_GID_UPDATE);
		}
	}
#endif				/* ENABLE_SUBIDS */
}

/*
 * update_user - delete the user entries
 *
 *	update_user() deletes the password file entries for this user
 *	and will update the group entries as required.
 */
static void update_user (void)
{
	if (pw_remove (user_name) == 0) {
		fprintf (stderr,
		         _("%s: cannot remove entry '%s' from %s\n"),
		         Prog, user_name, pw_dbname ());
		fail_exit (E_PW_UPDATE);
	}
	if (   is_shadow_pwd
	    && (spw_locate (user_name) != NULL)
	    && (spw_remove (user_name) == 0)) {
		fprintf (stderr,
		         _("%s: cannot remove entry '%s' from %s\n"),
		         Prog, user_name, spw_dbname ());
		fail_exit (E_PW_UPDATE);
	}
#ifdef ENABLE_SUBIDS
	if (is_sub_uid && sub_uid_remove(user_name, 0, ULONG_MAX) == 0) {
		fprintf (stderr,
			_("%s: cannot remove entry %lu from %s\n"),
			Prog, (unsigned long)user_id, sub_uid_dbname ());
		fail_exit (E_SUB_UID_UPDATE);
	}
	if (is_sub_gid && sub_gid_remove(user_name, 0, ULONG_MAX) == 0) {
		fprintf (stderr,
			_("%s: cannot remove entry %lu from %s\n"),
			Prog, (unsigned long)user_id, sub_gid_dbname ());
		fail_exit (E_SUB_GID_UPDATE);
	}
#endif				/* ENABLE_SUBIDS */
#ifdef WITH_AUDIT
	audit_logger (AUDIT_DEL_USER, Prog,
	              "deleting user entries",
	              user_name, user_id, SHADOW_AUDIT_SUCCESS);
#endif				/* WITH_AUDIT */
	SYSLOG ((LOG_INFO, "delete user '%s'\n", user_name));
}

/*
 * user_cancel - cancel cron and at jobs
 *
 *	user_cancel calls a script for additional cleanups like removal of
 *	cron, at, or print jobs.
 */

static void user_cancel (const char *user)
{
	const char *cmd;
	const char *argv[3];
	int status;

	cmd = getdef_str ("USERDEL_CMD");
	if (NULL == cmd) {
		return;
	}
	argv[0] = cmd;
	argv[1] = user;
	argv[2] = NULL;
	(void) run_command (cmd, argv, NULL, &status);
}

#ifdef EXTRA_CHECK_HOME_DIR
static bool path_prefix (const char *s1, const char *s2)
{
	return (   (strncmp (s2, s1, strlen (s1)) == 0)
	        && (   ('\0' == s2[strlen (s1)])
	            || ('/'  == s2[strlen (s1)])));
}
#endif				/* EXTRA_CHECK_HOME_DIR */

/*
 * is_owner - Check if path is owned by uid
 *
 * Return
 *  1: path exists and is owned by uid
 *  0: path is not owned by uid, or a failure occurred
 * -1: path does not exist
 */
static int is_owner (uid_t uid, const char *path)
{
	struct stat st;

	errno = 0;
	if (stat (path, &st) != 0) {
		if ((ENOENT == errno) || (ENOTDIR == errno)) {
			/* The file or directory does not exist */
			return -1;
		} else {
			return 0;
		}
	}
	return (st.st_uid == uid) ? 1 : 0;
}

static int remove_mailbox (void)
{
	int         i, errors = 0;
	char        *mailfile;
	const char  *maildir;

	maildir = getdef_str ("MAIL_DIR");
#ifdef MAIL_SPOOL_DIR
	if ((NULL == maildir) && (getdef_str ("MAIL_FILE") == NULL)) {
		maildir = MAIL_SPOOL_DIR;
	}
#endif				/* MAIL_SPOOL_DIR */
	if (NULL == maildir) {
		return 0;
	}

	if (prefix[0]) {
		xasprintf(&mailfile, "%s/%s/%s", prefix, maildir, user_name);
	} else {
		xasprintf(&mailfile, "%s/%s", maildir, user_name);
	}

	if (access (mailfile, F_OK) != 0) {
		if (ENOENT == errno) {
			fprintf (stderr,
			         _("%s: %s mail spool (%s) not found\n"),
			         Prog, user_name, mailfile);
			free(mailfile);
			return 0;
		} else {
			fprintf (stderr,
			         _("%s: warning: can't remove %s: %s\n"),
			         Prog, mailfile, strerror (errno));
			SYSLOG ((LOG_ERR, "Cannot remove %s: %s", mailfile, strerror (errno)));
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
			              "deleting mail file",
			              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			free(mailfile);
			return -1;
		}
	}

	if (fflg) {
		if (unlink (mailfile) != 0) {
			fprintf (stderr,
			         _("%s: warning: can't remove %s: %s\n"),
			         Prog, mailfile, strerror (errno));
			SYSLOG ((LOG_ERR, "Cannot remove %s: %s", mailfile, strerror (errno)));
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
			              "deleting mail file",
			              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			errors = 1;
			/* continue */
		}
#ifdef WITH_AUDIT
		else
		{
			audit_logger (AUDIT_DEL_USER, Prog,
			              "deleting mail file",
			              user_name, user_id, SHADOW_AUDIT_SUCCESS);
		}
#endif				/* WITH_AUDIT */
		free(mailfile);
		return errors;
	}
	i = is_owner (user_id, mailfile);
	if (i == 0) {
		fprintf (stderr,
		         _("%s: %s not owned by %s, not removing\n"),
		         Prog, mailfile, user_name);
		SYSLOG ((LOG_ERR,
		         "%s not owned by %s, not removed",
		         mailfile, strerror (errno)));
#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_USER, Prog,
		              "deleting mail file",
		              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
		free(mailfile);
		return 1;
	} else if (i == -1) {
		free(mailfile);
		return 0;		/* mailbox doesn't exist */
	}
	if (unlink (mailfile) != 0) {
		fprintf (stderr,
		         _("%s: warning: can't remove %s: %s\n"),
		         Prog, mailfile, strerror (errno));
		SYSLOG ((LOG_ERR, "Cannot remove %s: %s", mailfile, strerror (errno)));
#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_USER, Prog,
		              "deleting mail file",
		              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
		errors = 1;
		/* continue */
	}
#ifdef WITH_AUDIT
	else
	{
		audit_logger (AUDIT_DEL_USER, Prog,
		              "deleting mail file",
		              user_name, user_id, SHADOW_AUDIT_SUCCESS);
	}
#endif				/* WITH_AUDIT */
	free(mailfile);
	return errors;
}

#ifdef WITH_TCB
static int remove_tcbdir (const char *user_name, uid_t user_id)
{
	int   ret = 0;
	char  *buf;

	if (!getdef_bool ("USE_TCB")) {
		return 0;
	}

	if (asprintf(&buf, TCB_DIR "/%s", user_name) == -1) {
		fprintf(stderr,
		        _("%s: Can't allocate memory, tcb entry for %s not removed.\n"),
		        Prog, user_name);
		return 1;
	}
	if (shadowtcb_drop_priv () == SHADOWTCB_FAILURE) {
		fprintf (stderr, _("%s: Cannot drop privileges: %s\n"),
		         Prog, strerror (errno));
		shadowtcb_gain_priv ();
		free (buf);
		return 1;
	}
	/* Only remove directory contents with dropped privileges.
	 * We will regain them and remove the user's tcb directory afterwards.
	 */
	if (remove_tree (buf, false) != 0) {
		fprintf (stderr, _("%s: Cannot remove the content of %s: %s\n"),
		         Prog, buf, strerror (errno));
		shadowtcb_gain_priv ();
		free (buf);
		return 1;
	}
	shadowtcb_gain_priv ();
	free (buf);
	if (shadowtcb_remove (user_name) == SHADOWTCB_FAILURE) {
		fprintf (stderr, _("%s: Cannot remove tcb files for %s: %s\n"),
		         Prog, user_name, strerror (errno));
		ret = 1;
	}
	return ret;
}
#endif				/* WITH_TCB */

/*
 * main - userdel command
 */
int main (int argc, char **argv)
{
	int errors = 0; /* Error in the removal of the home directory */

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
#endif				/* WITH_AUDIT */

	{
		/*
		 * Parse the command line options.
		 */
		int c;
		static struct option long_options[] = {
			{"force",        no_argument,       NULL, 'f'},
			{"help",         no_argument,       NULL, 'h'},
			{"remove",       no_argument,       NULL, 'r'},
			{"root",         required_argument, NULL, 'R'},
			{"prefix",       required_argument, NULL, 'P'},
#ifdef WITH_SELINUX
			{"selinux-user", no_argument,       NULL, 'Z'},
#endif				/* WITH_SELINUX */
			{NULL, 0, NULL, '\0'}
		};
		while ((c = getopt_long (argc, argv,
#ifdef WITH_SELINUX
		                         "fhrR:P:Z",
#else				/* !WITH_SELINUX */
		                         "fhrR:P:",
#endif				/* !WITH_SELINUX */
		                         long_options, NULL)) != -1) {
			switch (c) {
			case 'f':	/* force remove even if not owned by user */
				fflg = true;
				break;
			case 'h':
				usage (E_SUCCESS);
				break;
			case 'r':	/* remove home dir and mailbox */
				rflg = true;
				break;
			case 'R': /* no-op, handled in process_root_flag () */
				Rflg = true;
				break;
			case 'P': /* no-op, handled in process_prefix_flag () */
				break;
#ifdef WITH_SELINUX
			case 'Z':
				if (prefix[0]) {
					fprintf (stderr,
					         _("%s: -Z cannot be used with --prefix\n"),
					         Prog);
					exit (E_BAD_ARG);
				}
				if (is_selinux_enabled () > 0) {
					Zflg = true;
				} else {
					fprintf (stderr,
					         _("%s: -Z requires SELinux enabled kernel\n"),
					         Prog);

					exit (E_BAD_ARG);
				}
				break;
#endif				/* WITH_SELINUX */
			default:
				usage (E_USAGE);
			}
		}
	}

	if ((optind + 1) != argc) {
		usage (E_USAGE);
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
			exit (E_PW_UPDATE);
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
		exit (E_PW_UPDATE);
	}
	(void) pam_end (pamh, retval);
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */

	is_shadow_pwd = spw_file_present ();
#ifdef SHADOWGRP
	is_shadow_grp = sgr_file_present ();
#endif				/* SHADOWGRP */
#ifdef ENABLE_SUBIDS
	is_sub_uid = sub_uid_file_present ();
	is_sub_gid = sub_gid_file_present ();
#endif				/* ENABLE_SUBIDS */

	/*
	 * Start with a quick check to see if the user exists.
	 */
	user_name = argv[argc - 1];
	{
		const struct passwd *pwd;

		if (run_parts ("/etc/shadow-maint/userdel-pre.d", user_name,
				"userdel")) {
			exit(1);
		}
		pw_open(O_RDONLY);
		pwd = pw_locate (user_name); /* we care only about local users */
		if (NULL == pwd) {
			pw_close();
			fprintf (stderr, _("%s: user '%s' does not exist\n"),
				 Prog, user_name);
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
			              "deleting user not found",
			              user_name, AUDIT_NO_ID,
			              SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			exit (E_NOTFOUND);
		}
		user_id = pwd->pw_uid;
		user_gid = pwd->pw_gid;

		if (prefix[0]) {
			xasprintf(&user_home, "%s/%s", prefix, pwd->pw_dir);
		} else {
			user_home = xstrdup(pwd->pw_dir);
		}
		pw_close();
	}
#ifdef WITH_TCB
	if (shadowtcb_set_user (user_name) == SHADOWTCB_FAILURE) {
		exit (E_NOTFOUND);
	}
#endif				/* WITH_TCB */
	/*
	 * Check to make certain the user isn't logged in.
	 * Note: This is a best effort basis. The user may log in between,
	 * a cron job may be started on her behalf, etc.
	 */
	if ((prefix[0] == '\0') && !Rflg && user_busy (user_name, user_id) != 0) {
		if (!fflg) {
#ifdef WITH_AUDIT
			audit_logger (AUDIT_DEL_USER, Prog,
			              "deleting user logged in",
			              user_name, AUDIT_NO_ID,
			              SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			exit (E_USER_BUSY);
		}
	}

	/*
	 * Do the hard stuff - open the files, create the user entries,
	 * create the home directory, then close and update the files.
	 */
	open_files ();
	update_user ();
	update_groups ();

	if (rflg) {
		errors += remove_mailbox ();
	}
	if (rflg) {
		int home_owned = is_owner (user_id, user_home);
		if (-1 == home_owned) {
			fprintf (stderr,
			         _("%s: %s home directory (%s) not found\n"),
			         Prog, user_name, user_home);
			rflg = 0;
		} else if ((0 == home_owned) && !fflg) {
			fprintf (stderr,
			         _("%s: %s not owned by %s, not removing\n"),
			         Prog, user_home, user_name);
			rflg = 0;
			errors++;
			/* continue */
		}
	}

#ifdef EXTRA_CHECK_HOME_DIR
	/* This may be slow, the above should be good enough. */
	if (rflg && !fflg) {
		struct passwd *pwd;
		/*
		 * For safety, refuse to remove the home directory if it
		 * would result in removing some other user's home
		 * directory. Still not perfect so be careful, but should
		 * prevent accidents if someone has /home or / as home
		 * directory...  --marekm
		 */
		prefix_setpwent ();
		while ((pwd = prefix_getpwent ())) {
			if (strcmp (pwd->pw_name, user_name) == 0) {
				continue;
			}
			if (path_prefix (user_home, pwd->pw_dir)) {
				fprintf (stderr,
				         _("%s: not removing directory %s (would remove home of user %s)\n"),
				         Prog, user_home, pwd->pw_name);
				rflg = false;
				errors++;
				/* continue */
				break;
			}
		}
		prefix_endpwent ();
	}
#endif				/* EXTRA_CHECK_HOME_DIR */

	if (rflg) {
#ifdef WITH_BTRFS
		int is_subvolume = btrfs_is_subvolume (user_home);
		if (is_subvolume < 0) {
		    errors++;
		    /* continue */
		}
		else if (is_subvolume > 0) {
			if (btrfs_remove_subvolume (user_home)) {
				fprintf (stderr,
				         _("%s: error removing subvolume %s\n"),
				         Prog, user_home);
				errors++;
				/* continue */
			}
		}
		else
#endif
		if (remove_tree (user_home, true) != 0) {
			fprintf (stderr,
			         _("%s: error removing directory %s\n"),
			         Prog, user_home);
			errors++;
			/* continue */
		}
#ifdef WITH_AUDIT
		else
		{
			audit_logger (AUDIT_DEL_USER, Prog,
			              "deleting home directory",
			              user_name, user_id, SHADOW_AUDIT_SUCCESS);
		}
#endif				/* WITH_AUDIT */
	}
#ifdef WITH_AUDIT
	if (0 != errors) {
		audit_logger (AUDIT_DEL_USER, Prog,
		              "deleting home directory",
		              user_name, AUDIT_NO_ID,
		              SHADOW_AUDIT_FAILURE);
	}
#endif				/* WITH_AUDIT */

#ifdef WITH_SELINUX
	if (Zflg) {
		if (del_seuser (user_name) != 0) {
			fprintf (stderr,
			         _("%s: warning: the user name %s to SELinux user mapping removal failed.\n"),
			         Prog, user_name);
#ifdef WITH_AUDIT
			audit_logger (AUDIT_ADD_USER, Prog,
			              "removing SELinux user mapping",
			              user_name, user_id, SHADOW_AUDIT_FAILURE);
#endif				/* WITH_AUDIT */
			fail_exit (E_SE_UPDATE);
		}
	}
#endif				/* WITH_SELINUX */

	/*
	 * Cancel any crontabs or at jobs. Have to do this before we remove
	 * the entry from /etc/passwd.
	 */
	if (prefix[0] == '\0')
		user_cancel (user_name);
	close_files ();

	if (run_parts ("/etc/shadow-maint/userdel-post.d", user_name, "userdel")) {
		exit(1);
	}

#ifdef WITH_TCB
	errors += remove_tcbdir (user_name, user_id);
#endif				/* WITH_TCB */

	nscd_flush_cache ("passwd");
	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_PASSWD | SSSD_DB_GROUP);

	return ((0 != errors) ? E_HOMEDIR : E_SUCCESS);
}

