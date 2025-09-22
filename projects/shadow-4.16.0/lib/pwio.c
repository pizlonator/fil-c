/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001       , Michał Moskal
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include "prototypes.h"
#include "defines.h"
#include <pwd.h>
#include <stdio.h>
#include "commonio.h"
#include "pwio.h"

static /*@null@*/ /*@only@*/void *passwd_dup (const void *ent)
{
	const struct passwd *pw = ent;

	return __pw_dup (pw);
}

static void
passwd_free(/*@only@*/void *ent)
{
	struct passwd *pw = ent;

	pw_free (pw);
}

static const char *passwd_getname (const void *ent)
{
	const struct passwd *pw = ent;

	return pw->pw_name;
}

static void *passwd_parse (const char *line)
{
	return sgetpwent (line);
}

static int passwd_put (const void *ent, FILE * file)
{
	const struct passwd *pw = ent;

	if (   (NULL == pw)
	    || (valid_field (pw->pw_name, ":\n") == -1)
	    || (valid_field (pw->pw_passwd, ":\n") == -1)
	    || (pw->pw_uid == (uid_t)-1)
	    || (pw->pw_gid == (gid_t)-1)
	    || (valid_field (pw->pw_gecos, ":\n") == -1)
	    || (valid_field (pw->pw_dir, ":\n") == -1)
	    || (valid_field (pw->pw_shell, ":\n") == -1)
	    || (strlen (pw->pw_name) + strlen (pw->pw_passwd) +
	        strlen (pw->pw_gecos) + strlen (pw->pw_dir) +
	        strlen (pw->pw_shell) + 100 > PASSWD_ENTRY_MAX_LENGTH)) {
		return -1;
	}

	return (putpwent (pw, file) == -1) ? -1 : 0;
}

static struct commonio_ops passwd_ops = {
	passwd_dup,
	passwd_free,
	passwd_getname,
	passwd_parse,
	passwd_put,
	fgets,
	fputs,
	NULL,			/* open_hook */
	NULL			/* close_hook */
};

static struct commonio_db passwd_db = {
	PASSWD_FILE,		/* filename */
	&passwd_ops,		/* ops */
	NULL,			/* fp */
#ifdef WITH_SELINUX
	NULL,			/* scontext */
#endif
	0644,                   /* st_mode */
	0,                      /* st_uid */
	0,                      /* st_gid */
	NULL,			/* head */
	NULL,			/* tail */
	NULL,			/* cursor */
	false,			/* changed */
	false,			/* isopen */
	false,			/* locked */
	false,			/* readonly */
	false			/* setname */
};

int pw_setdbname (const char *filename)
{
	return commonio_setname (&passwd_db, filename);
}

/*@observer@*/const char *pw_dbname (void)
{
	return passwd_db.filename;
}

int pw_lock (void)
{
	return commonio_lock (&passwd_db);
}

int pw_open (int mode)
{
	return commonio_open (&passwd_db, mode);
}

/*@observer@*/ /*@null@*/const struct passwd *pw_locate (const char *name)
{
	return commonio_locate (&passwd_db, name);
}

/*@observer@*/ /*@null@*/const struct passwd *pw_locate_uid (uid_t uid)
{
	const struct passwd *pwd;

	pw_rewind ();
	while (   ((pwd = pw_next ()) != NULL)
	       && (pwd->pw_uid != uid)) {
	}

	return pwd;
}

int pw_update (const struct passwd *pw)
{
	return commonio_update (&passwd_db, pw);
}

int pw_remove (const char *name)
{
	return commonio_remove (&passwd_db, name);
}

int pw_rewind (void)
{
	return commonio_rewind (&passwd_db);
}

/*@observer@*/ /*@null@*/const struct passwd *pw_next (void)
{
	return commonio_next (&passwd_db);
}

int pw_close (void)
{
	return commonio_close (&passwd_db);
}

int pw_unlock (void)
{
	return commonio_unlock (&passwd_db);
}

/*@null@*/struct commonio_entry *__pw_get_head (void)
{
	return passwd_db.head;
}

void __pw_del_entry (const struct commonio_entry *ent)
{
	commonio_del_entry (&passwd_db, ent);
}

struct commonio_db *__pw_get_db (void)
{
	return &passwd_db;
}

static int pw_cmp (const void *p1, const void *p2)
{
	const struct commonio_entry *const *ce1;
	const struct commonio_entry *const *ce2;
	const struct passwd *pw1, *pw2;
	uid_t u1, u2;

	ce1 = p1;
	pw1 = (*ce1)->eptr;
	if (pw1 == NULL)
		return 1;

	ce2 = p2;
	pw2 = (*ce2)->eptr;
	if (pw2 == NULL)
		return -1;

	u1 = pw1->pw_uid;
	u2 = pw2->pw_uid;

	if (u1 < u2)
		return -1;
	else if (u1 > u2)
		return 1;
	else
		return 0;
}

/* Sort entries by UID */
int pw_sort ()
{
	return commonio_sort (&passwd_db, pw_cmp);
}
