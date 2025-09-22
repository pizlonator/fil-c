/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <sys/types.h>
#include "prototypes.h"
#include "defines.h"
#include <pwd.h>

/*
 * pwd_to_spwd - create entries for new spwd structure
 *
 *	pwd_to_spwd() creates a new (struct spwd) containing the
 *	information in the pointed-to (struct passwd).
 */

struct spwd *pwd_to_spwd (const struct passwd *pw)
{
	static struct spwd sp;

	/*
	 * Nice, easy parts first.  The name and passwd map directly
	 * from the old password structure to the new one.
	 */
	sp.sp_namp = pw->pw_name;
	sp.sp_pwdp = pw->pw_passwd;

	{
		/*
		 * Defaults used if there is no pw_age information.
		 */
		sp.sp_min = 0;
		sp.sp_max = 10000;
		sp.sp_lstchg = gettime () / DAY;
		if (0 == sp.sp_lstchg) {
			/* Better disable aging than requiring a password
			 * change */
			sp.sp_lstchg = -1;
		}
	}

	/*
	 * These fields have no corresponding information in the password
	 * file.  They are set to uninitialized values.
	 */
	sp.sp_warn = -1;
	sp.sp_expire = -1;
	sp.sp_inact = -1;
	sp.sp_flag = SHADOW_SP_FLAG_UNSET;

	return &sp;
}

