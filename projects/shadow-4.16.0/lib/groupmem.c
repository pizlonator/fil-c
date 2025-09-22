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

#include "alloc.h"
#include "memzero.h"
#include "prototypes.h"
#include "defines.h"
#include "groupio.h"

/*@null@*/ /*@only@*/struct group *__gr_dup (const struct group *grent)
{
	struct group *gr;
	int i;

	gr = CALLOC(1, struct group);
	if (NULL == gr) {
		return NULL;
	}
	/* The libc might define other fields. They won't be copied. */
	gr->gr_gid = grent->gr_gid;
	/*@-mustfreeonly@*/
	gr->gr_name = strdup (grent->gr_name);
	/*@=mustfreeonly@*/
	if (NULL == gr->gr_name) {
		gr_free(gr);
		return NULL;
	}
	/*@-mustfreeonly@*/
	gr->gr_passwd = strdup (grent->gr_passwd);
	/*@=mustfreeonly@*/
	if (NULL == gr->gr_passwd) {
		gr_free(gr);
		return NULL;
	}

	for (i = 0; grent->gr_mem[i]; i++);

	/*@-mustfreeonly@*/
	gr->gr_mem = MALLOC(i + 1, char *);
	/*@=mustfreeonly@*/
	if (NULL == gr->gr_mem) {
		gr_free(gr);
		return NULL;
	}
	for (i = 0; grent->gr_mem[i]; i++) {
		gr->gr_mem[i] = strdup (grent->gr_mem[i]);
		if (NULL == gr->gr_mem[i]) {
			gr_free(gr);
			return NULL;
		}
	}
	gr->gr_mem[i] = NULL;

	return gr;
}

void gr_free_members (struct group *grent)
{
	if (NULL != grent->gr_mem) {
		size_t i;
		for (i = 0; NULL != grent->gr_mem[i]; i++) {
			free (grent->gr_mem[i]);
		}
		free (grent->gr_mem);
		grent->gr_mem = NULL;
	}
}

void
gr_free(/*@only@*/struct group *grent)
{
	free (grent->gr_name);
	if (NULL != grent->gr_passwd) {
		strzero (grent->gr_passwd);
		free (grent->gr_passwd);
	}
	gr_free_members(grent);
	free (grent);
}
