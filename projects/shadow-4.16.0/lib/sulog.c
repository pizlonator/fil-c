/*
 * SPDX-FileCopyrightText: 1989 - 1992, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>
#include "prototypes.h"
#include "defines.h"
#include "getdef.h"

/*
 * sulog - log a SU command execution result
 */
void sulog (const char *tty, bool success, const char *oldname, const char *name)
{
	const char *sulog_file;
	time_t now;
	struct tm *tm;
	FILE *fp;
	mode_t oldmask;
	gid_t oldgid = 0;

	if (success) {
		SYSLOG ((LOG_INFO,
			"Successful su for %s by %s",name,oldname));
	} else {
		SYSLOG ((LOG_NOTICE,
			"FAILED su for %s by %s",name,oldname));
	}

	sulog_file = getdef_str ("SULOG_FILE");
	if (NULL == sulog_file) {
		return;
	}

	oldgid = getgid ();
	oldmask = umask (077);
	/* Switch to group root to avoid creating the sulog file with
	 * the wrong group ownership. */
	if ((oldgid != 0) && (setgid (0) != 0)) {
		SYSLOG ((LOG_INFO,
		         "su session not logged to %s", sulog_file));
		/* Continue, but do not switch back to oldgid later */
		oldgid = 0;
	}
	fp = fopen (sulog_file, "a+");
	(void) umask (oldmask);
	if ((oldgid != 0) && (setgid (oldgid) != 0)) {
		perror ("setgid");
		SYSLOG ((LOG_ERR,
		         "can't switch back to group `%d' in sulog",
		         oldgid));
		/* Do not return if the group permission were raised. */
		exit (EXIT_FAILURE);
	}
	if (fp == NULL) {
		return;		/* can't open or create logfile */
	}

	(void) time (&now);
	tm = localtime (&now);

	fprintf (fp, "SU %.02d/%.02d %.02d:%.02d %c %s %s-%s\n",
		 tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
		 success ? '+' : '-', tty, oldname, name);

	(void) fflush (fp);
	fsync (fileno (fp));
	fclose (fp);
	/* TODO: log if failure */
}

