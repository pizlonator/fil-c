/*
 * SPDX-FileCopyrightText: 1992       , Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ifndef HAVE_LCKPWDF

#ident "$Id$"

#include "prototypes.h"
#include "defines.h"
#include "pwio.h"
#include "shadowio.h"
/*
 * lckpwdf - lock the password files
 */
int lckpwdf (void)
{
	int i;

	/*
	 * We have 15 seconds to lock the whole mess
	 */

	for (i = 0; i < 15; i++)
		if (pw_lock ())
			break;
		else
			sleep (1);

	/*
	 * Did we run out of time?
	 */

	if (i == 15)
		return -1;

	/*
	 * Nope, use any remaining time to lock the shadow password
	 * file.
	 */

	for (; i < 15; i++)
		if (spw_lock ())
			break;
		else
			sleep (1);

	/*
	 * Out of time yet?
	 */

	if (i == 15) {
		pw_unlock ();
		return -1;
	}

	/*
	 * Nope - and both files are now locked.
	 */

	return 0;
}

/*
 * ulckpwdf - unlock the password files
 */

int ulckpwdf (void)
{

	/*
	 * Unlock both files.
	 */

	return (pw_unlock () && spw_unlock ())? 0 : -1;
}
#else
extern int ISO_C_forbids_an_empty_translation_unit;
#endif
