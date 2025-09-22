/*
 * SPDX-FileCopyrightText: 1989 - 1991, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>
#include "prototypes.h"
#include "defines.h"
#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "getdef.h"
#include "string/sprintf.h"

#ident "$Id$"


void mailcheck (void)
{
	struct stat statbuf;
	char *mailbox;

	if (!getdef_bool ("MAIL_CHECK_ENAB")) {
		return;
	}

	/*
	 * Check incoming mail in Maildir format - J.
	 */
	mailbox = getenv ("MAILDIR");
	if (NULL != mailbox) {
		char  *newmail;

		xasprintf(&newmail, "%s/new", mailbox);

		if (stat (newmail, &statbuf) != -1 && statbuf.st_size != 0) {
			if (statbuf.st_mtime > statbuf.st_atime) {
				free(newmail);
				(void) puts (_("You have new mail."));
				return;
			}
		}
		free(newmail);
	}

	mailbox = getenv ("MAIL");
	if (NULL == mailbox) {
		return;
	}

	if (   (stat (mailbox, &statbuf) == -1)
	    || (statbuf.st_size == 0)) {
		(void) puts (_("No mail."));
	} else if (statbuf.st_atime > statbuf.st_mtime) {
		(void) puts (_("You have mail."));
	} else {
		(void) puts (_("You have new mail."));
	}
}

