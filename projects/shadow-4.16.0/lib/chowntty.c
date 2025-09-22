/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2001, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <grp.h>
#include "prototypes.h"
#include "defines.h"
#include <pwd.h>
#include "getdef.h"
#include "shadowlog.h"

/*
 *	chown_tty() sets the login tty to be owned by the new user ID
 *	with TTYPERM modes
 */

void chown_tty (const struct passwd *info)
{
	struct group *grent;
	gid_t gid;

	/*
	 * See if login.defs has some value configured for the port group
	 * ID.  Otherwise, use the user's primary group ID.
	 */

	grent = getgr_nam_gid (getdef_str ("TTYGROUP"));
	if (NULL != grent) {
		gid = grent->gr_gid;
		gr_free (grent);
	} else {
		gid = info->pw_gid;
	}

	/*
	 * Change the permissions on the TTY to be owned by the user with
	 * the group as determined above.
	 */

	if (   (fchown (STDIN_FILENO, info->pw_uid, gid) != 0)
	    || (fchmod (STDIN_FILENO, getdef_num ("TTYPERM", 0600)) != 0)) {
		int err = errno;
		FILE *shadow_logfd = log_get_logfd();

		fprintf (shadow_logfd,
		         _("Unable to change owner or mode of tty stdin: %s"),
		         strerror (err));
		SYSLOG ((LOG_WARN,
		         "unable to change owner or mode of tty stdin for user `%s': %s\n",
		         info->pw_name, strerror (err)));
		if (EROFS != err) {
			closelog ();
			exit (EXIT_FAILURE);
		}
	}
#ifdef __linux__
	/*
	 * Please don't add code to chown /dev/vcs* to the user logging in -
	 * it's a potential security hole.  I wouldn't like the previous user
	 * to hold the file descriptor open and watch my screen.  We don't
	 * have the *BSD revoke() system call yet, and vhangup() only works
	 * for tty devices (which vcs* is not).  --marekm
	 */
#endif
}

