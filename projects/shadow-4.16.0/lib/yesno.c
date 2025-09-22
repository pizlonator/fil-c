/*
 * SPDX-FileCopyrightText: 1992 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 2007 - 2008, Nicolas François
 * SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <config.h>

#ident "$Id$"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "prototypes.h"


/*
 * Synopsis
 *	bool yes_or_no(bool read_only);
 *
 * Arguments
 *	read_only
 *		In read-only mode, all questions are answered "no".  It
 *		will print "No" to stdout.
 *
 * Description
 *	After a yes/no question, this function gets the answer from the
 *	user.
 *
 *	Calls to this function will normally be preceeded by a prompt on
 *	stdout, so we should fflush(3).
 *
 * Return value
 *	false	"no"
 *	true	"yes"
 *
 * See also
 *	rpmatch(3)
 */


#if !defined(HAVE_RPMATCH)
static int rpmatch(const char *response);
#endif


bool
yes_or_no(bool read_only)
{
	bool   ret;
	char   *buf;
	size_t size;

	if (read_only) {
		puts(_("No"));
		return false;
	}

	fflush(stdout);

	buf = NULL;
	ret = false;
	size = 0;
	if (getline(&buf, &size, stdin) != -1)
		ret = rpmatch(buf) == 1;

	free(buf);
	return ret;
}


#if !defined(HAVE_RPMATCH)
static int
rpmatch(const char *response)
{
	if (response[0] == 'y' || response[0] == 'Y')
		return 1;

	if (response[0] == 'n' || response[0] == 'n')
		return 0;

	return -1;
}
#endif
