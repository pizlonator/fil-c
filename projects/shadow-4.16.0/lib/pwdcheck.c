/*
 * SPDX-FileCopyrightText: 2000 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2008, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#ifndef USE_PAM

#include <stdio.h>
#include <shadow.h>

#include "attr.h"
#include "prototypes.h"
#include "defines.h"
#include "pwauth.h"
#include "shadowlog.h"

void passwd_check (const char *user, const char *passwd, MAYBE_UNUSED const char *progname)
{
	struct spwd *sp;

	sp = getspnam (user); /* !USE_PAM, no need for xgetspnam */
	if (NULL != sp) {
		passwd = sp->sp_pwdp;
	}
	if (pw_auth (passwd, user, PW_LOGIN, NULL) != 0) {
		SYSLOG ((LOG_WARN, "incorrect password for `%s'", user));
		(void) sleep (1);
		fprintf (log_get_logfd(), _("Incorrect password for %s.\n"), user);
		exit (EXIT_FAILURE);
	}
}
#else			/* USE_PAM */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif			/* USE_PAM */
