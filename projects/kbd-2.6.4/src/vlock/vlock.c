
/*

  Main routine for vlock, the VT locking program for linux.

  Copyright (C) 1994-1998 Michael K. Johnson <johnsonm@redhat.com>
  Copyright (C) 2002, 2005 Dmitry V. Levin <ldv@altlinux.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>

#include "pam_auth.h"
#include "vlock.h"

#include "libcommon.h"

int main(int ac, char *const av[])
{
	int rc;
	const char dev_prefix[] = "/dev/";
	const char *username, *tty;
	pam_handle_t *pamh;

	set_progname(av[0]);
	setuplocale();

	/* 1st, parse command line arguments. */
	parse(ac, av);

	/* 2nd, find out tty name... */
	tty = ttyname(STDIN_FILENO);
	if (!tty)
		/* stdin is not a tty, so no need to try. */
		kbd_error(EXIT_FAILURE, 0, _("stdin is not a tty"));

	/* ... and strip its /dev/ prefix. */
	if (!strncmp(tty, dev_prefix, sizeof(dev_prefix) - 1))
		tty += sizeof(dev_prefix) - 1;

	/* 3rd, get username for PAM. */
	username = get_username();

	/* 4th, initialize system logger. */
	openlog("vlock", LOG_PID, LOG_AUTH);

	/* 5th, initialize PAM. */
	if (!(pamh = init_pam(username, tty, 0)))
		exit(EXIT_FAILURE);

	/* 6th, initialize VT, tty and screen. */
	if (!init_vt(tty)) {
		pam_end(pamh, PAM_SUCCESS);
		exit(EXIT_FAILURE);
	}

	/* 7th, Log the fact of console locking. */
	syslog(LOG_NOTICE, "Locked %s on %s for %s by (uid=%u)",
	       locked_name(), tty, username, getuid());

	rc = get_password(pamh, username, tty);
	restore_vt();

	return rc;
}
