/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2011-2018 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

/**
 * SECTION: update
 * @title: Tables update
 * @short_description: userspace mount information management
 *
 * The struct libmnt_update provides an abstraction to manage mount options in
 * userspace independently of system configuration. The userspace mount options
 * (e.g. user=) are stored in the /run/mount/utab file.
 *
 * It's recommended to use high-level struct libmnt_context API.
 */
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>

#include "mountP.h"
#include "mangle.h"
#include "pathnames.h"
#include "strutils.h"

struct libmnt_update {
	char		*target;
	struct libmnt_fs *fs;
	char		*filename;
	unsigned long	mountflags;

	int		act_fd;
	char		*act_filename;

	unsigned int	ready : 1,
			missing_options : 1;

	struct libmnt_table *mountinfo;
	struct libmnt_lock  *lock;
};

static int set_fs_root(struct libmnt_update *upd, struct libmnt_fs *fs, unsigned long mountflags);
static int utab_new_entry(struct libmnt_update *upd, struct libmnt_fs *fs, unsigned long mountflags);

/**
 * mnt_new_update:
 *
 * Returns: newly allocated update handler
 */
struct libmnt_update *mnt_new_update(void)
{
	struct libmnt_update *upd;

	upd = calloc(1, sizeof(*upd));
	if (!upd)
		return NULL;

	upd->act_fd = -1;
	DBG(UPDATE, ul_debugobj(upd, "allocate"));
	return upd;
}

/**
 * mnt_free_update:
 * @upd: update
 *
 * Deallocates struct libmnt_update handler.
 */
void mnt_free_update(struct libmnt_update *upd)
{
	if (!upd)
		return;

	DBG(UPDATE, ul_debugobj(upd, "free"));

	mnt_unref_lock(upd->lock);
	mnt_unref_fs(upd->fs);
	mnt_unref_table(upd->mountinfo);
	if (upd->act_fd >= 0)
		close(upd->act_fd);
	free(upd->target);
	free(upd->filename);
	free(upd->act_filename);
	free(upd);
}

/*
 * Returns 0 on success, <0 in case of error.
 */
int mnt_update_set_filename(struct libmnt_update *upd, const char *filename)
{
	const char *path = NULL;
	int rw = 0;

	if (!upd)
		return -EINVAL;

	/* filename explicitly defined */
	if (filename) {
		char *p = strdup(filename);
		if (!p)
			return -ENOMEM;

		free(upd->filename);
		upd->filename = p;
	}

	if (upd->filename)
		return 0;

	/* detect tab filename -- /run/mount/utab
	 */
	path = NULL;
	mnt_has_regular_utab(&path, &rw);
	if (!rw)
		return -EACCES;
	upd->filename = strdup(path);
	if (!upd->filename)
		return -ENOMEM;

	return 0;
}

/**
 * mnt_update_get_filename:
 * @upd: update
 *
 * This function returns the file name of the up-dated file.
 *
 * Returns: pointer to filename that will be updated or NULL in case of error.
 */
const char *mnt_update_get_filename(struct libmnt_update *upd)
{
	return upd ? upd->filename : NULL;
}

/**
 * mnt_update_is_ready:
 * @upd: update handler
 *
 * Returns: 1 if entry described by @upd is successfully prepared and will be
 * written to the utab file.
 */
int mnt_update_is_ready(struct libmnt_update *upd)
{
	return upd ? upd->ready : FALSE;
}

/**
 * mnt_update_set_fs:
 * @upd: update handler
 * @mountflags: MS_* flags
 * @target: umount target, must be NULL for mount
 * @fs: mount filesystem description, must be NULL for umount
 *
 * Returns: <0 in case on error, 0 on success, 1 if update is unnecessary.
 */
int mnt_update_set_fs(struct libmnt_update *upd, unsigned long mountflags,
		      const char *target, struct libmnt_fs *fs)
{
	int rc;

	if (!upd)
		return -EINVAL;
	if ((mountflags & MS_MOVE) && (!fs || !mnt_fs_get_srcpath(fs)))
		return -EINVAL;
	if (target && fs)
		return -EINVAL;

	DBG(UPDATE, ul_debugobj(upd,
			"resetting FS [target=%s, flags=0x%08lx]",
			target, mountflags));
	if (fs) {
		DBG(UPDATE, ul_debugobj(upd, "FS template:"));
		DBG(UPDATE, mnt_fs_print_debug(fs, stderr));
	}

	mnt_unref_fs(upd->fs);
	free(upd->target);
	upd->ready = 0;
	upd->fs = NULL;
	upd->target = NULL;
	upd->mountflags = 0;

	if (mountflags & MS_PROPAGATION)
		return 1;

	upd->mountflags = mountflags;

	rc = mnt_update_set_filename(upd, NULL);
	if (rc) {
		DBG(UPDATE, ul_debugobj(upd, "no writable file available [rc=%d]", rc));
		return rc;	/* error or no file available (rc = 1) */
	}
	if (target) {
		upd->target = strdup(target);
		if (!upd->target)
			return -ENOMEM;

	} else if (fs) {
		if (!(mountflags & MS_MOVE)) {
			rc = utab_new_entry(upd, fs, mountflags);
			if (rc)
			       return rc;
		} else {
			upd->fs = mnt_copy_mtab_fs(fs);
			if (!upd->fs)
				return -ENOMEM;
		}
	}

	DBG(UPDATE, ul_debugobj(upd, "ready"));
	upd->ready = 1;
	return 0;
}

/**
 * mnt_update_get_fs:
 * @upd: update
 *
 * Returns: update filesystem entry or NULL
 */
struct libmnt_fs *mnt_update_get_fs(struct libmnt_update *upd)
{
	return upd ? upd->fs : NULL;
}

/**
 * mnt_update_get_mflags:
 * @upd: update
 *
 * Returns: mount flags as was set by mnt_update_set_fs()
 */
unsigned long mnt_update_get_mflags(struct libmnt_update *upd)
{
	return upd ? upd->mountflags : 0;
}

/**
 * mnt_update_force_rdonly:
 * @upd: update
 * @rdonly: is read-only?
 *
 * Returns: 0 on success and negative number in case of error.
 */
int mnt_update_force_rdonly(struct libmnt_update *upd, int rdonly)
{
	int rc = 0;

	if (!upd || !upd->fs)
		return -EINVAL;

	if (rdonly && (upd->mountflags & MS_RDONLY))
		return 0;
	if (!rdonly && !(upd->mountflags & MS_RDONLY))
		return 0;

	if (rdonly)
		upd->mountflags &= ~MS_RDONLY;
	else
		upd->mountflags |= MS_RDONLY;

	return rc;
}


/*
 * Allocates an utab entry (upd->fs) for mount/remount. This function should be
 * called *before* mount(2) syscall. The @fs is used as a read-only template.
 *
 * Returns: 0 on success, negative number on error, 1 if utab's update is
 *          unnecessary.
 */
static int utab_new_entry(struct libmnt_update *upd, struct libmnt_fs *fs,
			  unsigned long mountflags)
{
	int rc = 0;
	const char *o, *a;
	char *u = NULL;

	assert(fs);
	assert(upd);
	assert(upd->fs == NULL);
	assert(!(mountflags & MS_MOVE));

	DBG(UPDATE, ul_debug("prepare utab entry"));

	o = mnt_fs_get_user_options(fs);
	a = mnt_fs_get_attributes(fs);
	upd->fs = NULL;

	if (o) {
		/* remove non-mtab options */
		rc = mnt_optstr_get_options(o, &u,
				mnt_get_builtin_optmap(MNT_USERSPACE_MAP),
				MNT_NOMTAB);
		if (rc)
			goto err;
	}

	if (!u && !a) {
		DBG(UPDATE, ul_debug("utab entry unnecessary (no options)"));
		return 1;
	}

	/* allocate the entry */
	upd->fs = mnt_copy_fs(NULL, fs);
	if (!upd->fs) {
		rc = -ENOMEM;
		goto err;
	}

	rc = mnt_fs_set_options(upd->fs, u);
	if (rc)
		goto err;
	rc = mnt_fs_set_attributes(upd->fs, a);
	if (rc)
		goto err;

	if (!(mountflags & MS_REMOUNT)) {
		rc = set_fs_root(upd, fs, mountflags);
		if (rc)
			goto err;
	}

	free(u);
	DBG(UPDATE, ul_debug("utab entry OK"));
	return 0;
err:
	free(u);
	mnt_unref_fs(upd->fs);
	upd->fs = NULL;
	return rc;
}

/*
 * Sets fs-root and fs-type to @upd->fs according to the @fs template and
 * @mountfalgs. For MS_BIND mountflag it reads information about the source
 * filesystem from /proc/self/mountinfo.
 */
static int set_fs_root(struct libmnt_update *upd, struct libmnt_fs *fs,
		       unsigned long mountflags)
{
	struct libmnt_fs *src_fs;
	char *fsroot = NULL;
	const char *src, *fstype;
	int rc = 0;

	DBG(UPDATE, ul_debug("setting FS root"));

	assert(upd);
	assert(upd->fs);
	assert(fs);

	fstype = mnt_fs_get_fstype(fs);

	if (mountflags & MS_BIND) {
		if (!upd->mountinfo)
			upd->mountinfo = mnt_new_table_from_file(_PATH_PROC_MOUNTINFO);
		src = mnt_fs_get_srcpath(fs);
		if (src) {
			 rc = mnt_fs_set_bindsrc(upd->fs, src);
			 if (rc)
				 goto err;
		}

	} else if (fstype && (strcmp(fstype, "btrfs") == 0 || strcmp(fstype, "auto") == 0)) {
		if (!upd->mountinfo)
			upd->mountinfo = mnt_new_table_from_file(_PATH_PROC_MOUNTINFO);
	}

	src_fs = mnt_table_get_fs_root(upd->mountinfo, fs,
					mountflags, &fsroot);
	if (src_fs) {
		src = mnt_fs_get_srcpath(src_fs);
		rc = mnt_fs_set_source(upd->fs, src);
		if (rc)
			goto err;

		mnt_fs_set_fstype(upd->fs, mnt_fs_get_fstype(src_fs));
	}

	upd->fs->root = fsroot;
	return 0;
err:
	free(fsroot);
	return rc;
}

/* mtab and fstab update -- returns zero on success
 */
static int fprintf_mtab_fs(FILE *f, struct libmnt_fs *fs)
{
	const char *o, *src, *fstype, *comm;
	char *m1, *m2, *m3, *m4;
	int rc;

	assert(fs);
	assert(f);

	comm = mnt_fs_get_comment(fs);
	src = mnt_fs_get_source(fs);
	fstype = mnt_fs_get_fstype(fs);
	o = mnt_fs_get_options(fs);

	m1 = src ? mangle(src) : "none";
	m2 = mangle(mnt_fs_get_target(fs));
	m3 = fstype ? mangle(fstype) : "none";
	m4 = o ? mangle(o) : "rw";

	if (m1 && m2 && m3 && m4) {
		if (comm)
			fputs(comm, f);
		rc = fprintf(f, "%s %s %s %s %d %d\n",
				m1, m2, m3, m4,
				mnt_fs_get_freq(fs),
				mnt_fs_get_passno(fs));
		if (rc > 0)
			rc = 0;
	} else
		rc = -ENOMEM;

	if (src)
		free(m1);
	free(m2);
	if (fstype)
		free(m3);
	if (o)
		free(m4);

	return rc;
}

static int fprintf_utab_fs(FILE *f, struct libmnt_fs *fs)
{
	char *p;
	int rc = 0;

	if (!fs || !f)
		return -EINVAL;

	if (mnt_fs_get_id(fs) > 0) {
		rc = fprintf(f, "ID=%d ", mnt_fs_get_id(fs));
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_source(fs));
		if (p) {
			rc = fprintf(f, "SRC=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_target(fs));
		if (p) {
			rc = fprintf(f, "TARGET=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_root(fs));
		if (p) {
			rc = fprintf(f, "ROOT=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_bindsrc(fs));
		if (p) {
			rc = fprintf(f, "BINDSRC=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_attributes(fs));
		if (p) {
			rc = fprintf(f, "ATTRS=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_user_options(fs));
		if (p) {
			rc = fprintf(f, "OPTS=%s", p);
			free(p);
		}
	}
	if (rc >= 0)
		rc = fprintf(f, "\n");

	if (rc > 0)
		rc = 0;	/* success */
	return rc;
}

static int update_table(struct libmnt_update *upd, struct libmnt_table *tb)
{
	FILE *f;
	int rc, fd;
	char *uq = NULL;

	if (!tb || !upd->filename)
		return -EINVAL;

	DBG(UPDATE, ul_debugobj(upd, "%s: updating", upd->filename));

	fd = mnt_open_uniq_filename(upd->filename, &uq);
	if (fd < 0)
		return fd;	/* error */

	f = fdopen(fd, "w" UL_CLOEXECSTR);
	if (f) {
		struct stat st;
		struct libmnt_iter itr;
		struct libmnt_fs *fs;

		mnt_reset_iter(&itr, MNT_ITER_FORWARD);

		if (tb->comms && mnt_table_get_intro_comment(tb))
			fputs(mnt_table_get_intro_comment(tb), f);

		while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
			rc = fprintf_utab_fs(f, fs);
			if (rc) {
				DBG(UPDATE, ul_debugobj(upd,
					"%s: write entry failed: %m", uq));
				goto leave;
			}
		}
		if (tb->comms && mnt_table_get_trailing_comment(tb))
			fputs(mnt_table_get_trailing_comment(tb), f);

		if (fflush(f) != 0) {
			rc = -errno;
			DBG(UPDATE, ul_debugobj(upd, "%s: fflush failed: %m", uq));
			goto leave;
		}

		rc = fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) ? -errno : 0;

		if (!rc && stat(upd->filename, &st) == 0)
			/* Copy uid/gid from the present file before renaming. */
			rc = fchown(fd, st.st_uid, st.st_gid) ? -errno : 0;

		fclose(f);
		f = NULL;

		if (!rc)
			rc = rename(uq, upd->filename) ? -errno : 0;
	} else {
		rc = -errno;
		close(fd);
	}

leave:
	if (f)
		fclose(f);

	unlink(uq);	/* be paranoid */
	free(uq);
	DBG(UPDATE, ul_debugobj(upd, "%s: done [rc=%d]", upd->filename, rc));
	return rc;
}

/**
 * mnt_table_write_file
 * @tb: parsed file (e.g. fstab)
 * @file: target
 *
 * This function writes @tb to @file.
 *
 * Returns: 0 on success, negative number on error.
 */
int mnt_table_write_file(struct libmnt_table *tb, FILE *file)
{
	int rc = 0;
	struct libmnt_iter itr;
	struct libmnt_fs *fs;

	if (tb->comms && mnt_table_get_intro_comment(tb))
		fputs(mnt_table_get_intro_comment(tb), file);

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);
	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
		rc = fprintf_mtab_fs(file, fs);
		if (rc)
			return rc;
	}
	if (tb->comms && mnt_table_get_trailing_comment(tb))
		fputs(mnt_table_get_trailing_comment(tb), file);

	if (fflush(file) != 0)
		rc = -errno;

	DBG(TAB, ul_debugobj(tb, "write file done [rc=%d]", rc));
	return rc;
}

/**
 * mnt_table_replace_file
 * @tb: parsed file (e.g. fstab)
 * @filename: target
 *
 * This function replaces @file by the new content from @tb.
 *
 * Returns: 0 on success, negative number on error.
 */
int mnt_table_replace_file(struct libmnt_table *tb, const char *filename)
{
	int fd, rc = 0;
	FILE *f;
	char *uq = NULL;

	DBG(TAB, ul_debugobj(tb, "%s: replacing", filename));

	fd = mnt_open_uniq_filename(filename, &uq);
	if (fd < 0)
		return fd;	/* error */

	f = fdopen(fd, "w" UL_CLOEXECSTR);
	if (f) {
		struct stat st;

		mnt_table_write_file(tb, f);

		if (fflush(f) != 0) {
			rc = -errno;
			DBG(UPDATE, ul_debug("%s: fflush failed: %m", uq));
			goto leave;
		}

		rc = fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) ? -errno : 0;

		if (!rc && stat(filename, &st) == 0)
			/* Copy uid/gid from the present file before renaming. */
			rc = fchown(fd, st.st_uid, st.st_gid) ? -errno : 0;

		fclose(f);
		f = NULL;

		if (!rc)
			rc = rename(uq, filename) ? -errno : 0;
	} else {
		rc = -errno;
		close(fd);
	}

leave:
	if (f)
		fclose(f);
	unlink(uq);
	free(uq);

	DBG(TAB, ul_debugobj(tb, "replace done [rc=%d]", rc));
	return rc;
}

static int add_file_entry(struct libmnt_table *tb, struct libmnt_update *upd)
{
	struct libmnt_fs *fs;

	assert(upd);

	fs = mnt_copy_fs(NULL, upd->fs);
	if (!fs)
		return -ENOMEM;

	mnt_table_add_fs(tb, fs);
	mnt_unref_fs(fs);

	return update_table(upd, tb);
}

static int update_add_entry(struct libmnt_update *upd)
{
	struct libmnt_table *tb;
	int rc = 0;

	assert(upd);
	assert(upd->fs);
	assert(upd->lock);

	DBG(UPDATE, ul_debugobj(upd, "%s: add entry", upd->filename));

	rc = mnt_lock_file(upd->lock);
	if (rc)
		return -MNT_ERR_LOCK;

	tb = __mnt_new_table_from_file(upd->filename, MNT_FMT_UTAB, 1);
	if (tb)
		rc = add_file_entry(tb, upd);

	mnt_unlock_file(upd->lock);
	mnt_unref_table(tb);
	return rc;
}

static int update_remove_entry(struct libmnt_update *upd)
{
	struct libmnt_table *tb;
	int rc = 0;

	assert(upd);
	assert(upd->target);
	assert(upd->lock);

	DBG(UPDATE, ul_debugobj(upd, "%s: remove entry", upd->filename));

	rc = mnt_lock_file(upd->lock);
	if (rc)
		return -MNT_ERR_LOCK;

	tb = __mnt_new_table_from_file(upd->filename, MNT_FMT_UTAB, 1);
	if (tb) {
		struct libmnt_fs *rem = mnt_table_find_target(tb, upd->target, MNT_ITER_BACKWARD);
		if (rem) {
			mnt_table_remove_fs(tb, rem);
			rc = update_table(upd, tb);
		}
	}

	mnt_unlock_file(upd->lock);
	mnt_unref_table(tb);
	return rc;
}

static int update_modify_target(struct libmnt_update *upd)
{
	struct libmnt_table *tb = NULL;
	int rc = 0;

	assert(upd);
	assert(upd->lock);
	DBG(UPDATE, ul_debugobj(upd, "%s: modify target", upd->filename));

	rc = mnt_lock_file(upd->lock);
	if (rc)
		return -MNT_ERR_LOCK;

	tb = __mnt_new_table_from_file(upd->filename, MNT_FMT_UTAB, 1);
	if (tb) {
		const char *upd_source = mnt_fs_get_srcpath(upd->fs);
		const char *upd_target = mnt_fs_get_target(upd->fs);
		struct libmnt_iter itr;
		struct libmnt_fs *fs;
		char *cn_target = mnt_resolve_path(upd_target, NULL);

		if (!cn_target) {
			rc = -ENOMEM;
			goto done;
		}

		mnt_reset_iter(&itr, MNT_ITER_BACKWARD);
		while (mnt_table_next_fs(tb, &itr, &fs) == 0) {
			char *p;
			const char *e;

			e = startswith(mnt_fs_get_target(fs), upd_source);
			if (!e || (*e && *e != '/'))
				continue;
			if (*e == '/')
				e++;		/* remove extra '/' */

			/* no subdirectory, replace entire path */
			if (!*e)
				rc = mnt_fs_set_target(fs, cn_target);

			/* update start of the path, keep subdirectory */
			else if (asprintf(&p, "%s/%s", cn_target, e) > 0) {
				rc = mnt_fs_set_target(fs, p);
				free(p);
			} else
				rc = -ENOMEM;

			if (rc < 0)
				break;
		}

		if (!rc)
			rc = update_table(upd, tb);
		free(cn_target);
	}

done:
	mnt_unlock_file(upd->lock);
	mnt_unref_table(tb);
	return rc;
}

/* replaces option in the table entry by new options from @udp */
static int update_modify_options(struct libmnt_update *upd)
{
	struct libmnt_table *tb = NULL;
	int rc = 0;
	struct libmnt_fs *fs;

	assert(upd);
	assert(upd->fs);
	assert(upd->lock);

	DBG(UPDATE, ul_debugobj(upd, "%s: modify options", upd->filename));

	fs = upd->fs;

	rc = mnt_lock_file(upd->lock);
	if (rc)
		return -MNT_ERR_LOCK;

	tb = __mnt_new_table_from_file(upd->filename, MNT_FMT_UTAB, 1);
	if (tb) {
		struct libmnt_fs *cur = mnt_table_find_target(tb,
					mnt_fs_get_target(fs),
					MNT_ITER_BACKWARD);
		if (cur) {
			rc = mnt_fs_set_attributes(cur,	mnt_fs_get_attributes(fs));
			if (!rc)
				rc = mnt_fs_set_options(cur, mnt_fs_get_options(fs));
			if (!rc)
				rc = update_table(upd, tb);
		} else
			rc = add_file_entry(tb, upd);	/* not found, add new */
	}

	mnt_unlock_file(upd->lock);
	mnt_unref_table(tb);
	return rc;
}

/* add user options missing in the table entry by new options from @udp */
static int update_add_options(struct libmnt_update *upd)
{
	struct libmnt_table *tb = NULL;
	int rc = 0;
	struct libmnt_fs *fs;

	assert(upd);
	assert(upd->fs);
	assert(upd->lock);

	if (!upd->fs->user_optstr)
		return 0;

	DBG(UPDATE, ul_debugobj(upd, "%s: add options", upd->filename));

	fs = upd->fs;

	rc = mnt_lock_file(upd->lock);
	if (rc)
		return -MNT_ERR_LOCK;

	tb = __mnt_new_table_from_file(upd->filename, MNT_FMT_UTAB, 1);
	if (tb) {
		struct libmnt_fs *cur = mnt_table_find_target(tb,
					mnt_fs_get_target(fs),
					MNT_ITER_BACKWARD);
		if (cur) {
			char *u = NULL;

			rc = mnt_optstr_get_missing(cur->user_optstr, upd->fs->user_optstr, &u);
			if (!rc && u) {
				DBG(UPDATE, ul_debugobj(upd, " add missing: %s", u));
				rc = mnt_optstr_append_option(&cur->user_optstr, u, NULL);
			}
			if (!rc && u)
				rc = update_table(upd, tb);

			if (rc == 1)	/* nothing is missing */
				rc = 0;
		} else
			rc = add_file_entry(tb, upd);	/* not found, add new */
	}

	mnt_unlock_file(upd->lock);
	mnt_unref_table(tb);
	return rc;

}

static int update_init_lock(struct libmnt_update *upd, struct libmnt_lock *lc)
{
	assert(upd);

	if (lc) {
		mnt_unref_lock(upd->lock);
		mnt_ref_lock(lc);
		upd->lock = lc;
	} else if (!upd->lock) {
		upd->lock = mnt_new_lock(upd->filename, 0);
		if (!upd->lock)
			return -ENOMEM;
		mnt_lock_block_signals(upd->lock, TRUE);
	}

	return 0;
}

/**
 * mnt_update_table:
 * @upd: update
 * @lc: lock or NULL
 *
 * High-level API to update /etc/mtab (or private /run/mount/utab file).
 *
 * The @lc lock is optional and will be created if necessary. Note that
 * an automatically created lock blocks all signals.
 *
 * See also mnt_lock_block_signals() and mnt_context_get_lock().
 *
 * Returns: 0 on success, negative number on error.
 */
int mnt_update_table(struct libmnt_update *upd, struct libmnt_lock *lc)
{
	int rc = -EINVAL;

	if (!upd || !upd->filename)
		return -EINVAL;
	if (!upd->ready)
		return 0;

	DBG(UPDATE, ul_debugobj(upd, "%s: update tab", upd->filename));
	if (upd->fs) {
		DBG(UPDATE, mnt_fs_print_debug(upd->fs, stderr));
	}

	rc = update_init_lock(upd, lc);
	if (rc)
		goto done;

	if (!upd->fs && upd->target)
		rc = update_remove_entry(upd);		/* umount */
	else if (upd->mountflags & MS_MOVE)
		rc = update_modify_target(upd);		/* move */
	else if (upd->mountflags & MS_REMOUNT)
		rc = update_modify_options(upd);	/* remount */
	else if (upd->fs && upd->missing_options)
		rc = update_add_options(upd);		/* mount by externel helper */
	else if (upd->fs)
		rc = update_add_entry(upd);		/* mount */

	upd->ready = 1;
done:
	DBG(UPDATE, ul_debugobj(upd, "%s: update tab: done [rc=%d]",
				upd->filename, rc));
	return rc;
}

int mnt_update_already_done(struct libmnt_update *upd)
{
	struct libmnt_table *tb = NULL;
	int rc = 0;

	if (!upd || !upd->filename || (!upd->fs && !upd->target))
		return -EINVAL;

	DBG(UPDATE, ul_debugobj(upd, "%s: checking for previous update", upd->filename));

	tb = __mnt_new_table_from_file(upd->filename, MNT_FMT_UTAB, 1);
	if (!tb)
		goto done;

	if (upd->fs) {
		/* mount */
		struct libmnt_fs *fs;
		const char *tgt = mnt_fs_get_target(upd->fs);
		const char *src = mnt_fs_get_bindsrc(upd->fs) ?
					mnt_fs_get_bindsrc(upd->fs) :
					mnt_fs_get_source(upd->fs);

		fs = mnt_table_find_pair(tb, src, tgt, MNT_ITER_BACKWARD);
		if (fs) {
			DBG(UPDATE, ul_debugobj(upd, "%s: found %s %s",
						upd->filename, src, tgt));

			/* Check if utab entry (probably writen by /sbin/mount.<type>
			 * helper) contains all options expected by this update */
			if (mnt_optstr_get_missing(fs->user_optstr, upd->fs->user_optstr, NULL) == 0) {
				upd->missing_options = 1;
				DBG(UPDATE, ul_debugobj(upd, " missing options detected"));
			} else
				rc = 1;
		}

	} else if (upd->target) {
		/* umount */
		if (!mnt_table_find_target(tb, upd->target, MNT_ITER_BACKWARD)) {
			DBG(UPDATE, ul_debugobj(upd, "%s: not-found (umounted) %s",
						upd->filename, upd->target));
			rc = 1;
		}
	}

	mnt_unref_table(tb);
done:
	DBG(UPDATE, ul_debugobj(upd, "%s: previous update check done [rc=%d]",
				upd->filename, rc));
	return rc;
}

int mnt_update_emit_event(struct libmnt_update *upd)
{
	char *filename;
	int fd;

	if (!upd || !upd->filename)
		return -EINVAL;

	if (asprintf(&filename, "%s.event", upd->filename) <= 0)
		return -ENOMEM;

	DBG(UPDATE, ul_debugobj(upd, "emitting utab event"));

	fd = open(filename, O_WRONLY|O_CREAT|O_CLOEXEC,
			    S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
	free(filename);
	if (fd < 0)
		return -errno;
	close(fd);
	return 0;
}

/*
 * Let's use /run/mount/utab.act file to report to libmount monitor that
 * libmount is working with utab. In this case, the monitor can ignore all
 * events from kernel until entire mount (with all steps) is done.
 *
 * For example mount NFS with x-* options, means
 * - create utab.act and mark it as used (by LOCK_SH)
 * - exec /sbin/mount.nfs
 *   - call mount(2) (kernel event on /proc/self/mounts)
 *   - utab update (NFS stuff)
 * - utab update (add x-* userspace options)
 * - unlink utab.act (if not use anyone else)
 * - release event by /run/mount/utab.event
 *
 * Note, this is used only when utab is in the game (x-* options).
 */
int mnt_update_start(struct libmnt_update *upd)
{
	int rc = 0;
	mode_t oldmask;

	if (!upd || !upd->filename)
		return -EINVAL;

	if (!upd->act_filename &&
	    asprintf(&upd->act_filename, "%s.act", upd->filename) <= 0)
		return -ENOMEM;

	/* Use exclusive lock to avoid some other proces will remove the the
	 * file before it's marked as used by LOCK_SH (below) */
	rc = update_init_lock(upd, NULL);
	if (rc)
		return rc;

	rc = mnt_lock_file(upd->lock);
	if (rc)
		return -MNT_ERR_LOCK;

	DBG(UPDATE, ul_debugobj(upd, "creating act file"));

	oldmask = umask(S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
	upd->act_fd = open(upd->act_filename, O_WRONLY|O_CREAT|O_CLOEXEC, S_IRUSR|S_IWUSR);
	umask(oldmask);

	if (upd->act_fd < 0) {
		rc = -errno;
		goto fail;
	}

	/* mark the file as used */
	rc = flock(upd->act_fd, LOCK_SH);
	if (rc) {
		rc = -errno;
		goto fail;
	}

	mnt_unlock_file(upd->lock);
	return 0;
fail:
	DBG(UPDATE, ul_debugobj(upd, "act file failed [rc=%d]", rc));
	mnt_unlock_file(upd->lock);
	unlink(upd->act_filename);
	if (upd->act_fd >= 0)
		close(upd->act_fd);
	upd->act_fd = -1;
	return rc;
}

int mnt_update_end(struct libmnt_update *upd)
{
	int rc;

	if (!upd || upd->act_fd < 0)
		return -EINVAL;

	DBG(UPDATE, ul_debugobj(upd, "removing act file"));

	/* make sure nobody else will use the file */
	rc = mnt_lock_file(upd->lock);
	if (rc)
		return -MNT_ERR_LOCK;

	/* mark the file as unused */
	flock(upd->act_fd, LOCK_UN);
	errno = 0;

	/* check if nobody else need the file (if yes, then the file is under LOCK_SH) )*/
	if (flock(upd->act_fd, LOCK_EX | LOCK_NB) != 0) {
		if (errno == EWOULDBLOCK)
			DBG(UPDATE, ul_debugobj(upd, "act file used, no unlink"));
	} else {
		DBG(UPDATE, ul_debugobj(upd, "unlinking act file"));
		unlink(upd->act_filename);
	}

	mnt_unlock_file(upd->lock);
	close(upd->act_fd);
	upd->act_fd = -1;
	return 0;
}

#ifdef TEST_PROGRAM

static int update(const char *target, struct libmnt_fs *fs, unsigned long mountflags)
{
	int rc;
	struct libmnt_update *upd;

	DBG(UPDATE, ul_debug("update test"));

	upd = mnt_new_update();
	if (!upd)
		return -ENOMEM;

	rc = mnt_update_set_fs(upd, mountflags, target, fs);
	if (rc == 1) {
		/* update is unnecessary */
		rc = 0;
		goto done;
	}
	if (rc) {
		fprintf(stderr, "failed to set FS\n");
		goto done;
	}

	/* [... mount(2) call should be here...]  */

	rc = mnt_update_table(upd, NULL);
done:
	mnt_free_update(upd);
	return rc;
}

static int test_add(struct libmnt_test *ts __attribute__((unused)),
		    int argc, char *argv[])
{
	struct libmnt_fs *fs = mnt_new_fs();
	int rc;

	if (argc < 5 || !fs)
		return -1;
	mnt_fs_set_source(fs, argv[1]);
	mnt_fs_set_target(fs, argv[2]);
	mnt_fs_set_fstype(fs, argv[3]);
	mnt_fs_set_options(fs, argv[4]);

	rc = update(NULL, fs, 0);
	mnt_unref_fs(fs);
	return rc;
}

static int test_remove(struct libmnt_test *ts __attribute__((unused)),
		       int argc, char *argv[])
{
	if (argc < 2)
		return -1;
	return update(argv[1], NULL, 0);
}

static int test_move(struct libmnt_test *ts __attribute__((unused)),
		     int argc, char *argv[])
{
	struct libmnt_fs *fs = mnt_new_fs();
	int rc;

	if (argc < 3)
		return -1;
	mnt_fs_set_source(fs, argv[1]);
	mnt_fs_set_target(fs, argv[2]);

	rc = update(NULL, fs, MS_MOVE);

	mnt_unref_fs(fs);
	return rc;
}

static int test_remount(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_fs *fs = mnt_new_fs();
	int rc;

	if (argc < 3)
		return -1;
	mnt_fs_set_target(fs, argv[1]);
	mnt_fs_set_options(fs, argv[2]);

	rc = update(NULL, fs, MS_REMOUNT);
	mnt_unref_fs(fs);
	return rc;
}

static int test_replace(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_fs *fs = mnt_new_fs();
	struct libmnt_table *tb = mnt_new_table();
	int rc;

	if (argc < 3)
		return -1;

	mnt_table_enable_comments(tb, TRUE);
	mnt_table_parse_fstab(tb, NULL);

	mnt_fs_set_source(fs, argv[1]);
	mnt_fs_set_target(fs, argv[2]);
	mnt_fs_append_comment(fs, "# this is new filesystem\n");

	mnt_table_add_fs(tb, fs);
	mnt_unref_fs(fs);

	rc = mnt_table_replace_file(tb, mnt_get_fstab_path());
	mnt_unref_table(tb);
	return rc;
}

int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
	{ "--add",    test_add,     "<src> <target> <type> <options>  add a line to mtab" },
	{ "--remove", test_remove,  "<target>                      MS_REMOUNT mtab change" },
	{ "--move",   test_move,    "<old_target>  <target>        MS_MOVE mtab change" },
	{ "--remount",test_remount, "<target>  <options>           MS_REMOUNT mtab change" },
	{ "--replace",test_replace, "<src> <target>                Add a line to LIBMOUNT_FSTAB and replace the original file" },
	{ NULL }
	};

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
