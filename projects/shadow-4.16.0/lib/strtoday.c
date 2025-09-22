/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1999, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#include <ctype.h>

#ident "$Id$"

#include "atoi/str2i.h"
#include "prototypes.h"
#include "getdate.h"


/*
 * strtoday() now uses get_date() (borrowed from GNU shellutils)
 * which can handle many date formats, for example:
 *	1970-09-17	# ISO 8601.
 *	70-9-17		# This century assumed by default.
 *	70-09-17	# Leading zeros are ignored.
 *	9/17/72		# Common U.S. writing.
 *	24 September 1972
 *	24 Sept 72	# September has a special abbreviation.
 *	24 Sep 72	# Three-letter abbreviations always allowed.
 *	Sep 24, 1972
 *	24-sep-72
 *	24sep72
 */
long strtoday (const char *str)
{
	time_t t;
	bool isnum = true;
	const char *s = str;

	/*
	 * get_date() interprets an empty string as the current date,
	 * which is not what we expect, unless you're a BOFH :-).
	 * (useradd sets sp_expire = current date for new lusers)
	 */
	if ((NULL == str) || ('\0' == *str)) {
		return -1;
	}

	/* If a numerical value is provided, this is already a number of
	 * days since EPOCH.
	 */
	if ('-' == *s) {
		s++;
	}
	while (' ' == *s) {
		s++;
	}
	while (isnum && ('\0' != *s)) {
		if (!isdigit (*s)) {
			isnum = false;
		}
		s++;
	}
	if (isnum) {
		long retdate;
		if (str2sl(&retdate, str) == -1)
			return -2;
		return retdate;
	}

	t = get_date(str, NULL);
	if ((time_t) - 1 == t) {
		return -2;
	}
	return t / DAY;
}
