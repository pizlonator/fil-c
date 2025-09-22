
/*

  Find out login name for vlock, the VT locking program for linux.

  Copyright (C) 2002 Dmitry V. Levin <ldv@altlinux.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

#include "vlock.h"

#include "libcommon.h"

/*
 * Try to find out proper login name.
 */
const char *
get_username(void)
{
	const char *name;
	struct passwd *pw = 0;
	uid_t uid         = getuid();

	char *logname = getenv("LOGNAME");

	if (logname) {
		pw = getpwnam(logname);
		/* Ensure uid is same as current. */
		if (pw && pw->pw_uid != uid)
			pw = 0;
	}
	if (!pw)
		pw = getpwuid(uid);

	if (!pw)
		kbd_error(EXIT_FAILURE, 0, _("unrecognized user"));

	name = strdup(pw->pw_name);
	if (!name)
		kbd_error(EXIT_FAILURE, errno, "strdup");

	endpwent();
	return name;
}
