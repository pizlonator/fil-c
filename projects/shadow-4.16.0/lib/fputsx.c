/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1999, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#include <stdio.h>
#include "defines.h"
#include "prototypes.h"

#ident "$Id$"


/*@null@*/char *
fgetsx(/*@returned@*/char *restrict buf, int cnt, FILE *restrict f)
{
	char *cp = buf;
	char *ep;

	while (cnt > 0) {
		if (fgets (cp, cnt, f) != cp) {
			if (cp == buf) {
				return 0;
			} else {
				break;
			}
		}
		ep = strrchr (cp, '\\');
		if ((NULL != ep) && (*(ep + 1) == '\n')) {
			cnt -= ep - cp;
			if (cnt > 0) {
				cp = ep;
				*cp = '\0';
			}
		} else {
			break;
		}
	}
	return buf;
}

int fputsx (const char *s, FILE * stream)
{
	int i;

	for (i = 0; '\0' != *s; i++, s++) {
		if (putc (*s, stream) == EOF) {
			return EOF;
		}

#if 0				/* The standard getgr*() can't handle that.  --marekm */
		if (i > (BUFSIZ / 2)) {
			if (putc ('\\', stream) == EOF ||
			    putc ('\n', stream) == EOF)
				return EOF;

			i = 0;
		}
#endif
	}
	return 0;
}

