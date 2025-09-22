/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <stdio.h>
#include "prototypes.h"
#include "defines.h"
#include "getdef.h"
/*
 * ttytype - set ttytype from port to terminal type mapping database
 */
void ttytype (const char *line)
{
	FILE *fp;
	char buf[BUFSIZ];
	const char *typefile;
	char *cp;
	char type[1024] = "";
	char port[1024];

	if (getenv ("TERM") != NULL) {
		return;
	}
	typefile = getdef_str ("TTYTYPE_FILE");
	if (NULL == typefile) {
		return;
	}

	fp = fopen (typefile, "r");
	if (NULL == fp) {
		if (errno != ENOENT)
			perror (typefile);
		return;
	}
	while (fgets (buf, sizeof buf, fp) == buf) {
		if (buf[0] == '#') {
			continue;
		}

		cp = strchr (buf, '\n');
		if (NULL != cp) {
			*cp = '\0';
		}

		if (   (sscanf (buf, "%1023s %1023s", type, port) == 2)
		    && (strcmp (line, port) == 0)) {
			break;
		}
	}
	if ((feof (fp) == 0) && (ferror (fp) == 0) && (type[0] != '\0')) {
		addenv ("TERM", type);
	}

	(void) fclose (fp);
}

