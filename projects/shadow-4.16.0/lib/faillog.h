/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * faillog.h - login failure logging file format
 *
 *	$Id$
 *
 * The login failure file is maintained by login(1) and faillog(8)
 * Each record in the file represents a separate UID and the file
 * is indexed in that fashion.
 */

#ifndef _FAILLOG_H
#define _FAILLOG_H

struct faillog {
	short fail_cnt;		/* failures since last success */
	short fail_max;		/* failures before turning account off */
	char fail_line[12];	/* last failure occurred here */
	time_t fail_time;	/* last failure occurred then */
	/*
	 * If nonzero, the account will be re-enabled if there are no
	 * failures for fail_locktime seconds since last failure.
	 */
	long fail_locktime;
};

#endif
