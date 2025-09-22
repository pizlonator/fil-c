/*
 * SPDX-FileCopyrightText: 1989 - 1991, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2010       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <stdio.h>

#include "alloc.h"
#include "defines.h"
#include "getdef.h"
#include "prototypes.h"
/*
 * motd -- output the /etc/motd file
 *
 * motd() determines the name of a login announcement file and outputs
 * it to the user's terminal at login time.  The MOTD_FILE configuration
 * option is a colon-delimited list of filenames.
 */
void motd (void)
{
	FILE *fp;
	char *motdlist;
	const char *motdfile;
	char *mb;
	int c;

	motdfile = getdef_str ("MOTD_FILE");
	if (NULL == motdfile) {
		return;
	}

	motdlist = xstrdup (motdfile);

	for (mb = motdlist; ;mb = NULL) {
		motdfile = strtok (mb, ":");
		if (NULL == motdfile) {
			break;
		}

		fp = fopen (motdfile, "r");
		if (NULL != fp) {
			while ((c = getc (fp)) != EOF) {
				putchar (c);
			}
			fclose (fp);
		}
	}
	fflush (stdout);

	free (motdlist);
}

