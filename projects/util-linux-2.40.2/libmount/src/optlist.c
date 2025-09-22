/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2022 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 *
 * The "optlist" is container for parsed mount options.
 *
 */
#include "strutils.h"
#include "list.h"
#include "mountP.h"
#include "mount-api-utils.h"

#define MNT_OL_MAXMAPS	8

enum libmnt_optsrc {
	MNT_OPTSRC_STRING,
	MNT_OPTSRC_FLAG
};

struct optlist_cache {
	unsigned long flags;
	char *optstr;

	unsigned int flags_ready: 1,
		     optstr_ready : 1;
};

struct libmnt_opt {
	char *name;
	char *value;

	struct list_head opts;	/* libmnt_optlist->opts member */

	const struct libmnt_optmap	*map;
	const struct libmnt_optmap	*ent;	/* map entry */

	enum libmnt_optsrc	src;

	unsigned int external : 1,	/* visible for external helpers only */
		     recursive : 1,	/* recursive flag */
		     sepnodata : 1,	/* value separator, but without data ("name=") */
		     is_linux : 1,	/* defined in ls->linux_map (VFS attr) */
		     quoted : 1;	/* name="value" */
};

struct libmnt_optlist {
	int refcount;
	unsigned int age;			/* incremented after each change */

	const struct libmnt_optmap	*linux_map;	/* map with MS_ flags */
	const struct libmnt_optmap	*maps[MNT_OL_MAXMAPS];
	size_t nmaps;

	struct optlist_cache cache_mapped[MNT_OL_MAXMAPS];	/* cache by map */
	struct optlist_cache cache_all[__MNT_OL_FLTR_COUNT];	/* from all maps, unknown, external, ... */

	unsigned long		propagation;	/* propagation MS_ flags */
	struct list_head	opts;		/* parsed options */

	unsigned int	merged : 1,		/* don't care about MNT_OPTSRC_* */
			is_remount : 1,
			is_bind : 1,
			is_rbind : 1,
			is_rdonly : 1,
			is_move : 1,
			is_silent : 1,
			is_recursive : 1;
};

struct libmnt_optlist *mnt_new_optlist(void)
{
	struct libmnt_optlist *ls = calloc(1, sizeof(*ls));

	if (!ls)
		return NULL;

	ls->refcount = 1;
	INIT_LIST_HEAD(&ls->opts);

	ls->linux_map = mnt_get_builtin_optmap(MNT_LINUX_MAP);

	DBG(OPTLIST, ul_debugobj(ls, "alloc"));
	return ls;
}

void mnt_ref_optlist(struct libmnt_optlist *ls)
{
	if (ls)
		ls->refcount++;
}

static void reset_cache(struct optlist_cache *cache)
{
	if (!cache || (cache->flags_ready == 0 && cache->optstr_ready == 0))
		return;
	free(cache->optstr);
	memset(cache, 0, sizeof(*cache));
}

void mnt_unref_optlist(struct libmnt_optlist *ls)
{
	size_t i;

	if (!ls)
		return;

	ls->refcount--;
	if (ls->refcount > 0)
		return;

	while (!list_empty(&ls->opts)) {
		struct libmnt_opt *opt = list_entry(ls->opts.next, struct libmnt_opt, opts);
		mnt_optlist_remove_opt(ls, opt);
	}

	for (i = 0; i < ls->nmaps; i++)
		reset_cache(&ls->cache_mapped[i]);

	for (i = 0; i < __MNT_OL_FLTR_COUNT; i++)
		reset_cache(&ls->cache_all[i]);

	free(ls);
}

int mnt_optlist_register_map(struct libmnt_optlist *ls, const struct libmnt_optmap *map)
{
	size_t i;

	if (!ls || !map)
		return -EINVAL;

	for (i = 0; i < ls->nmaps; i++) {
		if (ls->maps[i] == map)
			return 0;		/* already registred, ignore */
	}
	if (ls->nmaps + 1 >= MNT_OL_MAXMAPS)
		return -ERANGE;

	DBG(OPTLIST, ul_debugobj(ls, "registr map %p", map));
	ls->maps[ls->nmaps++] = map;
	return 0;
}

static size_t optlist_get_mapidx(struct libmnt_optlist *ls, const struct libmnt_optmap *map)
{
	size_t i;

	assert(ls);
	assert(map);

	for (i = 0; i < ls->nmaps; i++)
		if (map == ls->maps[i])
			return i;

	return (size_t) -1;
}

static void optlist_cleanup_cache(struct libmnt_optlist *ls)
{
	size_t i;

	ls->age++;

	if (list_empty(&ls->opts))
		return;

	for (i = 0; i < ARRAY_SIZE(ls->cache_mapped); i++)
		reset_cache(&ls->cache_mapped[i]);

	for (i = 0; i < __MNT_OL_FLTR_COUNT; i++)
		reset_cache(&ls->cache_all[i]);
}

int mnt_optlist_remove_opt(struct libmnt_optlist *ls, struct libmnt_opt *opt)
{
	if (!opt)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, " remove %s", opt->name));

	if (opt->map && opt->ent && opt->map == ls->linux_map) {
		if (opt->ent->id & MS_PROPAGATION)
			ls->propagation &= ~opt->ent->id;
		else if (opt->ent->id == MS_REMOUNT)
			ls->is_remount = 0;
		else if (opt->ent->id == (MS_BIND|MS_REC))
			ls->is_rbind = 0;
		else if (opt->ent->id == MS_BIND)
			ls->is_bind = 0;
		else if (opt->ent->id == MS_RDONLY)
			ls->is_rdonly = 0;
		else if (opt->ent->id == MS_MOVE)
			ls->is_move = 0;
		else if (opt->ent->id == MS_SILENT)
			ls->is_silent = 0;

		if (opt->ent->id & MS_REC)
			ls->is_recursive = 0;
	}

	optlist_cleanup_cache(ls);

	list_del_init(&opt->opts);
	free(opt->value);
	free(opt->name);
	free(opt);

	return 0;
}

int mnt_optlist_remove_named(struct libmnt_optlist *ls, const char *name,
			     const struct libmnt_optmap *map)
{
	struct libmnt_opt *opt = mnt_optlist_get_named(ls, name, map);

	return opt ? mnt_optlist_remove_opt(ls, opt) : 0;
}

int mnt_optlist_next_opt(struct libmnt_optlist *ls,
			struct libmnt_iter *itr, struct libmnt_opt **opt)
{
	int rc = 1;

	if (!ls || !itr)
		return -EINVAL;
	if (opt)
		*opt = NULL;

	if (!itr->head)
		MNT_ITER_INIT(itr, &ls->opts);
	if (itr->p != itr->head) {
		if (opt)
			*opt = MNT_ITER_GET_ENTRY(itr, struct libmnt_opt, opts);
		MNT_ITER_ITERATE(itr);
		rc = 0;
	}

	return rc;
}

struct libmnt_opt *mnt_optlist_get_opt(struct libmnt_optlist *ls,
			unsigned long id, const struct libmnt_optmap *map)
{
	struct libmnt_iter itr;
	struct libmnt_opt *opt;

	if (!ls || !map)
		return NULL;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_optlist_next_opt(ls, &itr, &opt) == 0) {
		if (opt->external)
			continue;
		if (map && opt->map != map)
			continue;
		if (opt->ent && (unsigned long) opt->ent->id == id)
			return opt;
	}

	return NULL;
}

struct libmnt_opt *mnt_optlist_get_named(struct libmnt_optlist *ls,
			const char *name, const struct libmnt_optmap *map)
{
	struct libmnt_iter itr;
	struct libmnt_opt *opt;

	if (!ls || !name)
		return NULL;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_optlist_next_opt(ls, &itr, &opt) == 0) {
		if (opt->external)
			continue;
		if (map && map != opt->map)
			continue;
		if (opt->name && strcmp(opt->name, name) == 0)
			return opt;
	}

	return NULL;
}

static int is_equal_opts(struct libmnt_opt *a, struct libmnt_opt *b)
{
	if (a->map != b->map)
		return 0;
	if (a->ent && b->ent && a->ent != b->ent)
		return 0;
	if ((a->value && !b->value) || (!a->value && b->value))
		return 0;
	if (strcmp(a->name, b->name) != 0)
		return 0;
	if (a->value && b->value && strcmp(a->value, b->value) != 0)
		return 0;

	return 1;
}

int mnt_optlist_merge_opts(struct libmnt_optlist *ls)
{
	struct libmnt_iter itr;
	struct libmnt_opt *opt;

	if (!ls)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, "merging"));
	ls->merged = 1;

	/* deduplicate, keep last instance of the option only */
	mnt_reset_iter(&itr, MNT_ITER_BACKWARD);

	while (mnt_optlist_next_opt(ls, &itr, &opt) == 0) {
		struct libmnt_iter xtr;
		struct libmnt_opt *x;

		mnt_reset_iter(&xtr, MNT_ITER_FORWARD);
		while (mnt_optlist_next_opt(ls, &xtr, &x) == 0) {
			int rem = 0;

			if (opt == x)
				break;	/* no another instance */

			/* remove duplicate option */
			if (is_equal_opts(opt, x))
				rem = 1;

			/* remove inverted option */
			else if (opt->ent && x->ent
			    && opt->map == x->map
			    && opt->ent->id == x->ent->id
			    && (opt->ent->mask & MNT_INVERT
				    || x->ent->mask & MNT_INVERT))
				rem = 1;

			if (rem) {
				/* me sure @itr does not point to removed item */
				if (itr.p == &x->opts)
					itr.p = x->opts.prev;
				mnt_optlist_remove_opt(ls, x);
			}

		}
	}

	return 0;
}

#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT
static inline int flag_to_attr(unsigned long flag, uint64_t *attr)
{
	uint64_t a = 0;

	switch (flag) {
	case MS_RDONLY:
		a = MOUNT_ATTR_RDONLY;
		break;
	case MS_NOSUID:
		a = MOUNT_ATTR_NOSUID;
		break;
	case MS_NODEV:
		a = MOUNT_ATTR_NODEV;
		break;
	case MS_NOEXEC:
		a = MOUNT_ATTR_NOEXEC;
		break;
	case MS_NODIRATIME:
		a = MOUNT_ATTR_NODIRATIME;
		break;
	case MS_RELATIME:
		a = MOUNT_ATTR_RELATIME;
		break;
	case MS_NOATIME:
		a =  MOUNT_ATTR_NOATIME;
		break;
	case MS_STRICTATIME:
		a = MOUNT_ATTR_STRICTATIME;
		break;
	case MS_NOSYMFOLLOW:
		a = MOUNT_ATTR_NOSYMFOLLOW;
		break;
	default:
		return -1;
	}

	if (attr)
		*attr = a;
	return 0;
}

/*
 * Is the @opt relevant for mount_setattr() ?
 */
static inline int is_vfs_opt(struct libmnt_opt *opt)
{
	if (!opt->map || !opt->ent || !opt->ent->id || !opt->is_linux)
		return 0;

	return flag_to_attr(opt->ent->id, NULL) < 0 ? 0 : 1;
}
#endif

static struct libmnt_opt *optlist_new_opt(struct libmnt_optlist *ls,
			const char *name, size_t namesz,
			const char *value, size_t valsz,
			const struct libmnt_optmap *map,
			const struct libmnt_optmap *ent,
			struct list_head *where)

{
	struct libmnt_opt *opt;

	opt = calloc(1, sizeof(*opt));
	if (!opt)
		return NULL;

	INIT_LIST_HEAD(&opt->opts);
	opt->map = map;
	opt->ent = ent;

	if (valsz) {
		if (*value == '"' && *(value + valsz - 1) == '"') {
			opt->quoted = 1;
			value++;
			valsz -= 2;
		}
		opt->value = strndup(value, valsz);
		if (!opt->value)
			goto fail;

	} else if (value) {
		/* separator specified, but empty value ("name=") */
		opt->sepnodata = 1;
	}
	if (namesz) {
		opt->name = strndup(name, namesz);
		if (!opt->name)
			goto fail;
	}

	if (where)
		list_add(&opt->opts, where);
	else
		list_add_tail(&opt->opts, &ls->opts);

	/* shortcuts */
	if (map && ent && map == ls->linux_map) {
		opt->is_linux = 1;

		if (ent->id & MS_PROPAGATION)
			ls->propagation |= ent->id;
		else if (opt->ent->id == MS_REMOUNT)
			ls->is_remount = 1;
		else if (opt->ent->id == (MS_REC|MS_BIND))
			ls->is_rbind = 1;
		else if (opt->ent->id == MS_BIND)
			ls->is_bind = 1;
		else if (opt->ent->id == MS_RDONLY)
			ls->is_rdonly = opt->ent->mask & MNT_INVERT ? 0 : 1;
		else if (opt->ent->id == MS_MOVE)
			ls->is_move = 1;
		else if (opt->ent->id == MS_SILENT)
			ls->is_silent = 1;

		if (opt->ent->id & MS_REC) {
			ls->is_recursive = 1;
			opt->recursive = 1;
		}
	}
#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT
	if (!opt->recursive && opt->value
	    && is_vfs_opt(opt) && strcmp(opt->value, "recursive") == 0)
		opt->recursive = 1;
#endif
	if (ent && map) {
		DBG(OPTLIST, ul_debugobj(ls, " added %s [id=0x%08x map=%p]",
				opt->name, ent->id, map));
	} else {
		DBG(OPTLIST, ul_debugobj(ls, " added %s", opt->name));
	}
	return opt;
fail:
	mnt_optlist_remove_opt(ls, opt);
	return NULL;
}

static int optlist_add_optstr(struct libmnt_optlist *ls, const char *optstr,
			const struct libmnt_optmap *map, struct list_head *where)
{
	char *p = (char *) optstr, *name, *val;
	size_t namesz, valsz;
	int rc;

	if (!ls)
		return -EINVAL;
	if (map && (rc =  mnt_optlist_register_map(ls, map)))
		return rc;
	if (!optstr)
		return 0;

	while (ul_optstr_next(&p, &name, &namesz, &val, &valsz) == 0) {

		struct libmnt_opt *opt;
		const struct libmnt_optmap *e = NULL, *m = NULL;

		if (map)
			m = mnt_optmap_get_entry(&map, 1, name, namesz, &e);
		if (!m && ls->nmaps)
			m = mnt_optmap_get_entry(ls->maps, ls->nmaps, name, namesz, &e);

		/* TODO: add the option more than once if belongs to the more maps */

		opt = optlist_new_opt(ls, name, namesz, val, valsz, m, e, where);
		if (!opt)
			return -ENOMEM;
		opt->src = MNT_OPTSRC_STRING;
		if (where)
			where = &opt->opts;
	}

	optlist_cleanup_cache(ls);

	return 0;
}

/*
 * The library differentiate between options specified by flags and strings by
 * default.  In this case mnt_optlist_set_optstr() replaces all options
 * specified by strings for the @map or for all maps if @map is NULL
  *
  * If optlist is marked as merged by mnt_optlist_set_merged() than this
  * function replaced all options for the @map or for all maps if @map is NULL.
 */
int mnt_optlist_set_optstr(struct libmnt_optlist *ls, const char *optstr,
			  const struct libmnt_optmap *map)
{
	struct list_head *p, *next;

	if (!ls)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, "set %s", optstr));

	/* remove all already set options */
	list_for_each_safe(p, next, &ls->opts) {
		struct libmnt_opt *opt = list_entry(p, struct libmnt_opt, opts);

		if (opt->external)
			continue;
		if (map && opt->map != map)
			continue;
		if (ls->merged || opt->src == MNT_OPTSRC_STRING)
			mnt_optlist_remove_opt(ls, opt);
	}

	return optlist_add_optstr(ls, optstr, map, NULL);
}

int mnt_optlist_append_optstr(struct libmnt_optlist *ls, const char *optstr,
			const struct libmnt_optmap *map)
{
	if (!ls)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, "append %s", optstr));
	return optlist_add_optstr(ls, optstr, map, NULL);
}

int mnt_optlist_prepend_optstr(struct libmnt_optlist *ls, const char *optstr,
			const struct libmnt_optmap *map)
{
	if (!ls)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, "prepend %s", optstr));
	return optlist_add_optstr(ls, optstr, map, &ls->opts);
}

static int optlist_add_flags(struct libmnt_optlist *ls, unsigned long flags,
			const struct libmnt_optmap *map, struct list_head *where)
{
	const struct libmnt_optmap *ent;
	int rc;

	if (!ls || !map)
		return -EINVAL;

	if (map && (rc = mnt_optlist_register_map(ls, map)))
		return rc;

	for (ent = map; ent && ent->name; ent++) {

		char *p;
		size_t sz;
		struct libmnt_opt *opt;

		if ((ent->mask & MNT_INVERT)
		    || ent->name == NULL
		    || ent->id == 0
		    || (flags & ent->id) != (unsigned long) ent->id)
			continue;

		/* don't add options which require values (e.g. offset=%d) */
		p = strchr(ent->name, '=');
		if (p) {
			if (p > ent->name && *(p - 1) == '[')
				p--;		 /* name[=] */
			else
				continue;	/* name=<value> */
			sz = p - ent->name;
			p -= sz;
		} else {
			p = (char *) ent->name;
			sz = strlen(ent->name); /* alone "name" */
		}

		opt = optlist_new_opt(ls, p, sz, NULL, 0, map, ent, where);
		if (!opt)
			return -ENOMEM;
		opt->src = MNT_OPTSRC_FLAG;
		if (where)
			where = &opt->opts;
	}

	optlist_cleanup_cache(ls);

	return 0;
}

int mnt_optlist_append_flags(struct libmnt_optlist *ls, unsigned long flags,
			  const struct libmnt_optmap *map)
{
	if (!ls || !map)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, "append 0x%08lx", flags));
	return optlist_add_flags(ls, flags, map, NULL);
}


int mnt_optlist_set_flags(struct libmnt_optlist *ls, unsigned long flags,
			  const struct libmnt_optmap *map)
{
	struct list_head *p, *next;

	if (!ls || !map)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, "set 0x%08lx", flags));

	/* remove all already set options */
	list_for_each_safe(p, next, &ls->opts) {
		struct libmnt_opt *opt = list_entry(p, struct libmnt_opt, opts);

		if (opt->external)
			continue;
		if (map && opt->map != map)
			continue;
		if (ls->merged || opt->src == MNT_OPTSRC_FLAG)
			mnt_optlist_remove_opt(ls, opt);
	}

	return mnt_optlist_append_flags(ls, flags, map);
}

int mnt_optlist_remove_flags(struct libmnt_optlist *ls, unsigned long flags,
			const struct libmnt_optmap *map)
{
	struct list_head *p, *next;

	if (!ls || !map)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, "remove 0x%08lx", flags));

	list_for_each_safe(p, next, &ls->opts) {
		struct libmnt_opt *opt = list_entry(p, struct libmnt_opt, opts);

		if (opt->external || !opt->ent)
			continue;
		if (map && opt->map != map)
			continue;
		if (opt->ent->id & flags)
			mnt_optlist_remove_opt(ls, opt);
	}
	return 0;
}

int mnt_optlist_insert_flags(struct libmnt_optlist *ls, unsigned long flags,
			const struct libmnt_optmap *map,
			unsigned long after,
			const struct libmnt_optmap *after_map)
{
	struct libmnt_opt *opt;

	if (!ls || !map || !after || !after_map)
		return -EINVAL;

	opt = mnt_optlist_get_opt(ls, after, after_map);
	if (!opt)
		return -EINVAL;

	DBG(OPTLIST, ul_debugobj(ls, "insert 0x%08lx (after %s)",
				flags, opt->ent ? opt->ent->name : "???"));

	return optlist_add_flags(ls, flags, map, &opt->opts);
}

static int is_wanted_opt(struct libmnt_opt *opt, const struct libmnt_optmap *map,
		unsigned int what)
{
	switch (what) {
	case MNT_OL_FLTR_DFLT:
		if (opt->external)
			return 0;
		if (map && opt->map != map)
			return 0;
		break;
	case MNT_OL_FLTR_ALL:
		break;
	case MNT_OL_FLTR_UNKNOWN:
		if (opt->map || opt->external)
			return 0;
		break;
	case MNT_OL_FLTR_HELPERS:
		if (opt->ent && opt->ent->mask & MNT_NOHLPS)
			return 0;
		break;
	case MNT_OL_FLTR_MTAB:
		if (opt->ent && opt->ent->mask & MNT_NOMTAB)
			return 0;
		break;
	}

	return 1;
}

static struct optlist_cache *get_cache(	struct libmnt_optlist *ls,
					const struct libmnt_optmap *map,
					unsigned int what)
{
	switch (what) {
	case MNT_OL_FLTR_DFLT:
		if (map) {
			const size_t idx = optlist_get_mapidx(ls, map);
			if (idx == (size_t) -1)
				return NULL;
			return &ls->cache_mapped[idx];
		}
		return &ls->cache_all[MNT_OL_FLTR_DFLT];

	case MNT_OL_FLTR_ALL:
	case MNT_OL_FLTR_UNKNOWN:
	case MNT_OL_FLTR_HELPERS:
	case MNT_OL_FLTR_MTAB:
		return &ls->cache_all[what];

	default:
		break;
	}

	return NULL;
}

/*
 * Returns flags (bit mask from options map entries).
 */
int mnt_optlist_get_flags(struct libmnt_optlist *ls, unsigned long *flags,
			  const struct libmnt_optmap *map, unsigned int what)
{
	struct optlist_cache *cache;

	if (!ls || !map || !flags)
		return -EINVAL;

	cache = get_cache(ls, map, what);
	if (!cache)
		return -EINVAL;

	if (!cache->flags_ready) {
		struct libmnt_iter itr;
		struct libmnt_opt *opt;
		unsigned long fl = 0;

		mnt_reset_iter(&itr, MNT_ITER_FORWARD);

		while (mnt_optlist_next_opt(ls, &itr, &opt) == 0) {
			if (map != opt->map)
				continue;
			if (!opt->ent || !opt->ent->id)
				continue;
			if (!is_wanted_opt(opt, map, what))
				continue;

			if (opt->ent->mask & MNT_INVERT)
				fl &= ~opt->ent->id;
			else
				fl |= opt->ent->id;
		}

		cache->flags = fl;
		cache->flags_ready = 1;
	}

	*flags = cache->flags;

	DBG(OPTLIST, ul_debugobj(ls, "return flags 0x%08lx [map=%p]", *flags, map));
	return 0;
}


/*
 * Like mnt_optlist_get_flags() for VFS flags, but converts classic MS_* flags to
 * new MOUNT_ATTR_*
 */
#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT

#define MNT_RESETABLE_ATTRS	(MOUNT_ATTR_RDONLY| MOUNT_ATTR_NOSUID| \
				 MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC| \
				 MOUNT_ATTR_NOATIME|  MOUNT_ATTR_NODIRATIME | \
				 MOUNT_ATTR_NOSYMFOLLOW)

int mnt_optlist_get_attrs(struct libmnt_optlist *ls, uint64_t *set, uint64_t *clr, int rec)
{
	struct libmnt_iter itr;
	struct libmnt_opt *opt;
	uint64_t remount_reset = 0;

	if (!ls || !ls->linux_map || !set || !clr)
		return -EINVAL;

	*set = 0, *clr = 0;
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	/* The classic mount(2) MS_REMOUNT resets all flags which are not
	 * specified (except atime stuff). For backward compatibility we need
	 * to emulate this semantic by mount_setattr(). The new
	 * mount_setattr() has simple set/unset sematinc and nothing is
	 * internally in kernel reseted.
	 */
	if (mnt_optlist_is_remount(ls)
	    && !mnt_optlist_is_bind(ls)
	    && rec == MNT_OL_NOREC)
		remount_reset = (MOUNT_ATTR_RDONLY| MOUNT_ATTR_NOSUID| \
				 MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC| \
				 MOUNT_ATTR_NOSYMFOLLOW);

	while (mnt_optlist_next_opt(ls, &itr, &opt) == 0) {
		uint64_t x = 0;

		if (ls->linux_map != opt->map)
			continue;
		if (!opt->ent || !opt->ent->id)
			continue;

		if (rec == MNT_OL_REC && !opt->recursive)
			continue;
		if (rec == MNT_OL_NOREC && opt->recursive)
			continue;

		if (!is_wanted_opt(opt, ls->linux_map, MNT_OL_FLTR_DFLT))
			continue;
		if (flag_to_attr( opt->ent->id, &x) < 0)
			continue;

		if (x && remount_reset)
			remount_reset &= ~x;

		if (opt->ent->mask & MNT_INVERT) {
			DBG(OPTLIST, ul_debugobj(ls, " clr: %s", opt->ent->name));
			/*
			 * All atime settings are mutually exclusive so *clr must
			 * have MOUNT_ATTR__ATIME set.
			 *
			 * See the function fs/namespace.c:build_mount_kattr()
			 * in the linux kernel source.
			 */
			if (x == MOUNT_ATTR_RELATIME || x == MOUNT_ATTR_NOATIME ||
			    x == MOUNT_ATTR_STRICTATIME)
				*clr |= MOUNT_ATTR__ATIME;
			else
				*clr |= x;
		} else {
			DBG(OPTLIST, ul_debugobj(ls, " set: %s", opt->ent->name));
			*set |= x;

			if (x == MOUNT_ATTR_RELATIME || x == MOUNT_ATTR_NOATIME ||
			    x == MOUNT_ATTR_STRICTATIME)
				*clr |= MOUNT_ATTR__ATIME;
		}
	}

	if (remount_reset)
		*clr |= remount_reset;

	DBG(OPTLIST, ul_debugobj(ls, "return attrs set=0x%08" PRIx64
				      ", clr=0x%08" PRIx64 " %s",
				*set, *clr,
				rec == MNT_OL_REC ? "[rec]" :
				rec == MNT_OL_NOREC ? "[norec]" : ""));
	return 0;
}

#else
int mnt_optlist_get_attrs(struct libmnt_optlist *ls __attribute__((__unused__)),
			  uint64_t *set __attribute__((__unused__)),
			  uint64_t *clr __attribute__((__unused__)),
			  int mask __attribute__((__unused__)))
{
	return 0;
}
#endif /* USE_LIBMOUNT_MOUNTFD_SUPPORT */

int mnt_optlist_strdup_optstr(struct libmnt_optlist *ls, char **optstr,
			const struct libmnt_optmap *map, unsigned int what)
{
	struct libmnt_iter itr;
	struct libmnt_opt *opt;
	struct ul_buffer buf = UL_INIT_BUFFER;
	char *str = NULL;
	int rc = 0, is_rdonly = 0, xx_wanted = 0;

	if (!ls || !optstr)
		return -EINVAL;

	*optstr = NULL;

	/* For generic options srings ro/rw is expected at the begining */
	if ((!map || map == ls->linux_map)
	     && (what == MNT_OL_FLTR_DFLT ||
		 what == MNT_OL_FLTR_ALL ||
		 what == MNT_OL_FLTR_HELPERS)) {

		rc = mnt_buffer_append_option(&buf, "rw", 2, NULL, 0, 0);
		if (rc)
			goto fail;
		xx_wanted = 1;
	}

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_optlist_next_opt(ls, &itr, &opt) == 0) {
		if (!opt->name)
			continue;
		if (opt->map == ls->linux_map && opt->ent->id == MS_RDONLY) {
			is_rdonly = opt->ent->mask & MNT_INVERT ? 0 : 1;
			continue;
		}
		if (!is_wanted_opt(opt, map, what))
			continue;
		rc = mnt_buffer_append_option(&buf,
					opt->name, strlen(opt->name),
					opt->value ? opt->value :
						     opt->sepnodata ? "" : NULL,
					opt->value ? strlen(opt->value) : 0,
					opt->quoted);
		if (rc)
			goto fail;
	}

	str = ul_buffer_get_data(&buf, NULL, NULL);

	/* convert 'rw' at the beginning to 'ro' if necessary */
	if (str && is_rdonly && xx_wanted
	    && (what == MNT_OL_FLTR_DFLT ||
		what == MNT_OL_FLTR_ALL ||
		what == MNT_OL_FLTR_HELPERS)) {

		str[0] = 'r';
		str[1] = 'o';
	}

	if (optstr)
		*optstr = str;
	return 0;
fail:
	ul_buffer_free_data(&buf);
	return rc;
}

int mnt_optlist_get_optstr(struct libmnt_optlist *ls, const char **optstr,
			const struct libmnt_optmap *map, unsigned int what)
{
	struct optlist_cache *cache;

	if (!ls || !optstr)
		return -EINVAL;

	*optstr = NULL;

	cache = get_cache(ls, map, what);
	if (!cache)
		return -EINVAL;

	if (!cache->optstr_ready) {
		char *str = NULL;
		int rc = mnt_optlist_strdup_optstr(ls, &str, map, what);

		if (rc)
			return rc;

		cache->optstr = str;
		cache->optstr_ready = 1;
	}

	*optstr = cache->optstr;
	return 0;
}

struct libmnt_optlist *mnt_copy_optlist(struct libmnt_optlist *ls)
{
	struct libmnt_optlist *n = mnt_new_optlist();
	struct libmnt_iter itr;
	struct libmnt_opt *opt;
	size_t i;

	if (!n)
		return NULL;

	n->age = ls->age;
	n->linux_map = ls->linux_map;

	for (i = 0; i < ls->nmaps; i++)
		n->maps[i] = ls->maps[i];
	n->nmaps = ls->nmaps;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_optlist_next_opt(ls, &itr, &opt) == 0) {
		struct libmnt_opt *no;

		no = optlist_new_opt(n,
			opt->name, opt->name ? strlen(opt->name) : 0,
			opt->value, opt->value ? strlen(opt->value) : 0,
			opt->map, opt->ent, NULL);
		if (no) {
			no->src = opt->src;
			no->external = opt->external;
			no->quoted = opt->quoted;
			no->sepnodata = opt->sepnodata;
		}
	}

	n->merged = ls->merged;
	return n;
}


int mnt_optlist_is_empty(struct libmnt_optlist *ls)
{
	return ls == NULL || list_empty(&ls->opts);
}

unsigned int mnt_optlist_get_age(struct libmnt_optlist *ls)
{
	return ls ? ls->age : 0;
}

int mnt_optlist_get_propagation(struct libmnt_optlist *ls)
{
	return ls ? ls->propagation : 0;
}

int mnt_optlist_is_propagation_only(struct libmnt_optlist *ls)
{
	unsigned long flags = 0, rest;

	if (!ls || !ls->propagation || !ls->nmaps)
		return 0;

	if (mnt_optlist_get_flags(ls, &flags, ls->linux_map, 0) != 0)
		return 0;

	rest = flags & ~MS_PROPAGATION;
	DBG(OPTLIST, ul_debugobj(ls, " propagation-only: %s",
		(rest == 0 || (rest & (MS_SILENT | MS_REC)) ? "y" : "n")));

	return (rest == 0 || (rest & (MS_SILENT | MS_REC)));
}

int mnt_optlist_is_remount(struct libmnt_optlist *ls)
{
	return ls && ls->is_remount;
}

int mnt_optlist_is_recursive(struct libmnt_optlist *ls)
{
	return ls && ls->is_recursive;
}

int mnt_optlist_is_move(struct libmnt_optlist *ls)
{
	return ls && ls->is_move;
}

int mnt_optlist_is_bind(struct libmnt_optlist *ls)
{
	return ls && (ls->is_bind || ls->is_rbind);
}

int mnt_optlist_is_rbind(struct libmnt_optlist *ls)
{
	return ls && ls->is_rbind;
}

int mnt_optlist_is_rdonly(struct libmnt_optlist *ls)
{
	return ls && ls->is_rdonly;
}

int mnt_optlist_is_silent(struct libmnt_optlist *ls)
{
	return ls && ls->is_silent;
}


int mnt_opt_has_value(struct libmnt_opt *opt)
{
	return opt && opt->value;
}

const char *mnt_opt_get_value(struct libmnt_opt *opt)
{
	return opt->value;
}

const char *mnt_opt_get_name(struct libmnt_opt *opt)
{
	return opt->name;
}

const struct libmnt_optmap *mnt_opt_get_map(struct libmnt_opt *opt)
{
	return opt->map;
}

const struct libmnt_optmap *mnt_opt_get_mapent(struct libmnt_opt *opt)
{
	return opt->ent;
}

int mnt_opt_set_value(struct libmnt_opt *opt, const char *str)
{
	int rc;

	opt->recursive = 0;
	rc = strdup_to_struct_member(opt, value, str);

	if (rc == 0 && str && strcmp(str, "recursive") == 0)
		opt->recursive = 1;
	return rc;
}

int mnt_opt_set_u64value(struct libmnt_opt *opt, uint64_t num)
{
	char buf[ sizeof(stringify_value(UINT64_MAX)) ];

        snprintf(buf, sizeof(buf), "%"PRIu64, num);

	return mnt_opt_set_value(opt, buf);
}

int mnt_opt_set_quoted_value(struct libmnt_opt *opt, const char *str)
{
	opt->quoted = 1;
	return mnt_opt_set_value(opt, str);
}

int mnt_opt_set_external(struct libmnt_opt *opt, int enable)
{
	if (!opt)
		return -EINVAL;
	opt->external = enable ? 1 : 0;
	return 0;
}

int mnt_opt_is_external(struct libmnt_opt *opt)
{
	return opt && opt->external ? 1 : 0;
}

int mnt_opt_is_sepnodata(struct libmnt_opt *opt)
{
	return opt->sepnodata;
}


#ifdef TEST_PROGRAM

static int mk_optlist(struct libmnt_optlist **ol, const char *optstr)
{
	int rc = 0;

	*ol = mnt_new_optlist();
	if (!*ol)
		rc = -ENOMEM;

	if (!rc)
		rc = mnt_optlist_register_map(*ol, mnt_get_builtin_optmap(MNT_LINUX_MAP));
	if (!rc)
		rc = mnt_optlist_register_map(*ol, mnt_get_builtin_optmap(MNT_USERSPACE_MAP));
	if (!rc && optstr)
		rc = mnt_optlist_append_optstr(*ol, optstr, NULL);
	if (rc) {
		mnt_unref_optlist(*ol);
		*ol = NULL;
	}
	return rc;
}

static void dump_optlist(struct libmnt_optlist *ol)
{
	struct libmnt_iter itr;
	struct libmnt_opt *opt;
	int i = 0;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_optlist_next_opt(ol, &itr, &opt) == 0) {
		if (opt->ent)
			printf("#%02d [%p:0x%08x] name:'%s',\tvalue:'%s'\n",
				++i, opt->map, opt->ent->id, opt->name, opt->value);
		else
			printf("#%02d [         unknown         ] name:'%s',\tvalue:'%s'\n",
				++i, opt->name, opt->value);

	}
}

static const struct libmnt_optmap *get_map(const char *name)
{
	if (name && strcmp(name, "linux") == 0)
		return mnt_get_builtin_optmap(MNT_LINUX_MAP);
	if (name && strcmp(name, "user") == 0)
		return mnt_get_builtin_optmap(MNT_USERSPACE_MAP);
	return NULL;
}

static inline unsigned long str2flg(const char *str)
{
	return (unsigned long) strtox64_or_err(str, "connt convert string to flags");
}

static int test_append_str(struct libmnt_test *ts __attribute__((unused)),
			   int argc, char *argv[])
{
	struct libmnt_optlist *ol;
	int rc;

	if (argc < 3)
		return -EINVAL;
	rc = mk_optlist(&ol, argv[1]);
	if (!rc)
		rc = mnt_optlist_append_optstr(ol, argv[2], get_map(argv[3]));
	if (!rc)
		dump_optlist(ol);
	mnt_unref_optlist(ol);
	return rc;
}

static int test_prepend_str(struct libmnt_test *ts __attribute__((unused)),
			    int argc, char *argv[])
{
	struct libmnt_optlist *ol;
	int rc;

	if (argc < 3)
		return -EINVAL;
	rc = mk_optlist(&ol, argv[1]);
	if (!rc)
		rc = mnt_optlist_prepend_optstr(ol, argv[2], get_map(argv[3]));
	if (!rc)
		dump_optlist(ol);
	mnt_unref_optlist(ol);
	return rc;
}

static int test_set_str(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_optlist *ol;
	int rc;

	if (argc < 3)
		return -EINVAL;
	rc = mk_optlist(&ol, argv[1]);
	if (!rc)
		rc = mnt_optlist_set_optstr(ol, argv[2], get_map(argv[3]));
	if (!rc)
		dump_optlist(ol);
	mnt_unref_optlist(ol);
	return rc;
}

static int test_append_flg(struct libmnt_test *ts __attribute__((unused)),
			   int argc, char *argv[])
{
	struct libmnt_optlist *ol;
	int rc;

	if (argc < 4)
		return -EINVAL;
	rc = mk_optlist(&ol, argv[1]);
	if (!rc)
		rc = mnt_optlist_append_flags(ol, str2flg(argv[2]), get_map(argv[3]));
	if (!rc)
		dump_optlist(ol);
	mnt_unref_optlist(ol);
	return rc;
}

static int test_set_flg(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_optlist *ol;
	int rc;

	if (argc < 4)
		return -EINVAL;
	rc = mk_optlist(&ol, argv[1]);
	if (!rc)
		rc = mnt_optlist_set_flags(ol, str2flg(argv[2]), get_map(argv[3]));
	if (!rc)
		dump_optlist(ol);
	mnt_unref_optlist(ol);
	return rc;
}

static int test_get_str(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_optlist *ol;
	const struct libmnt_optmap *map;
	const char *str = NULL;
	int rc;
	unsigned long flags = 0;

	if (argc < 2)
		return -EINVAL;
	rc = mk_optlist(&ol, argv[1]);
	if (rc)
		goto done;

	map = get_map(argv[2]);
	mnt_optlist_merge_opts(ol);

	/* We always call mnt_optlist_get_optstr() two times to test the cache */
	if (map) {
		rc = mnt_optlist_get_optstr(ol, &str, map, MNT_OL_FLTR_DFLT);
		if (!rc)
			rc = mnt_optlist_get_optstr(ol, &str, map, MNT_OL_FLTR_DFLT);
		if (!rc)
			rc = mnt_optlist_get_flags(ol, &flags, map, MNT_OL_FLTR_DFLT);
		if (!rc)
			rc = mnt_optlist_get_flags(ol, &flags, map, MNT_OL_FLTR_DFLT);
		if (!rc)
			printf("Default: %s [0x%08lx] (in %s map)\n", str, flags, argv[2]);
	}

	rc = mnt_optlist_get_optstr(ol, &str, NULL, MNT_OL_FLTR_DFLT);
	if (!rc)
		rc = mnt_optlist_get_optstr(ol, &str, NULL, MNT_OL_FLTR_DFLT);
	if (!rc)
		printf("Default: %s\n", str);


	rc = mnt_optlist_get_optstr(ol, &str, NULL, MNT_OL_FLTR_ALL);
	if (!rc)
		rc = mnt_optlist_get_optstr(ol, &str, NULL, MNT_OL_FLTR_ALL);
	if (!rc)
		printf("All:     %s\n", str);

	rc = mnt_optlist_get_optstr(ol, &str, NULL, MNT_OL_FLTR_UNKNOWN);
	if (!rc)
		rc = mnt_optlist_get_optstr(ol, &str, NULL, MNT_OL_FLTR_UNKNOWN);
	if (!rc)
		printf("Unknown: %s\n", str);

	rc = mnt_optlist_get_optstr(ol, &str, NULL, MNT_OL_FLTR_HELPERS);
	if (!rc)
		rc = mnt_optlist_get_optstr(ol, &str, NULL, MNT_OL_FLTR_HELPERS);
	if (!rc)
		printf("Helpers: %s\n", str);
done:
	mnt_unref_optlist(ol);
	return rc;
}

static int test_get_flg(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_optlist *ol;
	unsigned long flags = 0;
	int rc;

	if (argc < 3)
		return -EINVAL;
	rc = mk_optlist(&ol, argv[1]);
	if (!rc)
		rc = mnt_optlist_get_flags(ol, &flags, get_map(argv[2]), 0);
	if (!rc)
		printf("0x%08lx\n", flags);
	mnt_unref_optlist(ol);
	return rc;
}

static int test_split(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	struct libmnt_optlist *ol;
	int rc;
	struct libmnt_iter itr;
	struct libmnt_opt *opt;
	const char *name, *value;

	if (argc != 2)
		return -EINVAL;
	rc = mk_optlist(&ol, argv[1]);
	if (rc)
		goto done;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_optlist_next_opt(ol, &itr, &opt) == 0) {
		name = mnt_opt_get_name(opt);
		value = mnt_opt_get_value(opt);

		printf("%s = %s\n", name, value ?: "(null)");
	}

done:
	mnt_unref_optlist(ol);
	return rc;
}


int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
		{ "--append-str",  test_append_str,  "<list> <str> [linux|user]  append to the list" },
		{ "--prepend-str", test_prepend_str, "<list> <str> [linux|user]  prepend to the list" },
		{ "--set-str",     test_set_str,     "<list> <str> [linux|user]  set to the list" },
		{ "--append-flg",  test_append_flg,  "<list> <flg>  linux|user   append to the list" },
		{ "--set-flg",     test_set_flg,     "<list> <flg>  linux|user   set to the list" },
		{ "--get-str",     test_get_str,     "<list> [linux|user]        all options in string" },
		{ "--get-flg",     test_get_flg,     "<list>  linux|user         all options by flags" },
		{ "--split",       test_split,       "<list>                     split options into key-value pairs"},

		{ NULL }
	};
	return  mnt_run_test(tss, argc, argv);
}
#endif /* TEST_PROGRAM */
