/*
 * SPDX-FileCopyrightText: 1991 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2008, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <grp.h>
#include <pwd.h>
#include <stdio.h>

#include "alloc.h"
#include "defines.h"
#include "prototypes.h"
#include "shadowlog.h"

/*
 * Global variables
 */
static const char Prog[] = "groups";

/* local function prototypes */
static void print_groups (const char *member);

/*
 * print_groups - print the groups which the named user is a member of
 *
 *	print_groups() scans the groups file for the list of groups which
 *	the user is listed as being a member of.
 */
static void print_groups (const char *member)
{
	int groups = 0;
	struct group *grp;
	struct passwd *pwd;
	bool flag = false;

	pwd = getpwnam (member); /* local, no need for xgetpwnam */
	if (NULL == pwd) {
		(void) fprintf (stderr, _("%s: unknown user %s\n"),
		                Prog, member);
		exit (EXIT_FAILURE);
	}

	setgrent ();
	while ((grp = getgrent ()) != NULL) {
		if (is_on_list (grp->gr_mem, member)) {
			if (0 != groups) {
				(void) putchar (' ');
			}
			groups++;

			(void) printf ("%s", grp->gr_name);
			if (grp->gr_gid == pwd->pw_gid) {
				flag = true;
			}
		}
	}
	endgrent ();

	/* The user may not be in the list of members of its primary group */
	if (!flag) {
		grp = getgrgid (pwd->pw_gid); /* local, no need for xgetgrgid */
		if (NULL != grp) {
			if (0 != groups) {
				(void) putchar (' ');
			}
			groups++;

			(void) printf ("%s", grp->gr_name);
		}
	}

	if (0 != groups) {
		(void) putchar ('\n');
	}
}

/*
 * groups - print out the groups a process is a member of
 */
int main (int argc, char **argv)
{
	long sys_ngroups;
	GETGROUPS_T *groups;

	sys_ngroups = sysconf (_SC_NGROUPS_MAX);
	groups = XMALLOC(sys_ngroups, GETGROUPS_T);

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	log_set_progname(Prog);
	log_set_logfd(stderr);

	if (argc == 1) {

		/*
		 * Called with no arguments - give the group set for the
		 * current user.
		 */

		int i;
		int pri_grp; /* TODO: should be GETGROUPS_T */
		/*
		 * This system supports concurrent group sets, so I can ask
		 * the system to tell me which groups are currently set for
		 * this process.
		 */
		int ngroups = getgroups (sys_ngroups, groups);
		if (ngroups < 0) {
			perror ("getgroups");
			exit (EXIT_FAILURE);
		}

		/*
		 * The groupset includes the primary group as well.
		 */
		pri_grp = getegid ();
		for (i = 0; i < ngroups; i++) {
			if (pri_grp == (int) groups[i]) {
				break;
			}
		}

		if (i != ngroups) {
			pri_grp = -1;
		}

		/*
		 * Print out the name of every group in the current group
		 * set. Unknown groups are printed as their decimal group ID
		 * values.
		 */
		if (-1 != pri_grp) {
			struct group *gr;
			/* local, no need for xgetgrgid */
			gr = getgrgid (pri_grp);
			if (NULL != gr) {
				(void) printf ("%s", gr->gr_name);
			} else {
				(void) printf ("%d", pri_grp);
			}
		}

		for (i = 0; i < ngroups; i++) {
			struct group *gr;
			if ((0 != i) || (-1 != pri_grp)) {
				(void) putchar (' ');
			}

			/* local, no need for xgetgrgid */
			gr = getgrgid (groups[i]);
			if (NULL != gr) {
				(void) printf ("%s", gr->gr_name);
			} else {
				(void) printf ("%ld", (long) groups[i]);
			}
		}
		(void) putchar ('\n');
	} else {

		/*
		 * The invoker wanted to know about some other user. Use
		 * that name to look up the groups instead.
		 */
		print_groups (argv[1]);
	}
	return EXIT_SUCCESS;
}

