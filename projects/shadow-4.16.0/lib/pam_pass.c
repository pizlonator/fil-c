/*
 * SPDX-FileCopyrightText: 1997 - 1999, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ifdef USE_PAM

#ident "$Id$"


/*
 * Change the user's password using PAM.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "defines.h"
#include "pam_defs.h"
#include "prototypes.h"
#include "shadowlog.h"

void do_pam_passwd (const char *user, bool silent, bool change_expired)
{
	pam_handle_t *pamh = NULL;
	int flags = 0, ret;
	FILE *shadow_logfd = log_get_logfd();

	if (silent)
		flags |= PAM_SILENT;
	if (change_expired)
		flags |= PAM_CHANGE_EXPIRED_AUTHTOK;

	ret = pam_start ("passwd", user, &conv, &pamh);
	if (ret != PAM_SUCCESS) {
		fprintf (shadow_logfd,
			 _("passwd: pam_start() failed, error %d\n"), ret);
		exit (10);	/* XXX */
	}

	ret = pam_chauthtok (pamh, flags);
	if (ret != PAM_SUCCESS) {
		fprintf (shadow_logfd, _("passwd: %s\n"), pam_strerror (pamh, ret));
		fputs (_("passwd: password unchanged\n"), shadow_logfd);
		pam_end (pamh, ret);
		exit (10);	/* XXX */
	}

	fputs (_("passwd: password updated successfully\n"), shadow_logfd);
	(void) pam_end (pamh, PAM_SUCCESS);
}
#else				/* !USE_PAM */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* !USE_PAM */
