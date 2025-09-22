/*
 * SPDX-FileCopyrightText: 2000       , International Business Machines
 * SPDX-FileCopyrightText: 2000       , George Kraft IV, gk4@us.ibm.com, 03/23/2000
 * SPDX-FileCopyrightText: 2000 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef USE_PAM
#include "pam_defs.h"
#endif				/* USE_PAM */
#include <pwd.h>

#include "alloc.h"
#include "defines.h"
#include "prototypes.h"
#include "groupio.h"
#ifdef SHADOWGRP
#include "sgroupio.h"
#endif
#include "shadowlog.h"

/* Exit Status Values */
/*@-exitarg@*/
#define EXIT_SUCCESS		0	/* success */
#define EXIT_USAGE		1	/* invalid command syntax */
#define EXIT_GROUP_FILE		2	/* group file access problems */
#define EXIT_NOT_ROOT		3	/* not superuser  */
#define EXIT_NOT_EROOT		4	/* not effective superuser  */
#define EXIT_NOT_PRIMARY	5	/* not primary owner of group  */
#define EXIT_NOT_MEMBER		6	/* member of group does not exist */
#define EXIT_MEMBER_EXISTS	7	/* member of group already exists */
#define EXIT_INVALID_USER	8	/* specified user does not exist */
#define EXIT_INVALID_GROUP	9	/* specified group does not exist */

/*
 * Global variables
 */
static const char Prog[] = "groupmems";

static char *adduser = NULL;
static char *deluser = NULL;
static char *thisgroup = NULL;
static bool purge = false;
static bool list = false;
static int exclusive = 0;
static bool gr_locked = false;
#ifdef SHADOWGRP
/* Indicate if shadow groups are enabled on the system
 * (/etc/gshadow present) */
static bool is_shadowgrp;
static bool sgr_locked = false;
#endif

/* local function prototypes */
static char *whoami (void);
static void add_user (const char *user,
                      const struct group *grp);
static void remove_user (const char *user,
                         const struct group *grp);
static void purge_members (const struct group *grp);
static void display_members (const char *const *members);
NORETURN static void usage (int status);
static void process_flags (int argc, char **argv);
static void check_perms (void);
NORETURN static void fail_exit (int code);
#define isroot()		(getuid () == 0)

static char *whoami (void)
{
	/* local, no need for xgetgrgid */
	struct group *grp = getgrgid (getgid ());
	/* local, no need for xgetpwuid */
	struct passwd *usr = getpwuid (getuid ());

	if (   (NULL != usr)
	    && (NULL != grp)
	    && (0 == strcmp (usr->pw_name, grp->gr_name))) {
		return xstrdup (usr->pw_name);
	} else {
		return NULL;
	}
}

/*
 * add_user - Add a user to the specified group
 */
static void add_user (const char *user,
                      const struct group *grp)
{
	struct group *newgrp;

	/* Make sure the user is not already part of the group */
	if (is_on_list (grp->gr_mem, user)) {
		fprintf (stderr,
		         _("%s: user '%s' is already a member of '%s'\n"),
		         Prog, user, grp->gr_name);
		fail_exit (EXIT_MEMBER_EXISTS);
	}

	newgrp = __gr_dup(grp);
	if (NULL == newgrp) {
		fprintf (stderr,
		         _("%s: Out of memory. Cannot update %s.\n"),
		         Prog, gr_dbname ());
		fail_exit (13);
	}

	/* Add the user to the /etc/group group */
	newgrp->gr_mem = add_list (newgrp->gr_mem, user);

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		const struct sgrp *sg = sgr_locate (newgrp->gr_name);
		struct sgrp *newsg;

		if (NULL == sg) {
			/* Create a shadow group based on this group */
			static struct sgrp sgrent;
			sgrent.sg_name = xstrdup (newgrp->gr_name);
			sgrent.sg_mem = dup_list (newgrp->gr_mem);
			sgrent.sg_adm = XMALLOC(1, char *);
#ifdef FIRST_MEMBER_IS_ADMIN
			if (sgrent.sg_mem[0]) {
				sgrent.sg_adm[0] = xstrdup (sgrent.sg_mem[0]);
				sgrent.sg_adm[1] = NULL;
			} else
#endif
			{
				sgrent.sg_adm[0] = NULL;
			}

			/* Move any password to gshadow */
			sgrent.sg_passwd = newgrp->gr_passwd;
			newgrp->gr_passwd = SHADOW_PASSWD_STRING;

			newsg = &sgrent;
		} else {
			newsg = __sgr_dup (sg);
			if (NULL == newsg) {
				fprintf (stderr,
				         _("%s: Out of memory. Cannot update %s.\n"),
				         Prog, sgr_dbname ());
				fail_exit (13);
			}
			/* Add the user to the members */
			newsg->sg_mem = add_list (newsg->sg_mem, user);
			/* Do not touch the administrators */
		}

		if (sgr_update (newsg) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, sgr_dbname (), newsg->sg_name);
			fail_exit (13);
		}
	}
#endif

	if (gr_update (newgrp) == 0) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"),
		         Prog, gr_dbname (), newgrp->gr_name);
		fail_exit (13);
	}
}

/*
 * remove_user - Remove a user from a given group
 */
static void remove_user (const char *user,
                         const struct group *grp)
{
	struct group *newgrp;

	/* Check if the user is a member of the specified group */
	if (!is_on_list (grp->gr_mem, user)) {
		fprintf (stderr,
		         _("%s: user '%s' is not a member of '%s'\n"),
		         Prog, user, grp->gr_name);
		fail_exit (EXIT_NOT_MEMBER);
	}

	newgrp = __gr_dup (grp);
	if (NULL == newgrp) {
		fprintf (stderr,
		         _("%s: Out of memory. Cannot update %s.\n"),
		         Prog, gr_dbname ());
		fail_exit (13);
	}

	/* Remove the user from the /etc/group group */
	newgrp->gr_mem = del_list (newgrp->gr_mem, user);

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		const struct sgrp *sg = sgr_locate (newgrp->gr_name);
		struct sgrp *newsg;

		if (NULL == sg) {
			/* Create a shadow group based on this group */
			static struct sgrp sgrent;
			sgrent.sg_name = xstrdup (newgrp->gr_name);
			sgrent.sg_mem = dup_list (newgrp->gr_mem);
			sgrent.sg_adm = XMALLOC(1, char *);
#ifdef FIRST_MEMBER_IS_ADMIN
			if (sgrent.sg_mem[0]) {
				sgrent.sg_adm[0] = xstrdup (sgrent.sg_mem[0]);
				sgrent.sg_adm[1] = NULL;
			} else
#endif
			{
				sgrent.sg_adm[0] = NULL;
			}

			/* Move any password to gshadow */
			sgrent.sg_passwd = newgrp->gr_passwd;
			newgrp->gr_passwd = SHADOW_PASSWD_STRING;

			newsg = &sgrent;
		} else {
			newsg = __sgr_dup (sg);
			if (NULL == newsg) {
				fprintf (stderr,
				         _("%s: Out of memory. Cannot update %s.\n"),
				         Prog, sgr_dbname ());
				fail_exit (13);
			}
			/* Remove the user from the members */
			newsg->sg_mem = del_list (newsg->sg_mem, user);
			/* Remove the user from the administrators */
			newsg->sg_adm = del_list (newsg->sg_adm, user);
		}

		if (sgr_update (newsg) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, sgr_dbname (), newsg->sg_name);
			fail_exit (13);
		}
	}
#endif

	if (gr_update (newgrp) == 0) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"),
		         Prog, gr_dbname (), newgrp->gr_name);
		fail_exit (13);
	}
}

/*
 * purge_members - Remove every members of the specified group
 */
static void purge_members (const struct group *grp)
{
	struct group *newgrp = __gr_dup (grp);

	if (NULL == newgrp) {
		fprintf (stderr,
		         _("%s: Out of memory. Cannot update %s.\n"),
		         Prog, gr_dbname ());
		fail_exit (13);
	}

	/* Remove all the members of the /etc/group group */
	newgrp->gr_mem[0] = NULL;

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		const struct sgrp *sg = sgr_locate (newgrp->gr_name);
		struct sgrp *newsg;

		if (NULL == sg) {
			/* Create a shadow group based on this group */
			static struct sgrp sgrent;
			sgrent.sg_name = xstrdup (newgrp->gr_name);
			sgrent.sg_mem = XMALLOC(1, char *);
			sgrent.sg_mem[0] = NULL;
			sgrent.sg_adm = XMALLOC(1, char *);
			sgrent.sg_adm[0] = NULL;

			/* Move any password to gshadow */
			sgrent.sg_passwd = newgrp->gr_passwd;
			newgrp->gr_passwd = xstrdup(SHADOW_PASSWD_STRING);

			newsg = &sgrent;
		} else {
			newsg = __sgr_dup (sg);
			if (NULL == newsg) {
				fprintf (stderr,
				         _("%s: Out of memory. Cannot update %s.\n"),
				         Prog, sgr_dbname ());
				fail_exit (13);
			}
			/* Remove all the members of the /etc/gshadow
			 * group */
			newsg->sg_mem[0] = NULL;
			/* Remove all the administrators of the
			 * /etc/gshadow group */
			newsg->sg_adm[0] = NULL;
		}

		if (sgr_update (newsg) == 0) {
			fprintf (stderr,
			         _("%s: failed to prepare the new %s entry '%s'\n"),
			         Prog, sgr_dbname (), newsg->sg_name);
			fail_exit (13);
		}
	}
#endif

	if (gr_update (newgrp) == 0) {
		fprintf (stderr,
		         _("%s: failed to prepare the new %s entry '%s'\n"),
		         Prog, gr_dbname (), newgrp->gr_name);
		fail_exit (13);
	}
}

static void display_members (const char *const *members)
{
	int i;

	for (i = 0; NULL != members[i]; i++) {
		printf ("%s ", members[i]);

		if (NULL == members[i + 1]) {
			printf ("\n");
		} else {
			printf (" ");
		}
	}
}

NORETURN
static void
usage (int status)
{
	FILE *usageout = (EXIT_SUCCESS != status) ? stderr : stdout;
	(void) fprintf (usageout,
	                _("Usage: %s [options] [action]\n"
	                  "\n"
	                  "Options:\n"),
	                Prog);
	(void) fputs (_("  -g, --group groupname         change groupname instead of the user's group\n"
	                "                                (root only)\n"), usageout);
	(void) fputs (_("  -R, --root CHROOT_DIR         directory to chroot into\n"), usageout);
	(void) fputs (_("\n"), usageout);
	(void) fputs (_("Actions:\n"), usageout);
	(void) fputs (_("  -a, --add username            add username to the members of the group\n"), usageout);
	(void) fputs (_("  -d, --delete username         remove username from the members of the group\n"), usageout);
	(void) fputs (_("  -h, --help                    display this help message and exit\n"), usageout);
	(void) fputs (_("  -p, --purge                   purge all members from the group\n"), usageout);
	(void) fputs (_("  -l, --list                    list the members of the group\n"), usageout);
	exit (status);
}

/*
 * process_flags - perform command line argument setting
 */
static void process_flags (int argc, char **argv)
{
	int c;
	static struct option long_options[] = {
		{"add",    required_argument, NULL, 'a'},
		{"delete", required_argument, NULL, 'd'},
		{"group",  required_argument, NULL, 'g'},
		{"help",   no_argument,       NULL, 'h'},
		{"list",   no_argument,       NULL, 'l'},
		{"purge",  no_argument,       NULL, 'p'},
		{"root",   required_argument, NULL, 'R'},
		{NULL, 0, NULL, '\0'}
	};

	while ((c = getopt_long (argc, argv, "a:d:g:hlpR:",
	                         long_options, NULL)) != EOF) {
		switch (c) {
		case 'a':
			adduser = xstrdup (optarg);
			++exclusive;
			break;
		case 'd':
			deluser = xstrdup (optarg);
			++exclusive;
			break;
		case 'g':
			thisgroup = xstrdup (optarg);
			break;
		case 'h':
			usage (EXIT_SUCCESS);
			/*@notreached@*/break;
		case 'l':
			list = true;
			++exclusive;
			break;
		case 'p':
			purge = true;
			++exclusive;
			break;
		case 'R': /* no-op, handled in process_root_flag () */
			break;
		default:
			usage (EXIT_USAGE);
		}
	}

	if ((exclusive > 1) || (optind < argc)) {
		usage (EXIT_USAGE);
	}

	/* local, no need for xgetpwnam */
	if (   (NULL != adduser)
	    && (getpwnam (adduser) == NULL)) {
		fprintf (stderr, _("%s: user '%s' does not exist\n"),
		         Prog, adduser);
		fail_exit (EXIT_INVALID_USER);
	}

}

static void check_perms (void)
{
	if (!list) {
#ifdef USE_PAM
		pam_handle_t *pamh = NULL;
		int retval;
		struct passwd *pampw;

		pampw = getpwuid (getuid ()); /* local, no need for xgetpwuid */
		if (NULL == pampw) {
			fprintf (stderr,
			         _("%s: Cannot determine your user name.\n"),
			         Prog);
			fail_exit (1);
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
			fail_exit (1);
		}
		(void) pam_end (pamh, retval);
#endif
	}
}

static void fail_exit (int code)
{
	if (gr_locked) {
		if (gr_unlock () == 0) {
			fprintf (stderr,
			         _("%s: failed to unlock %s\n"),
			         Prog, gr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
			/* continue */
		}
	}

#ifdef SHADOWGRP
	if (sgr_locked) {
		if (sgr_unlock () == 0) {
			fprintf (stderr,
			         _("%s: failed to unlock %s\n"),
			         Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
			/* continue */
		}
	}
#endif

	exit (code);
}

static void open_files (void)
{
	if (!list) {
		if (gr_lock () == 0) {
			fprintf (stderr,
			         _("%s: cannot lock %s; try again later.\n"),
			         Prog, gr_dbname ());
			fail_exit (EXIT_GROUP_FILE);
		}
		gr_locked = true;

#ifdef SHADOWGRP
		if (is_shadowgrp) {
			if (sgr_lock () == 0) {
				fprintf (stderr,
				         _("%s: cannot lock %s; try again later.\n"),
				         Prog, sgr_dbname ());
				fail_exit (EXIT_GROUP_FILE);
			}
			sgr_locked = true;
		}
#endif
	}

	if (gr_open (list ? O_RDONLY : O_CREAT | O_RDWR) == 0) {
		fprintf (stderr, _("%s: cannot open %s\n"), Prog, gr_dbname ());
		fail_exit (EXIT_GROUP_FILE);
	}

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		if (sgr_open (list ? O_RDONLY : O_CREAT | O_RDWR) == 0) {
			fprintf (stderr, _("%s: cannot open %s\n"), Prog, sgr_dbname ());
			fail_exit (EXIT_GROUP_FILE);
		}
	}
#endif
}

static void close_files (void)
{
	if ((gr_close () == 0) && !list) {
		fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, gr_dbname ());
		SYSLOG ((LOG_ERR, "failure while writing changes to %s", gr_dbname ()));
		fail_exit (EXIT_GROUP_FILE);
	}
	if (gr_locked) {
		if (gr_unlock () == 0) {
			fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, gr_dbname ());
			SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
			/* continue */
		}
		gr_locked = false;
	}

#ifdef SHADOWGRP
	if (is_shadowgrp) {
		if ((sgr_close () == 0) && !list) {
			fprintf (stderr, _("%s: failure while writing changes to %s\n"), Prog, sgr_dbname ());
			SYSLOG ((LOG_ERR, "failure while writing changes to %s", sgr_dbname ()));
			fail_exit (EXIT_GROUP_FILE);
		}
		if (sgr_locked) {
			if (sgr_unlock () == 0) {
				fprintf (stderr, _("%s: failed to unlock %s\n"), Prog, sgr_dbname ());
				SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
				/* continue */
			}
			sgr_locked = false;
		}
	}
#endif
}

int main (int argc, char **argv)
{
	char *name;
	const struct group *grp;

	log_set_progname(Prog);
	log_set_logfd(stderr);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	process_root_flag ("-R", argc, argv);

	OPENLOG (Prog);

#ifdef SHADOWGRP
	is_shadowgrp = sgr_file_present ();
#endif

	process_flags (argc, argv);

	if (NULL == thisgroup) {
		name = whoami ();
		if (!list && (NULL == name)) {
			fprintf (stderr, _("%s: your groupname does not match your username\n"), Prog);
			fail_exit (EXIT_NOT_PRIMARY);
		}
	} else {
		name = thisgroup;
		if (!list && !isroot ()) {
			fprintf (stderr, _("%s: only root can use the -g/--group option\n"), Prog);
			fail_exit (EXIT_NOT_ROOT);
		}
	}

	check_perms ();

	open_files ();

	grp = gr_locate (name);
	if (NULL == grp) {
		fprintf (stderr, _("%s: group '%s' does not exist in %s\n"),
		         Prog, name, gr_dbname ());
		fail_exit (EXIT_INVALID_GROUP);
	}

	if (list) {
		display_members ((const char *const *)grp->gr_mem);
	} else if (NULL != adduser) {
		add_user (adduser, grp);
	} else if (NULL != deluser) {
		remove_user (deluser, grp);
	} else if (purge) {
		purge_members (grp);
	}

	close_files ();

	exit (EXIT_SUCCESS);
}

