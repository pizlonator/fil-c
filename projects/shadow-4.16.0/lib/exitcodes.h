/*
 * SPDX-FileCopyrightText: 2005 - 2006, Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* $Id$ */

/*
 * Exit codes used by shadow programs
 */
#define E_SUCCESS       	EXIT_SUCCESS	/* success */
/*
 * FIXME: other values should differ from EXIT_FAILURE (and EXIT_SUCCESS).
 *
 * FIXME: reserve EXIT_FAILURE for internal failures.
 */
#define E_NOPERM        	1	/* permission denied */
#define E_USAGE         	2	/* invalid command syntax */
#define E_BAD_ARG       	3	/* invalid argument to option */
#define E_PASSWD_NOTFOUND	14	/* not found password file */
#define E_SHADOW_NOTFOUND	15	/* not found shadow password file */
#define E_GROUP_NOTFOUND	16	/* not found group file */
#define E_GSHADOW_NOTFOUND	17	/* not found shadow group file */
#define E_CMD_NOEXEC		126	/* can't run command/shell */
#define E_CMD_NOTFOUND		127	/* can't find command/shell to run */
