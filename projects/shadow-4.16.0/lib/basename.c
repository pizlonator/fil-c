/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * basename.c - not worth copyrighting :-).  Some versions of Linux libc
 * already have basename(), other versions don't.  To avoid confusion,
 * we will not use the function from libc and use a different name here.
 * --marekm
 */

#include <config.h>

#ident "$Id$"

#include "defines.h"
#include "prototypes.h"
/*@observer@*/const char *Basename (const char *str)
{
	if (str == NULL) {
		abort ();
	}

	char *cp = strrchr (str, '/');

	return (NULL != cp) ? cp + 1 : str;
}
