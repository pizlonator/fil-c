/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2008, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * id - print current process user identification information
 *
 *	Print the current process identifiers. This includes the
 *	UID, GID, effective-UID and effective-GID. Optionally print
 *	the concurrent group set if the current system supports it.
 */

#include <config.h>

#ident "$Id$"

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>

#include "alloc.h"
#include "defines.h"

/* local function prototypes */
static void usage (void);

static void usage (void)
{
	(void) fputs (_("Usage: id [-a]\n"), stderr);
	exit (EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	uid_t ruid, euid;
	gid_t rgid, egid;
	long sys_ngroups;

/*
 * This block of declarations is particularly strained because of several
 * different ways of doing concurrent groups. Old BSD systems used int for
 * gid's, but short for the type passed to getgroups(). Newer systems use
 * gid_t for everything. Some systems have a small and fixed NGROUPS,
 * usually about 16 or 32. Others use bigger values.
 */
	GETGROUPS_T *groups;
	int ngroups;
	bool aflg = 0;
	struct passwd *pw;
	struct group *gr;

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

	/*
	 * Dynamically get the maximum number of groups from system, instead
	 * of using the symbolic constant NGROUPS_MAX. This ensures that the
	 * group limit is not hard coded into the binary, so it will still
	 * work if the system library is recompiled.
	 */
	sys_ngroups = sysconf (_SC_NGROUPS_MAX);
	groups = MALLOC(sys_ngroups, GETGROUPS_T);

	/*
	 * See if the -a flag has been given to print out the concurrent
	 * group set.
	 */

	if (argc > 1) {
		if (argc > 2 || strcmp(argv[1], "-a") != 0)
			usage();
		else
			aflg = true;
	}

	ruid = getuid ();
	euid = geteuid ();
	rgid = getgid ();
	egid = getegid ();

	/*
	 * Print out the real user ID and group ID. If the user or group
	 * does not exist, just give the numerical value.
	 */

	pw = getpwuid (ruid); /* local, no need for xgetpwuid */
	if (NULL != pw) {
		(void) printf ("UID=%lu(%s)",
		               (unsigned long) ruid, pw->pw_name);
	} else {
		(void) printf ("UID=%lu", (unsigned long) ruid);
	}

	gr = getgrgid (rgid);; /* local, no need for xgetgrgid */
	if (NULL != gr) {
		(void) printf (" GID=%lu(%s)",
		               (unsigned long) rgid, gr->gr_name);
	} else {
		(void) printf (" GID=%lu", (unsigned long) rgid);
	}

	/*
	 * Print out the effective user ID and group ID if they are
	 * different from the real values.
	 */

	if (ruid != euid) {
		pw = getpwuid (euid); /* local, no need for xgetpwuid */
		if (NULL != pw) {
			(void) printf (" EUID=%lu(%s)",
			               (unsigned long) euid, pw->pw_name);
		} else {
			(void) printf (" EUID=%lu", (unsigned long) euid);
		}
	}
	if (rgid != egid) {
		gr = getgrgid (egid); /* local, no need for xgetgrgid */
		if (NULL != gr) {
			(void) printf (" EGID=%lu(%s)",
			               (unsigned long) egid, gr->gr_name);
		} else {
			(void) printf (" EGID=%lu", (unsigned long) egid);
		}
	}

	/*
	 * Print out the concurrent group set if the user has requested it.
	 * The group numbers will be printed followed by their names.
	 */
	if (aflg && (ngroups = getgroups (sys_ngroups, groups)) != -1) {
		int i;

		/*
		 * Start off the group message. It will be of the format
		 *
		 *      groups=###(aaa),###(aaa),###(aaa)
		 *
		 * where "###" is a numerical value and "aaa" is the
		 * corresponding name for each respective numerical value.
		 */
		(void) puts (_(" groups="));
		for (i = 0; i < ngroups; i++) {
			if (0 != i)
				(void) putchar (',');

			/* local, no need for xgetgrgid */
			gr = getgrgid (groups[i]);
			if (NULL != gr) {
				(void) printf ("%lu(%s)",
				               (unsigned long) groups[i],
				               gr->gr_name);
			} else {
				(void) printf ("%lu",
				               (unsigned long) groups[i]);
			}
		}
	}
	free (groups);

	/*
	 * Finish off the line.
	 */
	(void) putchar ('\n');

	return EXIT_SUCCESS;
}

