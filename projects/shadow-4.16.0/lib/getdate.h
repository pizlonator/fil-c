/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1997 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _GETDATE_H_
#define _GETDATE_H_

#include <config.h>
#include "defines.h"

time_t get_date (const char *p, /*@null@*/const time_t *now);
#endif
