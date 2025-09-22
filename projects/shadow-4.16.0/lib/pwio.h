/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Michał Moskal
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* $Id$ */
#ifndef _PWIO_H
#define _PWIO_H

#include <sys/types.h>
#include <pwd.h>

extern int pw_close (void);
extern /*@observer@*/ /*@null@*/const struct passwd *pw_locate (const char *name);
extern /*@observer@*/ /*@null@*/const struct passwd *pw_locate_uid (uid_t uid);
extern int pw_lock (void);
extern int pw_setdbname (const char *filename);
extern /*@observer@*/const char *pw_dbname (void);
extern /*@observer@*/ /*@null@*/const struct passwd *pw_next (void);
extern int pw_open (int mode);
extern int pw_remove (const char *name);
extern int pw_rewind (void);
extern int pw_unlock (void);
extern int pw_update (const struct passwd *pw);
extern int pw_sort (void);

#endif
