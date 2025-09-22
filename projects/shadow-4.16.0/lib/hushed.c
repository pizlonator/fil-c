/*
 * SPDX-FileCopyrightText: 1991 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1991 - 1993, Chip Rosenthal
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>
#include "defines.h"
#include "prototypes.h"
#include "getdef.h"
#include "string/sprintf.h"


/*
 * hushed - determine if a user receives login messages
 *
 * Look in the hushed-logins file (or user's home directory) to see
 * if the user is to receive the login-time messages.
 */
bool hushed (const char *username)
{
	bool           found;
	char           buf[BUFSIZ];
	FILE           *fp;
	const char     *hushfile;
	struct passwd  *pw;

	/*
	 * Get the name of the file to use.  If this option is not
	 * defined, default to a noisy login.
	 */

	hushfile = getdef_str ("HUSHLOGIN_FILE");
	if (NULL == hushfile) {
		return false;
	}

	pw = getpwnam (username);
	if (NULL == pw) {
		return false;
	}

	/*
	 * If this is not a fully rooted path then see if the
	 * file exists in the user's home directory.
	 */

	if (hushfile[0] != '/') {
		SNPRINTF(buf, "%s/%s", pw->pw_dir, hushfile);
		return (access (buf, F_OK) == 0);
	}

	/*
	 * If this is a fully rooted path then go through the file
	 * and see if this user, or its shell is in there.
	 */

	fp = fopen (hushfile, "r");
	if (NULL == fp) {
		return false;
	}
	for (found = false; !found && (fgets (buf, sizeof buf, fp) == buf);) {
		buf[strcspn (buf, "\n")] = '\0';
		found = (strcmp (buf, pw->pw_shell) == 0) ||
		        (strcmp (buf, pw->pw_name) == 0);
	}
	(void) fclose (fp);
	return found;
}

