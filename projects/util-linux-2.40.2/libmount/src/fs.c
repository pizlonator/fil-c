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
 * SECTION: fs
 * @title: Filesystem
 * @short_description: represents one entry from fstab, or mountinfo file
 *
 */
#include <ctype.h>
#include <blkid.h>
#include <stddef.h>

#include "mountP.h"
#include "strutils.h"

/**
 * mnt_new_fs:
 *
 * The initial refcount is 1, and needs to be decremented to
 * release the resources of the filesystem.
 *
 * Returns: newly allocated struct libmnt_fs.
 */
struct libmnt_fs *mnt_new_fs(void)
{
	struct libmnt_fs *fs = calloc(1, sizeof(*fs));

	if (!fs)
		return NULL;

	fs->refcount = 1;
	INIT_LIST_HEAD(&fs->ents);
	DBG(FS, ul_debugobj(fs, "alloc"));
	return fs;
}

/**
 * mnt_free_fs:
 * @fs: fs pointer
 *
 * Deallocates the fs. This function does not care about reference count. Don't
 * use this function directly -- it's better to use mnt_unref_fs().
 *
 * The reference counting is supported since util-linux v2.24.
 */
void mnt_free_fs(struct libmnt_fs *fs)
{
	if (!fs)
		return;

	DBG(FS, ul_debugobj(fs, "free [refcount=%d]", fs->refcount));

	mnt_reset_fs(fs);
	free(fs);
}

/**
 * mnt_reset_fs:
 * @fs: fs pointer
 *
 * Resets (zeroize) @fs.
 */
void mnt_reset_fs(struct libmnt_fs *fs)
{
	int ref;

	if (!fs)
		return;

	ref = fs->refcount;

	list_del(&fs->ents);
	free(fs->source);
	free(fs->bindsrc);
	free(fs->tagname);
	free(fs->tagval);
	free(fs->root);
	free(fs->swaptype);
	free(fs->target);
	free(fs->fstype);
	free(fs->optstr);
	free(fs->vfs_optstr);
	free(fs->fs_optstr);
	free(fs->user_optstr);
	free(fs->attrs);
	free(fs->opt_fields);
	free(fs->comment);

	mnt_unref_optlist(fs->optlist);
	fs->optlist = NULL;

	fs->opts_age = 0;

	memset(fs, 0, sizeof(*fs));
	INIT_LIST_HEAD(&fs->ents);
	fs->refcount = ref;
}

/**
 * mnt_ref_fs:
 * @fs: fs pointer
 *
 * Increments reference counter.
 */
void mnt_ref_fs(struct libmnt_fs *fs)
{
	if (fs) {
		fs->refcount++;
		/*DBG(FS, ul_debugobj(fs, "ref=%d", fs->refcount));*/
	}
}

/**
 * mnt_unref_fs:
 * @fs: fs pointer
 *
 * De-increments reference counter, on zero the @fs is automatically
 * deallocated by mnt_free_fs().
 */
void mnt_unref_fs(struct libmnt_fs *fs)
{
	if (fs) {
		fs->refcount--;
		/*DBG(FS, ul_debugobj(fs, "unref=%d", fs->refcount));*/
		if (fs->refcount <= 0)
			mnt_free_fs(fs);
	}
}

static inline int update_str(char **dest, const char *src)
{
	size_t sz;
	char *x;

	assert(dest);

	if (!src) {
		free(*dest);
		*dest = NULL;
		return 0;	/* source (old) is empty */
	}

	sz = strlen(src) + 1;
	x = realloc(*dest, sz);
	if (!x)
		return -ENOMEM;
	*dest = x;
	memcpy(*dest, src, sz);
	return 0;
}

/* This function does NOT overwrite (replace) the string in @new, the string in
 * @new has to be NULL otherwise this is no-op. */
static inline int cpy_str_at_offset(void *new, const void *old, size_t offset)
{
	char **o = (char **) ((char *) old + offset);
	char **n = (char **) ((char *) new + offset);

	if (*n)
		return 0;	/* already set, don't overwrite */

	return update_str(n, *o);
}

static inline int sync_opts_from_optlist(struct libmnt_fs *fs, struct libmnt_optlist *ol)
{
	unsigned int age;

	assert(fs);
	assert(ol);

	age = mnt_optlist_get_age(ol);
	if (age != fs->opts_age) {
		const char *p;
		int rc;

		/* All options */
		rc = mnt_optlist_get_optstr(ol, &p, NULL, 0);
		if (!rc)
			rc = strdup_to_struct_member(fs, optstr, p);

		/* FS options */
		if (!rc)
			rc = mnt_optlist_get_optstr(ol, &p, NULL, MNT_OL_FLTR_UNKNOWN);
		if (!rc)
			rc = strdup_to_struct_member(fs, fs_optstr, p);

		/* VFS options */
		if (!rc)
			rc = mnt_optlist_get_optstr(ol, &p, mnt_get_builtin_optmap(MNT_LINUX_MAP), 0);
		if (!rc)
			rc = strdup_to_struct_member(fs, vfs_optstr, p);

		/* Userspace options */
		if (!rc)
			rc = mnt_optlist_get_optstr(ol, &p, mnt_get_builtin_optmap(MNT_USERSPACE_MAP), 0);
		if (!rc)
			rc = strdup_to_struct_member(fs, user_optstr, p);

		if (rc) {
			DBG(FS, ul_debugobj(fs, "sync failed [rc=%d]", rc));
			return rc;
		} else {
			DBG(FS, ul_debugobj(fs, "synced: "
				"vfs: '%s' fs: '%s' user: '%s', optstr: '%s'",
				fs->vfs_optstr, fs->fs_optstr, fs->user_optstr, fs->optstr));
			fs->opts_age = age;
		}
	}
	return 0;
}

/* If @optlist is not NULL then @fs will read all option strings from @optlist.
 * It means that mnt_fs_get_*_options() won't be read-only operations. */
int mnt_fs_follow_optlist(struct libmnt_fs *fs, struct libmnt_optlist *ol)
{
	assert(fs);

	if (fs->optlist == ol)
		return 0;
	if (fs->optlist)
		mnt_unref_optlist(fs->optlist);

	fs->opts_age = 0;
	fs->optlist = ol;

	if (ol)
		mnt_ref_optlist(ol);
	return 0;
}

/**
 * mnt_copy_fs:
 * @dest: destination FS
 * @src: source FS
 *
 * If @dest is NULL, then a new FS is allocated, if any @dest field is already
 * set, then the field is NOT overwritten.
 *
 * This function does not copy userdata (se mnt_fs_set_userdata()). A new copy is
 * not linked with any existing mnt_tab or optlist.
 *
 * Returns: @dest or NULL in case of error
 */
struct libmnt_fs *mnt_copy_fs(struct libmnt_fs *dest,
			      const struct libmnt_fs *src)
{
	const struct libmnt_fs *org = dest;

	if (!src)
		return NULL;
	if (!dest) {
		dest = mnt_new_fs();
		if (!dest)
			return NULL;

		dest->tab	 = NULL;
	}

	dest->id         = src->id;
	dest->parent     = src->parent;
	dest->devno      = src->devno;
	dest->tid        = src->tid;

	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, source)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, tagname)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, tagval)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, root)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, swaptype)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, target)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, fstype)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, optstr)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, vfs_optstr)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, fs_optstr)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, user_optstr)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, attrs)))
		goto err;
	if (cpy_str_at_offset(dest, src, offsetof(struct libmnt_fs, bindsrc)))
		goto err;

	dest->freq       = src->freq;
	dest->passno     = src->passno;
	dest->flags      = src->flags;
	dest->size	 = src->size;
	dest->usedsize   = src->usedsize;
	dest->priority   = src->priority;

	return dest;
err:
	if (!org)
		mnt_free_fs(dest);
	return NULL;
}

/*
 * This function copies all @fs description except information that does not
 * belong to /etc/mtab (e.g. VFS and userspace mount options with MNT_NOMTAB
 * mask).
 *
 * Returns: copy of @fs.
 */
struct libmnt_fs *mnt_copy_mtab_fs(struct libmnt_fs *fs)
{
	struct libmnt_fs *n = mnt_new_fs();

	assert(fs);
	if (!n)
		return NULL;
	if (fs->optlist)
		sync_opts_from_optlist(fs, fs->optlist);

	if (strdup_between_structs(n, fs, source))
		goto err;
	if (strdup_between_structs(n, fs, target))
		goto err;
	if (strdup_between_structs(n, fs, fstype))
		goto err;

	if (fs->vfs_optstr) {
		char *p = NULL;
		mnt_optstr_get_options(fs->vfs_optstr, &p,
				mnt_get_builtin_optmap(MNT_LINUX_MAP),
				MNT_NOMTAB);
		n->vfs_optstr = p;
	}

	if (fs->user_optstr) {
		char *p = NULL;
		mnt_optstr_get_options(fs->user_optstr, &p,
				mnt_get_builtin_optmap(MNT_USERSPACE_MAP),
				MNT_NOMTAB);
		n->user_optstr = p;
	}

	if (strdup_between_structs(n, fs, fs_optstr))
		goto err;

	/* we cannot copy original optstr, the new optstr has to be without
	 * non-mtab options -- so, let's generate a new string */
	n->optstr = mnt_fs_strdup_options(n);

	n->freq       = fs->freq;
	n->passno     = fs->passno;
	n->flags      = fs->flags;

	return n;
err:
	mnt_free_fs(n);
	return NULL;

}

/**
 * mnt_fs_get_userdata:
 * @fs: struct libmnt_file instance
 *
 * Returns: private data set by mnt_fs_set_userdata() or NULL.
 */
void *mnt_fs_get_userdata(struct libmnt_fs *fs)
{
	if (!fs)
		return NULL;
	return fs->userdata;
}

/**
 * mnt_fs_set_userdata:
 * @fs: struct libmnt_file instance
 * @data: user data
 *
 * The "userdata" are library independent data.
 *
 * Returns: 0 or negative number in case of error (if @fs is NULL).
 */
int mnt_fs_set_userdata(struct libmnt_fs *fs, void *data)
{
	if (!fs)
		return -EINVAL;
	fs->userdata = data;
	return 0;
}

/**
 * mnt_fs_get_srcpath:
 * @fs: struct libmnt_file (fstab/mtab/mountinfo) fs
 *
 * The mount "source path" is:
 * - a directory for 'bind' mounts (in fstab or mtab only)
 * - a device name for standard mounts
 *
 * See also mnt_fs_get_tag() and mnt_fs_get_source().
 *
 * Returns: mount source path or NULL in case of error or when the path
 * is not defined.
 */
const char *mnt_fs_get_srcpath(struct libmnt_fs *fs)
{
	if (!fs)
		return NULL;

	/* fstab-like fs */
	if (fs->tagname)
		return NULL;	/* the source contains a "NAME=value" */
	return fs->source;
}

/**
 * mnt_fs_get_source:
 * @fs: struct libmnt_file (fstab/mtab/mountinfo) fs
 *
 * Returns: mount source. Note that the source could be unparsed TAG
 * (LABEL/UUID). See also mnt_fs_get_srcpath() and mnt_fs_get_tag().
 */
const char *mnt_fs_get_source(struct libmnt_fs *fs)
{
	return fs ? fs->source : NULL;
}

/*
 * Used by the parser ONLY (@source has to be freed on error)
 */
int __mnt_fs_set_source_ptr(struct libmnt_fs *fs, char *source)
{
	char *t = NULL, *v = NULL;

	assert(fs);

	if (source && blkid_parse_tag_string(source, &t, &v) == 0 &&
	    !mnt_valid_tagname(t)) {
		/* parsable but unknown tag -- ignore */
		free(t);
		free(v);
		t = v = NULL;
	}

	if (fs->source != source)
		free(fs->source);

	free(fs->tagname);
	free(fs->tagval);

	fs->source = source;
	fs->tagname = t;
	fs->tagval = v;
	return 0;
}

/**
 * mnt_fs_set_source:
 * @fs: fstab/mtab/mountinfo entry
 * @source: new source
 *
 * This function creates a private copy (strdup()) of @source.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_source(struct libmnt_fs *fs, const char *source)
{
	char *p = NULL;
	int rc;

	if (!fs)
		return -EINVAL;

	if (source) {
		p = strdup(source);
		if (!p)
			return -ENOMEM;
	}

	rc = __mnt_fs_set_source_ptr(fs, p);
	if (rc)
		free(p);
	return rc;
}

/**
 * mnt_fs_streq_srcpath:
 * @fs: fs
 * @path: source path
 *
 * Compares @fs source path with @path. The redundant slashes are ignored.
 * This function compares strings and does not canonicalize the paths.
 * See also more heavy and generic mnt_fs_match_source().
 *
 * Returns: 1 if @fs source path equal to @path, otherwise 0.
 */
int mnt_fs_streq_srcpath(struct libmnt_fs *fs, const char *path)
{
	const char *p;

	if (!fs)
		return 0;

	p = mnt_fs_get_srcpath(fs);

	if (!mnt_fs_is_pseudofs(fs))
		return streq_paths(p, path);

	if (!p && !path)
		return 1;

	return p && path && strcmp(p, path) == 0;
}

/**
 * mnt_fs_get_table:
 * @fs: table entry
 * @tb: table that contains @fs
 *
 * Returns: 0 or negative number on error (if @fs or @tb is NULL).
 *
 * Since: 2.34
 */
int mnt_fs_get_table(struct libmnt_fs *fs, struct libmnt_table **tb)
{
	if (!fs || !tb)
		return -EINVAL;

	*tb = fs->tab;
	return 0;
}

/**
 * mnt_fs_streq_target:
 * @fs: fs
 * @path: mount point
 *
 * Compares @fs target path with @path. The redundant slashes are ignored.
 * This function compares strings and does not canonicalize the paths.
 * See also more generic mnt_fs_match_target().
 *
 * Returns: 1 if @fs target path equal to @path, otherwise 0.
 */
int mnt_fs_streq_target(struct libmnt_fs *fs, const char *path)
{
	return fs && streq_paths(mnt_fs_get_target(fs), path);
}

/**
 * mnt_fs_get_tag:
 * @fs: fs
 * @name: returns pointer to NAME string
 * @value: returns pointer to VALUE string
 *
 * "TAG" is NAME=VALUE (e.g. LABEL=foo)
 *
 * The TAG is the first column in the fstab file. The TAG or "srcpath" always has
 * to be set for all entries.
 *
 * See also mnt_fs_get_source().
 *
 * <informalexample>
 *   <programlisting>
 *	char *src;
 *	struct libmnt_fs *fs = mnt_table_find_target(tb, "/home", MNT_ITER_FORWARD);
 *
 *	if (!fs)
 *		goto err;
 *
 *	src = mnt_fs_get_srcpath(fs);
 *	if (!src) {
 *		char *tag, *val;
 *		if (mnt_fs_get_tag(fs, &tag, &val) == 0)
 *			printf("%s: %s\n", tag, val);	// LABEL or UUID
 *	} else
 *		printf("device: %s\n", src);		// device or bind path
 *   </programlisting>
 * </informalexample>
 *
 * Returns: 0 on success or negative number in case a TAG is not defined.
 */
int mnt_fs_get_tag(struct libmnt_fs *fs, const char **name, const char **value)
{
	if (fs == NULL || !fs->tagname)
		return -EINVAL;
	if (name)
		*name = fs->tagname;
	if (value)
		*value = fs->tagval;
	return 0;
}

/**
 * mnt_fs_get_target:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: pointer to mountpoint path or NULL
 */
const char *mnt_fs_get_target(struct libmnt_fs *fs)
{
	return fs ? fs->target : NULL;
}

/**
 * mnt_fs_set_target:
 * @fs: fstab/mtab/mountinfo entry
 * @tgt: mountpoint
 *
 * This function creates a private copy (strdup()) of @tgt.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_target(struct libmnt_fs *fs, const char *tgt)
{
	return strdup_to_struct_member(fs, target, tgt);
}

int __mnt_fs_set_target_ptr(struct libmnt_fs *fs, char *tgt)
{
	assert(fs);

	free(fs->target);
	fs->target = tgt;
	return 0;
}

static int mnt_fs_get_flags(struct libmnt_fs *fs)
{
	return fs ? fs->flags : 0;
}

/**
 * mnt_fs_get_propagation:
 * @fs: mountinfo entry
 * @flags: returns propagation MS_* flags as present in the mountinfo file
 *
 * Note that this function sets @flags to zero if no propagation flags are found
 * in the mountinfo file. The kernel default is MS_PRIVATE, this flag is not stored
 * in the mountinfo file.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_get_propagation(struct libmnt_fs *fs, unsigned long *flags)
{
	if (!fs || !flags)
		return -EINVAL;

	*flags = 0;

	if (!fs->opt_fields)
		return 0;

	 /*
	 * The optional fields format is incompatible with mount options
	 * ... we have to parse the field here.
	 */
	*flags |= strstr(fs->opt_fields, "shared:") ? MS_SHARED : MS_PRIVATE;

	if (strstr(fs->opt_fields, "master:"))
		*flags |= MS_SLAVE;
	if (strstr(fs->opt_fields, "unbindable"))
		*flags |= MS_UNBINDABLE;

	return 0;
}

/**
 * mnt_fs_is_kernel:
 * @fs: filesystem
 *
 * Returns: 1 if the filesystem description is read from kernel e.g. /proc/mounts.
 */
int mnt_fs_is_kernel(struct libmnt_fs *fs)
{
	return mnt_fs_get_flags(fs) & MNT_FS_KERNEL ? 1 : 0;
}

/**
 * mnt_fs_is_swaparea:
 * @fs: filesystem
 *
 * Returns: 1 if the filesystem uses "swap" as a type
 */
int mnt_fs_is_swaparea(struct libmnt_fs *fs)
{
	return mnt_fs_get_flags(fs) & MNT_FS_SWAP ? 1 : 0;
}

/**
 * mnt_fs_is_pseudofs:
 * @fs: filesystem
 *
 * Returns: 1 if the filesystem is a pseudo fs type (proc, cgroups)
 */
int mnt_fs_is_pseudofs(struct libmnt_fs *fs)
{
	return mnt_fs_get_flags(fs) & MNT_FS_PSEUDO ? 1 : 0;
}

/**
 * mnt_fs_is_netfs:
 * @fs: filesystem
 *
 * Returns: 1 if the filesystem is a network filesystem
 */
int mnt_fs_is_netfs(struct libmnt_fs *fs)
{
	return mnt_fs_get_flags(fs) & MNT_FS_NET ? 1 : 0;
}

/**
 * mnt_fs_is_regularfs:
 * @fs: filesystem
 *
 * Returns: 1 if the filesystem is a regular filesystem (not network or pseudo filesystem).
 *
 * Since: 2.38
 */
int mnt_fs_is_regularfs(struct libmnt_fs *fs)
{
	return !(mnt_fs_is_pseudofs(fs)
		 || mnt_fs_is_netfs(fs)
		 || mnt_fs_is_swaparea(fs));
}

/**
 * mnt_fs_get_fstype:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: pointer to filesystem type.
 */
const char *mnt_fs_get_fstype(struct libmnt_fs *fs)
{
	return fs ? fs->fstype : NULL;
}

/* Used by the struct libmnt_file parser only */
int __mnt_fs_set_fstype_ptr(struct libmnt_fs *fs, char *fstype)
{
	assert(fs);

	if (fstype != fs->fstype)
		free(fs->fstype);

	fs->fstype = fstype;
	fs->flags &= ~MNT_FS_PSEUDO;
	fs->flags &= ~MNT_FS_NET;
	fs->flags &= ~MNT_FS_SWAP;

	/* save info about pseudo filesystems */
	if (fs->fstype) {
		if (mnt_fstype_is_pseudofs(fs->fstype))
			fs->flags |= MNT_FS_PSEUDO;
		else if (mnt_fstype_is_netfs(fs->fstype))
			fs->flags |= MNT_FS_NET;
		else if (!strcmp(fs->fstype, "swap"))
			fs->flags |= MNT_FS_SWAP;
	}
	return 0;
}

/**
 * mnt_fs_set_fstype:
 * @fs: fstab/mtab/mountinfo entry
 * @fstype: filesystem type
 *
 * This function creates a private copy (strdup()) of @fstype.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_fstype(struct libmnt_fs *fs, const char *fstype)
{
	char *p = NULL;

	if (!fs)
		return -EINVAL;
	if (fstype) {
		p = strdup(fstype);
		if (!p)
			return -ENOMEM;
	}
	return  __mnt_fs_set_fstype_ptr(fs, p);
}

/*
 * Merges @vfs and @fs options strings into a new string.
 * This function cares about 'ro/rw' options. The 'ro' is
 * always used if @vfs or @fs is read-only.
 * For example:
 *
 *    mnt_merge_optstr("rw,noexec", "ro,journal=update")
 *
 *           returns: "ro,noexec,journal=update"
 *
 *    mnt_merge_optstr("rw,noexec", "rw,journal=update")
 *
 *           returns: "rw,noexec,journal=update"
 */
static char *merge_optstr(const char *vfs, const char *fs)
{
	char *res, *p;
	size_t sz;
	int ro = 0, rw = 0;

	if (!vfs && !fs)
		return NULL;
	if (!vfs || !fs)
		return strdup(fs ? fs : vfs);
	if (!strcmp(vfs, fs))
		return strdup(vfs);		/* e.g. "aaa" and "aaa" */

	/* leave space for the leading "r[ow],", "," and the trailing zero */
	sz = strlen(vfs) + strlen(fs) + 5;
	res = malloc(sz);
	if (!res)
		return NULL;
	p = res + 3;			/* make a room for rw/ro flag */

	snprintf(p, sz - 3, "%s,%s", vfs, fs);

	/* remove 'rw' flags */
	rw += !mnt_optstr_remove_option(&p, "rw");	/* from vfs */
	rw += !mnt_optstr_remove_option(&p, "rw");	/* from fs */

	/* remove 'ro' flags if necessary */
	if (rw != 2) {
		ro += !mnt_optstr_remove_option(&p, "ro");
		if (ro + rw < 2)
			ro += !mnt_optstr_remove_option(&p, "ro");
	}

	if (!strlen(p))
		memcpy(res, ro ? "ro" : "rw", 3);
	else
		memcpy(res, ro ? "ro," : "rw,", 3);
	return res;
}

/**
 * mnt_fs_strdup_options:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Merges all mount options (VFS, FS and userspace) to one options string
 * and returns the result. This function does not modify @fs.
 *
 * Returns: pointer to string (can be freed by free(3)) or NULL in case of error.
 */
char *mnt_fs_strdup_options(struct libmnt_fs *fs)
{
	char *res;

	if (!fs)
		return NULL;
	if (fs->optlist)
		sync_opts_from_optlist(fs, fs->optlist);

	errno = 0;
	if (fs->optstr)
		return strdup(fs->optstr);

	res = merge_optstr(fs->vfs_optstr, fs->fs_optstr);
	if (!res && errno)
		return NULL;
	if (fs->user_optstr &&
	    mnt_optstr_append_option(&res, fs->user_optstr, NULL)) {
		free(res);
		res = NULL;
	}
	return res;
}

/**
 * mnt_fs_get_options:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: pointer to string or NULL in case of error.
 */
const char *mnt_fs_get_options(struct libmnt_fs *fs)
{
	if (fs && fs->optlist)
		sync_opts_from_optlist(fs, fs->optlist);

	return fs ? fs->optstr : NULL;
}

/**
 * mnt_fs_get_optional_fields
 * @fs: mountinfo entry pointer
 *
 * Returns: pointer to string with mountinfo optional fields
 *          or NULL in case of error.
 */
const char *mnt_fs_get_optional_fields(struct libmnt_fs *fs)
{
	return fs ? fs->opt_fields : NULL;
}

/**
 * mnt_fs_set_options:
 * @fs: fstab/mtab/mountinfo entry pointer
 * @optstr: options string
 *
 * Splits @optstr to VFS, FS and userspace mount options and updates relevant
 * parts of @fs.
 *
 * Returns: 0 on success, or negative number in case of error.
 */
int mnt_fs_set_options(struct libmnt_fs *fs, const char *optstr)
{
	char *v = NULL, *f = NULL, *u = NULL, *n = NULL;

	if (!fs)
		return -EINVAL;

	if (fs->optlist) {
		fs->opts_age = 0;
		return mnt_optlist_set_optstr(fs->optlist, optstr, NULL);
	}

	if (optstr) {
		int rc = mnt_split_optstr(optstr, &u, &v, &f, 0, 0);
		if (rc)
			return rc;
		n = strdup(optstr);
		if (!n) {
			free(u);
			free(v);
			free(f);
			return -ENOMEM;
		}
	}

	free(fs->fs_optstr);
	free(fs->vfs_optstr);
	free(fs->user_optstr);
	free(fs->optstr);

	fs->fs_optstr = f;
	fs->vfs_optstr = v;
	fs->user_optstr = u;
	fs->optstr = n;

	return 0;
}

/**
 * mnt_fs_append_options:
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: mount options
 *
 * Parses (splits) @optstr and appends results to VFS, FS and userspace lists
 * of options.
 *
 * If @optstr is NULL, then @fs is not modified and 0 is returned.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_append_options(struct libmnt_fs *fs, const char *optstr)
{
	char *v = NULL, *f = NULL, *u = NULL;
	int rc;

	if (!fs)
		return -EINVAL;
	if (!optstr)
		return 0;
	if (fs->optlist) {
		fs->opts_age = 0;
		return mnt_optlist_append_optstr(fs->optlist, optstr, NULL);
	}

	rc = mnt_split_optstr(optstr, &u, &v, &f, 0, 0);
	if (rc)
		return rc;

	if (!rc && v)
		rc = mnt_optstr_append_option(&fs->vfs_optstr, v, NULL);
	if (!rc && f)
		rc = mnt_optstr_append_option(&fs->fs_optstr, f, NULL);
	if (!rc && u)
		rc = mnt_optstr_append_option(&fs->user_optstr, u, NULL);
	if (!rc)
		rc = mnt_optstr_append_option(&fs->optstr, optstr, NULL);

	free(v);
	free(f);
	free(u);

	return rc;
}

/**
 * mnt_fs_prepend_options:
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: mount options
 *
 * Parses (splits) @optstr and prepends the results to VFS, FS and userspace lists
 * of options.
 *
 * If @optstr is NULL, then @fs is not modified and 0 is returned.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_prepend_options(struct libmnt_fs *fs, const char *optstr)
{
	char *v = NULL, *f = NULL, *u = NULL;
	int rc;

	if (!fs)
		return -EINVAL;
	if (!optstr)
		return 0;

	if (fs->optlist) {
		fs->opts_age = 0;
		return mnt_optlist_prepend_optstr(fs->optlist, optstr, NULL);
	}

	rc = mnt_split_optstr(optstr, &u, &v, &f, 0, 0);
	if (rc)
		return rc;

	if (!rc && v)
		rc = mnt_optstr_prepend_option(&fs->vfs_optstr, v, NULL);
	if (!rc && f)
		rc = mnt_optstr_prepend_option(&fs->fs_optstr, f, NULL);
	if (!rc && u)
		rc = mnt_optstr_prepend_option(&fs->user_optstr, u, NULL);
	if (!rc)
		rc = mnt_optstr_prepend_option(&fs->optstr, optstr, NULL);

	free(v);
	free(f);
	free(u);

	return rc;
}


/**
 * mnt_fs_get_fs_options:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: pointer to superblock (fs-depend) mount option string or NULL.
 */
const char *mnt_fs_get_fs_options(struct libmnt_fs *fs)
{
	if (!fs)
		return NULL;
	if (fs->optlist)
		sync_opts_from_optlist(fs, fs->optlist);

	return fs->fs_optstr;
}

/**
 * mnt_fs_get_vfs_options:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: pointer to fs-independent (VFS) mount option string or NULL.
 */
const char *mnt_fs_get_vfs_options(struct libmnt_fs *fs)
{
	if (!fs)
		return NULL;
	if (fs->optlist)
		sync_opts_from_optlist(fs, fs->optlist);

	return fs->vfs_optstr;
}

/**
 * mnt_fs_get_vfs_options_all:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: pointer to newlly allocated string (can be freed by free(3)) or
 * NULL in case of error.  The string contains all (including defaults) mount
 * options.
 */
char *mnt_fs_get_vfs_options_all(struct libmnt_fs *fs)
{
	const struct libmnt_optmap *map = mnt_get_builtin_optmap(MNT_LINUX_MAP);
	const struct libmnt_optmap *ent;
	const char *opts = mnt_fs_get_options(fs);
	char *result = NULL;
	unsigned long flags = 0;

	if (!opts || mnt_optstr_get_flags(opts, &flags, map))
		return NULL;

	for (ent = map ; ent && ent->name ; ent++){
		if (ent->id & flags) { /* non-default value */
			if (!(ent->mask & MNT_INVERT))
				mnt_optstr_append_option(&result, ent->name, NULL);
			else
				continue;
		} else if (ent->mask & MNT_INVERT)
			mnt_optstr_append_option(&result, ent->name, NULL);
	}

	return result;
}

/**
 * mnt_fs_get_user_options:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: pointer to userspace mount option string or NULL.
 */
const char *mnt_fs_get_user_options(struct libmnt_fs *fs)
{
	if (!fs)
		return NULL;
	if (fs->optlist)
		sync_opts_from_optlist(fs, fs->optlist);

	return fs->user_optstr;
}

/**
 * mnt_fs_get_attributes:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: pointer to attributes string or NULL.
 */
const char *mnt_fs_get_attributes(struct libmnt_fs *fs)
{
	return fs ? fs->attrs : NULL;
}

/**
 * mnt_fs_set_attributes:
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: options string
 *
 * Sets mount attributes. The attributes are mount(2) and mount(8) independent
 * options, these options are not sent to the kernel and are not interpreted by
 * libmount. The attributes are stored in /run/mount/utab only.
 *
 * The attributes are managed by libmount in userspace only. It's possible
 * that information stored in userspace will not be available for libmount
 * after CLONE_FS unshare. Be careful, and don't use attributes if possible.
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_attributes(struct libmnt_fs *fs, const char *optstr)
{
	return strdup_to_struct_member(fs, attrs, optstr);
}

/**
 * mnt_fs_append_attributes
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: options string
 *
 * Appends mount attributes. (See mnt_fs_set_attributes()).
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_append_attributes(struct libmnt_fs *fs, const char *optstr)
{
	if (!fs)
		return -EINVAL;
	if (!optstr)
		return 0;
	return mnt_optstr_append_option(&fs->attrs, optstr, NULL);
}

/**
 * mnt_fs_prepend_attributes
 * @fs: fstab/mtab/mountinfo entry
 * @optstr: options string
 *
 * Prepends mount attributes. (See mnt_fs_set_attributes()).
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_prepend_attributes(struct libmnt_fs *fs, const char *optstr)
{
	if (!fs)
		return -EINVAL;
	if (!optstr)
		return 0;
	return mnt_optstr_prepend_option(&fs->attrs, optstr, NULL);
}


/**
 * mnt_fs_get_freq:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: dump frequency in days.
 */
int mnt_fs_get_freq(struct libmnt_fs *fs)
{
	return fs ? fs->freq : 0;
}

/**
 * mnt_fs_set_freq:
 * @fs: fstab/mtab entry pointer
 * @freq: dump frequency in days
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_freq(struct libmnt_fs *fs, int freq)
{
	if (!fs)
		return -EINVAL;
	fs->freq = freq;
	return 0;
}

/**
 * mnt_fs_get_passno:
 * @fs: fstab/mtab entry pointer
 *
 * Returns: "pass number on parallel fsck".
 */
int mnt_fs_get_passno(struct libmnt_fs *fs)
{
	return fs ? fs->passno: 0;
}

/**
 * mnt_fs_set_passno:
 * @fs: fstab/mtab entry pointer
 * @passno: pass number
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_passno(struct libmnt_fs *fs, int passno)
{
	if (!fs)
		return -EINVAL;
	fs->passno = passno;
	return 0;
}

/**
 * mnt_fs_get_root:
 * @fs: /proc/self/mountinfo entry
 *
 * Returns: root of the mount within the filesystem or NULL
 */
const char *mnt_fs_get_root(struct libmnt_fs *fs)
{
	return fs ? fs->root : NULL;
}

/**
 * mnt_fs_set_root:
 * @fs: mountinfo entry
 * @path: root path
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_root(struct libmnt_fs *fs, const char *path)
{
	return strdup_to_struct_member(fs, root, path);
}

/**
 * mnt_fs_get_swaptype:
 * @fs: /proc/swaps entry
 *
 * Returns: swap type or NULL
 */
const char *mnt_fs_get_swaptype(struct libmnt_fs *fs)
{
	return fs ? fs->swaptype : NULL;
}

/**
 * mnt_fs_get_size:
 * @fs: /proc/swaps entry
 *
 * Returns: size
 */
off_t mnt_fs_get_size(struct libmnt_fs *fs)
{
	return fs ? fs->size : 0;
}

/**
 * mnt_fs_get_usedsize:
 * @fs: /proc/swaps entry
 *
 * Returns: used size
 */
off_t mnt_fs_get_usedsize(struct libmnt_fs *fs)
{
	return fs ? fs->usedsize : 0;
}

/**
 * mnt_fs_get_priority:
 * @fs: /proc/swaps entry
 *
 * Returns: priority
 */
int mnt_fs_get_priority(struct libmnt_fs *fs)
{
	return fs ? fs->priority : 0;
}

/**
 * mnt_fs_set_priority:
 * @fs: /proc/swaps entry
 * @prio: priority
 *
 * Since: 2.28
 *
 * Returns: 0 or -1 in case of error
 */
int mnt_fs_set_priority(struct libmnt_fs *fs, int prio)
{
	if (!fs)
		return -EINVAL;
	fs->priority = prio;
	return 0;
}

/**
 * mnt_fs_get_bindsrc:
 * @fs: /run/mount/utab entry
 *
 * Returns: full path that was used for mount(2) on MS_BIND
 */
const char *mnt_fs_get_bindsrc(struct libmnt_fs *fs)
{
	return fs ? fs->bindsrc : NULL;
}

/**
 * mnt_fs_set_bindsrc:
 * @fs: filesystem
 * @src: path
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_set_bindsrc(struct libmnt_fs *fs, const char *src)
{
	return strdup_to_struct_member(fs, bindsrc, src);
}

/**
 * mnt_fs_get_id:
 * @fs: /proc/self/mountinfo entry
 *
 * Returns: mount ID (unique identifier of the mount) or negative number in case of error.
 */
int mnt_fs_get_id(struct libmnt_fs *fs)
{
	return fs ? fs->id : -EINVAL;
}

/**
 * mnt_fs_get_parent_id:
 * @fs: /proc/self/mountinfo entry
 *
 * Returns: parent mount ID or negative number in case of error.
 */
int mnt_fs_get_parent_id(struct libmnt_fs *fs)
{
	return fs ? fs->parent : -EINVAL;
}

/**
 * mnt_fs_get_devno:
 * @fs: /proc/self/mountinfo entry
 *
 * Returns: value of st_dev for files on filesystem or 0 in case of error.
 */
dev_t mnt_fs_get_devno(struct libmnt_fs *fs)
{
	return fs ? fs->devno : 0;
}

/**
 * mnt_fs_get_tid:
 * @fs: /proc/tid/mountinfo entry
 *
 * Returns: TID (task ID) for filesystems read from the mountinfo file
 */
pid_t mnt_fs_get_tid(struct libmnt_fs *fs)
{
	return fs ? fs->tid : 0;
}

/**
 * mnt_fs_get_option:
 * @fs: fstab/mtab/mountinfo entry pointer
 * @name: option name
 * @value: returns pointer to the beginning of the value (e.g. name=VALUE) or NULL
 * @valsz: returns size of options value or 0
 *
 * Returns: 0 on success, 1 when @name not found or negative number in case of error.
 */
int mnt_fs_get_option(struct libmnt_fs *fs, const char *name,
		char **value, size_t *valsz)
{
	char rc = 1;

	if (!fs)
		return -EINVAL;

	if (fs->optlist)
		sync_opts_from_optlist(fs, fs->optlist);

	if (fs->fs_optstr)
		rc = mnt_optstr_get_option(fs->fs_optstr, name, value, valsz);
	if (rc == 1 && fs->vfs_optstr)
		rc = mnt_optstr_get_option(fs->vfs_optstr, name, value, valsz);
	if (rc == 1 && fs->user_optstr)
		rc = mnt_optstr_get_option(fs->user_optstr, name, value, valsz);
	return rc;
}

/**
 * mnt_fs_get_attribute:
 * @fs: fstab/mtab/mountinfo entry pointer
 * @name: option name
 * @value: returns pointer to the beginning of the value (e.g. name=VALUE) or NULL
 * @valsz: returns size of options value or 0
 *
 * Returns: 0 on success, 1 when @name not found or negative number in case of error.
 */
int mnt_fs_get_attribute(struct libmnt_fs *fs, const char *name,
		char **value, size_t *valsz)
{
	char rc = 1;

	if (!fs)
		return -EINVAL;
	if (fs->attrs)
		rc = mnt_optstr_get_option(fs->attrs, name, value, valsz);
	return rc;
}

/**
 * mnt_fs_get_comment:
 * @fs: fstab/mtab/mountinfo entry pointer
 *
 * Returns: comment string
 */
const char *mnt_fs_get_comment(struct libmnt_fs *fs)
{
	if (!fs)
		return NULL;
	return fs->comment;
}

/**
 * mnt_fs_set_comment:
 * @fs: fstab entry pointer
 * @comm: comment string
 *
 * Note that the comment has to be terminated by '\n' (new line), otherwise
 * the whole filesystem entry will be written as a comment to the tabfile (e.g.
 * fstab).
 *
 * Returns: 0 on success or <0 in case of error.
 */
int mnt_fs_set_comment(struct libmnt_fs *fs, const char *comm)
{
	return strdup_to_struct_member(fs, comment, comm);
}

/**
 * mnt_fs_append_comment:
 * @fs: fstab entry pointer
 * @comm: comment string
 *
 * See also mnt_fs_set_comment().
 *
 * Returns: 0 on success or <0 in case of error.
 */
int mnt_fs_append_comment(struct libmnt_fs *fs, const char *comm)
{
	if (!fs)
		return -EINVAL;

	return strappend(&fs->comment, comm);
}

/**
 * mnt_fs_match_target:
 * @fs: filesystem
 * @target: mountpoint path
 * @cache: tags/paths cache or NULL
 *
 * Possible are three attempts:
 *	1) compare @target with @fs->target
 *
 *	2) realpath(@target) with @fs->target
 *
 *	3) realpath(@target) with realpath(@fs->target) if @fs is not from
 *	   /proc/self/mountinfo.
 *
 *	   However, if mnt_cache_set_targets(cache, mtab) was called, and the
 *	   path @target or @fs->target is found in the @mtab, the canonicalization is
 *	   is not performed (see mnt_resolve_target()).
 *
 * The 2nd and 3rd attempts are not performed when @cache is NULL.
 *
 * Returns: 1 if @fs target is equal to @target, else 0.
 */
int mnt_fs_match_target(struct libmnt_fs *fs, const char *target,
			struct libmnt_cache *cache)
{
	int rc = 0;

	if (!fs || !target || !fs->target)
		return 0;

	/* 1) native paths */
	rc = mnt_fs_streq_target(fs, target);

	if (!rc && cache) {
		/* 2) - canonicalized and non-canonicalized */
		char *cn = mnt_resolve_target(target, cache);
		rc = (cn && mnt_fs_streq_target(fs, cn));

		/* 3) - canonicalized and canonicalized */
		if (!rc && cn && !mnt_fs_is_kernel(fs) && !mnt_fs_is_swaparea(fs)) {
			char *tcn = mnt_resolve_target(fs->target, cache);
			rc = (tcn && strcmp(cn, tcn) == 0);
		}
	}

	return rc;
}

/**
 * mnt_fs_match_source:
 * @fs: filesystem
 * @source: tag or path (device or so) or NULL
 * @cache: tags/paths cache or NULL
 *
 * Four attempts are possible:
 *	1) compare @source with @fs->source
 *	2) compare realpath(@source) with @fs->source
 *	3) compare realpath(@source) with realpath(@fs->source)
 *	4) compare realpath(@source) with evaluated tag from @fs->source
 *
 * The 2nd, 3rd and 4th attempts are not performed when @cache is NULL. The
 * 2nd and 3rd attempts are not performed if @fs->source is tag.
 *
 * Returns: 1 if @fs source is equal to @source, else 0.
 */
int mnt_fs_match_source(struct libmnt_fs *fs, const char *source,
			struct libmnt_cache *cache)
{
	char *cn;
	const char *src, *t, *v;

	if (!fs)
		return 0;

	/* 1) native paths... */
	if (mnt_fs_streq_srcpath(fs, source) == 1)
		return 1;

	if (!source || !fs->source)
		return 0;

	/* ... and tags */
	if (fs->tagname && strcmp(source, fs->source) == 0)
		return 1;

	if (!cache)
		return 0;
	if (fs->flags & (MNT_FS_NET | MNT_FS_PSEUDO))
		return 0;

	cn = mnt_resolve_spec(source, cache);
	if (!cn)
		return 0;

	/* 2) canonicalized and native */
	src = mnt_fs_get_srcpath(fs);
	if (src && mnt_fs_streq_srcpath(fs, cn))
		return 1;

	/* 3) canonicalized and canonicalized */
	if (src) {
		src = mnt_resolve_path(src, cache);
		if (src && !strcmp(cn, src))
			return 1;
	}
	if (src || mnt_fs_get_tag(fs, &t, &v))
		/* src path does not match and the tag is not defined */
		return 0;

	/* read @source's tags to the cache */
	if (mnt_cache_read_tags(cache, cn) < 0) {
		if (errno == EACCES) {
			/* we don't have permissions to read TAGs from
			 * @source, but can translate the @fs tag to devname.
			 *
			 * (because libblkid uses udev symlinks and this is
			 * accessible for non-root uses)
			 */
			char *x = mnt_resolve_tag(t, v, cache);
			if (x && !strcmp(x, cn))
				return 1;
		}
		return 0;
	}

	/* 4) has the @source a tag that matches with the tag from @fs ? */
	if (mnt_cache_device_has_tag(cache, cn, t, v))
		return 1;

	return 0;
}

/**
 * mnt_fs_match_fstype:
 * @fs: filesystem
 * @types: filesystem name or comma delimited list of filesystems
 *
 * For more details see mnt_match_fstype().
 *
 * Returns: 1 if @fs type is matching to @types, else 0. The function returns
 * 0 when types is NULL.
 */
int mnt_fs_match_fstype(struct libmnt_fs *fs, const char *types)
{
	return mnt_match_fstype(fs->fstype, types);
}

/**
 * mnt_fs_match_options:
 * @fs: filesystem
 * @options: comma delimited list of options (and nooptions)
 *
 * For more details see mnt_match_options().
 *
 * Returns: 1 if @fs type is matching to @options, else 0. The function returns
 * 0 when types is NULL.
 */
int mnt_fs_match_options(struct libmnt_fs *fs, const char *options)
{
	return mnt_match_options(mnt_fs_get_options(fs), options);
}

/**
 * mnt_fs_print_debug
 * @fs: fstab/mtab/mountinfo entry
 * @file: file stream
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_fs_print_debug(struct libmnt_fs *fs, FILE *file)
{
	if (!fs || !file)
		return -EINVAL;

	if (fs->optlist)
		sync_opts_from_optlist(fs, fs->optlist);

	fprintf(file, "------ fs:\n");
	fprintf(file, "source: %s\n", mnt_fs_get_source(fs));
	fprintf(file, "target: %s\n", mnt_fs_get_target(fs));
	fprintf(file, "fstype: %s\n", mnt_fs_get_fstype(fs));

	if (mnt_fs_get_options(fs))
		fprintf(file, "optstr: %s\n", mnt_fs_get_options(fs));
	if (mnt_fs_get_vfs_options(fs))
		fprintf(file, "VFS-optstr: %s\n", mnt_fs_get_vfs_options(fs));
	if (mnt_fs_get_fs_options(fs))
		fprintf(file, "FS-opstr: %s\n", mnt_fs_get_fs_options(fs));
	if (mnt_fs_get_user_options(fs))
		fprintf(file, "user-optstr: %s\n", mnt_fs_get_user_options(fs));
	if (mnt_fs_get_optional_fields(fs))
		fprintf(file, "optional-fields: '%s'\n", mnt_fs_get_optional_fields(fs));
	if (mnt_fs_get_attributes(fs))
		fprintf(file, "attributes: %s\n", mnt_fs_get_attributes(fs));

	if (mnt_fs_get_root(fs))
		fprintf(file, "root:   %s\n", mnt_fs_get_root(fs));

	if (mnt_fs_get_swaptype(fs))
		fprintf(file, "swaptype: %s\n", mnt_fs_get_swaptype(fs));
	if (mnt_fs_get_size(fs))
		fprintf(file, "size: %jd\n", mnt_fs_get_size(fs));
	if (mnt_fs_get_usedsize(fs))
		fprintf(file, "usedsize: %jd\n", mnt_fs_get_usedsize(fs));
	if (mnt_fs_get_priority(fs))
		fprintf(file, "priority: %d\n", mnt_fs_get_priority(fs));

	if (mnt_fs_get_bindsrc(fs))
		fprintf(file, "bindsrc: %s\n", mnt_fs_get_bindsrc(fs));
	if (mnt_fs_get_freq(fs))
		fprintf(file, "freq:   %d\n", mnt_fs_get_freq(fs));
	if (mnt_fs_get_passno(fs))
		fprintf(file, "pass:   %d\n", mnt_fs_get_passno(fs));
	if (mnt_fs_get_id(fs))
		fprintf(file, "id:     %d\n", mnt_fs_get_id(fs));
	if (mnt_fs_get_parent_id(fs))
		fprintf(file, "parent: %d\n", mnt_fs_get_parent_id(fs));
	if (mnt_fs_get_devno(fs))
		fprintf(file, "devno:  %d:%d\n", major(mnt_fs_get_devno(fs)),
						minor(mnt_fs_get_devno(fs)));
	if (mnt_fs_get_tid(fs))
		fprintf(file, "tid:    %d\n", mnt_fs_get_tid(fs));
	if (mnt_fs_get_comment(fs))
		fprintf(file, "comment: '%s'\n", mnt_fs_get_comment(fs));

	return 0;
}

/**
 * mnt_free_mntent:
 * @mnt: mount entry
 *
 * Deallocates the "mntent.h" mount entry.
 */
void mnt_free_mntent(struct mntent *mnt)
{
	if (mnt) {
		free(mnt->mnt_fsname);
		free(mnt->mnt_dir);
		free(mnt->mnt_type);
		free(mnt->mnt_opts);
		free(mnt);
	}
}

/**
 * mnt_fs_to_mntent:
 * @fs: filesystem
 * @mnt: mount description (as described in mntent.h)
 *
 * Copies the information from @fs to struct mntent @mnt. If @mnt is already set,
 * then the struct mntent items are reallocated and updated. See also
 * mnt_free_mntent().
 *
 * Returns: 0 on success and a negative number in case of error.
 */
int mnt_fs_to_mntent(struct libmnt_fs *fs, struct mntent **mnt)
{
	int rc;
	struct mntent *m;

	if (!fs || !mnt)
		return -EINVAL;

	m = *mnt;
	if (!m) {
		m = calloc(1, sizeof(*m));
		if (!m)
			return -ENOMEM;
	}

	if ((rc = update_str(&m->mnt_fsname, mnt_fs_get_source(fs))))
		goto err;
	if ((rc = update_str(&m->mnt_dir, mnt_fs_get_target(fs))))
		goto err;
	if ((rc = update_str(&m->mnt_type, mnt_fs_get_fstype(fs))))
		goto err;

	errno = 0;
	m->mnt_opts = mnt_fs_strdup_options(fs);
	if (!m->mnt_opts && errno) {
		rc = -errno;
		goto err;
	}

	m->mnt_freq = mnt_fs_get_freq(fs);
	m->mnt_passno = mnt_fs_get_passno(fs);

	if (!m->mnt_fsname) {
		m->mnt_fsname = strdup("none");
		if (!m->mnt_fsname)
			goto err;
	}
	*mnt = m;

	return 0;
err:
	if (m != *mnt)
		mnt_free_mntent(m);
	return rc;
}
