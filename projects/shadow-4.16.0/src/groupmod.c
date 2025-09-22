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

#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
#include "pam_defs.h"
#include <pwd.h>
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */

#include "alloc.h"
#include "chkname.h"
#include "defines.h"
#include "groupio.h"
#include "pwio.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#ifdef	SHADOWGRP
#include "sgroupio.h"
#endif
#include "shadowlog.h"
#include "string/stpecpy.h"
#include "string/stpeprintf.h"
/*
 * exit status values
 */
/*@-exitarg@*/
#define E_SUCCESS	0	/* success */
#define E_USAGE		2	/* invalid command syntax */
#define E_BAD_ARG	3	/* invalid argument to option */
#define E_GID_IN_USE	4	/* gid already in use (and no -o) */
#define E_NOTFOUND	6	/* specified group doesn't exist */
#define E_NAME_IN_USE	9	/* group name already in use */
#define E_GRP_UPDATE	10	/* can't update group file */
#define E_CLEANUP_SERVICE	11	/* can't setup cleanup service */
#define E_PAM_USERNAME	12	/* can't determine your username for use with pam */
#define E_PAM_ERROR	13	/* pam returned an error, see Syslog facility id groupmod */


/*
 * Global variables
 */
static const char Prog[] = "groupmod";

#ifdef	SHADOWGRP
static bool is_shadow_grp;
#endif				/* SHADOWGRP */
static char *group_name;
static char *group_newname;
static char *group_passwd;
static gid_t group_id;
static gid_t group_newid;

static const char* prefix = "";
static char *user_list;

static struct cleanup_info_mod info_passwd;
static struct cleanup_info_mod info_group;
#ifdef	SHADOWGRP
static struct cleanup_info_mod info_gshadow;
#endif

static bool
    aflg = false,               /* append -U members rather than replace them */
    oflg = false,		/* permit non-unique group ID to be specified with -g */
    gflg = false,		/* new ID value for the group */
    nflg = false,		/* a new name has been specified for the group */
    pflg = false;		/* new encrypted password */

/* local function prototypes */
static void usage (int status);
static void new_grent (struct group *);

#ifdef SHADOWGRP
static void new_sgent (struct sgrp *);
#endif
static void grp_update (void);
static void check_new_gid (void);
static void check_new_name (void);
static void process_flags (int, char **);
static void lock_files (void);
static void prepare_failure_reports (void);
static void open_files (void);
static void close_files (void);
static void update_primary_groups (gid_t ogid, gid_t ngid);


/*
 * usage - display usage message and exit
 */

static void usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options] GROUP\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -a, --append                  append the users mentioned by -U option to the group \n"
	                "                                without removing existing user members\n"), usageout);
	(void) fputs (_("  -g, --gid GID                 change the group ID to GID\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -n, --new-name NEW_GROUP      change the name to NEW_GROUP\n"), usageout);
	(void) fputs (_("  -o, --non-unique              allow to use a duplicate (non-unique) GID\n"), usageout);
	(void) fputs (_("  -p, --password PASSWORD       change the password to this (encrypted)\n"
	                "                                PASSWORD\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -P, --prefix PREFIX_DIR       prefix directory where are located the /etc/* files\n"), usageout);
	(void) fputs (_("  -U, --users USERS             list of user members of this group\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * new_grent - updates the values in a group file entry
 *
 *	new_grent() takes all of the values that have been entered and fills
 *	in a (struct group) with them.
 */
static void new_grent (struct group *grent)
{
	if (nflg) {
		grent->gr_name = xstrdup (group_newname);
	}

	if (gflg) {
		grent->gr_gid = group_newid;
	}

	if (   pflg
#ifdef SHADOWGRP
	    && (   (!is_shadow_grp)
	        || (strcmp (grent->gr_passwd, SHADOW_PASSWD_STRING) != 0))
#endif
		) {
		/* Update the password in group if there is no gshadow
		 * file or if the password is currently in group
		 * (gr_passwd != "x").  We do not force the usage of
		 * shadow passwords if it was not the case before.
		 */
		grent->gr_passwd = group_passwd;
	}
}

#ifdef	SHADOWGRP
/*
 * new_sgent - updates the values in a shadow group file entry
 *
 *	new_sgent() takes all of the values that have been entered and fills
 *	in a (struct sgrp) with them.
 */
static void new_sgent (struct sgrp *sgent)
{
	if (nflg) {
		sgent->sg_name = xstrdup (group_newname);
	}

	/* Always update the shadowed password if there is a shadow entry
	 * (even if shadowed passwords might not be enabled for this group
	 * (gr_passwd != "x")).
	 * It seems better to update the password in both places in case a
	 * shadow and a non shadow entry exist.
	 * This might occur only if there were already both entries.
	 */
	if (pflg) {
		sgent->sg_passwd = group_passwd;
	}
}
#endif				/* SHADOWGRP */

/*
 * grp_update - update group file entries
 *
 *	grp_update() updates the new records in the memory databases.
 */
static void grp_update (void)
{
	struct group grp;
	const struct group *ogrp;

#ifdef	SHADOWGRP
	struct sgrp sgrp;
	const struct sgrp *osgrp = NULL;
#endif				/* SHADOWGRP */

	/*
	 * Get the current settings for this group.
	 */
	ogrp = gr_locate (group_name);
	if (NULL == ogrp) {
		fprintf (stderr,
		         _("%s: group '%s' does not exist in %s\n"),
		         Prog, group_name, gr_dbname ());
		exit (E_GRP_UPDATE);
	}
	grp = *ogrp;
	new_grent (&grp);
#ifdef	SHADOWGRP
	if (   is_shadow_grp
	    && (pflg || nflg)) {
		osgrp = sgr_locate (group_name);
		if (NULL != osgrp) {
			sgrp = *osgrp;
			new_sgent (&sgrp);
		} else if (   pflg
		           && (strcmp (grp.gr_passwd, SHADOW_PASSWD_STRING) == 0)) {
			static char *empty = NULL;
			/* If there is a gshadow file with no entries for
			 * the group, but the group file indicates a
			 * shadowed password, we force the creation of a
			 * gshadow entry when a new password is requested.
			 */
			bzero(&sgrp, sizeof sgrp);
			sgrp.sg_name   = xstrdup (grp.gr_name);
			sgrp.sg_passwd = xstrdup (grp.gr_passwd);
			sgrp.sg_adm    = &empty;
			sgrp.sg_mem    = dup_list (grp.gr_mem);
			new_sgent (&sgrp);
			osgrp = &sgrp; /* entry needs to be committed */
		}
	}
#endif				/* SHADOWGRP */

	if (gflg) {
		update_primary_groups (ogrp->gr_gid, group_newid);
	}

	if (user_list) {
		char *token;

		if (!aflg) {
			// requested to replace the existing groups
			grp.gr_mem = XMALLOC(1, char *);
			grp.gr_mem[0] = NULL;
		} else {
			// append to existing groups
			if (NULL != grp.gr_mem[0])
				grp.gr_mem = dup_list (grp.gr_mem);
		}

		token = strtok(user_list, ",");
		while (token) {
			if (prefix_getpwnam (token) == NULL) {
				fprintf (stderr, _("Invalid member username %s\n"), token);
				exit (E_GRP_UPDATE);
			}
			grp.gr_mem = add_list(grp.gr_mem, token);
			token = strtok(NULL, ",");
		}
	}

	/*
	 * Write out the new group file entry.
	 */
	if (gr_update (&grp) == 0) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"),
		         Prog, gr_dbname (), grp.gr_name);
		exit (E_GRP_UPDATE);
	}
	if (nflg && (gr_remove (group_name) == 0)) {
		fprintf (stderr,
		         _("%s: cannot remove entry '%s' from %s\n"),
		         Prog, grp.gr_name, gr_dbname ());
		exit (E_GRP_UPDATE);
	}

#ifdef	SHADOWGRP
	/*
	 * Make sure there was a shadow entry to begin with.
	 */
	if (NULL != osgrp) {
		/*
		 * Write out the new shadow group entries as well.
		 */
		if (sgr_update (&sgrp) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, sgr_dbname (), sgrp.sg_name);
			exit (E_GRP_UPDATE);
		}
		if (nflg && (sgr_remove (group_name) == 0)) {
			fprintf (stderr,
			         _("%s: cannot remove entry '%s' from %s\n"),
			         Prog, group_name, sgr_dbname ());
			exit (E_GRP_UPDATE);
		}
	}
#endif				/* SHADOWGRP */
}

/*
 * check_new_gid - check the new GID value for uniqueness
 *
 *	check_new_gid() insures that the new GID value is unique.
 */
static void check_new_gid (void)
{
	/*
	 * First, the easy stuff. If the ID can be duplicated, or if the ID
	 * didn't really change, just return. If the ID didn't change, turn
	 * off those flags. No sense doing needless work.
	 */
	if (group_id == group_newid) {
		gflg = 0;
		return;
	}

	if (oflg ||
	    (getgrgid (group_newid) == NULL) /* local, no need for xgetgrgid */
	   ) {
		return;
	}

	/*
	 * Tell the user what they did wrong.
	 */
	fprintf (stderr,
	         _("%s: GID '%lu' already exists\n"),
	         Prog, (unsigned long) group_newid);
	exit (E_GID_IN_USE);
}

/*
 * check_new_name - check the new name for uniqueness
 *
 *	check_new_name() insures that the new name does not exist already.
 *	You can't have the same name twice, period.
 */
static void check_new_name (void)
{
	/*
	 * Make sure they are actually changing the name.
	 */
	if (strcmp (group_name, group_newname) == 0) {
		nflg = 0;
		return;
	}

	if (is_valid_group_name (group_newname)) {

		/*
		 * If the entry is found, too bad.
		 */
		/* local, no need for xgetgrnam */
		if (prefix_getgrnam (group_newname) != NULL) {
			fprintf (stderr,
			         _("%s: group '%s' already exists\n"),
			         Prog, group_newname);
			exit (E_NAME_IN_USE);
		}
		return;
	}

	/*
	 * All invalid group names land here.
	 */

	fprintf (stderr,
	         _("%s: invalid group name '%s'\n"),
	         Prog, group_newname);
	exit (E_BAD_ARG);
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
	int c;
	static struct option long_options[] = {
		{"append",     no_argument,       NULL, 'a'},
		{"gid",        required_argument, NULL, 'g'},
		{"help",       no_argument,       NULL, 'h'},
		{"new-name",   required_argument, NULL, 'n'},
		{"non-unique", no_argument,       NULL, 'o'},
		{"password",   required_argument, NULL, 'p'},
		{"root",       required_argument, NULL, 'R'},
		{"prefix",     required_argument, NULL, 'P'},
		{"users",      required_argument, NULL, 'U'},
		{NULL, 0, NULL, '\0'}
	};
	while ((c = getopt_long (argc, argv, "ag:hn:op:R:P:U:",
		                 long_options, NULL)) != -1) {
		switch (c) {
		case 'a':
			aflg = true;
			break;
		case 'g':
			gflg = true;
			if (   (get_gid(optarg, &group_newid) == -1)
			    || (group_newid == (gid_t)-1)) {
				fprintf (stderr,
				         _("%s: invalid group ID '%s'\n"),
				         Prog, optarg);
				exit (E_BAD_ARG);
			}
			break;
		case 'h':
			usage (E_SUCCESS);
			break;
		case 'n':
			nflg = true;
			group_newname = optarg;
			break;
		case 'o':
			oflg = true;
			break;
		case 'p':
			group_passwd = optarg;
			pflg = true;
			break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
		case 'P': /* no-op, handled in process_prefix_flag () */
			break;
		case 'U':
			user_list = optarg;
			break;
		default:
			usage (E_USAGE);
		}
	}

	if (oflg && !gflg) {
		usage (E_USAGE);
	}

	if (optind != (argc - 1)) {
		usage (E_USAGE);
	}

	group_name = argv[argc - 1];
}

/*
 * close_files - close all of the files that were opened
 *
 *	close_files() closes all of the files that were opened for this new
 *	group. This causes any modified entries to be written out.
 */
static void close_files (void)
{
	if (gr_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, gr_dbname ());
		exit (E_GRP_UPDATE);
	}
#ifdef WITH_AUDIT
	audit_logger (AUDIT_USER_ACCT, Prog,
	              info_group.audit_msg,
	              group_name, AUDIT_NO_ID,
	              SHADOW_AUDIT_SUCCESS);
#endif
	SYSLOG ((LOG_INFO,
	         "group changed in %s (%s)",
	         gr_dbname (), info_group.action));
	del_cleanup (cleanup_report_mod_group);

	cleanup_unlock_group (NULL);
	del_cleanup (cleanup_unlock_group);

#ifdef	SHADOWGRP
	if (   is_shadow_grp
	    && (pflg || nflg)) {
		if (sgr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, sgr_dbname ());
			exit (E_GRP_UPDATE);
		}
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_ACCT, Prog,
		              info_gshadow.audit_msg,
		              group_name, AUDIT_NO_ID,
		              SHADOW_AUDIT_SUCCESS);
#endif
		SYSLOG ((LOG_INFO,
		         "group changed in %s (%s)",
		         sgr_dbname (), info_gshadow.action));
		del_cleanup (cleanup_report_mod_gshadow);

		cleanup_unlock_gshadow (NULL);
		del_cleanup (cleanup_unlock_gshadow);
	}
#endif				/* SHADOWGRP */

	if (gflg) {
		if (pw_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, pw_dbname ());
			exit (E_GRP_UPDATE);
		}
#ifdef WITH_AUDIT
		audit_logger (AUDIT_USER_ACCT, Prog,
		              info_passwd.audit_msg,
		              group_name, AUDIT_NO_ID,
		              SHADOW_AUDIT_SUCCESS);
#endif
		SYSLOG ((LOG_INFO,
		         "group changed in %s (%s)",
		         pw_dbname (), info_passwd.action));
		del_cleanup (cleanup_report_mod_passwd);

		cleanup_unlock_passwd (NULL);
		del_cleanup (cleanup_unlock_passwd);
	}

#ifdef WITH_AUDIT
	audit_logger (AUDIT_USER_ACCT, Prog,
	              "modifying group",
	              group_name, AUDIT_NO_ID,
	              SHADOW_AUDIT_SUCCESS);
#endif
}

/*
 * prepare_failure_reports - Prepare the cleanup_info structure for logging
 * of success and failure to syslog or audit.
 */
static void prepare_failure_reports (void)
{
	char *gr, *gr_end;
#ifdef	SHADOWGRP
	char *sgr, *sgr_end;
#endif
	char *pw, *pw_end;

	info_group.name   = group_name;
#ifdef	SHADOWGRP
	info_gshadow.name = group_name;
#endif
	info_passwd.name  = group_name;

	gr                     = XMALLOC(512, char);
	info_group.audit_msg   = gr;
	gr_end                 = gr + 512;
#ifdef	SHADOWGRP
	sgr                    = XMALLOC(512, char);
	info_gshadow.audit_msg = sgr;
	sgr_end                = sgr + 512;
#endif
	pw                     = XMALLOC(512, char);
	info_passwd.audit_msg  = pw;
	pw_end                 = pw + 512;

	gr = stpeprintf(gr, gr_end, "changing %s; ", gr_dbname ());
#ifdef	SHADOWGRP
	sgr = stpeprintf(sgr, sgr_end, "changing %s; ", sgr_dbname ());
#endif
	pw = stpeprintf(pw, pw_end, "changing %s; ", pw_dbname ());

	info_group.action   = gr;
#ifdef	SHADOWGRP
	info_gshadow.action = sgr;
#endif
	info_passwd.action  = pw;

	gr  = stpeprintf(gr, gr_end,
	                 "group %s/%ju", group_name, (uintmax_t) group_id);
#ifdef	SHADOWGRP
	sgr = stpeprintf(sgr, sgr_end,
	                 "group %s", group_name);
#endif
	pw  = stpeprintf(pw, pw_end,
	                 "group %s/%ju", group_name, (uintmax_t) group_id);

	if (nflg) {
		gr = stpecpy(gr, gr_end, ", new name: ");
		gr = stpecpy(gr, gr_end, group_newname);
#ifdef	SHADOWGRP
		sgr = stpecpy(sgr, sgr_end, ", new name: ");
		sgr = stpecpy(sgr, sgr_end, group_newname);
#endif
		pw = stpecpy(pw, pw_end, ", new name: ");
		pw = stpecpy(pw, pw_end, group_newname);
	}
	if (pflg) {
		gr = stpecpy(gr, gr_end, ", new password");
#ifdef	SHADOWGRP
		sgr = stpecpy(sgr, sgr_end, ", new password");
#endif
	}
	if (gflg) {
		gr = stpecpy(gr, gr_end, ", new gid: ");
		stpeprintf(gr, gr_end, "%ju", (uintmax_t) group_newid);

		pw = stpecpy(pw, pw_end, ", new gid: ");
		stpeprintf(pw, pw_end, "%ju", (uintmax_t) group_newid);
	}

// FIXME: add a system cleanup
	add_cleanup (cleanup_report_mod_group, &info_group);
#ifdef	SHADOWGRP
	if (   is_shadow_grp
	    && (pflg || nflg)) {
		add_cleanup (cleanup_report_mod_gshadow, &info_gshadow);
	}
#endif
	if (gflg) {
		add_cleanup (cleanup_report_mod_passwd, &info_passwd);
	}

}

/*
 * lock_files - lock the accounts databases
 *
 *	lock_files() locks the group, gshadow, and passwd databases.
 */
static void lock_files (void)
{
	if (gr_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, gr_dbname ());
		exit (E_GRP_UPDATE);
	}
	add_cleanup (cleanup_unlock_group, NULL);

#ifdef	SHADOWGRP
	if (   is_shadow_grp
	    && (pflg || nflg)) {
		if (sgr_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sgr_dbname ());
			exit (E_GRP_UPDATE);
		}
		add_cleanup (cleanup_unlock_gshadow, NULL);
	}
#endif

	if (gflg) {
		if (pw_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, pw_dbname ());
			exit (E_GRP_UPDATE);
		}
		add_cleanup (cleanup_unlock_passwd, NULL);
	}
}


/*
 * open_files - open the accounts databases
 *
 *	open_files() opens the group, gshadow, and passwd databases.
 */
static void open_files (void)
{
	if (gr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, gr_dbname ());
		SYSLOG ((LOG_WARN, "cannot open %s", gr_dbname ()));
		exit (E_GRP_UPDATE);
	}

#ifdef	SHADOWGRP
	if (   is_shadow_grp
	    && (pflg || nflg)) {
		if (sgr_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_WARN, "cannot open %s", sgr_dbname ()));
			exit (E_GRP_UPDATE);
		}
	}
#endif				/* SHADOWGRP */

	if (gflg) {
		if (pw_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, pw_dbname ());
			SYSLOG ((LOG_WARN, "cannot open %s", gr_dbname ()));
			exit (E_GRP_UPDATE);
		}
	}
}

void update_primary_groups (gid_t ogid, gid_t ngid)
{
	struct passwd *pwd;

	prefix_setpwent ();
	while ((pwd = prefix_getpwent ()) != NULL) {
		if (pwd->pw_gid == ogid) {
			const struct passwd *lpwd;
			struct passwd npwd;
			lpwd = pw_locate (pwd->pw_name);
			if (NULL == lpwd) {
				fprintf (stderr,
				         _("%s: user '%s' does not exist in %s\n"),
				         Prog, pwd->pw_name, pw_dbname ());
				exit (E_GRP_UPDATE);
			} else {
				npwd = *lpwd;
				npwd.pw_gid = ngid;
				if (pw_update (&npwd) == 0) {
					fprintf (stderr,
					         _("%s: failed to prepare the new %s entry '%s'\n"),
					         Prog, pw_dbname (), npwd.pw_name);
					exit (E_GRP_UPDATE);
				}
			}
		}
	}
	prefix_endpwent ();
}

/*
 * main - groupmod command
 *
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

	if (atexit (do_cleanups) != 0) {
		fprintf (stderr,
		         _("%s: Cannot setup cleanup service.\n"),
		         Prog);
		exit (E_CLEANUP_SERVICE);
	}

	process_flags (argc, argv);

#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
	{
		struct passwd *pampw;
		pampw = getpwuid (getuid ()); /* local, no need for xgetpwuid */
		if (NULL == pampw) {
			fprintf (stderr,
			         _("%s: Cannot determine your user name.\n"),
			         Prog);
			exit (E_PAM_USERNAME);
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
		exit (E_PAM_ERROR);
	}
	(void) pam_end (pamh, retval);
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */

#ifdef SHADOWGRP
	is_shadow_grp = sgr_file_present ();
#endif
	{
		struct group *grp;
		/*
		 * Start with a quick check to see if the group exists.
		 */
		grp = prefix_getgrnam (group_name); /* local, no need for xgetgrnam */
		if (NULL == grp) {
			fprintf (stderr,
			         _("%s: group '%s' does not exist\n"),
			         Prog, group_name);
			exit (E_NOTFOUND);
		} else {
			group_id = grp->gr_gid;
		}
	}

	if (gflg) {
		check_new_gid ();
	}

	if (nflg) {
		check_new_name ();
	}

	lock_files ();

	/*
	 * Now if the group is not changed, it's our fault.
	 * Make sure failures will be reported.
	 */
	prepare_failure_reports ();

	/*
	 * Do the hard stuff - open the files, create the group entries,
	 * then close and update the files.
	 */
	open_files ();

	grp_update ();

	close_files ();

	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_GROUP);

	return E_SUCCESS;
}

