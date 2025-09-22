/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 * SPDX-FileCopyrightText: 2023       , Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Replacements for malloc and strdup with error checking.  Too trivial
   to be worth copyrighting :-).  I did that because a lot of code used
   malloc and strdup without checking for NULL pointer, and I like some
   message better than a core dump...  --marekm

   Yeh, but.  Remember that bailing out might leave the system in some
   bizarre state.  You really want to put in error checking, then add
   some back-out failure recovery code. -- jfh */

#include <config.h>

#ident "$Id$"

#include "alloc.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>

#include "defines.h"
#include "prototypes.h"
#include "shadowlog.h"


extern inline void *xmalloc(size_t size);
extern inline void *xmallocarray(size_t nmemb, size_t size);
extern inline void *mallocarray(size_t nmemb, size_t size);
extern inline void *reallocarrayf(void *p, size_t nmemb, size_t size);
extern inline char *xstrdup(const char *str);


void *
xcalloc(size_t nmemb, size_t size)
{
	void  *p;

	p = calloc(nmemb, size);
	if (p == NULL)
		goto x;

	return p;

x:
	fprintf(log_get_logfd(), _("%s: %s\n"),
	        log_get_progname(), strerror(errno));
	exit(13);
}


void *
xreallocarray(void *p, size_t nmemb, size_t size)
{
	p = reallocarrayf(p, nmemb, size);
	if (p == NULL)
		goto x;

	return p;

x:
	fprintf(log_get_logfd(), _("%s: %s\n"),
	        log_get_progname(), strerror(errno));
	exit(13);
}
