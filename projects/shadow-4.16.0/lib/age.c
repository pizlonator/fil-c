/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

#include "adds.h"
#include "defines.h"
#include "exitcodes.h"
#include "prototypes.h"


#ident "$Id$"

#ifndef PASSWD_PROGRAM
#define PASSWD_PROGRAM "/bin/passwd"
#endif
/*
 * expire - force password change if password expired
 *
 *	expire() calls /bin/passwd to change the user's password
 *	if it has expired.
 */
int expire (const struct passwd *pw, /*@null@*/const struct spwd *sp)
{
	int status;
	pid_t child;
	pid_t pid;

	if (NULL == sp) {
		return 0;
	}

	/*
	 * See if the user's password has expired, and if so
	 * force them to change their password.
	 */

	status = isexpired (pw, sp);
	switch (status) {
	case 0:
		return 0;
	case 1:
		(void) fputs (_("Your password has expired."), stdout);
		break;
	case 2:
		(void) fputs (_("Your password is inactive."), stdout);
		break;
	case 3:
		(void) fputs (_("Your login has expired."), stdout);
		break;
	}

	/*
	 * Setting the maximum valid period to less than the minimum
	 * valid period means that the minimum period will never
	 * occur while the password is valid, so the user can never
	 * change that password.
	 */

	if ((status > 1) || (sp->sp_max < sp->sp_min)) {
		(void) puts (_("  Contact the system administrator."));
		exit (EXIT_FAILURE);
	}
	(void) puts (_("  Choose a new password."));
	(void) fflush (stdout);

	/*
	 * Close all the files so that unauthorized access won't
	 * occur.  This needs to be done anyway because those files
	 * might become stale after "passwd" is executed.
	 */

	endspent ();
	endpwent ();
#ifdef SHADOWGRP
	endsgent ();
#endif
	endgrent ();

	/*
	 * Execute the /bin/passwd command.  The exit status will be
	 * examined to see what the result is.  If there are any
	 * errors the routine will exit.  This forces the user to
	 * change their password before being able to use the account.
	 */

	pid = fork ();
	if (0 == pid) {
		int err;

		/*
		 * Set the UID to be that of the user.  This causes
		 * passwd to work just like it would had they executed
		 * it from the command line while logged in.
		 */
#if defined(HAVE_INITGROUPS) && ! defined(USE_PAM)
		if (setup_uid_gid (pw, false) != 0)
#else
		if (setup_uid_gid (pw) != 0)
#endif
		{
			_exit (126);
		}

		(void) execl (PASSWD_PROGRAM, PASSWD_PROGRAM, pw->pw_name, (char *) NULL);
		err = errno;
		perror ("Can't execute " PASSWD_PROGRAM);
		_exit ((ENOENT == err) ? E_CMD_NOTFOUND : E_CMD_NOEXEC);
	} else if ((pid_t) -1 == pid) {
		perror ("fork");
		exit (EXIT_FAILURE);
	}

	while (((child = wait (&status)) != pid) && (child != (pid_t)-1));

	if ((child == pid) && (0 == status)) {
		return 1;
	}

	exit (EXIT_FAILURE);
 /*@notreached@*/}

/*
 * agecheck - see if warning is needed for password expiration
 *
 *	agecheck sees how many days until the user's password is going
 *	to expire and warns the user of the pending password expiration.
 */

void agecheck (/*@null@*/const struct spwd *sp)
{
	long now = time(NULL) / DAY;
	long remain;

	if (NULL == sp) {
		return;
	}

	/*
	 * The last, max, and warn fields must be supported or the
	 * warning period cannot be calculated.
	 */

	if (   (-1 == sp->sp_lstchg)
	    || (-1 == sp->sp_max)
	    || (-1 == sp->sp_warn)) {
		return;
	}

	if (0 == sp->sp_lstchg) {
		(void) puts (_("You must change your password."));
		return;
	}

	remain = addsl(sp->sp_lstchg, sp->sp_max, -now);

	if (remain <= sp->sp_warn) {
		if (remain > 1) {
			(void) printf (_("Your password will expire in %ld days.\n"),
			               remain);
		} else if (1 == remain) {
			(void) puts (_("Your password will expire tomorrow."));
		} else if (remain == 0) {
			(void) puts (_("Your password will expire today."));
		}
	}
}

