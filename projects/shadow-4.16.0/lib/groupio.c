/*
 * SPDX-FileCopyrightText: 1990 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001       , Michał Moskal
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <assert.h>
#include <stdio.h>

#include "alloc.h"
#include "prototypes.h"
#include "defines.h"
#include "commonio.h"
#include "getdef.h"
#include "groupio.h"


static /*@null@*/struct commonio_entry *merge_group_entries (
	/*@null@*/ /*@returned@*/struct commonio_entry *gr1,
	/*@null@*/struct commonio_entry *gr2);
static int split_groups (unsigned int max_members);
static int group_open_hook (void);

static /*@null@*/ /*@only@*/void *group_dup (const void *ent)
{
	const struct group *gr = ent;

	return __gr_dup (gr);
}

static void
group_free(/*@only@*/void *ent)
{
	struct group *gr = ent;

	gr_free (gr);
}

static const char *group_getname (const void *ent)
{
	const struct group *gr = ent;

	return gr->gr_name;
}

static void *group_parse (const char *line)
{
	return sgetgrent (line);
}

static int group_put (const void *ent, FILE * file)
{
	const struct group *gr = ent;

	if (   (NULL == gr)
	    || (valid_field (gr->gr_name, ":\n") == -1)
	    || (valid_field (gr->gr_passwd, ":\n") == -1)
	    || (gr->gr_gid == (gid_t)-1)) {
		return -1;
	}

	/* FIXME: fail also if gr->gr_mem == NULL ?*/
	if (NULL != gr->gr_mem) {
		size_t i;
		for (i = 0; NULL != gr->gr_mem[i]; i++) {
			if (valid_field (gr->gr_mem[i], ",:\n") == -1) {
				return -1;
			}
		}
	}

	return (putgrent (gr, file) == -1) ? -1 : 0;
}

static int group_close_hook (void)
{
	unsigned int max_members = getdef_unum("MAX_MEMBERS_PER_GROUP", 0);

	if (0 == max_members) {
		return 1;
	}

	return split_groups (max_members);
}

static struct commonio_ops group_ops = {
	group_dup,
	group_free,
	group_getname,
	group_parse,
	group_put,
	fgetsx,
	fputsx,
	group_open_hook,
	group_close_hook
};

static /*@owned@*/struct commonio_db group_db = {
	GROUP_FILE,		/* filename */
	&group_ops,		/* ops */
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

int gr_setdbname (const char *filename)
{
	return commonio_setname (&group_db, filename);
}

/*@observer@*/const char *gr_dbname (void)
{
	return group_db.filename;
}

int gr_lock (void)
{
	return commonio_lock (&group_db);
}

int gr_open (int mode)
{
	return commonio_open (&group_db, mode);
}

/*@observer@*/ /*@null@*/const struct group *gr_locate (const char *name)
{
	return commonio_locate (&group_db, name);
}

/*@observer@*/ /*@null@*/const struct group *gr_locate_gid (gid_t gid)
{
	const struct group *grp;

	gr_rewind ();
	while (   ((grp = gr_next ()) != NULL)
	       && (grp->gr_gid != gid)) {
	}

	return grp;
}

int gr_update (const struct group *gr)
{
	return commonio_update (&group_db, gr);
}

int gr_remove (const char *name)
{
	return commonio_remove (&group_db, name);
}

int gr_rewind (void)
{
	return commonio_rewind (&group_db);
}

/*@observer@*/ /*@null@*/const struct group *gr_next (void)
{
	return commonio_next (&group_db);
}

int gr_close (void)
{
	return commonio_close (&group_db);
}

int gr_unlock (void)
{
	return commonio_unlock (&group_db);
}

void __gr_set_changed (void)
{
	group_db.changed = true;
}

/*@dependent@*/ /*@null@*/struct commonio_entry *__gr_get_head (void)
{
	return group_db.head;
}

/*@observer@*/const struct commonio_db *__gr_get_db (void)
{
	return &group_db;
}

void __gr_del_entry (const struct commonio_entry *ent)
{
	commonio_del_entry (&group_db, ent);
}

static int gr_cmp (const void *p1, const void *p2)
{
	const struct commonio_entry *const *ce1;
	const struct commonio_entry *const *ce2;
	const struct group *g1, *g2;
	gid_t u1, u2;

	ce1 = p1;
	g1 = (*ce1)->eptr;
	if (g1 == NULL) {
		return 1;
	}

	ce2 = p2;
	g2 = (*ce2)->eptr;
	if (g2 == NULL) {
		return -1;
	}

	u1 = g1->gr_gid;
	u2 = g2->gr_gid;

	if (u1 < u2) {
		return -1;
	} else if (u1 > u2) {
		return 1;
	} else {
		return 0;
	}
}

/* Sort entries by GID */
int gr_sort ()
{
	return commonio_sort (&group_db, gr_cmp);
}

static int group_open_hook (void)
{
	unsigned int max_members = getdef_unum("MAX_MEMBERS_PER_GROUP", 0);
	struct commonio_entry *gr1, *gr2;

	if (0 == max_members) {
		return 1;
	}

	for (gr1 = group_db.head; NULL != gr1; gr1 = gr1->next) {
		for (gr2 = gr1->next; NULL != gr2; gr2 = gr2->next) {
			struct group *g1 = gr1->eptr;
			struct group *g2 = gr2->eptr;
			if (NULL != g1 &&
			    NULL != g2 &&
			    0 == strcmp (g1->gr_name, g2->gr_name) &&
			    0 == strcmp (g1->gr_passwd, g2->gr_passwd) &&
			    g1->gr_gid == g2->gr_gid) {
				/* Both group entries refer to the same
				 * group. It is a split group. Merge the
				 * members. */
				gr1 = merge_group_entries (gr1, gr2);
				if (NULL == gr1)
					return 0;
				/* Unlink gr2 */
				if (NULL != gr2->next) {
					gr2->next->prev = gr2->prev;
				}
				/* gr2 does not start with head */
				assert (NULL != gr2->prev);
				gr2->prev->next = gr2->next;
			}
		}
		assert (NULL != gr1);
	}

	return 1;
}

/*
 * Merge the list of members of the two group entries.
 *
 * The commonio_entry arguments shall be group entries.
 *
 * You should not merge the members of two groups if they don't have the
 * same name, password and gid.
 *
 * It merge the members of the second entry in the first one, and return
 * the modified first entry on success, or NULL on failure (with errno
 * set).
 */
static /*@null@*/struct commonio_entry *merge_group_entries (
	/*@null@*/ /*@returned@*/struct commonio_entry *gr1,
	/*@null@*/struct commonio_entry *gr2)
{
	char          *new_line;
	char          **new_members;
	size_t        i;
	size_t        members = 0;
	struct group  *gptr1;
	struct group  *gptr2;

	if (NULL == gr2 || NULL == gr1) {
		errno = EINVAL;
		return NULL;
	}

	gptr1 = gr1->eptr;
	gptr2 = gr2->eptr;
	if (NULL == gptr2 || NULL == gptr1) {
		errno = EINVAL;
		return NULL;
	}

	/* Concatenate the 2 lines */
	if (asprintf(&new_line, "%s\n%s", gr1->line, gr2->line) == -1)
		return NULL;

	/* Concatenate the 2 list of members */
	for (i=0; NULL != gptr1->gr_mem[i]; i++);
	members += i;
	for (i=0; NULL != gptr2->gr_mem[i]; i++) {
		char **pmember = gptr1->gr_mem;
		while (NULL != *pmember) {
			if (0 == strcmp(*pmember, gptr2->gr_mem[i])) {
				break;
			}
			pmember++;
		}
		if (NULL == *pmember) {
			members++;
		}
	}
	new_members = CALLOC (members + 1, char *);
	if (NULL == new_members) {
		free(new_line);
		return NULL;
	}
	for (i=0; NULL != gptr1->gr_mem[i]; i++) {
		new_members[i] = gptr1->gr_mem[i];
	}
	/* NULL termination enforced by above calloc */

	members = i;
	for (i=0; NULL != gptr2->gr_mem[i]; i++) {
		char **pmember = new_members;
		while (NULL != *pmember) {
			if (0 == strcmp(*pmember, gptr2->gr_mem[i])) {
				break;
			}
			pmember++;
		}
		if (NULL == *pmember) {
			new_members[members] = gptr2->gr_mem[i];
			members++;
			new_members[members] = NULL;
		}
	}

	gr1->line = new_line;
	gptr1->gr_mem = new_members;

	return gr1;
}

/*
 * Scan the group database and split the groups which have more members
 * than specified, if this is the result from a current change.
 *
 * Return 0 on failure (errno set) and 1 on success.
 */
static int split_groups (unsigned int max_members)
{
	struct commonio_entry *gr;

	for (gr = group_db.head; NULL != gr; gr = gr->next) {
		struct group *gptr = gr->eptr;
		struct commonio_entry *new;
		struct group *new_gptr;
		unsigned int members = 0;
		unsigned int i;

		/* Check if this group must be split */
		if (!gr->changed) {
			continue;
		}
		if (NULL == gptr) {
			continue;
		}
		for (members = 0; NULL != gptr->gr_mem[members]; members++);
		if (members <= max_members) {
			continue;
		}

		new = MALLOC(1, struct commonio_entry);
		if (NULL == new) {
			return 0;
		}
		new->eptr = group_dup(gr->eptr);
		if (NULL == new->eptr) {
			free (new);
			errno = ENOMEM;
			return 0;
		}
		new_gptr = new->eptr;
		new->line = NULL;
		new->changed = true;

		/* Enforce the maximum number of members on gptr */
		for (i = max_members; NULL != gptr->gr_mem[i]; i++) {
			free (gptr->gr_mem[i]);
			gptr->gr_mem[i] = NULL;
		}
		/* Shift all the members */
		/* The number of members in new_gptr will be check later */
		for (i = 0; NULL != new_gptr->gr_mem[i + max_members]; i++) {
			free (new_gptr->gr_mem[i]);
			new_gptr->gr_mem[i] = new_gptr->gr_mem[i + max_members];
			new_gptr->gr_mem[i + max_members] = NULL;
		}
		for (; NULL != new_gptr->gr_mem[i]; i++) {
			free (new_gptr->gr_mem[i]);
			new_gptr->gr_mem[i] = NULL;
		}

		/* insert the new entry in the list */
		new->prev = gr;
		new->next = gr->next;
		gr->next = new;
	}

	return 1;
}

