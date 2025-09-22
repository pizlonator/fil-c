/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2010-2018 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

/**
 * SECTION: context-umount
 * @title: Umount context
 * @short_description: high-level API to umount operation.
 */

#include <sys/wait.h>
#include <sys/mount.h>

#include "pathnames.h"
#include "loopdev.h"
#include "strutils.h"
#include "mountP.h"

/*
 * umount2 flags
 */
#ifndef MNT_FORCE
# define MNT_FORCE        0x00000001	/* Attempt to forcibly umount */
#endif

#ifndef MNT_DETACH
# define MNT_DETACH       0x00000002	/* Just detach from the tree */
#endif

#ifndef UMOUNT_NOFOLLOW
# define UMOUNT_NOFOLLOW  0x00000008	/* Don't follow symlink on umount */
#endif

#ifndef UMOUNT_UNUSED
# define UMOUNT_UNUSED    0x80000000	/* Flag guaranteed to be unused */
#endif

/* search in mountinfo */
static int __mountinfo_find_umount_fs(struct libmnt_context *cxt,
			    const char *tgt,
			    struct libmnt_fs **pfs)
{
	int rc;
	struct libmnt_ns *ns_old;
	struct libmnt_table *mountinfo = NULL;
	struct libmnt_fs *fs;
	char *loopdev = NULL;

	assert(cxt);
	assert(tgt);
	assert(pfs);

	*pfs = NULL;
	DBG(CXT, ul_debugobj(cxt, " search %s in mountinfo", tgt));

	/*
	 * The mount table may be huge, and on systems with utab we have to
	 * merge userspace mount options into /proc/self/mountinfo. This all is
	 * expensive. The tab filter can be used to filter out entries, then a mount
	 * table and utab are very tiny files.
	 *
	 * The filter uses mnt_fs_streq_{target,srcpath} function where all
	 * paths should be absolute and canonicalized. This is done within
	 * mnt_context_get_mountinfo_for_target() where LABEL, UUID or symlinks are
	 * canonicalized. If --no-canonicalize is enabled than the target path
	 * is expected already canonical.
	 *
	 * Anyway it's better to read huge mount table than canonicalize target
	 * paths. It means we use the filter only if --no-canonicalize enabled.
	 *
	 * It also means that we have to read mount table from kernel.
	 */
	if (mnt_context_is_nocanonicalize(cxt) && *tgt == '/')
		rc = mnt_context_get_mountinfo_for_target(cxt, &mountinfo, tgt);
	else
		rc = mnt_context_get_mountinfo(cxt, &mountinfo);

	if (rc) {
		DBG(CXT, ul_debugobj(cxt, "umount: failed to read mountinfo"));
		return rc;
	}

	if (mnt_table_get_nents(mountinfo) == 0) {
		DBG(CXT, ul_debugobj(cxt, "umount: mountinfo empty"));
		return 1;
	}

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

try_loopdev:
	fs = mnt_table_find_target(mountinfo, tgt, MNT_ITER_BACKWARD);
	if (!fs && mnt_context_is_swapmatch(cxt)) {
		/*
		 * Maybe the option is source rather than target (sometimes
		 * people use e.g. "umount /dev/sda1")
		 */
		fs = mnt_table_find_source(mountinfo, tgt, MNT_ITER_BACKWARD);

		if (fs) {
			struct libmnt_fs *fs1 = mnt_table_find_target(mountinfo,
							mnt_fs_get_target(fs),
							MNT_ITER_BACKWARD);
			if (!fs1) {
				DBG(CXT, ul_debugobj(cxt, "mountinfo is broken?!?!"));
				rc = -EINVAL;
				goto err;
			}
			if (fs != fs1) {
				/* Something was stacked over `file' on the
				 * same mount point. */
				DBG(CXT, ul_debugobj(cxt,
						"umount: %s: %s is mounted "
						"over it on the same point",
						tgt, mnt_fs_get_source(fs1)));
				rc = -EINVAL;
				goto err;
			}
		}
	}

	if (!fs && !loopdev && mnt_context_is_swapmatch(cxt)) {
		/*
		 * Maybe the option is /path/file.img, try to convert to /dev/loopN
		 */
		struct stat st;

		if (mnt_safe_stat(tgt, &st) == 0 && S_ISREG(st.st_mode)) {
			int count;
			struct libmnt_cache *cache = mnt_context_get_cache(cxt);
			const char *bf = cache ? mnt_resolve_path(tgt, cache) : tgt;

			count = loopdev_count_by_backing_file(bf, &loopdev);
			if (count == 1) {
				DBG(CXT, ul_debugobj(cxt,
					"umount: %s --> %s (retry)", tgt, loopdev));
				tgt = loopdev;
				goto try_loopdev;

			} else if (count > 1)
				DBG(CXT, ul_debugobj(cxt,
					"umount: warning: %s is associated "
					"with more than one loopdev", tgt));
		}
	}

	*pfs = fs;
	free(loopdev);
	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	DBG(CXT, ul_debugobj(cxt, "umount fs: %s", fs ? mnt_fs_get_target(fs) :
							"<not found>"));
	return fs ? 0 : 1;
err:
	free(loopdev);
	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;
	return rc;
}

/**
 * mnt_context_find_umount_fs:
 * @cxt: mount context
 * @tgt: mountpoint, device, ...
 * @pfs: returns point to filesystem
 *
 * Returns: 0 on success, <0 on error, 1 if target filesystem not found
 */
int mnt_context_find_umount_fs(struct libmnt_context *cxt,
			       const char *tgt,
			       struct libmnt_fs **pfs)
{
	if (pfs)
		*pfs = NULL;

	if (!cxt || !tgt || !pfs)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "umount: lookup FS for '%s'", tgt));

	if (!*tgt)
		return 1; /* empty string is not an error */

	/* In future this function should be extended to support for example
	 * fsinfo() (or another cheap way kernel will support), for now the
	 * default is expensive mountinfo.
	 */
	return __mountinfo_find_umount_fs(cxt, tgt, pfs);
}

/* Check if there is something important in the utab file. The parsed utab is
 * stored in context->utab and deallocated by mnt_free_context().
 *
 * This function exists to avoid (if possible) /proc/self/mountinfo usage, so
 * don't use things like mnt_resolve_target(), mnt_context_get_mountinfo() etc here.
 * See lookup_umount_fs() for more details.
 */
static int has_utab_entry(struct libmnt_context *cxt, const char *target)
{
	struct libmnt_cache *cache = NULL;
	struct libmnt_fs *fs;
	struct libmnt_iter itr;
	char *cn = NULL;
	int rc = 0;

	assert(cxt);

	if (!cxt->utab) {
		const char *path = mnt_get_utab_path();

		if (!path || is_file_empty(path))
			return 0;
		cxt->utab = mnt_new_table();
		if (!cxt->utab)
			return 0;
		cxt->utab->fmt = MNT_FMT_UTAB;
		if (mnt_table_parse_file(cxt->utab, path))
			return 0;
	}

	/* paths in utab are canonicalized */
	cache = mnt_context_get_cache(cxt);
	cn = mnt_resolve_path(target, cache);
	mnt_reset_iter(&itr, MNT_ITER_BACKWARD);

	while (mnt_table_next_fs(cxt->utab, &itr, &fs) == 0) {
		if (mnt_fs_streq_target(fs, cn)) {
			rc = 1;
			break;
		}
	}

	if (!cache)
		free(cn);
	return rc;
}

/* returns: 1 not found; <0 on error; 1 success */
static int lookup_umount_fs_by_statfs(struct libmnt_context *cxt, const char *tgt)
{
	struct stat st;
	const char *type;

	assert(cxt);
	assert(cxt->fs);

	DBG(CXT, ul_debugobj(cxt, " lookup by statfs"));

	/*
	 * Let's try to avoid mountinfo usage at all to minimize performance
	 * degradation. Don't forget that kernel has to compose *whole*
	 * mountinfo about all mountpoints although we look for only one entry.
	 *
	 * All we need is fstype and to check if there is no userspace mount
	 * options for the target (e.g. helper=udisks to call /sbin/umount.udisks).
	 *
	 * So, let's use statfs() if possible (it's bad idea for --lazy/--force
	 * umounts as target is probably unreachable NFS, also for --detach-loop
	 * as this additionally needs to know the name of the loop device).
	 *
	 * For the "umount --read-only" command, we need to read the mountinfo
	 * to obtain the mount source.
	 */
	if (mnt_context_is_restricted(cxt)
	    || *tgt != '/'
	    || mnt_context_within_helper(cxt)
	    || mnt_context_is_force(cxt)
	    || mnt_context_is_lazy(cxt)
	    || mnt_context_is_nocanonicalize(cxt)
	    || mnt_context_is_loopdel(cxt)
	    || mnt_context_is_rdonly_umount(cxt)
	    || mnt_safe_stat(tgt, &st) != 0 || !S_ISDIR(st.st_mode)
	    || has_utab_entry(cxt, tgt))
		return 1; /* not found */

	type = mnt_fs_get_fstype(cxt->fs);
	if (!type) {
		struct statfs vfs;
		int fd;

		DBG(CXT, ul_debugobj(cxt, "  trying fstatfs()"));

		/* O_PATH avoids triggering automount points. */
		fd = open(tgt, O_PATH);
		if (fd >= 0) {
			if (fstatfs(fd, &vfs) == 0)
				type = mnt_statfs_get_fstype(&vfs);
			close(fd);
		}
		if (type) {
			int rc = mnt_fs_set_fstype(cxt->fs, type);
			if (rc)
				return rc;
		}
	}
	if (type) {
		DBG(CXT, ul_debugobj(cxt, "  umount: disabling mountinfo"));
		mnt_context_disable_mtab(cxt, TRUE);

		DBG(CXT, ul_debugobj(cxt,
			"  mountinfo unnecessary [type=%s]", type));
		return 0;
	}

	return 1; /* not found */
}

/* returns: 1 not found; <0 on error; 1 success */
static int lookup_umount_fs_by_mountinfo(struct libmnt_context *cxt, const char *tgt)
{
	struct libmnt_fs *fs = NULL;
	int rc;

	assert(cxt);
	assert(cxt->fs);

	DBG(CXT, ul_debugobj(cxt, " lookup by mountinfo"));

	/* search */
	rc = __mountinfo_find_umount_fs(cxt, tgt, &fs);
	if (rc != 0)
		return rc;

	/* apply result */
	if (fs != cxt->fs) {
		mnt_fs_set_source(cxt->fs, NULL);
		mnt_fs_set_target(cxt->fs, NULL);

		if (!mnt_copy_fs(cxt->fs, fs)) {
			DBG(CXT, ul_debugobj(cxt, "  failed to copy FS"));
			return -errno;
		}
		DBG(CXT, ul_debugobj(cxt, "  mountinfo applied"));
	}

	cxt->flags |= MNT_FL_TAB_APPLIED;
	return 0;
}

/* This function searchs for FS according to cxt->fs->target,
 * apply result to cxt->fs and it's umount replacement to
 * mnt_context_apply_fstab(), use mnt_context_tab_applied()
 * to check result.
 *
 * The goal is to minimize situations when we need to parse
 * /proc/self/mountinfo.
 */
static int lookup_umount_fs(struct libmnt_context *cxt)
{
	const char *tgt;
	int rc = 0;

	assert(cxt);
	assert(cxt->fs);

	DBG(CXT, ul_debugobj(cxt, "umount: lookup FS"));

	tgt = mnt_fs_get_target(cxt->fs);
	if (!tgt) {
		DBG(CXT, ul_debugobj(cxt, " undefined target"));
		return -EINVAL;
	}

	/* try get fs type by statfs() */
	rc = lookup_umount_fs_by_statfs(cxt, tgt);
	if (rc <= 0)
		goto done;

	/* get complete fs from fs entry from mountinfo */
	rc = lookup_umount_fs_by_mountinfo(cxt, tgt);
	if (rc <= 0)
		goto done;

	DBG(CXT, ul_debugobj(cxt, " cannot find '%s'", tgt));
	return 0;	/* this is correct! */

done:
	if (rc == 0 && cxt->fs) {
		struct libmnt_optlist *ol = mnt_context_get_optlist(cxt);

		if (!ol)
			return -ENOMEM;

		rc = mnt_optlist_set_optstr(ol, mnt_fs_get_options(cxt->fs), NULL);
	}
	DBG(CXT, ul_debugobj(cxt, "  lookup done [rc=%d]", rc));
	return rc;
}

/* check if @devname is loopdev and if the device is associated
 * with a source from @fstab_fs
 */
static int is_associated_fs(const char *devname, struct libmnt_fs *fs)
{
	uintmax_t offset = 0;
	const char *src, *optstr;
	char *val;
	size_t valsz;
	int flags = 0;

	/* check if it begins with /dev/loop */
	if (strncmp(devname, _PATH_DEV_LOOP, sizeof(_PATH_DEV_LOOP) - 1) != 0)
		return 0;

	src = mnt_fs_get_srcpath(fs);
	if (!src)
		return 0;

	/* check for the offset option in @fs */
	optstr = mnt_fs_get_user_options(fs);

	if (optstr &&
	    mnt_optstr_get_option(optstr, "offset", &val, &valsz) == 0) {
		flags |= LOOPDEV_FL_OFFSET;

		if (mnt_parse_offset(val, valsz, &offset) != 0)
			return 0;
	}

	return loopdev_is_used(devname, src, offset, 0, flags);
}

/* returns: <0 on error; 1 not found (not wanted) */
static int prepare_helper_from_option(struct libmnt_context *cxt,
				       const char *name)
{
	struct libmnt_optlist *ol;
	struct libmnt_opt *opt;
	const char *suffix;

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	opt = mnt_optlist_get_named(ol, name, cxt->map_userspace);
	if (!opt || !mnt_opt_has_value(opt))
		return 1;

	suffix = mnt_opt_get_value(opt);
	DBG(CXT, ul_debugobj(cxt, "umount: umount.%s %s requested", suffix, name));

	return mnt_context_prepare_helper(cxt, "umount", suffix);
}

static int is_fuse_usermount(struct libmnt_context *cxt, int *errsv)
{
	struct libmnt_ns *ns_old;
	struct libmnt_optlist *ol;
	struct libmnt_opt *opt;
	const char *type = mnt_fs_get_fstype(cxt->fs);
	const char *val = NULL;;
	uid_t uid, entry_uid;

	*errsv = 0;

	if (!type)
		return 0;

	if (strcmp(type, "fuse") != 0 &&
	    strcmp(type, "fuseblk") != 0 &&
	    strncmp(type, "fuse.", 5) != 0 &&
	    strncmp(type, "fuseblk.", 8) != 0)
		return 0;

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return 0;

	opt = mnt_optlist_get_named(ol, "user_id", NULL);
	if (opt)
		val = mnt_opt_get_value(opt);
	if (!val || mnt_opt_get_map(opt))
		return 0;

	if (mnt_parse_uid(val, strlen(val), &entry_uid) != 0)
		return 0;

	/* get current user */
	ns_old = mnt_context_switch_origin_ns(cxt);
	if (!ns_old) {
		*errsv = -MNT_ERR_NAMESPACE;
		return 0;
	}

	uid = getuid();

	if (!mnt_context_switch_ns(cxt, ns_old)) {
		*errsv = -MNT_ERR_NAMESPACE;
		return 0;
	}

	return uid == entry_uid;
}

/*
 * Note that cxt->fs contains relevant mountinfo entry!
 */
static int evaluate_permissions(struct libmnt_context *cxt)
{
	unsigned long fstab_flags = 0;
	struct libmnt_table *fstab;
	const char *tgt, *src, *optstr;
	int rc = 0, ok = 0;
	struct libmnt_fs *fs;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!mnt_context_is_restricted(cxt))
		 return 0;		/* superuser mount */

	DBG(CXT, ul_debugobj(cxt, "umount: evaluating permissions"));

	if (!mnt_context_tab_applied(cxt)) {
		DBG(CXT, ul_debugobj(cxt,
				"cannot find %s in mountinfo and you are not root",
				mnt_fs_get_target(cxt->fs)));
		goto eperm;
	}

	if (!mnt_context_is_nohelpers(cxt)) {
		rc = prepare_helper_from_option(cxt, "uhelper");
		if (rc < 0)
			return rc;	/* error */
		if (rc == 0 && cxt->helper)
			return 0;	/* we'll call /sbin/umount.<uhelper> */
	}

	/*
	 * Check if this is a fuse mount for the current user,
	 * if so then unmounting is allowed
	 */
	if (is_fuse_usermount(cxt, &rc)) {
		DBG(CXT, ul_debugobj(cxt, "fuse user mount, umount is allowed"));
		return 0;
	}
	if (rc)
		return rc;

	/*
	 * User mounts have to be in /etc/fstab
	 */
	rc = mnt_context_get_fstab(cxt, &fstab);
	if (rc)
		return rc;

	tgt = mnt_fs_get_target(cxt->fs);
	src = mnt_fs_get_source(cxt->fs);

	if (mnt_fs_get_bindsrc(cxt->fs)) {
		src = mnt_fs_get_bindsrc(cxt->fs);
		DBG(CXT, ul_debugobj(cxt,
				"umount: using bind source: %s", src));
	}

	/* If fstab contains the two lines
	 *	/dev/sda1 /mnt/zip auto user,noauto  0 0
	 *	/dev/sda4 /mnt/zip auto user,noauto  0 0
	 * then "mount /dev/sda4" followed by "umount /mnt/zip" used to fail.
	 * So, we must not look for the file, but for the pair (dev,file) in fstab.
	  */
	fs = mnt_table_find_pair(fstab, src, tgt, MNT_ITER_FORWARD);
	if (!fs) {
		/*
		 * It's possible that there is /path/file.img in fstab and
		 * /dev/loop0 in mountinfo -- then we have to check the relation
		 * between loopdev and the file.
		 */
		fs = mnt_table_find_target(fstab, tgt, MNT_ITER_FORWARD);
		if (fs) {
			struct libmnt_cache *cache = mnt_context_get_cache(cxt);
			const char *sp = mnt_fs_get_srcpath(cxt->fs);		/* devname from mountinfo */
			const char *dev = sp && cache ? mnt_resolve_path(sp, cache) : sp;

			if (!dev || !is_associated_fs(dev, fs))
				fs = NULL;
		}
		if (!fs) {
			DBG(CXT, ul_debugobj(cxt,
					"umount %s: mountinfo disagrees with fstab",
					tgt));
			goto eperm;
		}
	}

	/*
	 * User mounting and unmounting is allowed only if fstab contains one
	 * of the options `user', `users' or `owner' or `group'.
	 *
	 * The option `users' allows arbitrary users to mount and unmount -
	 * this may be a security risk.
	 *
	 * The options `user', `owner' and `group' only allow unmounting by the
	 * user that mounted (visible in mountinfo).
	 */
	optstr = mnt_fs_get_user_options(fs);	/* FSTAB mount options! */
	if (!optstr)
		goto eperm;

	if (mnt_optstr_get_flags(optstr, &fstab_flags,
				mnt_get_builtin_optmap(MNT_USERSPACE_MAP)))
		goto eperm;

	if (fstab_flags & MNT_MS_USERS) {
		DBG(CXT, ul_debugobj(cxt,
			"umount: promiscuous setting ('users') in fstab"));
		return 0;
	}
	/*
	 * Check user=<username> setting from utab if there is a user, owner or
	 * group option in /etc/fstab
	 */
	if (fstab_flags & (MNT_MS_USER | MNT_MS_OWNER | MNT_MS_GROUP)) {

		struct libmnt_optlist *ol;
		struct libmnt_opt *opt;
		char *curr_user = NULL;
		struct libmnt_ns *ns_old;

		DBG(CXT, ul_debugobj(cxt,
				"umount: checking user=<username> from mountinfo"));

		ns_old = mnt_context_switch_origin_ns(cxt);
		if (!ns_old)
			return -MNT_ERR_NAMESPACE;

		curr_user = mnt_get_username(getuid());

		if (!mnt_context_switch_ns(cxt, ns_old)) {
			free(curr_user);
			return -MNT_ERR_NAMESPACE;
		}
		if (!curr_user) {
			DBG(CXT, ul_debugobj(cxt, "umount %s: cannot "
				"convert %d to username", tgt, getuid()));
			goto eperm;
		}

		/* get "user=" from utab */
		ol = mnt_context_get_optlist(cxt);
		if (!ol) {
			free(curr_user);
			return -ENOMEM;
		}
		opt = mnt_optlist_get_named(ol, "user", cxt->map_userspace);
		if (opt && mnt_opt_has_value(opt))
			ok = !strcmp(curr_user, mnt_opt_get_value(opt));

		free(curr_user);
	}

	if (ok) {
		DBG(CXT, ul_debugobj(cxt, "umount %s is allowed", tgt));
		return 0;
	}
eperm:
	DBG(CXT, ul_debugobj(cxt, "umount is not allowed for you"));
	return -EPERM;
}

static int exec_helper(struct libmnt_context *cxt)
{
	char *namespace = NULL;
	struct libmnt_ns *ns_tgt = mnt_context_get_target_ns(cxt);
	int rc;
	pid_t pid;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));
	assert(cxt->helper_exec_status == 1);

	if (mnt_context_is_fake(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "fake mode: does not execute helper"));
		cxt->helper_exec_status = rc = 0;
		return rc;
	}

	if (ns_tgt->fd != -1
	    && asprintf(&namespace, "/proc/%i/fd/%i",
			getpid(), ns_tgt->fd) == -1) {
		return -ENOMEM;
	}

	DBG_FLUSH;

	pid = fork();
	switch (pid) {
	case 0:
	{
		const char *args[12], *type;
		int i = 0;

		if (drop_permissions() != 0)
			_exit(EXIT_FAILURE);

		if (!mnt_context_switch_origin_ns(cxt))
			_exit(EXIT_FAILURE);

		type = mnt_fs_get_fstype(cxt->fs);

		args[i++] = cxt->helper;			/* 1 */
		args[i++] = mnt_fs_get_target(cxt->fs);		/* 2 */

		if (mnt_context_is_nomtab(cxt))
			args[i++] = "-n";			/* 3 */
		if (mnt_context_is_lazy(cxt))
			args[i++] = "-l";			/* 4 */
		if (mnt_context_is_force(cxt))
			args[i++] = "-f";			/* 5 */
		if (mnt_context_is_verbose(cxt))
			args[i++] = "-v";			/* 6 */
		if (mnt_context_is_rdonly_umount(cxt))
			args[i++] = "-r";			/* 7 */
		if (type
		    && strchr(type, '.')
		    && !endswith(cxt->helper, type)) {
			args[i++] = "-t";			/* 8 */
			args[i++] = type;			/* 9 */
		}
		if (namespace) {
			args[i++] = "-N";			/* 10 */
			args[i++] = namespace;			/* 11 */
		}

		args[i] = NULL;					/* 12 */
		for (i = 0; args[i]; i++)
			DBG(CXT, ul_debugobj(cxt, "argv[%d] = \"%s\"",
							i, args[i]));
		DBG_FLUSH;
		execv(cxt->helper, (char * const *) args);
		_exit(EXIT_FAILURE);
	}
	default:
	{
		int st;

		if (waitpid(pid, &st, 0) == (pid_t) -1) {
			cxt->helper_status = -1;
			rc = -errno;
		} else {
			cxt->helper_status = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
			cxt->helper_exec_status = rc = 0;
		}
		DBG(CXT, ul_debugobj(cxt, "%s executed [status=%d, rc=%d%s]",
				cxt->helper,
				cxt->helper_status, rc,
				rc ? " waitpid failed" : ""));
		break;
	}

	case -1:
		cxt->helper_exec_status = rc = -errno;
		DBG(CXT, ul_debugobj(cxt, "fork() failed"));
		break;
	}

	free(namespace);
	return rc;
}

/*
 * mnt_context_helper_setopt() backend.
 *
 * This function applies umount.type command line option (for example parsed
 * by getopt() or getopt_long()) to @cxt. All unknown options are ignored and
 * then 1 is returned.
 *
 * Returns: negative number on error, 1 if @c is unknown option, 0 on success.
 */
int mnt_context_umount_setopt(struct libmnt_context *cxt, int c, char *arg)
{
	int rc = -EINVAL;

	assert(cxt);
	assert(cxt->action == MNT_ACT_UMOUNT);

	switch(c) {
	case 'n':
		rc = mnt_context_disable_mtab(cxt, TRUE);
		break;
	case 'l':
		rc = mnt_context_enable_lazy(cxt, TRUE);
		break;
	case 'f':
		rc = mnt_context_enable_force(cxt, TRUE);
		break;
	case 'v':
		rc = mnt_context_enable_verbose(cxt, TRUE);
		break;
	case 'r':
		rc = mnt_context_enable_rdonly_umount(cxt, TRUE);
		break;
	case 't':
		if (arg)
			rc = mnt_context_set_fstype(cxt, arg);
		break;
	case 'N':
		if (arg)
			rc = mnt_context_set_target_ns(cxt, arg);
		break;
	default:
		return 1;
	}

	return rc;
}

/* Check whether the kernel supports the UMOUNT_NOFOLLOW flag */
static int umount_nofollow_support(void)
{
	int res = umount2("", UMOUNT_UNUSED);
	if (res != -1 || errno != EINVAL)
		return 0;

	res = umount2("", UMOUNT_NOFOLLOW);
	if (res != -1 || errno != ENOENT)
		return 0;

	return 1;
}

static int do_umount(struct libmnt_context *cxt)
{
	int rc = 0, flags = 0;
	const char *src, *target;
	char *tgtbuf = NULL;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));
	assert(cxt->syscall_status == 1);

	if (cxt->helper)
		return exec_helper(cxt);

	src = mnt_fs_get_srcpath(cxt->fs);
	target = mnt_fs_get_target(cxt->fs);

	if (!target)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "do umount"));

	if (mnt_context_is_restricted(cxt) && !mnt_context_is_fake(cxt)) {
		/*
		 * extra paranoia for non-root users
		 * -- chdir to the parent of the mountpoint and use NOFOLLOW
		 *    flag to avoid races and symlink attacks.
		 */
		if (umount_nofollow_support())
			flags |= UMOUNT_NOFOLLOW;

		rc = mnt_chdir_to_parent(target, &tgtbuf);
		if (rc)
			return rc;
		target = tgtbuf;
	}

	if (mnt_context_is_lazy(cxt))
		flags |= MNT_DETACH;

	if (mnt_context_is_force(cxt))
		flags |= MNT_FORCE;

	DBG(CXT, ul_debugobj(cxt, "umount(2) [target='%s', flags=0x%08x]%s",
				target, flags,
				mnt_context_is_fake(cxt) ? " (FAKE)" : ""));

	if (mnt_context_is_fake(cxt))
		rc = 0;
	else {
		rc = flags ? umount2(target, flags) : umount(target);
		if (rc < 0)
			cxt->syscall_status = -errno;
		free(tgtbuf);
	}

	/*
	 * try remount read-only
	 */
	if (rc < 0
	    && cxt->syscall_status == -EBUSY
	    && mnt_context_is_rdonly_umount(cxt)
	    && src) {
		struct libmnt_optlist *ol = mnt_context_get_optlist(cxt);

		/* keep info about remount in mount flags */
		assert(ol);
		mnt_optlist_append_flags(ol, MS_REMOUNT | MS_RDONLY, cxt->map_linux);

		mnt_context_enable_loopdel(cxt, FALSE);

		DBG(CXT, ul_debugobj(cxt,
			"umount(2) failed [errno=%d] -- trying to remount read-only",
			-cxt->syscall_status));

		rc = mount(src, mnt_fs_get_target(cxt->fs), NULL,
			    MS_REMOUNT | MS_RDONLY, NULL);
		if (rc < 0) {
			cxt->syscall_status = -errno;
			DBG(CXT, ul_debugobj(cxt,
				"read-only re-mount(2) failed [errno=%d]",
				-cxt->syscall_status));

			return -cxt->syscall_status;
		}
		cxt->syscall_status = 0;
		DBG(CXT, ul_debugobj(cxt, "read-only re-mount(2) success"));
		return 0;
	}

	if (rc < 0) {
		DBG(CXT, ul_debugobj(cxt, "umount(2) failed [errno=%d]",
			-cxt->syscall_status));
		return -cxt->syscall_status;
	}

	cxt->syscall_status = 0;
	DBG(CXT, ul_debugobj(cxt, "umount(2) success"));
	return 0;
}

/**
 * mnt_context_prepare_umount:
 * @cxt: mount context
 *
 * Prepare context for umounting, unnecessary for mnt_context_umount().
 *
 * Returns: 0 on success, and negative number in case of error.
 */
int mnt_context_prepare_umount(struct libmnt_context *cxt)
{
	int rc;
	unsigned long flags = 0;
	struct libmnt_ns *ns_old;

	if (!cxt || !cxt->fs || mnt_fs_is_swaparea(cxt->fs))
		return -EINVAL;
	if (!mnt_context_get_source(cxt) && !mnt_context_get_target(cxt))
		return -EINVAL;
	if (cxt->flags & MNT_FL_PREPARED)
		return 0;

	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);

	free(cxt->helper);	/* be paranoid */
	cxt->helper = NULL;
	cxt->action = MNT_ACT_UMOUNT;

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	rc = lookup_umount_fs(cxt);
	if (!rc)
		rc = mnt_context_merge_mflags(cxt);
	if (!rc)
		rc = evaluate_permissions(cxt);

	if (!rc && !mnt_context_is_nohelpers(cxt) && !cxt->helper) {
		/* on helper= mount option based helper */
		rc = prepare_helper_from_option(cxt, "helper");
		if (rc < 0)
			return rc;
		if (!cxt->helper)
			/* on fstype based helper */
			rc = mnt_context_prepare_helper(cxt, "umount", NULL);
	}

	if (!rc)
		rc = mnt_context_get_user_mflags(cxt, &flags);

	if (!rc && (flags & MNT_MS_LOOP))
		/* loop option explicitly specified in utab, detach this loop */
		mnt_context_enable_loopdel(cxt, TRUE);

	if (!rc && mnt_context_is_loopdel(cxt) && cxt->fs) {
		const char *src = mnt_fs_get_srcpath(cxt->fs);

		if (src && (!is_loopdev(src) || loopdev_is_autoclear(src)))
			mnt_context_enable_loopdel(cxt, FALSE);
	}

	if (rc) {
		DBG(CXT, ul_debugobj(cxt, "umount: preparing failed"));
		return rc;
	}
	cxt->flags |= MNT_FL_PREPARED;

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	return rc;
}

/**
 * mnt_context_do_umount:
 * @cxt: mount context
 *
 * Umount filesystem by umount(2) or fork()+exec(/sbin/umount.type).
 * Unnecessary for mnt_context_umount().
 *
 * See also mnt_context_disable_helpers().
 *
 * WARNING: non-zero return code does not mean that umount(2) syscall or
 *          umount.type helper wasn't successfully called.
 *
 *          Check mnt_context_get_status() after error!
*
 * Returns: 0 on success;
 *         >0 in case of umount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
int mnt_context_do_umount(struct libmnt_context *cxt)
{
	int rc;
	struct libmnt_ns *ns_old;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);
	assert((cxt->flags & MNT_FL_PREPARED));
	assert((cxt->action == MNT_ACT_UMOUNT));
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	rc = do_umount(cxt);
	if (rc)
		goto end;

	if (mnt_context_get_status(cxt) && !mnt_context_is_fake(cxt)) {
		/*
		 * Umounted, do some post-umount operations
		 *	- remove loopdev
		 *	- refresh in-memory utab stuff if remount rather than
		 *	  umount has been performed
		 */
		if (mnt_context_is_loopdel(cxt)
		    && !mnt_optlist_is_remount(cxt->optlist))
			rc = mnt_context_delete_loopdev(cxt);
	}
end:
	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	return rc;
}

/**
 * mnt_context_finalize_umount:
 * @cxt: context
 *
 * Mtab update, etc. Unnecessary for mnt_context_umount(), but should be called
 * after mnt_context_do_umount(). See also mnt_context_set_syscall_status().
 *
 * Returns: negative number on error, 0 on success.
 */
int mnt_context_finalize_umount(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_PREPARED));
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	rc = mnt_context_prepare_update(cxt);
	if (!rc)
		rc = mnt_context_update_tabs(cxt);
	return rc;
}


/**
 * mnt_context_umount:
 * @cxt: umount context
 *
 * High-level, umounts filesystem by umount(2) or fork()+exec(/sbin/umount.type).
 *
 * This is similar to:
 *
 *	mnt_context_prepare_umount(cxt);
 *	mnt_context_do_umount(cxt);
 *	mnt_context_finalize_umount(cxt);
 *
 * See also mnt_context_disable_helpers().
 *
 * WARNING: non-zero return code does not mean that umount(2) syscall or
 *          umount.type helper wasn't successfully called.
 *
 *          Check mnt_context_get_status() after error!
 *
 * Returns: 0 on success;
 *         >0 in case of umount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
int mnt_context_umount(struct libmnt_context *cxt)
{
	int rc;
	struct libmnt_ns *ns_old;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);

	DBG(CXT, ul_debugobj(cxt, "umount: %s", mnt_context_get_target(cxt)));

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	rc = mnt_context_prepare_umount(cxt);
	if (!rc)
		rc = mnt_context_prepare_update(cxt);
	if (!rc)
		rc = mnt_context_do_umount(cxt);
	if (!rc)
		rc = mnt_context_update_tabs(cxt);

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	return rc;
}


/**
 * mnt_context_next_umount:
 * @cxt: context
 * @itr: iterator
 * @fs: returns the current filesystem
 * @mntrc: returns the return code from mnt_context_umount()
 * @ignored: returns 1 for not matching
 *
 * This function tries to umount the next filesystem from mountinfo file.
 *
 * You can filter out filesystems by:
 *	mnt_context_set_options_pattern() to simulate umount -a -O pattern
 *	mnt_context_set_fstype_pattern()  to simulate umount -a -t pattern
 *
 * If the filesystem is not mounted or does not match the defined criteria,
 * then the function mnt_context_next_umount() returns zero, but the @ignored is
 * non-zero. Note that the root filesystem is always ignored.
 *
 * If umount(2) syscall or umount.type helper failed, then the
 * mnt_context_next_umount() function returns zero, but the @mntrc is non-zero.
 * Use also mnt_context_get_status() to check if the filesystem was
 * successfully umounted.
 *
 * Returns: 0 on success,
 *         <0 in case of error (!= umount(2) errors)
 *          1 at the end of the list.
 */
int mnt_context_next_umount(struct libmnt_context *cxt,
			   struct libmnt_iter *itr,
			   struct libmnt_fs **fs,
			   int *mntrc,
			   int *ignored)
{
	struct libmnt_table *mountinfo;
	const char *tgt;
	int rc;

	if (ignored)
		*ignored = 0;
	if (mntrc)
		*mntrc = 0;

	if (!cxt || !fs || !itr)
		return -EINVAL;

	rc = mnt_context_get_mountinfo(cxt, &mountinfo);
	cxt->mountinfo = NULL;		/* do not reset mountinfo */
	mnt_reset_context(cxt);

	if (rc)
		return rc;

	cxt->mountinfo = mountinfo;

	do {
		rc = mnt_table_next_fs(mountinfo, itr, fs);
		if (rc != 0)
			return rc;	/* no more filesystems (or error) */

		tgt = mnt_fs_get_target(*fs);
	} while (!tgt);

	DBG(CXT, ul_debugobj(cxt, "next-umount: trying %s [fstype: %s, t-pattern: %s, options: %s, O-pattern: %s]", tgt,
				 mnt_fs_get_fstype(*fs), cxt->fstype_pattern, mnt_fs_get_options(*fs), cxt->optstr_pattern));

	/* ignore filesystems which don't match options patterns */
	if ((cxt->fstype_pattern && !mnt_fs_match_fstype(*fs,
					cxt->fstype_pattern)) ||

	/* ignore filesystems which don't match type patterns */
	   (cxt->optstr_pattern && !mnt_fs_match_options(*fs,
					cxt->optstr_pattern))) {
		if (ignored)
			*ignored = 1;

		DBG(CXT, ul_debugobj(cxt, "next-umount: not-match"));
		return 0;
	}

	rc = mnt_context_set_fs(cxt, *fs);
	if (rc)
		return rc;
	rc = mnt_context_umount(cxt);
	if (mntrc)
		*mntrc = rc;
	return 0;
}


int mnt_context_get_umount_excode(
			struct libmnt_context *cxt,
			int rc,
			char *buf,
			size_t bufsz)
{
	if (mnt_context_helper_executed(cxt))
		/*
		 * /sbin/umount.<type> called, return status
		 */
		return mnt_context_get_helper_status(cxt);

	if (rc == 0 && mnt_context_get_status(cxt) == 1)
		/*
		 * Libmount success && syscall success.
		 */
		return MNT_EX_SUCCESS;

	if (!mnt_context_syscall_called(cxt)) {
		/*
		 * libmount errors (extra library checks)
		 */
		if (rc == -EPERM && !mnt_context_tab_applied(cxt)) {
			/* failed to evaluate permissions because not found
			 * relevant entry in mountinfo */
			if (buf)
				snprintf(buf, bufsz, _("not mounted"));
			return MNT_EX_USAGE;
		}

		if (rc == -MNT_ERR_LOCK) {
			if (buf)
				snprintf(buf, bufsz, _("locking failed"));
			return MNT_EX_FILEIO;
		}

		if (rc == -MNT_ERR_NAMESPACE) {
			if (buf)
				snprintf(buf, bufsz, _("failed to switch namespace"));
			return MNT_EX_SYSERR;
		}
		return mnt_context_get_generic_excode(rc, buf, bufsz,
					_("umount failed: %m"));

	} if (mnt_context_get_syscall_errno(cxt) == 0) {
		/*
		 * umount(2) syscall success, but something else failed
		 * (probably error in utab processing).
		 */
		if (rc == -MNT_ERR_LOCK) {
			if (buf)
				snprintf(buf, bufsz, _("filesystem was unmounted, but failed to update userspace mount table"));
			return MNT_EX_FILEIO;
		}

		if (rc == -MNT_ERR_NAMESPACE) {
			if (buf)
				snprintf(buf, bufsz, _("filesystem was unmounted, but failed to switch namespace back"));
			return MNT_EX_SYSERR;

		}

		if (rc < 0)
			return mnt_context_get_generic_excode(rc, buf, bufsz,
				_("filesystem was unmounted, but any subsequent operation failed: %m"));

		return MNT_EX_SOFTWARE;	/* internal error */
	}

	/*
	 * umount(2) errors
	 */
	if (buf) {
		int syserr = mnt_context_get_syscall_errno(cxt);

		switch (syserr) {
		case ENXIO:
			snprintf(buf, bufsz, _("invalid block device"));	/* ??? */
			break;
		case EINVAL:
			snprintf(buf, bufsz, _("not mounted"));
			break;
		case EIO:
			snprintf(buf, bufsz, _("can't write superblock"));
			break;
		case EBUSY:
			snprintf(buf, bufsz, _("target is busy"));
			break;
		case ENOENT:
			snprintf(buf, bufsz, _("no mount point specified"));
			break;
		case EPERM:
			snprintf(buf, bufsz, _("must be superuser to unmount"));
			break;
		case EACCES:
			snprintf(buf, bufsz, _("block devices are not permitted on filesystem"));
			break;
		default:
			return mnt_context_get_generic_excode(syserr, buf, bufsz,_("umount(2) system call failed: %m"));
		}
	}
	return MNT_EX_FAIL;
}
