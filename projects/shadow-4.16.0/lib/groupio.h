/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001       , Michał Moskal
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* $Id$ */
#ifndef _GROUPIO_H
#define _GROUPIO_H

#include <sys/types.h>
#include <grp.h>

extern int gr_close (void);
extern /*@observer@*/ /*@null@*/const struct group *gr_locate (const char *name);
extern /*@observer@*/ /*@null@*/const struct group *gr_locate_gid (gid_t gid);
extern int gr_lock (void);
extern int gr_setdbname (const char *filename);
extern /*@observer@*/const char *gr_dbname (void);
extern /*@observer@*/ /*@null@*/const struct group *gr_next (void);
extern int gr_open (int mode);
extern int gr_remove (const char *name);
extern int gr_rewind (void);
extern int gr_unlock (void);
extern int gr_update (const struct group *gr);
extern int gr_sort (void);

#endif
