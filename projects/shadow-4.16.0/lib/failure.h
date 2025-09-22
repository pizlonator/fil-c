/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1997 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* $Id$ */
#ifndef _FAILURE_H_
#define _FAILURE_H_

#include "defines.h"
#include "faillog.h"

/*
 * failure - make failure entry
 *
 *	failure() creates a new (struct faillog) entry or updates an
 *	existing one with the current failed login information.
 */
extern void failure (uid_t, const char *, struct faillog *);

/*
 * failcheck - check for failures > allowable
 *
 *	failcheck() is called AFTER the password has been validated.  If the
 *	account has been "attacked" with too many login failures, failcheck()
 *	returns FALSE to indicate that the login should be denied even though
 *	the password is valid.
 */
extern int failcheck (uid_t uid, struct faillog *fl, bool failed);

/*
 * failprint - print line of failure information
 *
 *	failprint takes a (struct faillog) entry and formats it into a
 *	message which is displayed at login time.
 */
extern void failprint (const struct faillog *);

#endif

