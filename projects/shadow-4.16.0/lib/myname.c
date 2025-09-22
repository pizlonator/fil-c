/*
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * myname.c - determine the current username and get the passwd entry
 *
 */

#include <config.h>

#ident "$Id$"

#include "defines.h"
#include <pwd.h>
#include "prototypes.h"
/*@null@*/ /*@only@*/struct passwd *get_my_pwent (void)
{
	struct passwd *pw;
	const char *cp = getlogin ();
	uid_t ruid = getuid ();

	/*
	 * Try getlogin() first - if it fails or returns a non-existent
	 * username, or a username which doesn't match the real UID, fall
	 * back to getpwuid(getuid()).  This should work reasonably with
	 * usernames longer than the utmp limit (8 characters), as well as
	 * shared UIDs - but not both at the same time...
	 *
	 * XXX - when running from su, will return the current user (not
	 * the original user, like getlogin() does).  Does this matter?
	 */
	if ((NULL != cp) && ('\0' != *cp)) {
		pw = xgetpwnam (cp);
		if ((NULL != pw) && (pw->pw_uid == ruid)) {
			return pw;
		}
		if (NULL != pw) {
			pw_free (pw);
		}
	}

	return xgetpwuid (ruid);
}

