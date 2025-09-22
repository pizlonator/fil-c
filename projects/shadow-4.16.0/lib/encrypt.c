/*
 * SPDX-FileCopyrightText: 1990 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <unistd.h>
#include <stdio.h>

#include "prototypes.h"
#include "defines.h"
#include "shadowlog_internal.h"

/*@exposed@*//*@null@*/char *pw_encrypt (const char *clear, const char *salt)
{
	static char cipher[128];
	char *cp;

	cp = crypt (clear, salt);
	if (NULL == cp) {
		/*
		 * Single Unix Spec: crypt() may return a null pointer,
		 * and set errno to indicate an error. In this case return
		 * the NULL so the caller can handle appropriately.
		 */
		return NULL;
	}

	/* Some crypt() do not return NULL if the algorithm is not
	 * supported, and return a DES encrypted password. */
	if ((NULL != salt) && (salt[0] == '$') && (strlen (cp) <= 13))
	{
		/*@observer@*/const char *method;
		switch (salt[1])
		{
			case '1':
				method = "MD5";
				break;
			case '2':
				method = "BCRYPT";
				break;
			case '5':
				method = "SHA256";
				break;
			case '6':
				method = "SHA512";
				break;
			case 'y':
				method = "YESCRYPT";
				break;
			default:
			{
				static char nummethod[4] = "$x$";
				nummethod[1] = salt[1];
				method = &nummethod[0];
			}
		}
		(void) fprintf (shadow_logfd,
		                _("crypt method not supported by libcrypt? (%s)\n"),
		                method);
		exit (EXIT_FAILURE);
	}

	if (strlen (cp) != 13) {
		return cp;	/* nonstandard crypt() in libc, better bail out */
	}

	strcpy (cipher, cp);

	return cipher;
}

