/*
 * SPDX-FileCopyrightText: 2004 The FreeBSD Project.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */


#ident "$Id$"

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

int main (void)
{
	const char *user, *tty;
	uid_t uid;

	tty = ttyname (0);
	if (NULL == tty) {
		tty = "UNKNOWN";
	}
	user = getlogin ();
	if (NULL == user) {
		user = "UNKNOWN";
	}

	char *ssh_origcmd = getenv("SSH_ORIGINAL_COMMAND");
	uid = getuid (); /* getuid() is always successful */
	openlog ("nologin", LOG_CONS, LOG_AUTH);
	syslog (LOG_CRIT, "Attempted login by %s (UID: %d) on %s%s%s",
	        user, uid, tty,
		(ssh_origcmd ? " SSH_ORIGINAL_COMMAND=" : ""),
		(ssh_origcmd ? ssh_origcmd : ""));
	closelog ();

	printf ("%s", "This account is currently not available.\n");

	return EXIT_FAILURE;
}
