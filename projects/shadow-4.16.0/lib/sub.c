/*
 * SPDX-FileCopyrightText: 1989 - 1991, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1999, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2006, Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include "prototypes.h"
#include "defines.h"
#define	MAX_SUBROOT2	"maximum subsystem depth reached\n"
#define	BAD_SUBROOT2	"invalid root `%s' for user `%s'\n"
#define	NO_SUBROOT2	"no subsystem root `%s' for user `%s'\n"
#define	MAX_DEPTH	1024
/*
 * subsystem - change to subsystem root
 *
 *	A subsystem login is indicated by the presence of a "*" as
 *	the first character of the login shell.  The given home
 *	directory will be used as the root of a new filesystem which
 *	the user is actually logged into.
 */
void subsystem (const struct passwd *pw)
{
	static int depth = 0;

	/*
	 * Prevent endless loop on misconfigured systems.
	 */
	if (++depth > MAX_DEPTH) {
		printf (_("Maximum subsystem depth reached\n"));
		SYSLOG ((LOG_WARN, MAX_SUBROOT2));
		closelog ();
		exit (EXIT_FAILURE);
	}

	/*
	 * The new root directory must begin with a "/" character.
	 */

	if (pw->pw_dir[0] != '/') {
		printf (_("Invalid root directory '%s'\n"), pw->pw_dir);
		SYSLOG ((LOG_WARN, BAD_SUBROOT2, pw->pw_dir, pw->pw_name));
		closelog ();
		exit (EXIT_FAILURE);
	}

	/*
	 * The directory must be accessible and the current process
	 * must be able to change into it.
	 */

	if (   (chroot (pw->pw_dir) != 0)
	    || (chdir ("/") != 0)) {
		(void) printf (_("Can't change root directory to '%s'\n"),
		               pw->pw_dir);
		SYSLOG ((LOG_WARN, NO_SUBROOT2, pw->pw_dir, pw->pw_name));
		closelog ();
		exit (EXIT_FAILURE);
	}
}

