
/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2008-2018 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

/**
 * SECTION: table
 * @title: Table of filesystems
 * @short_description: container for entries from fstab, mtab or mountinfo
 *
 * Note that mnt_table_find_* functions are mount(8) compatible. These functions
 * try to find an entry in more iterations, where the first attempt is always
 * based on comparison with unmodified (non-canonicalized or un-evaluated)
 * paths or tags. For example a fstab with two entries:
 * <informalexample>
 *   <programlisting>
 *	LABEL=foo	/foo	auto   rw
 *	/dev/foo	/foo	auto   rw
 *  </programlisting>
 * </informalexample>
 *
 * where both lines are used for the *same* device, then
 * <informalexample>
 *  <programlisting>
 *	mnt_table_find_source(tb, "/dev/foo", &fs);
 *  </programlisting>
 * </informalexample>
 * will returns the second line, and
 * <informalexample>
 *  <programlisting>
 *	mnt_table_find_source(tb, "LABEL=foo", &fs);
 *  </programlisting>
 * </informalexample>
 * will returns the first entry, and
 * <informalexample>
 *  <programlisting>
 *	mnt_table_find_source(tb, "UUID=anyuuid", &fs);
 *  </programlisting>
 * </informalexample>
 * will return the first entry (if UUID matches with the device).
 */
#include <blkid.h>

#include "mountP.h"
#include "strutils.h"
#include "loopdev.h"
#include "fileutils.h"
#include "canonicalize.h"

int is_mountinfo(struct libmnt_table *tb)
{
	struct libmnt_fs *fs;

	if (!tb)
		return 0;

	fs = list_first_entry(&tb->ents, struct libmnt_fs, ents);
	if (fs && mnt_fs_is_kernel(fs) && mnt_fs_get_root(fs))
		return 1;

	return 0;
}

/**
 * mnt_new_table:
 *
 * The tab is a container for struct libmnt_fs entries that usually represents a fstab,
 * mtab or mountinfo file from your system.
 *
 * See also mnt_table_parse_file().
 *
 * Returns: newly allocated tab struct.
 */
struct libmnt_table *mnt_new_table(void)
{
	struct libmnt_table *tb = NULL;

	tb = calloc(1, sizeof(*tb));
	if (!tb)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "alloc"));
	tb->refcount = 1;
	INIT_LIST_HEAD(&tb->ents);
	return tb;
}

/**
 * mnt_reset_table:
 * @tb: tab pointer
 *
 * Removes all entries (filesystems) from the table. The filesystems with zero
 * reference count will be deallocated.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_reset_table(struct libmnt_table *tb)
{
	if (!tb)
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "reset"));

	while (!list_empty(&tb->ents)) {
		struct libmnt_fs *fs = list_entry(tb->ents.next,
				                  struct libmnt_fs, ents);
		mnt_table_remove_fs(tb, fs);
	}

	tb->nents = 0;
	return 0;
}

/**
 * mnt_ref_table:
 * @tb: table pointer
 *
 * Increments reference counter.
 */
void mnt_ref_table(struct libmnt_table *tb)
{
	if (tb) {
		tb->refcount++;
		/*DBG(FS, ul_debugobj(tb, "ref=%d", tb->refcount));*/
	}
}

/**
 * mnt_unref_table:
 * @tb: table pointer
 *
 * De-increments reference counter, on zero the @tb is automatically
 * deallocated by mnt_free_table().
 */
void mnt_unref_table(struct libmnt_table *tb)
{
	if (tb) {
		tb->refcount--;
		/*DBG(FS, ul_debugobj(tb, "unref=%d", tb->refcount));*/
		if (tb->refcount <= 0)
			mnt_free_table(tb);
	}
}


/**
 * mnt_free_table:
 * @tb: tab pointer
 *
 * Deallocates the table. This function does not care about reference count. Don't
 * use this function directly -- it's better to use mnt_unref_table().
 *
 * The table entries (filesystems) are unreferenced by mnt_reset_table() and
 * cache by mnt_unref_cache().
 */
void mnt_free_table(struct libmnt_table *tb)
{
	if (!tb)
		return;

	mnt_reset_table(tb);
	DBG(TAB, ul_debugobj(tb, "free [refcount=%d]", tb->refcount));

	mnt_unref_cache(tb->cache);
	free(tb->comm_intro);
	free(tb->comm_tail);
	free(tb);
}

/**
 * mnt_table_get_nents:
 * @tb: pointer to tab
 *
 * Returns: number of entries in table.
 */
int mnt_table_get_nents(struct libmnt_table *tb)
{
	return tb ? tb->nents : 0;
}

/**
 * mnt_table_is_empty:
 * @tb: pointer to tab
 *
 * Returns: 1 if the table is without filesystems, or 0.
 */
int mnt_table_is_empty(struct libmnt_table *tb)
{
	return tb == NULL || list_empty(&tb->ents) ? 1 : 0;
}

/**
 * mnt_table_set_userdata:
 * @tb: pointer to tab
 * @data: pointer to user data
 *
 * Sets pointer to the private user data.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_set_userdata(struct libmnt_table *tb, void *data)
{
	if (!tb)
		return -EINVAL;

	tb->userdata = data;
	return 0;
}

/**
 * mnt_table_get_userdata:
 * @tb: pointer to tab
 *
 * Returns: pointer to user's data.
 */
void *mnt_table_get_userdata(struct libmnt_table *tb)
{
	return tb ? tb->userdata : NULL;
}

/**
 * mnt_table_enable_comments:
 * @tb: pointer to tab
 * @enable: TRUE or FALSE
 *
 * Enables parsing of comments.
 *
 * The initial (intro) file comment is accessible by
 * mnt_table_get_intro_comment(). The intro and the comment of the first fstab
 * entry has to be separated by blank line.  The filesystem comments are
 * accessible by mnt_fs_get_comment(). The trailing fstab comment is accessible
 * by mnt_table_get_trailing_comment().
 *
 * <informalexample>
 *  <programlisting>
 *	#
 *	# Intro comment
 *	#
 *
 *	# this comments belongs to the first fs
 *	LABEL=foo /mnt/foo auto defaults 1 2
 *	# this comments belongs to the second fs
 *	LABEL=bar /mnt/bar auto defaults 1 2
 *	# tailing comment
 *  </programlisting>
 * </informalexample>
 */
void mnt_table_enable_comments(struct libmnt_table *tb, int enable)
{
	if (tb)
		tb->comms = enable;
}

/**
 * mnt_table_with_comments:
 * @tb: pointer to table
 *
 * Returns: 1 if comments parsing is enabled, or 0.
 */
int mnt_table_with_comments(struct libmnt_table *tb)
{
	assert(tb);
	return tb ? tb->comms : 0;
}

/**
 * mnt_table_get_intro_comment:
 * @tb: pointer to tab
 *
 * Returns: initial comment in tb
 */
const char *mnt_table_get_intro_comment(struct libmnt_table *tb)
{
	return tb ? tb->comm_intro : NULL;
}

/**
 * mnt_table_set_into_comment:
 * @tb: pointer to tab
 * @comm: comment or NULL
 *
 * Sets the initial comment in tb.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_set_intro_comment(struct libmnt_table *tb, const char *comm)
{
	return strdup_to_struct_member(tb, comm_intro, comm);
}

/**
 * mnt_table_append_into_comment:
 * @tb: pointer to tab
 * @comm: comment of NULL
 *
 * Appends the initial comment in tb.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_append_intro_comment(struct libmnt_table *tb, const char *comm)
{
	if (!tb)
		return -EINVAL;
	return strappend(&tb->comm_intro, comm);
}

/**
 * mnt_table_get_trailing_comment:
 * @tb: pointer to tab
 *
 * Returns: table trailing comment
 */
const char *mnt_table_get_trailing_comment(struct libmnt_table *tb)
{
	return tb ? tb->comm_tail : NULL;
}

/**
 * mnt_table_set_trailing_comment
 * @tb: pointer to tab
 * @comm: comment string
 *
 * Sets the trailing comment in table.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_set_trailing_comment(struct libmnt_table *tb, const char *comm)
{
	return strdup_to_struct_member(tb, comm_tail, comm);
}

/**
 * mnt_table_append_trailing_comment:
 * @tb: pointer to tab
 * @comm: comment of NULL
 *
 * Appends to the trailing table comment.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_append_trailing_comment(struct libmnt_table *tb, const char *comm)
{
	if (!tb)
		return -EINVAL;
	return strappend(&tb->comm_tail, comm);
}

/**
 * mnt_table_set_cache:
 * @tb: pointer to tab
 * @mpc: pointer to struct libmnt_cache instance
 *
 * Sets up a cache for canonicalized paths and evaluated tags (LABEL/UUID). The
 * cache is recommended for mnt_table_find_*() functions.
 *
 * The cache could be shared between more tabs. Be careful when you share the
 * same cache between more threads -- currently the cache does not provide any
 * locking method.
 *
 * This function increments cache reference counter. It's recommended to use
 * mnt_unref_cache() after mnt_table_set_cache() if you want to keep the cache
 * referenced by @tb only.
 *
 * See also mnt_new_cache().
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_set_cache(struct libmnt_table *tb, struct libmnt_cache *mpc)
{
	if (!tb)
		return -EINVAL;

	mnt_ref_cache(mpc);			/* new */
	mnt_unref_cache(tb->cache);		/* old */
	tb->cache = mpc;
	return 0;
}

/**
 * mnt_table_get_cache:
 * @tb: pointer to tab
 *
 * Returns: pointer to struct libmnt_cache instance or NULL.
 */
struct libmnt_cache *mnt_table_get_cache(struct libmnt_table *tb)
{
	return tb ? tb->cache : NULL;
}

/**
 * mnt_table_find_fs:
 * @tb: tab pointer
 * @fs: entry to look for
 *
 * Checks if @fs is part of table @tb.
 *
 * Returns: index of @fs in table, 0 if not found or negative number in case of error.
 *
 * Since: 2.34
 */
int mnt_table_find_fs(struct libmnt_table *tb, struct libmnt_fs *fs)
{
	struct list_head *p;
	int i = 0;

	if (!tb || !fs)
		return -EINVAL;

	if (list_empty(&fs->ents))
		return 0;

	/* Let's use directly list rather than mnt_table_next_fs() as we
	 * compare list entry with fs only.
	 */
	list_for_each(p, &tb->ents) {
		++i;
		if (list_entry(p, struct libmnt_fs, ents) == fs)
			return i;
	}

	return 0;
}

/**
 * mnt_table_add_fs:
 * @tb: tab pointer
 * @fs: new entry
 *
 * Adds a new entry to tab and increment @fs reference counter. Don't forget to
 * use mnt_unref_fs() after mnt_table_add_fs() you want to keep the @fs
 * referenced by the table only.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_add_fs(struct libmnt_table *tb, struct libmnt_fs *fs)
{
	if (!tb || !fs)
		return -EINVAL;

	if (fs->tab)
		return -EBUSY;

	mnt_ref_fs(fs);
	list_add_tail(&fs->ents, &tb->ents);
	fs->tab = tb;
	tb->nents++;

	DBG(TAB, ul_debugobj(tb, "add entry: %s %s",
			mnt_fs_get_source(fs), mnt_fs_get_target(fs)));
	return 0;
}

static int __table_insert_fs(
			struct libmnt_table *tb, int before,
			struct libmnt_fs *pos, struct libmnt_fs *fs)
{
	struct list_head *head = pos ? &pos->ents : &tb->ents;

	if (before)
		list_add(&fs->ents, head);
	else
		list_add_tail(&fs->ents, head);

	fs->tab = tb;
	tb->nents++;

	DBG(TAB, ul_debugobj(tb, "insert entry: %s %s",
			mnt_fs_get_source(fs), mnt_fs_get_target(fs)));
	return 0;
}

/**
 * mnt_table_insert_fs:
 * @tb: tab pointer
 * @before: 1 to insert before pos, 0 to insert after pos
 * @pos: entry to specify position or NULL
 * @fs: new entry
 *
 * Adds a new entry to @tb before or after a specific table entry @pos. If the
 * @pos is NULL than add the begin of the @tab if @before is 1; or to the tail
 * of the @tb if @before is 0.
 *
 * This function increments reference to @fs. Don't forget to use
 * mnt_unref_fs() after mnt_table_insert_fs() if you want to keep the @fs
 * referenced by the table only.
 *
 * Returns: 0 on success or negative number in case of error.
 *
 * Since: 2.34
 */
int mnt_table_insert_fs(struct libmnt_table *tb, int before,
			struct libmnt_fs *pos, struct libmnt_fs *fs)
{
	if (!tb || !fs)
		return -EINVAL;

	if (fs->tab)
		return -EBUSY;

	if (pos && pos->tab != tb)
		return -ENOENT;

	mnt_ref_fs(fs);
	return __table_insert_fs(tb, before, pos, fs);
}

/**
 * mnt_table_move_fs:
 * @src: tab pointer of source table
 * @dst: tab pointer of destination table
 * @before: 1 to move before position, 0 to move after position
 * @pos: entry to specify position or NULL
 * @fs: entry to move
 *
 * Removes @fs from @src table and adds it before/after a specific entry @pos
 * of @dst table. If the @pos is NULL than add the begin of the @dst if @before
 * is 1; or to the tail of the @dst if @before is 0.
 *
 * The reference counter of @fs is not modified.
 *
 * Returns: 0 on success or negative number in case of error.
 *
 * Since: 2.34
 */
int mnt_table_move_fs(struct libmnt_table *src, struct libmnt_table *dst,
                      int before, struct libmnt_fs *pos, struct libmnt_fs *fs)
{
	if (!src || !dst || !fs)
		return -EINVAL;

	if (fs->tab != src || (pos && pos->tab != dst))
		return -ENOENT;

	/* remove from source */
	list_del_init(&fs->ents);
	src->nents--;

	/* insert to the destination */
	return __table_insert_fs(dst, before, pos, fs);
}


/**
 * mnt_table_remove_fs:
 * @tb: tab pointer
 * @fs: new entry
 *
 * Removes the @fs from the table and de-increment reference counter of the @fs. The
 * filesystem with zero reference counter will be deallocated. Don't forget to use
 * mnt_ref_fs() before call mnt_table_remove_fs() if you want to use @fs later.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_remove_fs(struct libmnt_table *tb, struct libmnt_fs *fs)
{
	if (!tb || !fs || fs->tab != tb)
		return -EINVAL;

	fs->tab = NULL;
	list_del_init(&fs->ents);

	mnt_unref_fs(fs);
	tb->nents--;
	return 0;
}

static inline struct libmnt_fs *get_parent_fs(struct libmnt_table *tb, struct libmnt_fs *fs)
{
	struct libmnt_iter itr;
	struct libmnt_fs *x;
	int parent_id = mnt_fs_get_parent_id(fs);

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);
	while (mnt_table_next_fs(tb, &itr, &x) == 0) {
		if (mnt_fs_get_id(x) == parent_id)
			return x;
	}

	return NULL;
}

/**
 * mnt_table_get_root_fs:
 * @tb: mountinfo file (/proc/self/mountinfo)
 * @root: NULL or returns pointer to the root filesystem (/)
 *
 * The function uses the parent ID from the mountinfo file to determine the
 * root filesystem (the filesystem with the smallest ID with parent ID missing
 * in the table). The function is designed mostly for applications where it is
 * necessary to sort mountpoints by IDs to get the tree of the mountpoints
 * (e.g. findmnt default output).
 *
 * If you're not sure, then use
 *
 *	mnt_table_find_target(tb, "/", MNT_ITER_BACKWARD);
 *
 * this is more robust and usable for arbitrary tab files (including fstab).
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_get_root_fs(struct libmnt_table *tb, struct libmnt_fs **root)
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs, *root_fs = NULL;
	int root_id = 0;

	if (!tb || !is_mountinfo(tb))
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "lookup root fs"));

	/* get smallest possible ID from the table */
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);
	while (mnt_table_next_fs(tb, &itr, &fs) == 0) {
		int id = mnt_fs_get_parent_id(fs);

		if (!root_fs || id < root_id) {
			root_fs = fs;
			root_id = id;
		}
	}

	/* go to the root node by "parent_id -> id" relation */
	while (root_fs) {
		struct libmnt_fs *x = get_parent_fs(tb, root_fs);
		if (!x || x == root_fs)
			break;
		DBG(TAB, ul_debugobj(tb, " messy mountinfo, walk to %s", mnt_fs_get_target(x)));
		root_fs = x;
	}

	if (root)
		*root = root_fs;

	return root_fs ? 0 : -EINVAL;
}

/**
 * mnt_table_next_child_fs:
 * @tb: mountinfo file (/proc/self/mountinfo)
 * @itr: iterator
 * @parent: parental FS
 * @chld: NULL or returns the next child filesystem
 *
 * Since version 2.40, the filesystems are returned in the order specified by
 * @itr. In the old versions the derection is always MNT_ITER_FORWARD.
 *
 * Returns: 0 on success, negative number in case of error or 1 at the end of list.
 */
int mnt_table_next_child_fs(struct libmnt_table *tb, struct libmnt_iter *itr,
			struct libmnt_fs *parent, struct libmnt_fs **chld)
{
	struct libmnt_fs *fs, *chfs = NULL;
	int parent_id, lastchld_id = 0, chld_id = 0;
	int direction;

	if (!tb || !itr || !parent || !is_mountinfo(tb))
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "lookup next child of '%s'",
				mnt_fs_get_target(parent)));
	parent_id = mnt_fs_get_id(parent);
	direction = mnt_iter_get_direction(itr);

	/* get ID of the previously returned child */
	if (itr->head && itr->p != itr->head) {
		fs = MNT_ITER_GET_ENTRY(itr, struct libmnt_fs, ents);
		MNT_ITER_ITERATE(itr);
		lastchld_id = mnt_fs_get_id(fs);
	}

	mnt_reset_iter(itr, direction);
	while (mnt_table_next_fs(tb, itr, &fs) == 0) {
		int id;

		if (mnt_fs_get_parent_id(fs) != parent_id)
			continue;

		id = mnt_fs_get_id(fs);

		/* avoid an infinite loop. This only happens in rare cases
		 * such as in early userspace when the rootfs is its own parent */
		if (id == parent_id)
			continue;

		if (direction == MNT_ITER_FORWARD) {
			/* return in the order of mounting */
			if ((!lastchld_id || id > lastchld_id) &&
			    (!chfs || id < chld_id)) {
				chfs = fs;
				chld_id = id;
			}
		} else {
			/* return last child first */
			if ((!lastchld_id || id < lastchld_id) &&
			    (!chfs || id > chld_id)) {
				chfs = fs;
				chld_id = id;
			}
		}
	}

	if (chld)
		*chld = chfs;
	if (!chfs)
		return 1;	/* end of iterator */

	/* set the iterator to the @chfs for the next call */
	mnt_table_set_iter(tb, itr, chfs);

	return 0;
}

/**
 * mnt_table_over_fs:
 * @tb: tab pointer
 * @parent: pointer to parental FS
 * @child: returns pointer to FS which over-mounting parent (optional)
 *
 * This function returns by @child the first filesystenm which is over-mounted
 * on @parent. It means the mountpoint of @child is the same as for @parent and
 * parent->id is the same as child->parent_id.
 *
 * Note that you need to call this function in loop until it returns 1 to get
 * the highest filesystem.
 *
 * Example:
 * <informalexample>
 *   <programlisting>
 *	while (mnt_table_over_fs(tb, cur, &over) == 0) {
 *		printf("%s overmounted by %d\n", mnt_fs_get_target(cur), mnt_fs_get_id(over));
 *		cur = over;
 *	}
 *   </programlisting>
 * </informalexample>
 *
 * Returns: 0 on success, negative number in case of error or 1 at the end of list.
 */
int mnt_table_over_fs(struct libmnt_table *tb, struct libmnt_fs *parent,
		      struct libmnt_fs **child)
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs = NULL;
	int id;
	const char *tgt;

	if (!tb || !parent || !is_mountinfo(tb))
		return -EINVAL;

	if (child)
		*child = NULL;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);
	id = mnt_fs_get_id(parent);
	tgt = mnt_fs_get_target(parent);

	while (mnt_table_next_fs(tb, &itr, &fs) == 0) {
		if (mnt_fs_get_parent_id(fs) == id &&
		    mnt_fs_streq_target(fs, tgt) == 1) {
			if (child)
				*child = fs;
			return 0;
		}
	}

	return 1;	/* nothing */
}

/**
 * mnt_table_next_fs:
 * @tb: tab pointer
 * @itr: iterator
 * @fs: NULL or returns the next tab entry
 *
 * Returns: 0 on success, negative number in case of error or 1 at the end of list.
 *
 * Example:
 * <informalexample>
 *   <programlisting>
 *	while(mnt_table_next_fs(tb, itr, &fs) == 0) {
 *		const char *dir = mnt_fs_get_target(fs);
 *		printf("mount point: %s\n", dir);
 *	}
 *   </programlisting>
 * </informalexample>
 *
 * lists all mountpoints from fstab in reverse order.
 */
int mnt_table_next_fs(struct libmnt_table *tb, struct libmnt_iter *itr, struct libmnt_fs **fs)
{
	int rc = 1;

	if (!tb || !itr)
		return -EINVAL;
	if (fs)
		*fs = NULL;

	if (!itr->head)
		MNT_ITER_INIT(itr, &tb->ents);
	if (itr->p != itr->head) {
		if (fs)
			*fs = MNT_ITER_GET_ENTRY(itr, struct libmnt_fs, ents);
		MNT_ITER_ITERATE(itr);
		rc = 0;
	}

	return rc;
}

/**
 * mnt_table_first_fs:
 * @tb: tab pointer
 * @fs: NULL or returns the first tab entry
 *
 * Returns: 0 on success, negative number in case of error or 1 at the end of list.
 */
int mnt_table_first_fs(struct libmnt_table *tb, struct libmnt_fs **fs)
{
	if (!tb)
		return -EINVAL;
	if (list_empty(&tb->ents))
		return 1;
	if (fs)
		*fs = list_first_entry(&tb->ents, struct libmnt_fs, ents);
	return 0;
}

/**
 * mnt_table_last_fs:
 * @tb: tab pointer
 * @fs: NULL or returns the last tab entry
 *
 * Returns: 0 on success, negative number in case of error or 1 at the end of list.
 */
int mnt_table_last_fs(struct libmnt_table *tb, struct libmnt_fs **fs)
{
	if (!tb)
		return -EINVAL;
	if (list_empty(&tb->ents))
		return 1;
	if (fs)
		*fs = list_last_entry(&tb->ents, struct libmnt_fs, ents);
	return 0;
}

/**
 * mnt_table_find_next_fs:
 * @tb: table
 * @itr: iterator
 * @match_func: function returning 1 or 0
 * @userdata: extra data for match_func
 * @fs: NULL or returns pointer to the next matching table entry
 *
 * This function allows searching in @tb.
 *
 * Returns: negative number in case of error, 1 at end of table or 0 o success.
 */
int mnt_table_find_next_fs(struct libmnt_table *tb, struct libmnt_iter *itr,
		int (*match_func)(struct libmnt_fs *, void *), void *userdata,
		struct libmnt_fs **fs)
{
	struct libmnt_fs *re = NULL;
	int match = 0;

	if (!tb || !itr || !match_func)
		return -EINVAL;

	DBG(TAB, ul_debugobj(tb, "lookup next fs"));

	if (fs)
		*fs = NULL;
	if (!itr->head)
		MNT_ITER_INIT(itr, &tb->ents);

	while (!match) {
		if (itr->p != itr->head) {
			re = MNT_ITER_GET_ENTRY(itr, struct libmnt_fs, ents);
			MNT_ITER_ITERATE(itr);
		} else
			return 1;	/*end */

		match = match_func(re, userdata);
	}

	if (fs)
		*fs = re;
	return 0;
}

static int mnt_table_move_parent(struct libmnt_table *tb, int oldid, int newid)
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs;

	if (!tb)
		return -EINVAL;
	if (list_empty(&tb->ents))
		return 0;

	DBG(TAB, ul_debugobj(tb, "moving parent ID from %d -> %d", oldid, newid));
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_table_next_fs(tb, &itr, &fs) == 0) {
		if (fs->parent == oldid)
			fs->parent = newid;
	}
	return 0;
}

/**
 * mnt_table_uniq_fs:
 * @tb: table
 * @flags: MNT_UNIQ_*
 * @cmp: function to compare filesystems
 *
 * This function de-duplicate the @tb, but does not change order of the
 * filesystems. The @cmp function has to return 0 if the filesystems are
 * equal, otherwise non-zero.
 *
 * The default is to keep in the table later mounted filesystems (function uses
 * backward mode iterator).
 *
 * @MNT_UNIQ_FORWARD:  remove later mounted filesystems
 * @MNT_UNIQ_KEEPTREE: keep parent->id relationship still valid
 *
 * Returns: negative number in case of error, or 0 o success.
 */
int mnt_table_uniq_fs(struct libmnt_table *tb, int flags,
				int (*cmp)(struct libmnt_table *,
					   struct libmnt_fs *,
					   struct libmnt_fs *))
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs;
	int direction = MNT_ITER_BACKWARD;

	if (!tb || !cmp)
		return -EINVAL;
	if (list_empty(&tb->ents))
		return 0;

	if (flags & MNT_UNIQ_FORWARD)
		direction = MNT_ITER_FORWARD;

	DBG(TAB, ul_debugobj(tb, "de-duplicate"));
	mnt_reset_iter(&itr, direction);

	if ((flags & MNT_UNIQ_KEEPTREE) && !is_mountinfo(tb))
		flags &= ~MNT_UNIQ_KEEPTREE;

	while (mnt_table_next_fs(tb, &itr, &fs) == 0) {
		int want = 1;
		struct libmnt_iter xtr;
		struct libmnt_fs *x;

		mnt_reset_iter(&xtr, direction);
		while (want && mnt_table_next_fs(tb, &xtr, &x) == 0) {
			if (fs == x)
				break;
			want = cmp(tb, x, fs) != 0;
		}

		if (!want) {
			if (flags & MNT_UNIQ_KEEPTREE)
				mnt_table_move_parent(tb, mnt_fs_get_id(fs),
							  mnt_fs_get_parent_id(fs));

			DBG(TAB, ul_debugobj(tb, "remove duplicate %s",
						mnt_fs_get_target(fs)));
			mnt_table_remove_fs(tb, fs);
		}
	}

	return 0;
}

/**
 * mnt_table_set_iter:
 * @tb: tab pointer
 * @itr: iterator
 * @fs: tab entry
 *
 * Sets @iter to the position of @fs in the file @tb.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_table_set_iter(struct libmnt_table *tb, struct libmnt_iter *itr, struct libmnt_fs *fs)
{
	if (!tb || !itr || !fs)
		return -EINVAL;

	if (fs->tab != tb)
		return -ENOENT;

	MNT_ITER_INIT(itr, &tb->ents);
	itr->p = &fs->ents;

	return 0;
}

/**
 * mnt_table_find_mountpoint:
 * @tb: tab pointer
 * @path: directory
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Same as mnt_get_mountpoint(), except this function does not rely on
 * st_dev numbers.
 *
 * Returns: a tab entry or NULL.
 */
struct libmnt_fs *mnt_table_find_mountpoint(struct libmnt_table *tb,
					    const char *path,
					    int direction)
{
	char *mnt;

	if (!tb || !path || !*path)
		return NULL;
	if (direction != MNT_ITER_FORWARD && direction != MNT_ITER_BACKWARD)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup MOUNTPOINT: '%s'", path));

	if (!mnt_is_path(path))
		return NULL;

	mnt = strdup(path);
	if (!mnt)
		return NULL;

	do {
		char *p;
		struct libmnt_fs *fs;

		fs = mnt_table_find_target(tb, mnt, direction);
		if (fs) {
			free(mnt);
			return fs;
		}

		p = stripoff_last_component(mnt);
		if (!p)
			break;
	} while (mnt && *(mnt + 1) != '\0');

	free(mnt);
	return mnt_table_find_target(tb, "/", direction);
}

/**
 * mnt_table_find_target:
 * @tb: tab pointer
 * @path: mountpoint directory
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Try to lookup an entry in the given tab, three iterations are possible, the first
 * with @path, the second with realpath(@path) and the third with realpath(@path)
 * against realpath(fs->target). The 2nd and 3rd iterations are not performed when
 * the @tb cache is not set (see mnt_table_set_cache()). If
 * mnt_cache_set_targets(cache, mtab) was called, the 3rd iteration skips any
 * @fs->target found in @mtab (see mnt_resolve_target()).
 *
 * Returns: a tab entry or NULL.
 */
struct libmnt_fs *mnt_table_find_target(struct libmnt_table *tb, const char *path, int direction)
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs = NULL;
	char *cn;

	if (!tb || !path || !*path)
		return NULL;
	if (direction != MNT_ITER_FORWARD && direction != MNT_ITER_BACKWARD)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup TARGET: '%s'", path));

	/* native @target */
	mnt_reset_iter(&itr, direction);
	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
		if (mnt_fs_streq_target(fs, path))
			return fs;
	}

	/* try absolute path */
	if (is_relative_path(path) && (cn = absolute_path(path))) {
		DBG(TAB, ul_debugobj(tb, "lookup absolute TARGET: '%s'", cn));
		mnt_reset_iter(&itr, direction);
		while (mnt_table_next_fs(tb, &itr, &fs) == 0) {
			if (mnt_fs_streq_target(fs, cn)) {
				free(cn);
				return fs;
			}
		}
		free(cn);
	}

	if (!tb->cache || !(cn = mnt_resolve_path(path, tb->cache)))
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup canonical TARGET: '%s'", cn));

	/* canonicalized paths in struct libmnt_table */
	mnt_reset_iter(&itr, direction);
	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
		if (mnt_fs_streq_target(fs, cn))
			return fs;
	}

	/* non-canonical path in struct libmnt_table
	 * -- note that mountpoint in /proc/self/mountinfo is already
	 *    canonicalized by the kernel
	 */
	mnt_reset_iter(&itr, direction);
	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
		char *p;

		if (!fs->target
		    || mnt_fs_is_swaparea(fs)
		    || mnt_fs_is_kernel(fs)
		    || (*fs->target == '/' && *(fs->target + 1) == '\0'))
		       continue;

		p = mnt_resolve_target(fs->target, tb->cache);
		/* both canonicalized, strcmp() is fine here */
		if (p && strcmp(cn, p) == 0)
			return fs;
	}
	return NULL;
}

/**
 * mnt_table_find_srcpath:
 * @tb: tab pointer
 * @path: source path (devname or dirname) or NULL
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Try to lookup an entry in the given tab, four iterations are possible, the first
 * with @path, the second with realpath(@path), the third with tags (LABEL, UUID, ..)
 * from @path and the fourth with realpath(@path) against realpath(entry->srcpath).
 *
 * The 2nd, 3rd and 4th iterations are not performed when the @tb cache is not
 * set (see mnt_table_set_cache()).
 *
 * For btrfs returns tab entry for default id.
 *
 * Note that NULL is a valid source path; it will be replaced with "none". The
 * "none" is used in /proc/{mounts,self/mountinfo} for pseudo filesystems.
 *
 * Returns: a tab entry or NULL.
 */
struct libmnt_fs *mnt_table_find_srcpath(struct libmnt_table *tb, const char *path, int direction)
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs = NULL;
	int ntags = 0, nents;
	char *cn;
	const char *p;

	if (!tb || !path || !*path)
		return NULL;
	if (direction != MNT_ITER_FORWARD && direction != MNT_ITER_BACKWARD)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup SRCPATH: '%s'", path));

	/* native paths */
	mnt_reset_iter(&itr, direction);

	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {

		if (mnt_fs_streq_srcpath(fs, path)) {
#ifdef HAVE_BTRFS_SUPPORT
			if (fs->fstype && !strcmp(fs->fstype, "btrfs")) {
				uint64_t default_id = btrfs_get_default_subvol_id(mnt_fs_get_target(fs));
				char *val;
				size_t len;

				if (default_id == UINT64_MAX)
					DBG(TAB, ul_debug("not found btrfs volume setting"));

				else if (mnt_fs_get_option(fs, "subvolid", &val, &len) == 0) {
					uint64_t subvol_id;

					if (mnt_parse_offset(val, len, &subvol_id)) {
						DBG(TAB, ul_debugobj(tb, "failed to parse subvolid="));
						continue;
					}
					if (subvol_id != default_id)
						continue;
				}
			}
#endif /* HAVE_BTRFS_SUPPORT */
			return fs;
		}
		if (mnt_fs_get_tag(fs, NULL, NULL) == 0)
			ntags++;
	}

	if (!path || !tb->cache || !(cn = mnt_resolve_path(path, tb->cache)))
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup canonical SRCPATH: '%s'", cn));

	nents = mnt_table_get_nents(tb);

	/* canonicalized paths in struct libmnt_table */
	if (ntags < nents) {
		mnt_reset_iter(&itr, direction);
		while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
			if (mnt_fs_streq_srcpath(fs, cn))
				return fs;
		}
	}

	/* evaluated tag */
	if (ntags) {
		int rc = mnt_cache_read_tags(tb->cache, cn);

		mnt_reset_iter(&itr, direction);

		if (rc == 0) {
			/* @path's TAGs are in the cache */
			while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
				const char *t, *v;

				if (mnt_fs_get_tag(fs, &t, &v))
					continue;

				if (mnt_cache_device_has_tag(tb->cache, cn, t, v))
					return fs;
			}
		} else if (rc < 0 && errno == EACCES) {
			/* @path is inaccessible, try evaluating all TAGs in @tb
			 * by udev symlinks -- this could be expensive on systems
			 * with a huge fstab/mtab */
			 while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
				 const char *t, *v, *x;
				 if (mnt_fs_get_tag(fs, &t, &v))
					 continue;
				 x = mnt_resolve_tag(t, v, tb->cache);

				 /* both canonicalized, strcmp() is fine here */
				 if (x && strcmp(x, cn) == 0)
					 return fs;
			 }
		}
	}

	/* non-canonicalized paths in struct libmnt_table */
	if (ntags <= nents) {
		mnt_reset_iter(&itr, direction);
		while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
			if (mnt_fs_is_netfs(fs) || mnt_fs_is_pseudofs(fs))
				continue;
			p = mnt_fs_get_srcpath(fs);
			if (p)
				p = mnt_resolve_path(p, tb->cache);

			/* both canonicalized, strcmp() is fine here */
			if (p && strcmp(p, cn) == 0)
				return fs;
		}
	}

	return NULL;
}


/**
 * mnt_table_find_tag:
 * @tb: tab pointer
 * @tag: tag name (e.g "LABEL", "UUID", ...)
 * @val: tag value
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Try to lookup an entry in the given tab, the first attempt is to lookup by @tag and
 * @val, for the second attempt the tag is evaluated (converted to the device
 * name) and mnt_table_find_srcpath() is performed. The second attempt is not
 * performed when @tb cache is not set (see mnt_table_set_cache()).

 * Returns: a tab entry or NULL.
 */
struct libmnt_fs *mnt_table_find_tag(struct libmnt_table *tb, const char *tag,
			const char *val, int direction)
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs = NULL;

	if (!tb || !tag || !*tag || !val)
		return NULL;
	if (direction != MNT_ITER_FORWARD && direction != MNT_ITER_BACKWARD)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup by TAG: %s %s", tag, val));

	/* look up by TAG */
	mnt_reset_iter(&itr, direction);
	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
		if (fs->tagname && fs->tagval &&
		    strcmp(fs->tagname, tag) == 0 &&
		    strcmp(fs->tagval, val) == 0)
			return fs;
	}

	if (tb->cache) {
		/* look up by device name */
		char *cn = mnt_resolve_tag(tag, val, tb->cache);
		if (cn)
			return mnt_table_find_srcpath(tb, cn, direction);
	}
	return NULL;
}

/**
 * mnt_table_find_target_with_option:
 * @tb: tab pointer
 * @path: mountpoint directory
 * @option: option name (e.g "subvol", "subvolid", ...)
 * @val: option value or NULL
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Try to lookup an entry in the given tab that matches combination of @path
 * and @option. In difference to mnt_table_find_target(), only @path iteration
 * is done. No lookup by device name, no canonicalization.
 *
 * Returns: a tab entry or NULL.
 *
 * Since: 2.28
 */
struct libmnt_fs *mnt_table_find_target_with_option(
			struct libmnt_table *tb, const char *path,
			const char *option, const char *val, int direction)
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs = NULL;
	char *optval = NULL;
	size_t optvalsz = 0, valsz = val ? strlen(val) : 0;

	if (!tb || !path || !*path || !option || !*option || !val)
		return NULL;
	if (direction != MNT_ITER_FORWARD && direction != MNT_ITER_BACKWARD)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup TARGET: '%s' with OPTION %s %s", path, option, val));

	/* look up by native @target with OPTION */
	mnt_reset_iter(&itr, direction);
	while (mnt_table_next_fs(tb, &itr, &fs) == 0) {
		if (mnt_fs_streq_target(fs, path)
		    && mnt_fs_get_option(fs, option, &optval, &optvalsz) == 0
		    && (!val || (optvalsz == valsz
				 && strncmp(optval, val, optvalsz) == 0)))
			return fs;
	}
	return NULL;
}

/**
 * mnt_table_find_source:
 * @tb: tab pointer
 * @source: TAG or path
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * This is a high-level API for mnt_table_find_{srcpath,tag}. You needn't care
 * about the @source format (device, LABEL, UUID, ...). This function parses
 * the @source and calls mnt_table_find_tag() or mnt_table_find_srcpath().
 *
 * Returns: a tab entry or NULL.
 */
struct libmnt_fs *mnt_table_find_source(struct libmnt_table *tb,
					const char *source, int direction)
{
	struct libmnt_fs *fs;
	char *t = NULL, *v = NULL;

	if (!tb)
		return NULL;
	if (direction != MNT_ITER_FORWARD && direction != MNT_ITER_BACKWARD)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup SOURCE: '%s'", source));

	if (blkid_parse_tag_string(source, &t, &v) || !mnt_valid_tagname(t))
		fs = mnt_table_find_srcpath(tb, source, direction);
	else
		fs = mnt_table_find_tag(tb, t, v, direction);

	free(t);
	free(v);

	return fs;
}

/**
 * mnt_table_find_pair
 * @tb: tab pointer
 * @source: TAG or path
 * @target: mountpoint
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * This function is implemented by mnt_fs_match_source() and
 * mnt_fs_match_target() functions. It means that this is more expensive than
 * others mnt_table_find_* function, because every @tab entry is fully evaluated.
 *
 * Returns: a tab entry or NULL.
 */
struct libmnt_fs *mnt_table_find_pair(struct libmnt_table *tb, const char *source,
				      const char *target, int direction)
{
	struct libmnt_fs *fs = NULL;
	struct libmnt_iter itr;

	if (!tb || !target || !*target || !source || !*source)
		return NULL;
	if (direction != MNT_ITER_FORWARD && direction != MNT_ITER_BACKWARD)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup SOURCE: %s TARGET: %s", source, target));

	mnt_reset_iter(&itr, direction);
	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {

		if (mnt_fs_match_target(fs, target, tb->cache) &&
		    mnt_fs_match_source(fs, source, tb->cache))
			return fs;
	}

	return NULL;
}

/**
 * mnt_table_find_devno
 * @tb: /proc/self/mountinfo
 * @devno: device number
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Note that zero could be a valid device number for the root pseudo filesystem (e.g.
 * tmpfs).
 *
 * Returns: a tab entry or NULL.
 */
struct libmnt_fs *mnt_table_find_devno(struct libmnt_table *tb,
				       dev_t devno, int direction)
{
	struct libmnt_fs *fs = NULL;
	struct libmnt_iter itr;

	if (!tb)
		return NULL;
	if (direction != MNT_ITER_FORWARD && direction != MNT_ITER_BACKWARD)
		return NULL;

	DBG(TAB, ul_debugobj(tb, "lookup DEVNO: %d", (int) devno));

	mnt_reset_iter(&itr, direction);

	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
		if (mnt_fs_get_devno(fs) == devno)
			return fs;
	}

	return NULL;
}

static char *remove_mountpoint_from_path(const char *path, const char *mnt)
{
        char *res;
	const char *p;
	size_t sz;

	sz = strlen(mnt);
	p = sz > 1 ? path + sz : path;

	res = *p ? strdup(p) : strdup("/");
	DBG(UTILS, ul_debug("%s fs-root is %s", path, res));
	return res;
}

#ifdef HAVE_BTRFS_SUPPORT
static int get_btrfs_fs_root(struct libmnt_table *tb, struct libmnt_fs *fs, char **root)
{
	char *vol = NULL, *p;
	size_t sz, volsz = 0;

	DBG(BTRFS, ul_debug("lookup for btrfs FS root"));
	*root = NULL;

	if (mnt_fs_get_option(fs, "subvolid", &vol, &volsz) == 0) {
		char *target;
		struct libmnt_fs *f;
		char subvolidstr[sizeof(stringify_value(UINT64_MAX))];

		DBG(BTRFS, ul_debug(" found subvolid=%s, checking", vol));

		assert (volsz + 1 < sizeof(stringify_value(UINT64_MAX)));
		memcpy(subvolidstr, vol, volsz);
		subvolidstr[volsz] = '\0';

		target = mnt_resolve_target(mnt_fs_get_target(fs), tb->cache);
		if (!target)
			goto err;

		DBG(BTRFS, ul_debug(" trying target=%s subvolid=%s", target, subvolidstr));
		f = mnt_table_find_target_with_option(tb, target,
					"subvolid", subvolidstr,
					MNT_ITER_BACKWARD);
		if (!tb->cache)
			free(target);
		if (!f)
			goto not_found;

		/* Instead of set of BACKREF queries constructing subvol path
		 * corresponding to a particular subvolid, use the one in
		 * mountinfo. Kernel keeps subvol path up to date.
		 */
		if (mnt_fs_get_option(f, "subvol", &vol, &volsz) != 0)
			goto not_found;

	} else if (mnt_fs_get_option(fs, "subvol", &vol, &volsz) != 0) {
		/* If fstab entry does not contain "subvol", we have to
		 * check, whether btrfs has default subvolume defined.
		 */
		uint64_t default_id;
		char *target;
		struct libmnt_fs *f;
		char default_id_str[sizeof(stringify_value(UINT64_MAX))];

		DBG(BTRFS, ul_debug(" subvolid/subvol not found, checking default"));

		default_id = btrfs_get_default_subvol_id(mnt_fs_get_target(fs));
		if (default_id == UINT64_MAX)
			goto not_found;

		/* Volume has default subvolume. Check if it matches to
		 * the one in mountinfo.
		 *
		 * Only kernel >= 4.2 reports subvolid. On older
		 * kernels, there is no reasonable way to detect which
		 * subvolume was mounted.
		 */
		target = mnt_resolve_target(mnt_fs_get_target(fs), tb->cache);
		if (!target)
			goto err;

		snprintf(default_id_str, sizeof(default_id_str), "%llu",
				(unsigned long long int) default_id);

		DBG(BTRFS, ul_debug(" trying target=%s default subvolid=%s",
					target, default_id_str));

		f = mnt_table_find_target_with_option(tb, target,
					"subvolid", default_id_str,
					MNT_ITER_BACKWARD);
		if (!tb->cache)
			free(target);
		if (!f)
			goto not_found;

		/* Instead of set of BACKREF queries constructing
		 * subvol path, use the one in mountinfo. Kernel does
		 * the evaluation for us.
		 */
		DBG(BTRFS, ul_debug("setting FS root: btrfs default subvolid = %s",
					default_id_str));

		if (mnt_fs_get_option(f, "subvol", &vol, &volsz) != 0)
			goto not_found;
	}

	DBG(BTRFS, ul_debug(" using subvol=%s", vol));
	sz = volsz;
	if (*vol != '/')
		sz++;
	*root = malloc(sz + 1);
	if (!*root)
		goto err;
	p = *root;
	if (*vol != '/')
		*p++ = '/';
	memcpy(p, vol, volsz);
	*(*root + sz) = '\0';
	return 0;

not_found:
	DBG(BTRFS, ul_debug(" not found btrfs volume setting"));
	return 1;
err:
	DBG(BTRFS, ul_debug(" error on btrfs volume setting evaluation"));
	return errno ? -errno : -1;
}
#endif /* HAVE_BTRFS_SUPPORT */

static const char *get_cifs_unc_subdir_path (const char *unc)
{
	/*
	 *  1 or more slash:     %*[/]
	 *  1 or more non-slash: %*[^/]
	 *  number of byte read: %n
	 */
	int share_end = 0;
	int r = sscanf(unc, "%*[/]%*[^/]%*[/]%*[^/]%n", &share_end);
	if (r == EOF || share_end == 0)
		return NULL;
	return unc + share_end;
}

/*
 * tb: /proc/self/mountinfo
 * fs: filesystem
 * mountflags: MS_BIND or 0
 * fsroot: fs-root that will probably be used in the mountinfo file
 *          for @fs after mount(2)
 *
 * For btrfs subvolumes this function returns NULL, but @fsroot properly set.
 *
 * If @tb is NULL then defaults to '/'.
 *
 * Returns: entry from @tb that will be used as a source for @fs if the @fs is
 *          bindmount.
 *
 * Don't export to library API!
 */
struct libmnt_fs *mnt_table_get_fs_root(struct libmnt_table *tb,
					struct libmnt_fs *fs,
					unsigned long mountflags,
					char **fsroot)
{
	char *root = NULL;
	const char *mnt = NULL;
	struct libmnt_fs *src_fs = NULL;

	assert(fs);
	assert(fsroot);

	DBG(TAB, ul_debug("lookup fs-root for '%s'", mnt_fs_get_source(fs)));

	if (tb && (mountflags & MS_BIND)) {
		const char *src, *src_root;
		char *xsrc = NULL;

		DBG(TAB, ul_debug("fs-root for bind"));

		src = xsrc = mnt_resolve_spec(mnt_fs_get_source(fs), tb->cache);
		if (src) {
			struct libmnt_fs *f = mnt_table_find_mountpoint(tb,
							src, MNT_ITER_BACKWARD);
			if (f)
				mnt = mnt_fs_get_target(f);
		}
		if (mnt)
			root = remove_mountpoint_from_path(src, mnt);

		if (xsrc && !tb->cache) {
			free(xsrc);
			src = NULL;
		}
		if (!mnt)
			goto err;

		src_fs = mnt_table_find_target(tb, mnt, MNT_ITER_BACKWARD);
		if (!src_fs)  {
			DBG(TAB, ul_debug("not found '%s' in mountinfo -- using default", mnt));
			goto dflt;
		}

		/* It's possible that fstab_fs source is subdirectory on btrfs
		 * subvolume or another bind mount. For example:
		 *
		 * /dev/sdc        /mnt/test       btrfs   subvol=/anydir
		 * /dev/sdc        /mnt/test       btrfs   defaults
		 * /mnt/test/foo   /mnt/test2      auto    bind
		 *
		 * in this case, the root for /mnt/test2 will be /anydir/foo on
		 * /dev/sdc. It means we have to compose the final root from
		 * root and src_root.
		 */
		src_root = mnt_fs_get_root(src_fs);

		DBG(FS, ul_debugobj(fs, "source root: %s, source FS root: %s", root, src_root));

		if (src_root && root && !startswith(root, src_root)) {
			if (strcmp(root, "/") == 0) {
				free(root);
				root = strdup(src_root);
				if (!root)
					goto err;
			} else {
				char *tmp;
				if (asprintf(&tmp, "%s%s", src_root, root) < 0)
					goto err;
				free(root);
				root = tmp;
			}
		}
	}

#ifdef HAVE_BTRFS_SUPPORT
	/*
	 * btrfs-subvolume mount -- get subvolume name and use it as a root-fs path
	 */
	else if (tb && fs->fstype &&
		 (!strcmp(fs->fstype, "btrfs") || !strcmp(fs->fstype, "auto"))) {
		if (get_btrfs_fs_root(tb, fs, &root) < 0)
			goto err;
	}
#endif /* HAVE_BTRFS_SUPPORT */

dflt:
	if (!root) {
		root = strdup("/");
		if (!root)
			goto err;
	}
	*fsroot = root;

	DBG(TAB, ul_debug("FS root result: %s", root));

	return src_fs;
err:
	free(root);
	return NULL;
}


int __mnt_table_is_fs_mounted(struct libmnt_table *tb, struct libmnt_fs *fstab_fs,
			      const char *tgt_prefix)
{
	struct libmnt_iter itr;
	struct libmnt_fs *fs;

	char *root = NULL;
	char *src2 = NULL;
	const char *src = NULL, *tgt = NULL;
	char *xtgt = NULL, *tgt_buf = NULL;
	int rc = 0;
	dev_t devno = 0;

	DBG(FS, ul_debugobj(fstab_fs, "mnt_table_is_fs_mounted: target=%s, source=%s",
				mnt_fs_get_target(fstab_fs),
				mnt_fs_get_source(fstab_fs)));

	if (mnt_fs_is_swaparea(fstab_fs) || mnt_table_is_empty(tb)) {
		DBG(FS, ul_debugobj(fstab_fs, "- ignore (swap or no data)"));
		return 0;
	}

	if (is_mountinfo(tb)) {
		/* @tb is mountinfo, so we can try to use fs-roots */
		struct libmnt_fs *rootfs;
		int flags = 0;

		if (mnt_fs_get_option(fstab_fs, "bind", NULL, NULL) == 0 ||
		    mnt_fs_get_option(fstab_fs, "rbind", NULL, NULL) == 0)
			flags = MS_BIND;

		rootfs = mnt_table_get_fs_root(tb, fstab_fs, flags, &root);
		if (rootfs) {
			const char *fstype = mnt_fs_get_fstype(rootfs);

			src = mnt_fs_get_srcpath(rootfs);
			if (fstype && strncmp(fstype, "nfs", 3) == 0 && root) {
				/* NFS stores the root at the end of the source */
				src = src2 = strconcat(src, root);
				free(root);
				root = NULL;
			}
		}
	}

	if (!src)
		src = mnt_fs_get_source(fstab_fs);

	if (src && tb->cache && !mnt_fs_is_pseudofs(fstab_fs))
		src = mnt_resolve_spec(src, tb->cache);

	if (src && root) {
		struct stat st;

		devno = mnt_fs_get_devno(fstab_fs);
		if (!devno && mnt_safe_stat(src, &st) == 0 && S_ISBLK(st.st_mode))
			devno = st.st_rdev;
	}

	tgt = mnt_fs_get_target(fstab_fs);

	if (!tgt || !src) {
		DBG(FS, ul_debugobj(fstab_fs, "- ignore (no source/target)"));
		goto done;
	}
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	DBG(FS, ul_debugobj(fstab_fs, "mnt_table_is_fs_mounted: src=%s, tgt=%s, root=%s", src, tgt, root));

	while (mnt_table_next_fs(tb, &itr, &fs) == 0) {

		int eq = mnt_fs_streq_srcpath(fs, src);

		if (!eq && devno && mnt_fs_get_devno(fs) == devno)
			eq = 1;

		if (!eq) {
			/* The source does not match. Maybe the source is a loop
			 * device backing file.
			 */
			uint64_t offset = 0;
			char *val;
			size_t len;
			int flags = 0;

			if (!mnt_fs_get_srcpath(fs) ||
			    !startswith(mnt_fs_get_srcpath(fs), "/dev/loop"))
				continue;	/* does not look like loopdev */

			if (mnt_fs_get_option(fstab_fs, "offset", &val, &len) == 0) {
				if (mnt_parse_offset(val, len, &offset)) {
					DBG(FS, ul_debugobj(fstab_fs, "failed to parse offset="));
					continue;
				}
				flags = LOOPDEV_FL_OFFSET;
			}

			DBG(FS, ul_debugobj(fs, "checking for loop: src=%s", mnt_fs_get_srcpath(fs)));
#if __linux__
			if (!loopdev_is_used(mnt_fs_get_srcpath(fs), src, offset, 0, flags))
				continue;

			DBG(FS, ul_debugobj(fs, "used loop"));
#endif
		}

		if (root) {
			const char *fstype = mnt_fs_get_fstype(fs);

			if (fstype && (strcmp(fstype, "cifs") == 0 ||
				       strcmp(fstype, "smb3") == 0)) {

				const char *sub = get_cifs_unc_subdir_path(src);
				const char *r = mnt_fs_get_root(fs);

				if (!sub || !r || (!streq_paths(sub, r) &&
						   !streq_paths("/", r)))
					continue;
			} else {
				const char *r = mnt_fs_get_root(fs);
				if (!r || strcmp(r, root) != 0)
					continue;
			}
		}

		/*
		 * Compare target, try to minimize the number of situations when we
		 * need to canonicalize the path to avoid readlink() on
		 * mountpoints.
		 */
		if (!xtgt) {
			if (tgt_prefix) {
				const char *p = *tgt == '/' ? tgt + 1 : tgt;
				if (!*p)
					tgt = tgt_prefix;	/* target is '/' */
				else {
					if (asprintf(&tgt_buf, "%s/%s", tgt_prefix, p) <= 0) {
						rc = -ENOMEM;
						goto done;
					}
					tgt = tgt_buf;
				}
			}

			if (mnt_fs_streq_target(fs, tgt))
				break;
			if (tb->cache)
				xtgt = mnt_resolve_path(tgt, tb->cache);
		}
		if (xtgt && mnt_fs_streq_target(fs, xtgt))
			break;
	}

	if (fs)
		rc = 1;		/* success */
done:
	free(root);
	free(tgt_buf);

	DBG(TAB, ul_debugobj(tb, "mnt_table_is_fs_mounted: %s [rc=%d]", src, rc));
	free(src2);
	return rc;
}

/**
 * mnt_table_is_fs_mounted:
 * @tb: /proc/self/mountinfo file
 * @fstab_fs: /etc/fstab entry
 *
 * Checks if the @fstab_fs entry is already in the @tb table. The "swap" is
 * ignored. This function explicitly compares the source, target and root of the
 * filesystems.
 *
 * Note that source and target are canonicalized only if a cache for @tb is
 * defined (see mnt_table_set_cache()). The target canonicalization may
 * trigger automount on autofs mountpoints!
 *
 * Don't use it if you want to know if a device is mounted, just use
 * mnt_table_find_source() on the device.
 *
 * This function is designed mostly for "mount -a".
 *
 * Returns: 0 or 1
 */
int mnt_table_is_fs_mounted(struct libmnt_table *tb, struct libmnt_fs *fstab_fs)
{
	return __mnt_table_is_fs_mounted(tb, fstab_fs, NULL);
}


#ifdef TEST_PROGRAM
#include "pathnames.h"

static int parser_errcb(struct libmnt_table *tb __attribute__((unused)),
			const char *filename, int line)
{
	fprintf(stderr, "%s:%d: parse error\n", filename, line);

	return 1;	/* all errors are recoverable -- this is the default */
}

static struct libmnt_table *create_table(const char *file, int comments)
{
	struct libmnt_table *tb;

	if (!file)
		return NULL;
	tb = mnt_new_table();
	if (!tb)
		goto err;

	mnt_table_enable_comments(tb, comments);
	mnt_table_set_parser_errcb(tb, parser_errcb);

	if (mnt_table_parse_file(tb, file) != 0)
		goto err;
	return tb;
err:
	fprintf(stderr, "%s: parsing failed\n", file);
	mnt_unref_table(tb);
	return NULL;
}

static int test_copy_fs(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_table *tb;
	struct libmnt_fs *fs;
	int rc = -1;

	if (argc != 2)
		return -1;

	tb = create_table(argv[1], FALSE);
	if (!tb)
		return -1;

	fs = mnt_table_find_target(tb, "/", MNT_ITER_FORWARD);
	if (!fs)
		goto done;

	printf("ORIGINAL:\n");
	mnt_fs_print_debug(fs, stdout);

	fs = mnt_copy_fs(NULL, fs);
	if (!fs)
		goto done;

	printf("COPY:\n");
	mnt_fs_print_debug(fs, stdout);
	mnt_unref_fs(fs);
	rc = 0;
done:
	mnt_unref_table(tb);
	return rc;
}

static int test_parse(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	struct libmnt_table *tb = NULL;
	struct libmnt_iter *itr = NULL;
	struct libmnt_fs *fs;
	int rc = -1;
	int parse_comments = FALSE;

	if (argc == 3 && !strcmp(argv[2], "--comments"))
		parse_comments = TRUE;

	tb = create_table(argv[1], parse_comments);
	if (!tb)
		return -1;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		goto done;

	if (mnt_table_get_intro_comment(tb))
		fprintf(stdout, "Initial comment:\n\"%s\"\n",
				mnt_table_get_intro_comment(tb));

	while(mnt_table_next_fs(tb, itr, &fs) == 0)
		mnt_fs_print_debug(fs, stdout);

	if (mnt_table_get_trailing_comment(tb))
		fprintf(stdout, "Trailing comment:\n\"%s\"\n",
				mnt_table_get_trailing_comment(tb));
	rc = 0;
done:
	mnt_free_iter(itr);
	mnt_unref_table(tb);
	return rc;
}

static int test_find_idx(struct libmnt_test *ts __attribute__((unused)),
			 int argc, char *argv[])
{
	struct libmnt_table *tb;
	struct libmnt_fs *fs = NULL;
	struct libmnt_cache *mpc = NULL;
	const char *file, *what;
	int rc = -1;

	if (argc != 3) {
		fprintf(stderr, "try --help\n");
		return -EINVAL;
	}

	file = argv[1], what = argv[2];

	tb = create_table(file, FALSE);
	if (!tb)
		goto done;

	/* create a cache for canonicalized paths */
	mpc = mnt_new_cache();
	if (!mpc)
		goto done;
	mnt_table_set_cache(tb, mpc);
	mnt_unref_cache(mpc);

	fs = mnt_table_find_target(tb, what, MNT_ITER_BACKWARD);

	if (!fs)
		fprintf(stderr, "%s: not found '%s'\n", file, what);
	else {
		int idx = mnt_table_find_fs(tb, fs);

		if (idx < 1)
			fprintf(stderr, "%s: not found '%s' fs pointer", file, what);
		else {
			printf("%s index is %d\n", what, idx);
			rc = 0;
		}
	}
done:
	mnt_unref_table(tb);
	return rc;
}

static int test_find(struct libmnt_test *ts __attribute__((unused)),
		     int argc, char *argv[], int dr)
{
	struct libmnt_table *tb;
	struct libmnt_fs *fs = NULL;
	struct libmnt_cache *mpc = NULL;
	const char *file, *find, *what;
	int rc = -1;

	if (argc != 4) {
		fprintf(stderr, "try --help\n");
		return -EINVAL;
	}

	file = argv[1], find = argv[2], what = argv[3];

	tb = create_table(file, FALSE);
	if (!tb)
		goto done;

	/* create a cache for canonicalized paths */
	mpc = mnt_new_cache();
	if (!mpc)
		goto done;
	mnt_table_set_cache(tb, mpc);
	mnt_unref_cache(mpc);

	if (strcasecmp(find, "source") == 0)
		fs = mnt_table_find_source(tb, what, dr);
	else if (strcasecmp(find, "target") == 0)
		fs = mnt_table_find_target(tb, what, dr);

	if (!fs)
		fprintf(stderr, "%s: not found %s '%s'\n", file, find, what);
	else {
		mnt_fs_print_debug(fs, stdout);
		rc = 0;
	}
done:
	mnt_unref_table(tb);
	return rc;
}

static int test_find_bw(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	return test_find(ts, argc, argv, MNT_ITER_BACKWARD);
}

static int test_find_fw(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	return test_find(ts, argc, argv, MNT_ITER_FORWARD);
}

static int test_find_pair(struct libmnt_test *ts __attribute__((unused)),
			  int argc, char *argv[])
{
	struct libmnt_table *tb;
	struct libmnt_fs *fs;
	struct libmnt_cache *mpc = NULL;
	int rc = -1;

	if (argc != 4)
		return -1;

	tb = create_table(argv[1], FALSE);
	if (!tb)
		return -1;
	mpc = mnt_new_cache();
	if (!mpc)
		goto done;
	mnt_table_set_cache(tb, mpc);
	mnt_unref_cache(mpc);

	fs = mnt_table_find_pair(tb, argv[2], argv[3], MNT_ITER_FORWARD);
	if (!fs)
		goto done;

	mnt_fs_print_debug(fs, stdout);
	rc = 0;
done:
	mnt_unref_table(tb);
	return rc;
}

static int test_find_mountpoint(struct libmnt_test *ts __attribute__((unused)),
				int argc, char *argv[])
{
	struct libmnt_table *tb;
	struct libmnt_fs *fs;
	struct libmnt_cache *mpc = NULL;
	int rc = -1;

	if (argc != 2)
		return -1;

	tb = mnt_new_table_from_file(_PATH_PROC_MOUNTINFO);
	if (!tb)
		return -1;
	mpc = mnt_new_cache();
	if (!mpc)
		goto done;
	mnt_table_set_cache(tb, mpc);
	mnt_unref_cache(mpc);

	fs = mnt_table_find_mountpoint(tb, argv[1], MNT_ITER_BACKWARD);
	if (!fs)
		goto done;

	mnt_fs_print_debug(fs, stdout);
	rc = 0;
done:
	mnt_unref_table(tb);
	return rc;
}

static int test_is_mounted(struct libmnt_test *ts __attribute__((unused)),
			   int argc, char *argv[])
{
	struct libmnt_table *tb = NULL, *fstab = NULL;
	struct libmnt_fs *fs;
	struct libmnt_iter *itr = NULL;
	struct libmnt_cache *mpc = NULL;

	if (argc != 2)
		return -1;

	tb = mnt_new_table_from_file("/proc/self/mountinfo");
	if (!tb) {
		fprintf(stderr, "failed to parse mountinfo\n");
		return -1;
	}

	fstab = create_table(argv[1], FALSE);
	if (!fstab)
		goto done;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		goto done;

	mpc = mnt_new_cache();
	if (!mpc)
		goto done;
	mnt_table_set_cache(tb, mpc);
	mnt_unref_cache(mpc);

	while (mnt_table_next_fs(fstab, itr, &fs) == 0) {
		if (mnt_table_is_fs_mounted(tb, fs))
			printf("%s already mounted on %s\n",
					mnt_fs_get_source(fs),
					mnt_fs_get_target(fs));
		else
			printf("%s not mounted on %s\n",
					mnt_fs_get_source(fs),
					mnt_fs_get_target(fs));
	}

done:
	mnt_unref_table(tb);
	mnt_unref_table(fstab);
	mnt_free_iter(itr);
	return 0;
}

/* returns 0 if @a and @b targets are the same */
static int test_uniq_cmp(struct libmnt_table *tb __attribute__((__unused__)),
			 struct libmnt_fs *a,
			 struct libmnt_fs *b)
{
	assert(a);
	assert(b);

	return mnt_fs_streq_target(a, mnt_fs_get_target(b)) ? 0 : 1;
}

static int test_uniq(struct libmnt_test *ts __attribute__((unused)),
		     int argc, char *argv[])
{
	struct libmnt_table *tb;
	int rc = -1;

	if (argc != 2) {
		fprintf(stderr, "try --help\n");
		return -EINVAL;
	}

	tb = create_table(argv[1], FALSE);
	if (!tb)
		goto done;

	if (mnt_table_uniq_fs(tb, 0, test_uniq_cmp) == 0) {
		struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_FORWARD);
		struct libmnt_fs *fs;
		if (!itr)
			goto done;
		while (mnt_table_next_fs(tb, itr, &fs) == 0)
			mnt_fs_print_debug(fs, stdout);
		mnt_free_iter(itr);
		rc = 0;
	}
done:
	mnt_unref_table(tb);
	return rc;
}


int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
	{ "--parse",    test_parse,        "<file> [--comments] parse and print tab" },
	{ "--find-forward",  test_find_fw, "<file> <source|target> <string>" },
	{ "--find-backward", test_find_bw, "<file> <source|target> <string>" },
	{ "--uniq-target",   test_uniq,    "<file>" },
	{ "--find-pair",     test_find_pair, "<file> <source> <target>" },
	{ "--find-fs",       test_find_idx, "<file> <target>" },
	{ "--find-mountpoint", test_find_mountpoint, "<path>" },
	{ "--copy-fs",       test_copy_fs, "<file>  copy root FS from the file" },
	{ "--is-mounted",    test_is_mounted, "<fstab> check what from fstab is already mounted" },
	{ NULL }
	};

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
