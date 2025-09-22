/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001       , Michał Moskal
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2013, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <stdio.h>

#include "alloc.h"
#include "defines.h"
#include "memzero.h"
#include "prototypes.h"
#include "pwio.h"

/*@null@*/ /*@only@*/struct passwd *__pw_dup (const struct passwd *pwent)
{
	struct passwd *pw;

	pw = CALLOC (1, struct passwd);
	if (NULL == pw) {
		return NULL;
	}
	/* The libc might define other fields. They won't be copied. */
	pw->pw_uid = pwent->pw_uid;
	pw->pw_gid = pwent->pw_gid;
	/*@-mustfreeonly@*/
	pw->pw_name = strdup (pwent->pw_name);
	/*@=mustfreeonly@*/
	if (NULL == pw->pw_name) {
		pw_free(pw);
		return NULL;
	}
	/*@-mustfreeonly@*/
	pw->pw_passwd = strdup (pwent->pw_passwd);
	/*@=mustfreeonly@*/
	if (NULL == pw->pw_passwd) {
		pw_free(pw);
		return NULL;
	}
	/*@-mustfreeonly@*/
	pw->pw_gecos = strdup (pwent->pw_gecos);
	/*@=mustfreeonly@*/
	if (NULL == pw->pw_gecos) {
		pw_free(pw);
		return NULL;
	}
	/*@-mustfreeonly@*/
	pw->pw_dir = strdup (pwent->pw_dir);
	/*@=mustfreeonly@*/
	if (NULL == pw->pw_dir) {
		pw_free(pw);
		return NULL;
	}
	/*@-mustfreeonly@*/
	pw->pw_shell = strdup (pwent->pw_shell);
	/*@=mustfreeonly@*/
	if (NULL == pw->pw_shell) {
		pw_free(pw);
		return NULL;
	}

	return pw;
}

void
pw_free(/*@only@*/struct passwd *pwent)
{
	if (pwent != NULL) {
		free (pwent->pw_name);
		if (pwent->pw_passwd) {
			strzero (pwent->pw_passwd);
			free (pwent->pw_passwd);
		}
		free (pwent->pw_gecos);
		free (pwent->pw_dir);
		free (pwent->pw_shell);
		free (pwent);
	}
}

