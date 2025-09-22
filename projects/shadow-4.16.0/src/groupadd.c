/*
 * SPDX-FileCopyrightText: 1991 - 1993, Julianne Frances Haugh
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
#include <stdio.h>
#include <sys/types.h>
#ifdef ACCT_TOOLS_SETUID
#ifdef USE_PAM
#include "pam_defs.h"
#include <pwd.h>
#endif				/* USE_PAM */
#endif				/* ACCT_TOOLS_SETUID */
#include "chkname.h"
#include "defines.h"
#include "getdef.h"
#include "groupio.h"
#include "memzero.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#ifdef	SHADOWGRP
#include "sgroupio.h"
#endif
#include "shadowlog.h"
#include "run_part.h"

/*
 * exit status values
 */
/*@-exitarg@*/
#define E_SUCCESS	0	/* success */
#define E_USAGE		2	/* invalid command syntax */
#define E_BAD_ARG	3	/* invalid argument to option */
#define E_GID_IN_USE	4	/* gid not unique (when -o not used) */
#define E_NAME_IN_USE	9	/* group name not unique */
#define E_GRP_UPDATE	10	/* can't update group file */

/*
 * Global variables
 */
static const char Prog[] = "groupadd";

static /*@null@*/char *group_name;
static gid_t group_id;
static /*@null@*/char *group_passwd;
static /*@null@*/char *empty_list = NULL;

static const char *prefix = "";
static char *user_list;

static bool oflg = false;	/* permit non-unique group ID to be specified with -g */
static bool gflg = false;	/* ID value for the new group */
static bool fflg = false;	/* if group already exists, do nothing and exit(0) */
static bool rflg = false;	/* create a system account */
static bool pflg = false;	/* new encrypted password */

#ifdef SHADOWGRP
static bool is_shadow_grp;
#endif

/* local function prototypes */
NORETURN static void usage (int status);
static void new_grent (struct group *grent);

#ifdef SHADOWGRP
static void new_sgent (struct sgrp *sgent);
#endif
static void grp_update (void);
static void check_new_name (void);
static void close_files (void);
static void open_files (void);
static void process_flags (int argc, char **argv);
static void check_flags (void);
static void check_perms (void);

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
	(void) fputs (_("  -f, --force                   exit successfully if the group already exists,\n"
	                "                                and cancel -g if the GID is already used\n"), usageout);
	(void) fputs (_("  -g, --gid GID                 use GID for the new group\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -K, --key KEY=VALUE           override /etc/login.defs defaults\n"), usageout);
	(void) fputs (_("  -o, --non-unique              allow to create groups with duplicate\n"
	                "                                (non-unique) GID\n"), usageout);
	(void) fputs (_("  -p, --password PASSWORD       use this encrypted password for the new group\n"), usageout);
	(void) fputs (_("  -r, --system                  create a system account\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -P, --prefix PREFIX_DIR       directory prefix\n"), usageout);
	(void) fputs (_("  -U, --users USERS             list of user members of this group\n"), usageout);
	(void) fputs ("\n", usageout);
	exit (status);
}

/*
 * new_grent - initialize the values in a group file entry
 *
 *	new_grent() takes all of the values that have been entered and fills
 *	in a (struct group) with them.
 */
static void new_grent (struct group *grent)
{
	memzero (grent, sizeof *grent);
	grent->gr_name = group_name;
	if (pflg) {
		grent->gr_passwd = group_passwd;
	} else {
		grent->gr_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */
	}
	grent->gr_gid = group_id;
	grent->gr_mem = &empty_list;
}

#ifdef	SHADOWGRP
/*
 * new_sgent - initialize the values in a shadow group file entry
 *
 *	new_sgent() takes all of the values that have been entered and fills
 *	in a (struct sgrp) with them.
 */
static void new_sgent (struct sgrp *sgent)
{
	memzero (sgent, sizeof *sgent);
	sgent->sg_name = group_name;
	if (pflg) {
		sgent->sg_passwd = group_passwd;
	} else {
		sgent->sg_passwd = "!";	/* XXX warning: const */
	}
	sgent->sg_adm = &empty_list;
	sgent->sg_mem = &empty_list;
}
#endif				/* SHADOWGRP */

/*
 * grp_update - add new group file entries
 *
 *	grp_update() writes the new records to the group files.
 */
static void grp_update (void)
{
	struct group grp;

#ifdef	SHADOWGRP
	struct sgrp sgrp;
#endif				/* SHADOWGRP */

	/*
	 * To add the group, we need to update /etc/group.
	 * Make sure failures will be reported.
	 */
	add_cleanup (cleanup_report_add_group_group, group_name);
#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		/* We also need to update /etc/gshadow */
		add_cleanup (cleanup_report_add_group_gshadow, group_name);
	}
#endif

	/*
	 * Create the initial entries for this new group.
	 */
	new_grent (&grp);
#ifdef	SHADOWGRP
	new_sgent (&sgrp);
	if (is_shadow_grp && pflg) {
		grp.gr_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */
	}
#endif				/* SHADOWGRP */

	if (user_list) {
		char *token;
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
#ifdef	SHADOWGRP
	/*
	 * Write out the new shadow group entries as well.
	 */
	if (is_shadow_grp && (sgr_update (&sgrp) == 0)) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"),
		         Prog, sgr_dbname (), sgrp.sg_name);
		exit (E_GRP_UPDATE);
	}
#endif				/* SHADOWGRP */
}

/*
 * check_new_name - check the new name for validity
 *
 *	check_new_name() insures that the new name doesn't contain any
 *	illegal characters.
 */
static void check_new_name (void)
{
	if (is_valid_group_name (group_name)) {
		return;
	}

	/*
	 * All invalid group names land here.
	 */

	fprintf (stderr, _("%s: '%s' is not a valid group name\n"),
	         Prog, group_name);

	exit (E_BAD_ARG);
}

/*
 * close_files - close all of the files that were opened
 *
 *	close_files() closes all of the files that were opened for this new
 *	group. This causes any modified entries to be written out.
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
	audit_logger (AUDIT_ADD_GROUP, Prog,
	              "adding group to /etc/group",
	              group_name, group_id, SHADOW_AUDIT_SUCCESS);
#endif
	SYSLOG ((LOG_INFO, "group added to %s: name=%s, GID=%u",
	         gr_dbname (), group_name, (unsigned int) group_id));
	del_cleanup (cleanup_report_add_group_group);

	cleanup_unlock_group (NULL);
	del_cleanup (cleanup_unlock_group);

	/* Now, write the changes in the shadow database */
#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, sgr_dbname ());
			exit (E_GRP_UPDATE);
		}
#ifdef WITH_AUDIT
		audit_logger (AUDIT_ADD_GROUP, Prog,
		              "adding group to /etc/gshadow",
		              group_name, group_id, SHADOW_AUDIT_SUCCESS);
#endif
		SYSLOG ((LOG_INFO, "group added to %s: name=%s",
		         sgr_dbname (), group_name));
		del_cleanup (cleanup_report_add_group_gshadow);

		cleanup_unlock_gshadow (NULL);
		del_cleanup (cleanup_unlock_gshadow);
	}
#endif				/* SHADOWGRP */

	/* Report success at the system level */
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_GROUP, Prog,
	              "", group_name, group_id, SHADOW_AUDIT_SUCCESS);
#endif
	SYSLOG ((LOG_INFO, "new group: name=%s, GID=%u",
	         group_name, (unsigned int) group_id));
	del_cleanup (cleanup_report_add_group);
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
#endif				/* SHADOWGRP */

	/*
	 * Now if the group is not added, it's our fault.
	 * Make sure failures will be reported.
	 */
	add_cleanup (cleanup_report_add_group, group_name);

	/* And now open the databases */
	if (gr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s: %s\n"), Prog, gr_dbname (), strerror(errno));
		SYSLOG ((LOG_WARN, "cannot open %s: %s", gr_dbname (), strerror(errno)));
		exit (E_GRP_UPDATE);
	}

#ifdef	SHADOWGRP
	if (is_shadow_grp) {
		if (sgr_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s: %s\n"),
			         Prog, sgr_dbname (), strerror(errno));
			SYSLOG ((LOG_WARN, "cannot open %s: %s", sgr_dbname (), strerror(errno)));
			exit (E_GRP_UPDATE);
		}
	}
#endif				/* SHADOWGRP */
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
	char *cp;
	int c;
	static struct option long_options[] = {
		{"force",      no_argument,       NULL, 'f'},
		{"gid",        required_argument, NULL, 'g'},
		{"help",       no_argument,       NULL, 'h'},
		{"key",        required_argument, NULL, 'K'},
		{"non-unique", no_argument,       NULL, 'o'},
		{"password",   required_argument, NULL, 'p'},
		{"system",     no_argument,       NULL, 'r'},
		{"root",       required_argument, NULL, 'R'},
		{"prefix",     required_argument, NULL, 'P'},
		{"users",      required_argument, NULL, 'U'},
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv, "fg:hK:op:rR:P:U:",
		                 long_options, NULL)) != -1) {
		switch (c) {
		case 'f':
			/*
			 * "force" - do nothing, just exit(0), if the
			 * specified group already exists. With -g, if
			 * specified gid already exists, choose another
			 * (unique) gid (turn off -g). Based on the RedHat's
			 * patch from shadow-utils-970616-9.
			 */
			fflg = true;
			break;
		case 'g':
			gflg = true;
			if (   (get_gid(optarg, &group_id) == -1)
			    || (group_id == (gid_t)-1)) {
				fprintf (stderr,
				         _("%s: invalid group ID '%s'\n"),
				         Prog, optarg);
				exit (E_BAD_ARG);
			}
			break;
		case 'h':
			usage (E_SUCCESS);
			/*@notreached@*/break;
		case 'K':
			/*
			 * override login.defs defaults (-K name=value)
			 * example: -K GID_MIN=100 -K GID_MAX=499
			 * note: -K GID_MIN=10,GID_MAX=499 doesn't work yet
			 */
			cp = strchr (optarg, '=');
			if (NULL == cp) {
				fprintf (stderr,
				         _("%s: -K requires KEY=VALUE\n"),
				         Prog);
				exit (E_BAD_ARG);
			}
			/* terminate name, point to value */
			*cp++ = '\0';
			if (putdef_str (optarg, cp, NULL) < 0) {
				exit (E_BAD_ARG);
			}
			break;
		case 'o':
			oflg = true;
			break;
		case 'p':
			pflg = true;
			group_passwd = optarg;
			break;
		case 'r':
			rflg = true;
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

	/*
	 * Check the flags consistency
	 */
	if (optind != argc - 1) {
		usage (E_USAGE);
	}
	group_name = argv[optind];

	check_flags ();
}

/*
 * check_flags - check flags and parameters consistency
 *
 *	It will not return if an error is encountered.
 */
static void check_flags (void)
{
	/* -o does not make sense without -g */
	if (oflg && !gflg) {
		usage (E_USAGE);
	}

	check_new_name ();

	/*
	 * Check if the group already exist.
	 */
	/* local, no need for xgetgrnam */
	if (prefix_getgrnam (group_name) != NULL) {
		/* The group already exist */
		if (fflg) {
			/* OK, no need to do anything */
			exit (E_SUCCESS);
		}
		fprintf (stderr,
		         _("%s: group '%s' already exists\n"),
		         Prog, group_name);
		exit (E_NAME_IN_USE);
	}

	if (gflg && (prefix_getgrgid (group_id) != NULL)) {
		/* A GID was specified, and a group already exist with that GID
		 *  - either we will use this GID anyway (-o)
		 *  - either we ignore the specified GID and
		 *    we will use another one (-f)
		 *  - either it is a failure
		 */
		if (oflg) {
			/* Continue with this GID */
		} else if (fflg) {
			/* Turn off -g, we can use any GID */
			gflg = false;
		} else {
			fprintf (stderr,
			         _("%s: GID '%lu' already exists\n"),
			         Prog, (unsigned long) group_id);
			exit (E_GID_IN_USE);
		}
	}
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
 * main - groupadd command
 */
int main (int argc, char **argv)
{
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

	/*
	 * Parse the command line options.
	 */
	process_flags (argc, argv);

	check_perms ();

	if (run_parts ("/etc/shadow-maint/groupadd-pre.d", group_name,
			Prog)) {
		exit(1);
	}

#ifdef SHADOWGRP
	is_shadow_grp = sgr_file_present ();
#endif

	/*
	 * Do the hard stuff - open the files, create the group entries,
	 * then close and update the files.
	 */
	open_files ();

	if (!gflg) {
		if (find_new_gid (rflg, &group_id, NULL) < 0) {
			exit (E_GID_IN_USE);
		}
	}

	grp_update ();
	close_files ();
	if (run_parts ("/etc/shadow-maint/groupadd-post.d", group_name,
			Prog)) {
		exit(1);
	}


	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_GROUP);

	return E_SUCCESS;
}

