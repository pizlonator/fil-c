
/*

  PAM initialization routine for vlock, the VT locking program for linux.

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
#include <stdlib.h>
#include <syslog.h>

#include <security/pam_misc.h>

#include "pam_auth.h"
#include "vlock.h"
#include "libcommon.h"

static struct pam_conv conv = {
	&misc_conv,
	NULL
};

pam_handle_t *
init_pam(const char *username, const char *tty, int log)
{
	pam_handle_t *pamh = 0;
	int rc             = pam_start("vlock", username, &conv, &pamh);

	if (rc != PAM_SUCCESS) {
		/* pam_strerror is not available atm. */
		if (log)
			syslog(LOG_WARNING, "pam_start failed: %m");
		else
			kbd_warning(errno, "pam_start");
		return 0;
	}

	rc = pam_set_item(pamh, PAM_TTY, tty);
	if (rc != PAM_SUCCESS) {
		if (log)
			syslog(LOG_WARNING, "pam_set_item: %s",
			       pam_strerror(pamh, rc));
		else
			kbd_warning(0, "pam_set_item: %s",
			            pam_strerror(pamh, rc));
		pam_end(pamh, rc);
		return 0;
	}

	return pamh;
}
