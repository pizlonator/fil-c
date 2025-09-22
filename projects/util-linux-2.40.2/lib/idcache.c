/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com>
 */
#include <wchar.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

#include "c.h"
#include "idcache.h"

struct identry *get_id(struct idcache *ic, unsigned long int id)
{
	struct identry *ent;

	if (!ic)
		return NULL;

	for (ent = ic->ent; ent; ent = ent->next) {
		if (ent->id == id)
			return ent;
	}

	return NULL;
}

struct idcache *new_idcache(void)
{
	return calloc(1, sizeof(struct idcache));
}

void free_idcache(struct idcache *ic)
{
	struct identry *ent;

	if (!ic)
		return;

	ent = ic->ent;

	while (ent) {
		struct identry *next = ent->next;
		free(ent->name);
		free(ent);
		ent = next;
	}

	free(ic);
}

static void add_id(struct idcache *ic, char *name, unsigned long int id)
{
	struct identry *ent, *x;
	int w = 0;

	if (!ic)
		return;

	ent = calloc(1, sizeof(struct identry));
	if (!ent)
		return;
	ent->id = id;

	if (name) {
#ifdef HAVE_WIDECHAR
		wchar_t wc[LOGIN_NAME_MAX + 1];

		if (mbstowcs(wc, name, LOGIN_NAME_MAX) > 0) {
			wc[LOGIN_NAME_MAX] = '\0';
			w = wcswidth(wc, LOGIN_NAME_MAX);
		}
		else
#endif
			w = strlen(name);
	}

	/* note, we ignore names with non-printable widechars */
	if (w > 0) {
		ent->name = strdup(name);
		if (!ent->name) {
			free(ent);
			return;
		}
	} else {
		if (asprintf(&ent->name, "%lu", id) < 0) {
			free(ent);
			return;
		}
	}

	for (x = ic->ent; x && x->next; x = x->next);

	if (x)
		x->next = ent;
	else
		ic->ent = ent;

	if (w <= 0)
		w = ent->name ? strlen(ent->name) : 0;
	ic->width = ic->width < w ? w : ic->width;
}

void add_uid(struct idcache *cache, unsigned long int id)
{
	struct identry *ent = get_id(cache, id);

	if (!ent) {
		struct passwd *pw = getpwuid((uid_t) id);
		add_id(cache, pw ? pw->pw_name : NULL, id);
	}
}

void add_gid(struct idcache *cache, unsigned long int id)
{
	struct identry *ent = get_id(cache, id);

	if (!ent) {
		struct group *gr = getgrgid((gid_t) id);
		add_id(cache, gr ? gr->gr_name : NULL, id);
	}
}

