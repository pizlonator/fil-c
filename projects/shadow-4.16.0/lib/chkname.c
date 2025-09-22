// SPDX-FileCopyrightText: 1990-1994, Julianne Frances Haugh
// SPDX-FileCopyrightText: 1996-2000, Marek Michałkiewicz
// SPDX-FileCopyrightText: 2001-2005, Tomasz Kłoczko
// SPDX-FileCopyrightText: 2005-2008, Nicolas François
// SPDX-FileCopyrightText: 2023-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


/*
 * is_valid_user_name(), is_valid_group_name() - check the new user/group
 * name for validity;
 * return values:
 *   true  - OK
 *   false - bad name
 */


#include <config.h>

#ident "$Id$"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

#include "defines.h"
#include "chkname.h"


int allow_bad_names = false;


size_t
login_name_max_size(void)
{
	long  conf;

	errno = 0;
	conf = sysconf(_SC_LOGIN_NAME_MAX);
	if (conf == -1 && errno != 0)
		return LOGIN_NAME_MAX;

	return conf;
}


static bool is_valid_name (const char *name)
{
	if (allow_bad_names) {
		return true;
	}

	/*
         * User/group names must match BRE regex:
         *    [a-zA-Z0-9_.][a-zA-Z0-9_.-]*$\?
         *
         * as a non-POSIX, extension, allow "$" as the last char for
         * sake of Samba 3.x "add machine script"
         *
         * Also do not allow fully numeric names or just "." or "..".
         */
	int numeric;

	if ('\0' == *name ||
	    ('.' == *name && (('.' == name[1] && '\0' == name[2]) ||
			      '\0' == name[1])) ||
	    !((*name >= 'a' && *name <= 'z') ||
	      (*name >= 'A' && *name <= 'Z') ||
	      (*name >= '0' && *name <= '9') ||
	      *name == '_' ||
	      *name == '.')) {
		return false;
	}

	numeric = isdigit(*name);

	while ('\0' != *++name) {
		if (!((*name >= 'a' && *name <= 'z') ||
		      (*name >= 'A' && *name <= 'Z') ||
		      (*name >= '0' && *name <= '9') ||
		      *name == '_' ||
		      *name == '.' ||
		      *name == '-' ||
		      (*name == '$' && name[1] == '\0')
		     )) {
			return false;
		}
		numeric &= isdigit(*name);
	}

	return !numeric;
}


bool
is_valid_user_name(const char *name)
{
	if (strlen(name) >= login_name_max_size())
		return false;

	return is_valid_name(name);
}


bool is_valid_group_name (const char *name)
{
	/*
	 * Arbitrary limit for group names.
	 * HP-UX 10 limits to 16 characters
	 */
	if (   (GROUP_NAME_MAX_LENGTH > 0)
	    && (strlen (name) > GROUP_NAME_MAX_LENGTH)) {
		return false;
	}

	return is_valid_name (name);
}
