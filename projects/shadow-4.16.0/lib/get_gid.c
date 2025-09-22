/*
 * SPDX-FileCopyrightText: 2009       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <config.h>

#ident "$Id$"

#include "prototypes.h"
#include "defines.h"


int
get_gid(const char *gidstr, gid_t *gid)
{
	char       *end;
	long long  val;

	errno = 0;
	val = strtoll(gidstr, &end, 10);
	if (   ('\0' == *gidstr)
	    || ('\0' != *end)
	    || (0 != errno)
	    || (/*@+longintegral@*/val != (gid_t)val)/*@=longintegral@*/) {
		return -1;
	}

	*gid = val;
	return 0;
}

