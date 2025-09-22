/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1991 - 1994, Chip Rosenthal
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ifndef USE_PAM

#ident "$Id$"

#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "prototypes.h"
#include "getdef.h"

/*
 * tz - return local timezone name
 *
 * tz() determines the name of the local timezone by reading the
 * contents of the file named by ``fname''.
 */
/*@observer@*/const char *tz (const char *fname)
{
	FILE *fp = NULL;
	static char tzbuf[BUFSIZ];
	const char *def_tz = "TZ=CST6CDT";

	fp = fopen (fname, "r");
	if (   (NULL == fp)
	    || (fgets (tzbuf, sizeof (tzbuf), fp) == NULL)) {
		def_tz = getdef_str ("ENV_TZ");
		if ((NULL == def_tz) || ('/' == def_tz[0])) {
			def_tz = "TZ=CST6CDT";
		}

		strcpy (tzbuf, def_tz);
	} else {
		/* Remove optional trailing '\n'. */
		tzbuf[strcspn (tzbuf, "\n")] = '\0';
	}

	if (NULL != fp) {
		(void) fclose (fp);
	}

	return tzbuf;
}
#else				/* !USE_PAM */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* !USE_PAM */

