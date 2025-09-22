/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001       , Michał Moskal
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2013, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include "prototypes.h"
#include "defines.h"
#include <shadow.h>
#include <stdio.h>

#include "alloc.h"
#include "memzero.h"
#include "shadowio.h"

/*@null@*/ /*@only@*/struct spwd *__spw_dup (const struct spwd *spent)
{
	struct spwd *sp;

	sp = CALLOC (1, struct spwd);
	if (NULL == sp) {
		return NULL;
	}
	/* The libc might define other fields. They won't be copied. */
	sp->sp_lstchg = spent->sp_lstchg;
	sp->sp_min    = spent->sp_min;
	sp->sp_max    = spent->sp_max;
	sp->sp_warn   = spent->sp_warn;
	sp->sp_inact  = spent->sp_inact;
	sp->sp_expire = spent->sp_expire;
	sp->sp_flag   = spent->sp_flag;
	/*@-mustfreeonly@*/
	sp->sp_namp   = strdup (spent->sp_namp);
	/*@=mustfreeonly@*/
	if (NULL == sp->sp_namp) {
		free(sp);
		return NULL;
	}
	/*@-mustfreeonly@*/
	sp->sp_pwdp = strdup (spent->sp_pwdp);
	/*@=mustfreeonly@*/
	if (NULL == sp->sp_pwdp) {
		free(sp->sp_namp);
		free(sp);
		return NULL;
	}

	return sp;
}

void
spw_free(/*@only@*/struct spwd *spent)
{
	if (spent != NULL) {
		free (spent->sp_namp);
		if (NULL != spent->sp_pwdp) {
			strzero (spent->sp_pwdp);
			free (spent->sp_pwdp);
		}
		free (spent);
	}
}

