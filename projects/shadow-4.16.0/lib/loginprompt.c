/*
 * SPDX-FileCopyrightText: 1989 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2011, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include "alloc.h"
#include "attr.h"
#include "memzero.h"
#include "prototypes.h"
#include "defines.h"
#include "getdef.h"

static void login_exit (MAYBE_UNUSED int sig)
{
	_exit (EXIT_FAILURE);
}

/*
 * login_prompt - prompt the user for their login name
 *
 * login_prompt() displays the standard login prompt.  If ISSUE_FILE
 * is set in login.defs, this file is displayed before the prompt.
 */

void login_prompt (char *name, int namesize)
{
	char buf[1024];

	char *cp;
	int i;
	FILE *fp;
	const char *fname = getdef_str ("ISSUE_FILE");

	sighandler_t sigquit;
	sighandler_t sigtstp;

	/*
	 * There is a small chance that a QUIT character will be part of
	 * some random noise during a prompt.  Deal with this by exiting
	 * instead of core dumping.  Do the same thing for SIGTSTP.
	 */

	sigquit = signal (SIGQUIT, login_exit);
	sigtstp = signal (SIGTSTP, login_exit);

	/*
	 * See if the user has configured the issue file to
	 * be displayed and display it before the prompt.
	 */

	if (NULL != fname) {
		fp = fopen (fname, "r");
		if (NULL != fp) {
			while ((i = getc (fp)) != EOF) {
				(void) putc (i, stdout);
			}

			(void) fclose (fp);
		}
	}
	(void) gethostname (buf, sizeof buf);
	printf (_("\n%s login: "), buf);
	(void) fflush (stdout);

	/*
	 * Read the user's response.  The trailing newline will be
	 * removed.
	 */

	MEMZERO(buf);
	if (fgets (buf, sizeof buf, stdin) != buf) {
		exit (EXIT_FAILURE);
	}

	cp = strchr (buf, '\n');
	if (NULL == cp) {
		exit (EXIT_FAILURE);
	}
	*cp = '\0';		/* remove \n [ must be there ] */

	/*
	 * Skip leading whitespace.  This makes "  username" work right.
	 * Then copy the rest (up to the end) into the username.
	 */

	for (cp = buf; *cp == ' ' || *cp == '\t'; cp++);

	for (i = 0; i < namesize - 1 && *cp != '\0'; name[i++] = *cp++);

	name[i] = '\0';

	/*
	 * Set the SIGQUIT handler back to its original value
	 */

	(void) signal (SIGQUIT, sigquit);
	(void) signal (SIGTSTP, sigtstp);
}

