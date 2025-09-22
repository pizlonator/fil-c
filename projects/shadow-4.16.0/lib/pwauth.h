/*
 * SPDX-FileCopyrightText: 1992 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2009       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 *	$Id$
 */

#ifndef _PWAUTH_H
#define _PWAUTH_H

#ifndef USE_PAM
int pw_auth (const char *cipher,
             const char *user,
             int flag,
             /*@null@*/const char *input);
#endif				/* !USE_PAM */

/*
 * Local access
 */

#define	PW_SU		1
#define	PW_LOGIN	2

/*
 * Administrative functions
 */

#define	PW_ADD		101
#define	PW_CHANGE	102
#define	PW_DELETE	103

/*
 * Network access
 */

#define	PW_TELNET	201
#define	PW_RLOGIN	202
#define	PW_FTP		203
#define	PW_REXEC	204

#endif /* _PWAUTH_H */
