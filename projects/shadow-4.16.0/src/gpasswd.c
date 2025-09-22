/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
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
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>

#include "agetpass.h"
#include "alloc.h"
#include "attr.h"
#include "defines.h"
#include "groupio.h"
#include "memzero.h"
#include "nscd.h"
#include "sssd.h"
#include "prototypes.h"
#ifdef SHADOWGRP
#include "sgroupio.h"
#endif
/*@-exitarg@*/
#include "exitcodes.h"
#include "shadowlog.h"
#include "string/sprintf.h"
#include "string/strtcpy.h"


/*
 * Global variables
 */
/* The name of this command, as it is invoked */
static const char Prog[] = "gpasswd";

#ifdef SHADOWGRP
/* Indicate if shadow groups are enabled on the system
 * (/etc/gshadow present) */
static bool is_shadowgrp;
#endif

/* Flags set by options */
static bool aflg = false;
static bool Aflg = false;
static bool dflg = false;
static bool Mflg = false;
static bool rflg = false;
static bool Rflg = false;
/* The name of the group that is being affected */
static char *group = NULL;
/* The name of the user being added (-a) or removed (-d) from group */
static char *user = NULL;
/* The new list of members set with -M */
static char *members = NULL;
#ifdef SHADOWGRP
/* The new list of group administrators set with -A */
static char *admins = NULL;
#endif
/* The name of the caller */
static char *myname = NULL;
/* The UID of the caller */
static uid_t bywho;
/* Indicate if gpasswd was called by root */
#define amroot	(0 == bywho)

/* The number of retries for th user to provide and repeat a new password */
#ifndef	RETRIES
#define	RETRIES	3
#endif

/* local function prototypes */
NORETURN static void failure(void);
static void usage (int status);
static void catch_signals (int killed);
static bool is_valid_user_list (const char *users);
static void process_flags (int argc, char **argv);
static void check_flags (int argc, int opt_index);
static void open_files (void);
static void close_files (void);
#ifdef SHADOWGRP
static void get_group (struct group *gr, struct sgrp *sg);
static void check_perms (const struct group *gr, const struct sgrp *sg);
static void update_group (struct group *gr, struct sgrp *sg);
static void change_passwd (struct group *gr, struct sgrp *sg);
#else
static void get_group (struct group *gr);
static void check_perms (const struct group *gr);
static void update_group (struct group *gr);
static void change_passwd (struct group *gr);
#endif
static void log_gpasswd_failure (const char *suffix);
static void log_gpasswd_failure_system (/*@null@*/MAYBE_UNUSED void *arg);
static void log_gpasswd_failure_group (/*@null@*/MAYBE_UNUSED void *arg);
#ifdef SHADOWGRP
static void log_gpasswd_failure_gshadow (/*@null@*/MAYBE_UNUSED void *arg);
#endif
static void log_gpasswd_success (const char *suffix);
static void log_gpasswd_success_system (/*@null@*/MAYBE_UNUSED void *arg);
static void log_gpasswd_success_group (/*@null@*/MAYBE_UNUSED void *arg);

/*
 * usage - display usage message
 */
static void usage (int status)
{
	FILE *usageout = (E_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [option] GROUP\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -a, --add USER                add USER to GROUP\n"), usageout);
	(void) fputs (_("  -d, --delete USER             remove USER from GROUP\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -Q, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("  -r, --remove-password         remove the GROUP's password\n"), usageout);
	(void) fputs (_("  -R, --restrict                restrict access to GROUP to its members\n"), usageout);
	(void) fputs (_("  -M, --members USER,...        set the list of members of GROUP\n"), usageout);
#ifdef SHADOWGRP
	(void) fputs (_("  -A, --administrators ADMIN,...\n"
	                "                                set the list of administrators for GROUP\n"), usageout);
	(void) fputs (_("Except for the -A and -M options, the options cannot be combined.\n"), usageout);
#else
	(void) fputs (_("The options cannot be combined.\n"), usageout);
#endif
	exit (status);
}

/*
 * catch_signals - set or reset termio modes.
 *
 *	catch_signals() is called before processing begins. signal() is then
 *	called with catch_signals() as the signal handler. If signal later
 *	calls catch_signals() with a signal number, the terminal modes are
 *	then reset.
 */
static void catch_signals (int killed)
{
	static TERMIO sgtty;

	if (0 != killed) {
		STTY (0, &sgtty);
	} else {
		GTTY (0, &sgtty);
	}

	if (0 != killed) {
		(void) write (STDOUT_FILENO, "\n", 1);
		_exit (killed);
	}
}

/*
 * is_valid_user_list - check a comma-separated list of user names for validity
 *
 *	is_valid_user_list scans a comma-separated list of user names and
 *	checks that each listed name exists is the user database.
 *
 *	It returns true if the list of users is valid.
 */
static bool is_valid_user_list (const char *users)
{
	const char *username;
	char *end;
	bool is_valid = true;
	/*@owned@*/char *tmpusers = xstrdup (users);

	for (username = tmpusers;
	     (NULL != username) && ('\0' != *username);
	     username = end) {
		end = strchr (username, ',');
		if (NULL != end) {
			*end = '\0';
			end++;
		}

		/*
		 * This user must exist.
		 */

		/* local, no need for xgetpwnam */
		if (getpwnam (username) == NULL) {
			fprintf (stderr, _("%s: user '%s' does not exist\n"),
			         Prog, username);
			is_valid = false;
		}
	}

	free (tmpusers);

	return is_valid;
}

static void failure(void)
{
	fprintf(stderr, _("%s: Permission denied.\n"), Prog);
	log_gpasswd_failure(": Permission denied");
	exit(E_NOPERM);
}

/*
 * process_flags - process the command line options and arguments
 */
static void process_flags (int argc, char **argv)
{
	int c;
	static struct option long_options[] = {
		{"add",             required_argument, NULL, 'a'},
		{"administrators",  required_argument, NULL, 'A'},
		{"delete",          required_argument, NULL, 'd'},
		{"help",            no_argument,       NULL, 'h'},
		{"members",         required_argument, NULL, 'M'},
		{"root",            required_argument, NULL, 'Q'},
		{"remove-password", no_argument,       NULL, 'r'},
		{"restrict",        no_argument,       NULL, 'R'},
		{NULL, 0, NULL, '\0'}
		};

	while ((c = getopt_long (argc, argv, "a:A:d:ghM:Q:rR",
	                         long_options, NULL)) != -1) {
		switch (c) {
		case 'a':	/* add a user */
			aflg = true;
			user = optarg;
			/* local, no need for xgetpwnam */
			if (getpwnam (user) == NULL) {
				fprintf (stderr,
				         _("%s: user '%s' does not exist\n"),
				         Prog, user);
				exit (E_BAD_ARG);
			}
			break;
#ifdef SHADOWGRP
		case 'A':	/* set the list of administrators */
			if (!is_shadowgrp) {
				fprintf (stderr,
				         _("%s: shadow group passwords required for -A\n"),
				         Prog);
				exit (E_GSHADOW_NOTFOUND);
			}
			admins = optarg;
			if (!is_valid_user_list (admins)) {
				exit (E_BAD_ARG);
			}
			Aflg = true;
			break;
#endif				/* SHADOWGRP */
		case 'd':	/* delete a user */
			dflg = true;
			user = optarg;
			break;
		case 'g':	/* no-op from normal password */
			break;
		case 'h':
			usage (E_SUCCESS);
			break;
		case 'M':	/* set the list of members */
			members = optarg;
			if (!is_valid_user_list (members)) {
				exit (E_BAD_ARG);
			}
			Mflg = true;
			break;
		case 'Q':	/* no-op, handled in process_root_flag () */
			break;
		case 'r':	/* remove group password */
			rflg = true;
			break;
		case 'R':	/* restrict group password */
			Rflg = true;
			break;
		default:
			usage (E_USAGE);
		}
	}

	/* Get the name of the group that is being affected. */
	group = argv[optind];

	check_flags (argc, optind);
}

/*
 * check_flags - check the validity of options
 */
static void check_flags (int argc, int opt_index)
{
	int exclusive = 0;
	/*
	 * Make sure exclusive flags are exclusive
	 */
	if (aflg) {
		exclusive++;
	}
	if (dflg) {
		exclusive++;
	}
	if (rflg) {
		exclusive++;
	}
	if (Rflg) {
		exclusive++;
	}
	if (Aflg || Mflg) {
		exclusive++;
	}
	if (exclusive > 1) {
		usage (E_USAGE);
	}

	/*
	 * Make sure one (and only one) group was provided
	 */
	if ((argc != (opt_index+1)) || (NULL == group)) {
		usage (E_USAGE);
	}
}

/*
 * open_files - lock and open the group databases
 *
 *	It will call exit in case of error.
 */
static void open_files (void)
{
	if (gr_lock () == 0) {
		fprintf (stderr,
		         _("%s: cannot lock %s; try again later.\n"),
		         Prog, gr_dbname ());
		exit (E_NOPERM);
	}
	add_cleanup (cleanup_unlock_group, NULL);

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		if (sgr_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, sgr_dbname ());
			exit (E_NOPERM);
		}
		add_cleanup (cleanup_unlock_gshadow, NULL);
	}
#endif				/* SHADOWGRP */

	add_cleanup (log_gpasswd_failure_system, NULL);

	if (gr_open (O_CREAT | O_RDWR) == 0) {
		fprintf (stderr,
		         _("%s: cannot open %s\n"),
		         Prog, gr_dbname ());
		SYSLOG ((LOG_WARN, "cannot open %s", gr_dbname ()));
		exit (E_NOPERM);
	}

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		if (sgr_open (O_CREAT | O_RDWR) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_WARN, "cannot open %s", sgr_dbname ()));
			exit (E_NOPERM);
		}
		add_cleanup (log_gpasswd_failure_gshadow, NULL);
	}
#endif				/* SHADOWGRP */

	add_cleanup (log_gpasswd_failure_group, NULL);
	del_cleanup (log_gpasswd_failure_system);
}

static void log_gpasswd_failure (const char *suffix)
{
#ifdef WITH_AUDIT
	char  buf[1024];
#endif

	if (aflg) {
		SYSLOG ((LOG_ERR,
		         "%s failed to add user %s to group %s%s",
		         myname, user, group, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "%s failed to add user %s to group %s%s",
		         myname, user, group, suffix);
		audit_logger (AUDIT_USER_ACCT, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_FAILURE);
#endif
	} else if (dflg) {
		SYSLOG ((LOG_ERR,
		         "%s failed to remove user %s from group %s%s",
		         myname, user, group, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "%s failed to remove user %s from group %s%s",
		         myname, user, group, suffix);
		audit_logger (AUDIT_USER_ACCT, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_FAILURE);
#endif
	} else if (rflg) {
		SYSLOG ((LOG_ERR,
		         "%s failed to remove password of group %s%s",
		         myname, group, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "%s failed to remove password of group %s%s",
		         myname, group, suffix);
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_FAILURE);
#endif
	} else if (Rflg) {
		SYSLOG ((LOG_ERR,
		         "%s failed to restrict access to group %s%s",
		         myname, group, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "%s failed to restrict access to group %s%s",
		         myname, group, suffix);
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_FAILURE);
#endif
	} else if (Aflg || Mflg) {
#ifdef SHADOWGRP
		if (Aflg) {
			SYSLOG ((LOG_ERR,
			         "%s failed to set the administrators of group %s to %s%s",
			         myname, group, admins, suffix));
#ifdef WITH_AUDIT
			SNPRINTF(buf, "%s failed to set the administrators of group %s to %s%s",
			         myname, group, admins, suffix);
			audit_logger (AUDIT_USER_ACCT, Prog,
			              buf,
			              group, AUDIT_NO_ID,
			              SHADOW_AUDIT_FAILURE);
#endif
		}
#endif				/* SHADOWGRP */
		if (Mflg) {
			SYSLOG ((LOG_ERR,
			         "%s failed to set the members of group %s to %s%s",
			         myname, group, members, suffix));
#ifdef WITH_AUDIT
			SNPRINTF(buf, "%s failed to set the members of group %s to %s%s",
			         myname, group, members, suffix);
			audit_logger (AUDIT_USER_ACCT, Prog,
			              buf,
			              group, AUDIT_NO_ID,
			              SHADOW_AUDIT_FAILURE);
#endif
		}
	} else {
		SYSLOG ((LOG_ERR,
		         "%s failed to change password of group %s%s",
		         myname, group, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "%s failed to change password of group %s%s",
		         myname, group, suffix);
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_FAILURE);
#endif
	}
}

static void log_gpasswd_failure_system (MAYBE_UNUSED void *arg)
{
	log_gpasswd_failure ("");
}

static void log_gpasswd_failure_group (MAYBE_UNUSED void *arg)
{
	char  buf[1024];

	SNPRINTF(buf, " in %s", gr_dbname());
	log_gpasswd_failure (buf);
}

#ifdef SHADOWGRP
static void log_gpasswd_failure_gshadow (MAYBE_UNUSED void *arg)
{
	char  buf[1024];

	SNPRINTF(buf, " in %s", sgr_dbname());
	log_gpasswd_failure (buf);
}
#endif				/* SHADOWGRP */

static void log_gpasswd_success (const char *suffix)
{
#ifdef WITH_AUDIT
	char  buf[1024];
#endif

	if (aflg) {
		SYSLOG ((LOG_INFO,
		         "user %s added by %s to group %s%s",
		         user, myname, group, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "user %s added by %s to group %s%s",
		         user, myname, group, suffix);
		audit_logger (AUDIT_USER_ACCT, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_SUCCESS);
#endif
	} else if (dflg) {
		SYSLOG ((LOG_INFO,
		         "user %s removed by %s from group %s%s",
		         user, myname, group, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "user %s removed by %s from group %s%s",
		         user, myname, group, suffix);
		audit_logger (AUDIT_USER_ACCT, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_SUCCESS);
#endif
	} else if (rflg) {
		SYSLOG ((LOG_INFO,
		         "password of group %s removed by %s%s",
		         group, myname, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "password of group %s removed by %s%s",
		         group, myname, suffix);
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_SUCCESS);
#endif
	} else if (Rflg) {
		SYSLOG ((LOG_INFO,
		         "access to group %s restricted by %s%s",
		         group, myname, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "access to group %s restricted by %s%s",
		         group, myname, suffix);
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_SUCCESS);
#endif
	} else if (Aflg || Mflg) {
#ifdef SHADOWGRP
		if (Aflg) {
			SYSLOG ((LOG_INFO,
			         "administrators of group %s set by %s to %s%s",
			         group, myname, admins, suffix));
#ifdef WITH_AUDIT
			SNPRINTF(buf, "administrators of group %s set by %s to %s%s",
			         group, myname, admins, suffix);
			audit_logger (AUDIT_USER_ACCT, Prog,
			              buf,
			              group, AUDIT_NO_ID,
			              SHADOW_AUDIT_SUCCESS);
#endif
		}
#endif				/* SHADOWGRP */
		if (Mflg) {
			SYSLOG ((LOG_INFO,
			         "members of group %s set by %s to %s%s",
			         group, myname, members, suffix));
#ifdef WITH_AUDIT
			SNPRINTF(buf, "members of group %s set by %s to %s%s",
			         group, myname, members, suffix);
			audit_logger (AUDIT_USER_ACCT, Prog,
			              buf,
			              group, AUDIT_NO_ID,
			              SHADOW_AUDIT_SUCCESS);
#endif
		}
	} else {
		SYSLOG ((LOG_INFO,
		         "password of group %s changed by %s%s",
		         group, myname, suffix));
#ifdef WITH_AUDIT
		SNPRINTF(buf, "password of group %s changed by %s%s",
		         group, myname, suffix);
		audit_logger (AUDIT_USER_CHAUTHTOK, Prog,
		              buf,
		              group, AUDIT_NO_ID,
		              SHADOW_AUDIT_SUCCESS);
#endif
	}
}

static void log_gpasswd_success_system (MAYBE_UNUSED void *arg)
{
	log_gpasswd_success ("");
}

static void log_gpasswd_success_group (MAYBE_UNUSED void *arg)
{
	char  buf[1024];

	SNPRINTF(buf, " in %s", gr_dbname());
	log_gpasswd_success (buf);
}

/*
 * close_files - close and unlock the group databases
 *
 *	This cause any changes in the databases to be committed.
 *
 *	It will call exit in case of error.
 */
static void close_files (void)
{
	if (gr_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while writing changes to %s\n"),
		         Prog, gr_dbname ());
		exit (E_NOPERM);
	}
	add_cleanup (log_gpasswd_success_group, NULL);
	del_cleanup (log_gpasswd_failure_group);

	cleanup_unlock_group (NULL);
	del_cleanup (cleanup_unlock_group);

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		if (sgr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while writing changes to %s\n"),
			         Prog, sgr_dbname ());
			exit (E_NOPERM);
		}
		del_cleanup (log_gpasswd_failure_gshadow);

		cleanup_unlock_gshadow (NULL);
		del_cleanup (cleanup_unlock_gshadow);
	}
#endif				/* SHADOWGRP */

	log_gpasswd_success_system (NULL);
	del_cleanup (log_gpasswd_success_group);
}

/*
 * check_perms - check if the user is allowed to change the password of
 *               the specified group.
 *
 *	It only returns if the user is allowed.
 */
#ifdef SHADOWGRP
static void check_perms (const struct group *gr, const struct sgrp *sg)
#else
static void check_perms (const struct group *gr)
#endif
{
	/*
	 * Only root can use the -M and -A options.
	 */
	if (!amroot && (Aflg || Mflg)) {
		failure ();
	}

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		/*
		 * The policy here for changing a group is that
		 * 1) you must be root or
		 * 2) you must be listed as an administrative member.
		 * Administrative members can do anything to a group that
		 * the root user can.
		 */
		if (!amroot && !is_on_list (sg->sg_adm, myname)) {
			failure ();
		}
	} else
#endif				/* SHADOWGRP */
	if (!amroot) {
		/*
		 * The policy here for changing a group is that
		 * 1) you must be root or
		 * 2) you must be the first listed member of the group.
		 * The first listed member of a group can do anything to
		 * that group that the root user can. The rationale for
		 * this hack is that the FIRST user is probably the most
		 * important user in this entire group.
		 *
		 * This feature enabled by default could be a security
		 * problem when installed on existing systems where the
		 * first group member might be just a normal user.
		 * --marekm
		 */
#if !defined(FIRST_MEMBER_IS_ADMIN)
		failure();
#endif
		if (gr->gr_mem[0] == NULL)
			failure();

		if (strcmp(gr->gr_mem[0], myname) != 0)
			failure();
	}
}

/*
 * update_group - Update the group information in the databases
 */
#ifdef SHADOWGRP
static void update_group (struct group *gr, struct sgrp *sg)
#else
static void update_group (struct group *gr)
#endif
{
	if (gr_update (gr) == 0) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"),
		         Prog, gr_dbname (), gr->gr_name);
		exit (1);
	}
#ifdef SHADOWGRP
	if (is_shadowgrp && (sgr_update (sg) == 0)) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"),
		         Prog, sgr_dbname (), sg->sg_name);
		exit (1);
	}
#endif				/* SHADOWGRP */
}

/*
 * get_group - get the current information for the group
 *
 *	The information are copied in group structure(s) so that they can be
 *	modified later.
 *
 *	Note: If !is_shadowgrp, *sg will not be initialized.
 */
#ifdef SHADOWGRP
static void get_group (struct group *gr, struct sgrp *sg)
#else
static void get_group (struct group *gr)
#endif
{
	struct group const*tmpgr = NULL;
#ifdef SHADOWGRP
	struct sgrp const*tmpsg = NULL;
#endif

	if (gr_open (O_RDONLY) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, gr_dbname ());
		SYSLOG ((LOG_WARN, "cannot open %s", gr_dbname ()));
		exit (E_NOPERM);
	}

	tmpgr = gr_locate (group);
	if (NULL == tmpgr) {
		fprintf (stderr,
		         _("%s: group '%s' does not exist in %s\n"),
		         Prog, group, gr_dbname ());
		exit (E_BAD_ARG);
	}

	*gr = *tmpgr;
	gr->gr_name = xstrdup (tmpgr->gr_name);
	gr->gr_passwd = xstrdup (tmpgr->gr_passwd);
	gr->gr_mem = dup_list (tmpgr->gr_mem);

	if (gr_close () == 0) {
		fprintf (stderr,
		         _("%s: failure while closing read-only %s\n"),
		         Prog, gr_dbname ());
		SYSLOG ((LOG_ERR,
		         "failure while closing read-only %s",
		         gr_dbname ()));
		exit (E_NOPERM);
	}

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		if (sgr_open (O_RDONLY) == 0) {
			fprintf (stderr,
			         _("%s: cannot open %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_WARN, "cannot open %s", sgr_dbname ()));
			exit (E_NOPERM);
		}
		tmpsg = sgr_locate (group);
		if (NULL != tmpsg) {
			*sg = *tmpsg;
			sg->sg_name = xstrdup (tmpsg->sg_name);
			sg->sg_passwd = xstrdup (tmpsg->sg_passwd);

			sg->sg_mem = dup_list (tmpsg->sg_mem);
			sg->sg_adm = dup_list (tmpsg->sg_adm);
		} else {
			sg->sg_name = xstrdup (group);
			sg->sg_passwd = gr->gr_passwd;
			gr->gr_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */

			sg->sg_mem = dup_list (gr->gr_mem);

			sg->sg_adm = XMALLOC(2, char *);
#ifdef FIRST_MEMBER_IS_ADMIN
			if (sg->sg_mem[0]) {
				sg->sg_adm[0] = xstrdup (sg->sg_mem[0]);
				sg->sg_adm[1] = NULL;
			} else
#endif
			{
				sg->sg_adm[0] = NULL;
			}

		}
		if (sgr_close () == 0) {
			fprintf (stderr,
			         _("%s: failure while closing read-only %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR,
			         "failure while closing read-only %s",
			         sgr_dbname ()));
			exit (E_NOPERM);
		}
	}
#endif				/* SHADOWGRP */
}

/*
 * change_passwd - change the group's password
 *
 *	Get the new password from the user and update the password in the
 *	group's structure.
 *
 *	It will call exit in case of error.
 */
#ifdef SHADOWGRP
static void change_passwd (struct group *gr, struct sgrp *sg)
#else
static void change_passwd (struct group *gr)
#endif
{
	char *cp;
	static char pass[BUFSIZ];
	int retries;
	const char *salt;

	/*
	 * A new password is to be entered and it must be encrypted, etc.
	 * The password will be prompted for twice, and both entries must be
	 * identical. There is no need to validate the old password since
	 * the invoker is either the group owner, or root.
	 */
	printf (_("Changing the password for group %s\n"), group);

	for (retries = 0; retries < RETRIES; retries++) {
		cp = agetpass (_("New Password: "));
		if (NULL == cp) {
			exit (1);
		}

		STRTCPY(pass, cp);
		erase_pass (cp);
		cp = agetpass (_("Re-enter new password: "));
		if (NULL == cp) {
			MEMZERO(pass);
			exit (1);
		}

		if (strcmp (pass, cp) == 0) {
			erase_pass (cp);
			break;
		}

		erase_pass (cp);
		MEMZERO(pass);

		if (retries + 1 < RETRIES) {
			puts (_("They don't match; try again"));
		}
	}

	if (retries == RETRIES) {
		fprintf (stderr, _("%s: Try again later\n"), Prog);
		exit (1);
	}

	salt = crypt_make_salt (NULL, NULL);
	cp = pw_encrypt (pass, salt);
	if (NULL == cp) {
		fprintf (stderr,
		         _("%s: failed to crypt password with salt '%s': %s\n"),
		         Prog, salt, strerror (errno));
		exit (1);
	}
	MEMZERO(pass);
#ifdef SHADOWGRP
	if (is_shadowgrp) {
		gr->gr_passwd = SHADOW_PASSWD_STRING;
		sg->sg_passwd = cp;
	} else
#endif
	{
		gr->gr_passwd = cp;
	}
}

/*
 * gpasswd - administer the /etc/group file
 */
int main (int argc, char **argv)
{
	struct group grent;
#ifdef SHADOWGRP
	struct sgrp sgent;
#endif
	struct passwd *pw = NULL;

#ifdef WITH_AUDIT
	audit_help_open ();
#endif

	sanitize_env ();
	check_fds ();

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	/*
	 * Make a note of whether or not this command was invoked by root.
	 * This will be used to bypass certain checks later on. Also, set
	 * the real user ID to match the effective user ID. This will
	 * prevent the invoker from issuing signals which would interfere
	 * with this command.
	 */
	bywho = getuid ();
	log_set_progname(Prog);
	log_set_logfd(stderr);

	OPENLOG (Prog);
	setbuf (stdout, NULL);
	setbuf (stderr, NULL);

	process_root_flag ("-Q", argc, argv);

#ifdef SHADOWGRP
	is_shadowgrp = sgr_file_present ();
#endif

	/*
	 * Determine the name of the user that invoked this command. This
	 * is really hit or miss because there are so many ways that command
	 * can be executed and so many ways to trip up the routines that
	 * report the user name.
	 */
	pw = get_my_pwent ();
	if (NULL == pw) {
		fprintf (stderr, _("%s: Cannot determine your user name.\n"),
		         Prog);
		SYSLOG ((LOG_WARN,
		         "Cannot determine the user name of the caller (UID %lu)",
		         (unsigned long) getuid ()));
		exit (E_NOPERM);
	}
	myname = xstrdup (pw->pw_name);

	/*
	 * Register an exit function to warn for any inconsistency that we
	 * could create.
	 */
	if (atexit (do_cleanups) != 0) {
		fprintf(stderr, "%s: cannot set exit function\n", Prog);
		exit (1);
	}

	/* Parse the options */
	process_flags (argc, argv);

	/*
	 * Replicate the group so it can be modified later on.
	 */
#ifdef SHADOWGRP
	get_group (&grent, &sgent);
#else
	get_group (&grent);
#endif

	/*
	 * Check if the user is allowed to change the password of this group.
	 */
#ifdef SHADOWGRP
	check_perms (&grent, &sgent);
#else
	check_perms (&grent);
#endif

	/*
	 * Removing a password is straight forward. Just set the password
	 * field to a "".
	 */
	if (rflg) {
#ifdef SHADOWGRP
		if (is_shadowgrp) {
			grent.gr_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */
			sgent.sg_passwd = "";	/* XXX warning: const */
		} else
#endif				/* SHADOWGRP */
		{
			grent.gr_passwd = "";	/* XXX warning: const */
		}
		goto output;
	} else if (Rflg) {
		/*
		 * Same thing for restricting the group. Set the password
		 * field to "!".
		 */
#ifdef SHADOWGRP
		if (is_shadowgrp) {
			grent.gr_passwd = SHADOW_PASSWD_STRING;	/* XXX warning: const */
			sgent.sg_passwd = "!";	/* XXX warning: const */
		} else
#endif				/* SHADOWGRP */
		{
			grent.gr_passwd = "!";	/* XXX warning: const */
		}
		goto output;
	}

	/*
	 * Adding a member to a member list is pretty straightforward as
	 * well. Call the appropriate routine and split.
	 */
	if (aflg) {
		printf (_("Adding user %s to group %s\n"), user, group);
		grent.gr_mem = add_list (grent.gr_mem, user);
#ifdef SHADOWGRP
		if (is_shadowgrp) {
			sgent.sg_mem = add_list (sgent.sg_mem, user);
		}
#endif
		goto output;
	}

	/*
	 * Removing a member from the member list is the same deal as adding
	 * one, except the routine is different.
	 */
	if (dflg) {
		bool removed = false;

		printf (_("Removing user %s from group %s\n"), user, group);

		if (is_on_list (grent.gr_mem, user)) {
			removed = true;
			grent.gr_mem = del_list (grent.gr_mem, user);
		}
#ifdef SHADOWGRP
		if (is_shadowgrp) {
			if (is_on_list (sgent.sg_mem, user)) {
				removed = true;
				sgent.sg_mem = del_list (sgent.sg_mem, user);
			}
		}
#endif
		if (!removed) {
			fprintf (stderr,
			         _("%s: user '%s' is not a member of '%s'\n"),
			         Prog, user, group);
			exit (E_BAD_ARG);
		}
		goto output;
	}
#ifdef SHADOWGRP
	/*
	 * Replacing the entire list of administrators is simple. Check the
	 * list to make sure everyone is a real user. Then slap the new list
	 * in place.
	 */
	if (Aflg) {
		sgent.sg_adm = comma_to_list (admins);
		if (!Mflg) {
			goto output;
		}
	}
#endif				/* SHADOWGRP */

	/*
	 * Replacing the entire list of members is simple. Check the list to
	 * make sure everyone is a real user. Then slap the new list in
	 * place.
	 */
	if (Mflg) {
#ifdef SHADOWGRP
		sgent.sg_mem = comma_to_list (members);
#endif
		grent.gr_mem = comma_to_list (members);
		goto output;
	}

	/*
	 * If the password is being changed, the input and output must both
	 * be a tty. The typical keyboard signals are caught so the termio
	 * modes can be restored.
	 */
	if ((isatty (0) == 0) || (isatty (1) == 0)) {
		fprintf (stderr, _("%s: Not a tty\n"), Prog);
		exit (E_NOPERM);
	}

	catch_signals (0);	/* save tty modes */

	(void) signal (SIGHUP, catch_signals);
	(void) signal (SIGINT, catch_signals);
	(void) signal (SIGQUIT, catch_signals);
	(void) signal (SIGTERM, catch_signals);
	(void) signal (SIGTSTP, catch_signals);

	/* Prompt for the new password */
#ifdef SHADOWGRP
	change_passwd (&grent, &sgent);
#else
	change_passwd (&grent);
#endif

	/*
	 * This is the common arrival point to output the new group file.
	 * The freshly crafted entry is in allocated space. The group file
	 * will be locked and opened for writing. The new entry will be
	 * output, etc.
	 */
      output:
	if (setuid (0) != 0) {
		fputs (_("Cannot change ID to root.\n"), stderr);
		SYSLOG ((LOG_ERR, "can't setuid(0)"));
		closelog ();
		exit (E_NOPERM);
	}
	pwd_init ();

	open_files ();

#ifdef SHADOWGRP
	update_group (&grent, &sgent);
#else
	update_group (&grent);
#endif

	close_files ();

	nscd_flush_cache ("group");
	sssd_flush_cache (SSSD_DB_GROUP);

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		free(sgent.sg_adm);
		free(sgent.sg_mem);
	}
#endif
	free(grent.gr_mem);
	exit (E_SUCCESS);
}

