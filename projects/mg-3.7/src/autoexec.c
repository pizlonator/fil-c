/* $OpenBSD: autoexec.c,v 1.18 2021/04/21 14:45:28 lum Exp $ */
/* this file is in the public domain */
/* Author: Vincent Labrecque <vincent@openbsd.org>	April 2002 */

#include <fnmatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "funmap.h"

struct autoexec {
	SLIST_ENTRY(autoexec) next;	/* link in the linked list */
	const char	*pattern;	/* Pattern to match to filenames */
	PF		 fp;
};

static SLIST_HEAD(, autoexec)	 autos;
static int			 ready;


#define AUTO_GROW 8
/*
 * Return a NULL terminated array of function pointers to be called
 * when we open a file that matches <fname>.  The list must be free(ed)
 * after use.
 */
PF *
find_autoexec(const char *fname)
{
	PF		*pfl, *npfl;
	int		 have, used;
	struct autoexec *ae;

	if (!ready)
		return (NULL);

	pfl = NULL;
	have = 0;
	used = 0;
	SLIST_FOREACH(ae, &autos, next) {
		if (fnmatch(ae->pattern, fname, 0) == 0) {
			if (used >= have) {
				npfl = reallocarray(pfl, have + AUTO_GROW + 1,
				    sizeof(PF));
				if (npfl == NULL)
					panic("out of memory");
				pfl = npfl;
				have += AUTO_GROW;
			}
			pfl[used++] = ae->fp;
		}
	}
	if (used)
		pfl[used] = NULL;

	return (pfl);
}

int
add_autoexec(const char *pattern, const char *func)
{
	PF		 fp;
	struct autoexec *ae;

	if (!ready) {
		SLIST_INIT(&autos);
		ready = 1;
	}
	fp = name_function(func);
	if (fp == NULL)
		return (FALSE);
	ae = malloc(sizeof(*ae));
	if (ae == NULL)
		return (FALSE);
	ae->fp = fp;
	ae->pattern = strdup(pattern);
	if (ae->pattern == NULL) {
		free(ae);
		return (FALSE);
	}
	SLIST_INSERT_HEAD(&autos, ae, next);

	return (TRUE);
}

/*
 * Register an auto-execute hook; that is, specify a filename pattern
 * (conforming to the shell's filename globbing rules) and an associated
 * function to execute when a file matching the specified pattern
 * is read into a buffer.
*/
int
auto_execute(int f, int n)
{
	char	patbuf[BUFSIZE], funcbuf[BUFSIZE], *patp, *funcp;
	int	s;

	if ((patp = eread("Filename pattern: ", patbuf, sizeof(patbuf),
	    EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (patp[0] == '\0')
		return (FALSE);
	if ((funcp = eread("Execute: ", funcbuf, sizeof(funcbuf),
	    EFNEW | EFCR | EFFUNC)) == NULL)
		return (ABORT);
	else if (funcp[0] == '\0')
		return (FALSE);
	if ((s = add_autoexec(patp, funcp)) != TRUE)
		return (s);
	return (TRUE);
}
