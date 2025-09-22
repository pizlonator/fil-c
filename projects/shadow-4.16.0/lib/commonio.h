/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* $Id$ */
#ifndef COMMONIO_H
#define COMMONIO_H


#include "attr.h"
#include "defines.h" /* bool */


/*
 * Linked list entry.
 */
struct commonio_entry {
	/*@null@*/char *line;
	/*@null@*/void *eptr;		/* struct passwd, struct spwd, ... */
	/*@dependent@*/ /*@null@*/struct commonio_entry *prev;
	/*@owned@*/ /*@null@*/struct commonio_entry *next;
	bool changed:1;
};

/*
 * Operations depending on database type: passwd, group, shadow etc.
 */
struct commonio_ops {
	/*
	 * Make a copy of the object (for example, struct passwd)
	 * and all strings pointed by it, in malloced memory.
	 */
	/*@null@*/ /*@only@*/void *(*dup) (const void *);

	/*
	 * free() the object including any strings pointed by it.
	 */
	void (*free)(/*@only@*/void *);

	/*
	 * Return the name of the object (for example, pw_name
	 * for struct passwd).
	 */
	const char *(*getname) (const void *);

	/*
	 * Parse a string, return object (in static area -
	 * should be copied using the dup operation above).
	 */
	void *(*parse) (const char *);

	/*
	 * Write the object to the file (this calls putpwent()
	 * for struct passwd, for example).
	 */
	int (*put) (const void *, FILE *);

	/*
	 * fgets and fputs (can be replaced by versions that
	 * understand line continuation conventions).
	 */
	ATTR_ACCESS(write_only, 1, 2)
	/*@null@*/char *(*fgets)(/*@returned@*/char *restrict s, int n,
	                         FILE *restrict stream);
	int (*fputs) (const char *, FILE *);

	/*
	 * open_hook and close_hook.
	 * If non NULL, these functions will be called after the database
	 * is open or before it is closed.
	 * They return 0 on failure and 1 on success.
	 */
	/*@null@*/int (*open_hook) (void);
	/*@null@*/int (*close_hook) (void);
};

/*
 * Database structure.
 */
struct commonio_db {
	/*
	 * Name of the data file.
	 */
	char filename[1024];

	/*
	 * Operations from above.
	 */
	/*@observer@*/const struct commonio_ops *ops;

	/*
	 * Currently open file stream.
	 */
	/*@dependent@*/ /*@null@*/FILE *fp;

#ifdef WITH_SELINUX
	/*@null@*/char *scontext;
#endif
	/*
	 * Default permissions and owner for newly created data file.
         */
	mode_t st_mode;
	uid_t st_uid;
	gid_t st_gid;
	/*
	 * Head, tail, current position in linked list.
	 */
	/*@owned@*/ /*@null@*/struct commonio_entry *head;
	/*@dependent@*/ /*@null@*/struct commonio_entry *tail;
	/*@dependent@*/ /*@null@*/struct commonio_entry *cursor;

	/*
	 * Various flags.
	 */
	bool changed:1;
	bool isopen:1;
	bool locked:1;
	bool readonly:1;
	bool setname:1;
};

extern int commonio_setname (struct commonio_db *, const char *);
extern bool commonio_present (const struct commonio_db *db);
extern int commonio_lock (struct commonio_db *);
extern int commonio_lock_nowait (struct commonio_db *, bool log);
extern int do_fcntl_lock (const char *file, bool log, short type);
extern int commonio_open (struct commonio_db *, int);
extern /*@observer@*/ /*@null@*/const void *commonio_locate (struct commonio_db *, const char *);
extern int commonio_update (struct commonio_db *, const void *);
#ifdef ENABLE_SUBIDS
extern int commonio_append (struct commonio_db *, const void *);
#endif				/* ENABLE_SUBIDS */
extern int commonio_remove (struct commonio_db *, const char *);
extern int commonio_rewind (struct commonio_db *);
extern /*@observer@*/ /*@null@*/const void *commonio_next (struct commonio_db *);
extern int commonio_close (struct commonio_db *);
extern int commonio_unlock (struct commonio_db *);
extern void commonio_del_entry (struct commonio_db *,
                                const struct commonio_entry *);
extern int commonio_sort_wrt (struct commonio_db *shadow,
                              const struct commonio_db *passwd);
extern int commonio_sort (struct commonio_db *db,
                          int (*cmp) (const void *, const void *));

#endif
