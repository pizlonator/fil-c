/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2012-2023 Karel Zak <kzak@redhat.com>
 *
 * Original implementation from Linux 0.99, without License and copyright in
 * the code. Karel Zak rewrote the code under GPL-2.0-or-later.
 */
#include "c.h"
#include "nls.h"
#include "xalloc.h"

#include "swapon-common.h"

/*
 * content of /proc/swaps and /etc/fstab
 */
static struct libmnt_table *swaps, *fstab;

struct libmnt_cache *mntcache;

static int table_parser_errcb(struct libmnt_table *tb __attribute__((__unused__)),
			const char *filename, int line)
{
	if (filename)
		warnx(_("%s: parse error at line %d -- ignored"), filename, line);
	return 1;
}

struct libmnt_table *get_fstab(const char *filename)
{
	if (!fstab) {
		fstab = mnt_new_table();
		if (!fstab)
			return NULL;
		mnt_table_set_parser_errcb(fstab, table_parser_errcb);
		mnt_table_set_cache(fstab, mntcache);
		if (mnt_table_parse_fstab(fstab, filename) != 0)
			return NULL;
	}

	return fstab;
}

struct libmnt_table *get_swaps(void)
{
	if (!swaps) {
		swaps = mnt_new_table();
		if (!swaps)
			return NULL;
		mnt_table_set_cache(swaps, mntcache);
		mnt_table_set_parser_errcb(swaps, table_parser_errcb);
		if (mnt_table_parse_swaps(swaps, NULL) != 0)
			return NULL;
	}

	return swaps;
}

void free_tables(void)
{
	mnt_unref_table(swaps);
	mnt_unref_table(fstab);
}

int match_swap(struct libmnt_fs *fs, void *data __attribute__((unused)))
{
	return fs && mnt_fs_is_swaparea(fs);
}

int is_active_swap(const char *filename)
{
	struct libmnt_table *st = get_swaps();
	return st && mnt_table_find_source(st, filename, MNT_ITER_BACKWARD);
}


int cannot_find(const char *special)
{
	warnx(_("cannot find the device for %s"), special);
	return -1;
}

/*
 * Lists with -L and -U option
 */
static const char **llist;
static size_t llct;
static const char **ulist;
static size_t ulct;


void add_label(const char *label)
{
	llist = xreallocarray(llist, ++llct, sizeof(char *));
	llist[llct - 1] = label;
}

const char *get_label(size_t i)
{
	return i < llct ? llist[i] : NULL;
}

size_t numof_labels(void)
{
	return llct;
}

void add_uuid(const char *uuid)
{
	ulist = xreallocarray(ulist, ++ulct, sizeof(char *));
	ulist[ulct - 1] = uuid;
}

const char *get_uuid(size_t i)
{
	return i < ulct ? ulist[i] : NULL;
}

size_t numof_uuids(void)
{
	return ulct;
}

