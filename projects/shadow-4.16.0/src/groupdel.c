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
#include <grp.h>
#include <pwd.h>
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>
#include "defines.h"
#include "groupio.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#ifdef	SHADOWGRP
#include "sgroupio.h"
#endif
#include "shadowlog.h"
#include "run_part.h"
/*
 * Global variables
 */
static const char Prog[] = "groupdel";

static char *group_name;
static gid_t group_id = -1;
static bool check_group_busy = true;

static const char* prefix = "";

#ifdef	SHADOWGRP
static bool is_shadow_grp;
#endif

/*
 * exit status values
 */
/*@-exitarg@*/
#define E_SUCCESS	0	/* success */
#define E_USAGE		2	/* invalid command syntax */
#define E_NOTFOUND	6	/* specified group doesn't exist */
#define E_GROUP_BUSY	8	/* can't remove user's primary group */
#define E_GRP_UPDATE	10	/* can't update group file */

/* local function prototypes */
NORETURN static void usage (int status);
static void grp_update (void);
static void close_files (void);
static void open_files (void);
static void group_busy (gid_t gid);
static void process_flags (int argc, char **argv);

/*
 * usage - display usage message and exit
 */
NORETURN
static void
usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options] GROUP\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -P, --prefix PREFIX_DIR       prefix directory where are located the /etc/* files\n"), usageout);
	(void) fputs (_("  -f, --force                   delete group even if it is the primary group of a user\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * grp_update - update group file entries
 *
 *	grp_update() writes the new records to the group files.
 */
static void grp_update (void)
{
	/*
	 * To add the group, we need to update /etc/group.
	 * Make sure failures will be reported.
	 */
	add_cleanup (cleanup_report_del_group_group, group_name);
#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		/* We also need to update /etc/gshadow */
		add_cleanup (cleanup_report_del_group_gshadow, group_name);
	}
#endif

	/*
	 * Delete the group entry.
	 */
	if (gr_remove (group_name) == 0) {
		fprintf (stderr,
		         _("%s: cannot remove entry '%s' from %s\n"),
		         Prog, group_name, gr_dbname ());
		exit (E_GRP_UPDATE);
	}

#ifdef	SHADOWGRP
	/*
	 * Delete the shadow group entries as well.
	 */
	if (is_shadow_grp && (sgr_locate (group_name) != NULL)) {
		if (sgr_remove (group_name) == 0) {
			fprintf (stderr,
			         _("%s: cannot remove entry '%s' from %s\n"),
			         Prog, group_name, sgr_dbname ());
			exit (E_GRP_UPDATE);
		}
	}
#endif				/* SHADOWGRP */
}

/*
 * close_files - close all of the files that were opened
 *
 *	close_files() closes all of the files that were opened for this
 *	new group.  This causes any modified entries to be written out.
 */
static void close_files (void)
{
	/* First, write the changes in the regular group database */
	if (gr_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, gr_dbname ());
		exit (E_GRP_UPDATE);
	}

#ifdef WITH_AUDIT
	audit_logger (AUDIT_DEL_GROUP, Prog,
	              "removing group from /etc/group",
	              group_name, group_id, SHADOW_AUDIT_SUCCESS);
#endif
	SYSLOG ((LOG_INFO,
	         "group '%s' removed from %s",
	         group_name, gr_dbname ()));
	del_cleanup (cleanup_report_del_group_group);

	cleanup_unlock_group (NULL);
	del_cleanup (cleanup_unlock_group);


	/* Then, write the changes in the shadow database */
#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, sgr_dbname ());
			exit (E_GRP_UPDATE);
		}

#ifdef WITH_AUDIT
		audit_logger (AUDIT_DEL_GROUP, Prog,
		              "removing group from /etc/gshadow",
		              group_name, group_id, SHADOW_AUDIT_SUCCESS);
#endif
		SYSLOG ((LOG_INFO,
		         "group '%s' removed from %s",
		         group_name, sgr_dbname ()));
		del_cleanup (cleanup_report_del_group_gshadow);

		cleanup_unlock_gshadow (NULL);
		del_cleanup (cleanup_unlock_gshadow);
	}
#endif				/* SHADOWGRP */

	/* Report success at the system level */
#ifdef WITH_AUDIT
	audit_logger (AUDIT_DEL_GROUP, Prog,
	              "", group_name, group_id, SHADOW_AUDIT_SUCCESS);
#endif
	SYSLOG ((LOG_INFO, "group '%s' removed\n", group_name));
	del_cleanup (cleanup_report_del_group);
}

/*
 * open_files - lock and open the group files
 *
 *	open_files() opens the two group files.
 */
static void open_files (void)
{
	/* First, lock the databases */
	if (gr_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, gr_dbname ());
		exit (E_GRP_UPDATE);
	}
	add_cleanup (cleanup_unlock_group, NULL);
#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sgr_dbname ());
			exit (E_GRP_UPDATE);
		}
		add_cleanup (cleanup_unlock_gshadow, NULL);
	}
#endif

	/*
	 * Now, if the group is not removed, it's our fault.
	 * Make sure failures will be reported.
	 */
	add_cleanup (cleanup_report_del_group, group_name);

	/* An now open the databases */
	if (gr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"),
		         Prog, gr_dbname ());
		SYSLOG ((LOG_WARN, "cannot open %s", gr_dbname ()));
		exit (E_GRP_UPDATE);
	}
#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_WARN, "cannot open %s", sgr_dbname ()));
			exit (E_GRP_UPDATE);
		}
	}
#endif				/* SHADOWGRP */
}

/*
 * group_busy - check if this is any user's primary group
 *
 *	group_busy verifies that this group is not the primary group
 *	for any user.  You must remove all users before you remove
 *	the group.
 */
static void group_busy (gid_t gid)
{
	struct passwd *pwd;

	/*
	 * Nice slow linear search.
	 */

	prefix_setpwent ();

	while ( ((pwd = prefix_getpwent ()) != NULL) && (pwd->pw_gid != gid) );

	prefix_endpwent ();

	/*
	 * If pwd isn't NULL, it stopped because the gid's matched.
	 */

	if (pwd == NULL) {
		return;
	}

	/*
	 * Can't remove the group.
	 */
	fprintf (stderr,
	         _("%s: cannot remove the primary group of user '%s'\n"),
	         Prog, pwd->pw_name);
	exit (E_GROUP_BUSY);
}

/*
 * process_flags - parse the command line options
 *
 *	It will not return if an error is encountered.
 */
static void process_flags (int argc, char **argv)
{
	/*
	 * Parse the command line options.
	 */
	int c;
	static struct option long_options[] = {
		{"help", no_argument,       NULL, 'h'},
		{"force", no_argument,      NULL, 'f'},
		{"root", required_argument, NULL, 'R'},
		{"prefix", required_argument, NULL, 'P'},
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv, "hfR:P:",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
		case 'P': /* no-op, handled in process_prefix_flag () */
			break;
		case 'f':
			check_group_busy = false;
			break;
		default:
			usage (E_USAGE);
		}
	}

	if (optind != argc - 1) {
		usage (E_USAGE);
	}
	group_name = argv[optind];
}

/*
 * main - groupdel command
 *
 *	The syntax of the groupdel command is
 *
 *	groupdel group
 *
 *	The named group will be deleted.
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
		exit (1);
	}

	process_flags (argc, argv);

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
		}

		group_id = grp->gr_gid;
	}

	/*
	 * Make sure this isn't the primary group of anyone.
	 */
	if (check_group_busy) {
		group_busy (group_id);
	}

	if (run_parts ("/etc/shadow-maint/groupdel-pre.d", group_name,
			Prog)) {
		exit(1);
	}

	/*
	 * Do the hard stuff - open the files, delete the group entries,
	 * then close and update the files.
	 */
	open_files ();

	grp_update ();

	close_files ();

	if (run_parts ("/etc/shadow-maint/groupdel-post.d", group_name,
			Prog)) {
		exit(1);
	}

	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_GROUP);

	return E_SUCCESS;
}

