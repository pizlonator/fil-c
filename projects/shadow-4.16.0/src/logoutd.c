/*
 * SPDX-FileCopyrightText: 1991 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utmpx.h>
#include "defines.h"
#include "prototypes.h"
#include "shadowlog.h"
#include "sizeof.h"
#include "string/zustr2stp.h"
/*
 * Global variables
 */
static const char Prog[] = "logoutd";

#ifndef DEFAULT_HUP_MESG
#define DEFAULT_HUP_MESG _("login time exceeded\n\n")
#endif

#ifndef HUP_MESG_FILE
#define HUP_MESG_FILE "/etc/logoutd.mesg"
#endif


/* local function prototypes */
static int check_login (const struct utmpx *ut);
static void send_mesg_to_tty (int tty_fd);


/*
 * check_login - check if user (struct utmpx) allowed to stay logged in
 */
static int
check_login(const struct utmpx *ut)
{
	char    user[sizeof(ut->ut_user) + 1];
	char    line[sizeof(ut->ut_line) + 1];
	time_t  now;

	ZUSTR2STP(user, ut->ut_user);
	ZUSTR2STP(line, ut->ut_line);

	(void) time (&now);

	/*
	 * Check if they are allowed to be logged in right now.
	 */
	if (!isttytime(user, line, now)) {
		return 0;
	}
	return 1;
}


static void send_mesg_to_tty (int tty_fd)
{
	TERMIO oldt, newt;
	FILE *mesg_file, *tty_file;
	bool is_tty;

	tty_file = fdopen (tty_fd, "w");
	if (NULL == tty_file) {
		return;
	}

	is_tty = (GTTY (tty_fd, &oldt) == 0);
	if (is_tty) {
		/* Suggested by Ivan Nejgebauar <ian@unsux.ns.ac.yu>:
		   set OPOST before writing the message. */
		newt = oldt;
		newt.c_oflag |= OPOST;
		STTY (tty_fd, &newt);
	}

	mesg_file = fopen (HUP_MESG_FILE, "r");
	if (NULL != mesg_file) {
		int c;
		while ((c = getc (mesg_file)) != EOF) {
			if (c == '\n') {
				putc ('\r', tty_file);
			}
			putc (c, tty_file);
		}
		fclose (mesg_file);
	} else {
		fputs (DEFAULT_HUP_MESG, tty_file);
	}
	fflush (tty_file);
	fclose (tty_file);

	if (is_tty) {
		STTY (tty_fd, &oldt);
	}
}


/*
 * logoutd - logout daemon to enforce /etc/porttime file policy
 *
 *	logoutd is started at system boot time and enforces the login
 *	time and port restrictions specified in /etc/porttime. The
 *	utmp file is periodically scanned and offending users are logged
 *	off from the system.
 */
int
main(int argc, char **argv)
{
	int    i;
	int    status;
	pid_t  pid;

	struct utmpx  *ut;
	char          user[sizeof (ut->ut_user) + 1];	/* terminating NUL */
	char          tty_name[sizeof (ut->ut_line) + 6];	/* /dev/ + NUL */
	int           tty_fd;

	if (1 != argc) {
		(void) fputs (_("Usage: logoutd\n"), stderr);
	}

	(void) setlocale (LC_ALL, "");
	(void) bindtextdomain (PACKAGE, LOCALEDIR);
	(void) textdomain (PACKAGE);

#ifndef DEBUG
	for (i = 0; close (i) == 0; i++);

	setpgrp ();

	/*
	 * Put this process in the background.
	 */
	pid = fork ();
	if (pid > 0) {
		/* parent */
		exit (EXIT_SUCCESS);
	} else if (pid < 0) {
		/* error */
		perror ("fork");
		exit (EXIT_FAILURE);
	}
#endif				/* !DEBUG */

	/*
	 * Start syslogging everything
	 */
	log_set_progname(Prog);
	log_set_logfd(stderr);

	OPENLOG (Prog);

	/*
	 * Scan the utmp file once per minute looking for users that
	 * are not supposed to still be logged in.
	 */
	while (true) {

		/*
		 * Attempt to re-open the utmp file. The file is only
		 * open while it is being used.
		 */
		setutxent();

		/*
		 * Read all of the entries in the utmp file. The entries
		 * for login sessions will be checked to see if the user
		 * is permitted to be signed on at this time.
		 */
		while ((ut = getutxent()) != NULL) {
			if (ut->ut_type != USER_PROCESS) {
				continue;
			}
			if (ut->ut_user[0] == '\0') {
				continue;
			}
			if (check_login (ut)) {
				continue;
			}

			/*
			 * Put the rest of this in a child process. This
			 * keeps the scan from waiting on other ports to die.
			 */

			pid = fork ();
			if (pid > 0) {
				/* parent */
				continue;
			} else if (pid < 0) {
				/* failed - give up until the next scan */
				break;
			}
			/* child */

			if (strncmp (ut->ut_line, "/dev/", 5) != 0) {
				strcpy (tty_name, "/dev/");
			} else {
				tty_name[0] = '\0';
			}

			strncat(tty_name, ut->ut_line, NITEMS(ut->ut_line));
#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif
			tty_fd =
			    open (tty_name, O_WRONLY | O_NDELAY | O_NOCTTY);
			if (tty_fd != -1) {
				send_mesg_to_tty (tty_fd);
				close (tty_fd);
				sleep (10);
			}

			if (ut->ut_pid > 1) {
				kill (-ut->ut_pid, SIGHUP);
				sleep (10);
				kill (-ut->ut_pid, SIGKILL);
			}

			ZUSTR2STP(user, ut->ut_user);

			SYSLOG ((LOG_NOTICE,
				 "logged off user '%s' on '%s'", user,
				 tty_name));

			/*
			 * This child has done all it can, drop dead.
			 */
			exit (EXIT_SUCCESS);
		}

		endutxent();

#ifndef DEBUG
		sleep (60);
#endif
		/*
		 * Reap any dead babies ...
		 */
		while (wait (&status) != -1);
	}

	return EXIT_FAILURE;
}

