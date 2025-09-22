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
#ifndef _SGROUPIO_H
#define _SGROUPIO_H

extern int sgr_close (void);
extern bool sgr_file_present (void);
extern /*@observer@*/ /*@null@*/const struct sgrp *sgr_locate (const char *name);
extern int sgr_lock (void);
extern int sgr_setdbname (const char *filename);
extern /*@observer@*/const char *sgr_dbname (void);
extern /*@null@*/const struct sgrp *sgr_next (void);
extern int sgr_open (int mode);
extern int sgr_remove (const char *name);
extern int sgr_rewind (void);
extern int sgr_unlock (void);
extern int sgr_update (const struct sgrp *sg);
extern int sgr_sort (void);

#endif
