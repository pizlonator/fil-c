/*
 * SPDX-FileCopyrightText: 1989 - 1991, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2009       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <stdio.h>
#include <errno.h>
#include "prototypes.h"
#include "defines.h"
#include "string/sprintf.h"


extern char **newenvp;
extern size_t newenvc;

/*
 * shell - execute the named program
 *
 *	shell begins by trying to figure out what argv[0] is going to
 *	be for the named process.  The user may pass in that argument,
 *	or it will be the last pathname component of the file with a
 *	'-' prepended.
 *	Then, it executes the named file.
 */

int shell (const char *file, /*@null@*/const char *arg, char *const envp[])
{
	int   err;
	char  arg0[1024];

	if (file == NULL) {
		errno = EINVAL;
		return errno;
	}

	/*
	 * The argv[0]'th entry is usually the path name, but
	 * for various reasons the invoker may want to override
	 * that.  So, we determine the 0'th entry only if they
	 * don't want to tell us what it is themselves.
	 */
	if (arg == NULL) {
		SNPRINTF(arg0, "-%s", Basename(file));
		arg = arg0;
	}

	/*
	 * First we try the direct approach.  The system should be
	 * able to figure out what we are up to without too much
	 * grief.
	 */
	(void) execle (file, arg, (char *) NULL, envp);
	err = errno;

	if (access (file, R_OK|X_OK) == 0) {
		/*
		 * Assume this is a shell script (with no shebang).
		 * Interpret it with /bin/sh
		 */
		(void) execle (SHELL, "sh", "-", file, (char *) NULL, envp);
		err = errno;
	}

	/*
	 * Obviously something is really wrong - I can't figure out
	 * how to execute this stupid shell, so I might as well give
	 * up in disgust ...
	 */
	SNPRINTF(arg0, _("Cannot execute %s"), file);
	errno = err;
	perror (arg0);
	return err;
}

