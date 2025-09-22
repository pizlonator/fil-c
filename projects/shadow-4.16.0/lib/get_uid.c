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
get_uid(const char *uidstr, uid_t *uid)
{
	char       *end;
	long long  val;

	errno = 0;
	val = strtoll(uidstr, &end, 10);
	if (   ('\0' == *uidstr)
	    || ('\0' != *end)
	    || (0 != errno)
	    || (/*@+longintegral@*/val != (uid_t)val)/*@=longintegral@*/) {
		return -1;
	}

	*uid = val;
	return 0;
}

