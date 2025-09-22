/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1997 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* $Id$ */
#ifndef _CHKNAME_H_
#define _CHKNAME_H_


/*
 * is_valid_user_name(), is_valid_group_name() - check the new user/group
 * name for validity;
 * return values:
 *   true  - OK
 *   false - bad name
 */


#include <config.h>

#include <stdbool.h>
#include <stddef.h>


extern size_t login_name_max_size(void);
extern bool is_valid_user_name (const char *name);
extern bool is_valid_group_name (const char *name);

#endif
