/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <sys/types.h>
#include <stdio.h>
#include <pwd.h>
#include <string.h>

#include "defines.h"
#include "prototypes.h"
#include "shadowlog_internal.h"

#define	NFIELDS	7

/*
 * sgetpwent - convert a string to a (struct passwd)
 *
 * sgetpwent() parses a string into the parts required for a password
 * structure.  Strict checking is made for the UID and GID fields and
 * presence of the correct number of colons.  Any failing tests result
 * in a NULL pointer being returned.
 *
 * NOTE: This function uses hard-coded string scanning functions for
 *	performance reasons.  I am going to come up with some conditional
 *	compilation glarp to improve on this in the future.
 */
struct passwd *
sgetpwent(const char *buf)
{
	static struct passwd pwent;
	static char pwdbuf[PASSWD_ENTRY_MAX_LENGTH];
	int i;
	char *cp;
	char *fields[NFIELDS];

	/*
	 * Copy the string to a static buffer so the pointers into
	 * the password structure remain valid.
	 */

	if (strlen (buf) >= sizeof pwdbuf) {
		fprintf (shadow_logfd,
		         "%s: Too long passwd entry encountered, file corruption?\n",
		         shadow_progname);
		return 0;	/* fail if too long */
	}
	strcpy (pwdbuf, buf);

	/*
	 * Save a pointer to the start of each colon separated
	 * field.  The fields are converted into NUL terminated strings.
	 */

	for (cp = pwdbuf, i = 0; (i < NFIELDS) && (NULL != cp); i++)
		fields[i] = strsep(&cp, ":");

	/* something at the end, columns over shot */
	if ( cp != NULL ) {
		return( NULL );
	}

	/*
	 * There must be exactly NFIELDS colon separated fields or
	 * the entry is invalid.  Also, the UID and GID must be non-blank.
	 */

	if (i != NFIELDS || *fields[2] == '\0' || *fields[3] == '\0')
		return NULL;

	/*
	 * Each of the fields is converted the appropriate data type
	 * and the result assigned to the password structure.  If the
	 * UID or GID does not convert to an integer value, a NULL
	 * pointer is returned.
	 */

	pwent.pw_name = fields[0];
	pwent.pw_passwd = fields[1];
	if (get_uid(fields[2], &pwent.pw_uid) == -1) {
		return NULL;
	}
	if (get_gid(fields[3], &pwent.pw_gid) == -1) {
		return NULL;
	}
	pwent.pw_gecos = fields[4];
	pwent.pw_dir = fields[5];
	pwent.pw_shell = fields[6];

	return &pwent;
}

