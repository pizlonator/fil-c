/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Separated from setup.c.  --marekm
 */

#include <config.h>

#ident "$Id$"

#include <stdio.h>
#include <grp.h>
#include <errno.h>
#include "prototypes.h"
#include "defines.h"
#include <pwd.h>
#include "getdef.h"

/*
 * setup_groups - set the group credentials
 *	set the group ID to the value from the password file entry
 *	set the supplementary group IDs
 *
 * In case of PAM enabled configurations, this shall be called before
 * pam_setcred.
 *
 * Returns 0 on success, or -1 on failure.
 */
int setup_groups (const struct passwd *info)
{
	/*
	 * Set the real group ID to the primary group ID in the password
	 * file.
	 */
	if (setgid (info->pw_gid) == -1) {
		int err = errno;
		perror ("setgid");
		SYSLOG ((LOG_ERR, "bad group ID `%d' for user `%s': %s\n",
		         info->pw_gid, info->pw_name, strerror (err)));
		closelog ();
		return -1;
	}
#ifdef HAVE_INITGROUPS
	/*
	 * For systems which support multiple concurrent groups, go get
	 * the group set from the /etc/group file.
	 */
	if (initgroups (info->pw_name, info->pw_gid) == -1) {
		int err = errno;
		perror ("initgroups");
		SYSLOG ((LOG_ERR, "initgroups failed for user `%s': %s\n",
		         info->pw_name, strerror (err)));
		closelog ();
		return -1;
	}
#endif
	return 0;
}

/*
 * change_uid - Set the real UID
 *
 * Returns 0 on success, or -1 on failure.
 */
int change_uid (const struct passwd *info)
{
	/*
	 * Set the real UID to the UID value in the password file.
	 */
	if (setuid (info->pw_uid) != 0) {
		int err = errno;
		perror ("setuid");
		SYSLOG ((LOG_ERR, "bad user ID `%d' for user `%s': %s\n",
		         (int) info->pw_uid, info->pw_name, strerror (err)));
		closelog ();
		return -1;
	}
	return 0;
}

/*
 *	setup_uid_gid() performs the following steps -
 *
 *	set the group ID to the value from the password file entry
 *	set the supplementary group IDs
 *	optionally call specified function which may add more groups
 *	set the user ID to the value from the password file entry
 *
 *	Returns 0 on success, or -1 on failure.
 */

#if defined (HAVE_INITGROUPS) && ! (defined USE_PAM)
int setup_uid_gid (const struct passwd *info, bool is_console)
#else
int setup_uid_gid (const struct passwd *info)
#endif
{
	if (setup_groups (info) < 0) {
		return -1;
	}

#if defined (HAVE_INITGROUPS) && ! defined (USE_PAM)
	if (is_console) {
		const char *cp = getdef_str ("CONSOLE_GROUPS");

		if ((NULL != cp) && (add_groups (cp) != 0)) {
			perror ("Warning: add_groups");
		}
	}
#endif				/* HAVE_INITGROUPS && !USE_PAM*/

	if (change_uid (info) < 0) {
		return -1;
	}

	return 0;
}

