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
 * This is X-mount.subdir= implementation. The code uses global hookset data
 * rather than per-callback (hook) data.
 *
 * Please, see the comment in libmount/src/hooks.c to understand how hooks work.
 */
#include <sched.h>

#include "mountP.h"
#include "fileutils.h"
#include "mount-api-utils.h"

struct hookset_data {
	char *subdir;
	char *org_target;
	int old_ns_fd;
	int new_ns_fd;
	unsigned int tmp_umounted : 1;
};

static int tmptgt_cleanup(struct hookset_data *);

static void free_hookset_data(	struct libmnt_context *cxt,
				const struct libmnt_hookset *hs)
{
	struct hookset_data *hsd = mnt_context_get_hookset_data(cxt, hs);

	if (!hsd)
		return;
	if (hsd->old_ns_fd >= 0)
		tmptgt_cleanup(hsd);

	free(hsd->org_target);
	free(hsd->subdir);
	free(hsd);

	mnt_context_set_hookset_data(cxt, hs, NULL);
}

/* global data, used by all callbacks */
static struct hookset_data *new_hookset_data(
				struct libmnt_context *cxt,
				const struct libmnt_hookset *hs)
{
	struct hookset_data *hsd = calloc(1, sizeof(struct hookset_data));

	if (hsd && mnt_context_set_hookset_data(cxt, hs, hsd) != 0) {
		/* probably ENOMEM problem */
		free(hsd);
		hsd = NULL;
	}
	return hsd;
}

/* de-initiallize this module */
static int hookset_deinit(struct libmnt_context *cxt, const struct libmnt_hookset *hs)
{
	DBG(HOOK, ul_debugobj(hs, "deinit '%s'", hs->name));

	/* remove all our hooks */
	while (mnt_context_remove_hook(cxt, hs, 0, NULL) == 0);

	/* free and remove global hookset data */
	free_hookset_data(cxt, hs);

	return 0;
}

/*
 * Initialize MNT_PATH_TMPTGT; mkdir, create a new namespace and
 * mark (bind mount) the directory as private.
 */
static int tmptgt_unshare(struct hookset_data *hsd)
{
#ifdef USE_LIBMOUNT_SUPPORT_NAMESPACES
	int rc = 0;

	hsd->old_ns_fd = hsd->new_ns_fd = -1;

	/* create directory */
	rc = ul_mkdir_p(MNT_PATH_TMPTGT, S_IRWXU);
	if (rc)
		goto fail;

	/* remember the current namespace */
	hsd->old_ns_fd = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
	if (hsd->old_ns_fd < 0)
		goto fail;

	/* create new namespace */
	if (unshare(CLONE_NEWNS) != 0)
		goto fail;

	/* try to set top-level directory as private, this is possible if
	 * MNT_RUNTIME_TOPDIR (/run) is a separated filesystem. */
	if (mount("none", MNT_RUNTIME_TOPDIR, NULL, MS_PRIVATE, NULL) != 0) {

		/* failed; create a mountpoint from MNT_PATH_TMPTGT */
		if (mount(MNT_PATH_TMPTGT, MNT_PATH_TMPTGT, "none", MS_BIND, NULL) != 0)
			goto fail;
		if (mount("none", MNT_PATH_TMPTGT, NULL, MS_PRIVATE, NULL) != 0)
			goto fail;
	}

	/* remember the new namespace */
	hsd->new_ns_fd = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
	if (hsd->new_ns_fd < 0)
		goto fail;

	DBG(UTILS, ul_debug(MNT_PATH_TMPTGT " unshared"));
	return 0;
fail:
	if (rc == 0)
		rc = errno ? -errno : -EINVAL;

	tmptgt_cleanup(hsd);
	DBG(UTILS, ul_debug(MNT_PATH_TMPTGT " unshare failed"));
	return rc;
#else
	return -ENOSYS;
#endif
}

/*
 * Clean up MNT_PATH_TMPTGT; umount and switch back to old namespace
 */
static int tmptgt_cleanup(struct hookset_data *hsd)
{
#ifdef USE_LIBMOUNT_SUPPORT_NAMESPACES
	if (!hsd->tmp_umounted) {
		umount(MNT_PATH_TMPTGT);
		hsd->tmp_umounted = 1;
	}

	if (hsd->new_ns_fd >= 0)
		close(hsd->new_ns_fd);

	if (hsd->old_ns_fd >= 0) {
		setns(hsd->old_ns_fd, CLONE_NEWNS);
		close(hsd->old_ns_fd);
	}

	hsd->new_ns_fd = hsd->old_ns_fd = -1;
	DBG(UTILS, ul_debug(MNT_PATH_TMPTGT " cleanup done"));
	return 0;
#else
	return -ENOSYS;
#endif
}

/*
 * Attach (move) MNT_PATH_TMPTGT/subdir to the parental namespace.
 */
static int do_mount_subdir(
			struct libmnt_context *cxt,
			struct hookset_data *hsd,
			const char *root)
{
	int rc = 0;
	const char *subdir = hsd->subdir;
	const char *target;

#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT
	struct libmnt_sysapi *api = mnt_context_get_sysapi(cxt);

	/* fallback only; necessary when hook_mount.c during preparation
	 * cannot open the tree -- for example when we call /sbin/mount.<type> */
	if (api && api->fd_tree < 0) {
		api->fd_tree = mnt_context_open_tree(cxt, NULL, (unsigned long) -1);
		if (api->fd_tree < 0)
			return api->fd_tree;
	}
#endif
	/* reset to the original mountpoint */
	mnt_fs_set_target(cxt->fs, hsd->org_target);
	target = mnt_fs_get_target(cxt->fs);

#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT
	if (api && api->fd_tree >= 0) {
		/* FD based way - unfortunately, it's impossible to open
		 * sub-directory on not-yet attached mount. It means
		 * hook_mount.c attaches FS to temporary directory, and we
		 * clone and move the subdir, and umount the old unshared
		 * temporary tree.
		 *
		 * The old mount(2) way does the same, but by BIND.
		 */
		int fd;

		DBG(HOOK, ul_debug("attach subdir '%s'", subdir));
		fd = open_tree(api->fd_tree, subdir,
					OPEN_TREE_CLOEXEC | OPEN_TREE_CLONE);
		set_syscall_status(cxt, "open_tree", fd >= 0);
		if (fd < 0)
			rc = -errno;

		if (!rc) {
			/* Note that the original parental namespace could be
			 * private, in this case, it will not see our final mount,
			 * so we need to move the the orignal namespace.
			 */
			setns(hsd->old_ns_fd, CLONE_NEWNS);

			rc = move_mount(fd, "", AT_FDCWD, target, MOVE_MOUNT_F_EMPTY_PATH);
			set_syscall_status(cxt, "move_mount", rc == 0);
			if (rc)
				rc = -errno;

			/* And move back to our private namespace to cleanup */
			setns(hsd->new_ns_fd, CLONE_NEWNS);
		}
		if (!rc) {
			close(api->fd_tree);
			api->fd_tree = fd;
		}
	} else
#endif
	{
		char *src = NULL;

		if (asprintf(&src, "%s/%s", root, subdir) < 0)
			return -ENOMEM;

		/* Classic mount(2) based way */
		DBG(HOOK, ul_debug("mount subdir %s to %s", src, target));
		rc = mount(src, target, NULL, MS_BIND, NULL);

		set_syscall_status(cxt, "mount", rc == 0);
		if (rc)
			rc = -errno;
		free(src);
	}

	if (!rc) {
		DBG(HOOK, ul_debug("umount old root %s", root));
		rc = umount(root);
		set_syscall_status(cxt, "umount", rc == 0);
		if (rc)
			rc = -errno;
		hsd->tmp_umounted = 1;

	}

	return rc;
}


static int hook_mount_post(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct hookset_data *hsd;
	int rc = 0;

	hsd = mnt_context_get_hookset_data(cxt, hs);
	if (!hsd || !hsd->subdir)
		return 0;

	/* bind subdir to the real target, umount temporary target */
	rc = do_mount_subdir(cxt, hsd, MNT_PATH_TMPTGT);
	if (rc)
		return rc;

	tmptgt_cleanup(hsd);

	return rc;
}

static int hook_mount_pre(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct hookset_data *hsd;
	int rc = 0;

	hsd = mnt_context_get_hookset_data(cxt, hs);
	if (!hsd)
		return 0;

	/* create unhared temporary target */
	hsd->org_target = strdup(mnt_fs_get_target(cxt->fs));
	if (!hsd->org_target)
		rc = -ENOMEM;
	if (!rc)
		rc = tmptgt_unshare(hsd);
	if (!rc)
		mnt_fs_set_target(cxt->fs, MNT_PATH_TMPTGT);
	if (!rc)
		rc = mnt_context_append_hook(cxt, hs,
				MNT_STAGE_MOUNT_POST,
				NULL, hook_mount_post);

	DBG(HOOK, ul_debugobj(hs, "unshared tmp target %s [rc=%d]",
				MNT_PATH_TMPTGT, rc));
	return rc;
}



static int is_subdir_required(struct libmnt_context *cxt, int *rc, char **subdir)
{
	struct libmnt_optlist *ol;
	struct libmnt_opt *opt;
	const char *dir = NULL;

	assert(cxt);
	assert(rc);

	*rc = 0;

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	opt = mnt_optlist_get_named(ol, "X-mount.subdir", cxt->map_userspace);
	if (!opt)
		return 0;

	dir = mnt_opt_get_value(opt);

	if (dir && *dir == '"')
		dir++;

	if (!dir || !*dir) {
		DBG(HOOK, ul_debug("failed to parse X-mount.subdir '%s'", dir));
		*rc = -MNT_ERR_MOUNTOPT;
	} else {
		*subdir = strdup(dir);
		if (!*subdir)
			*rc = -ENOMEM;
	}

	return *rc == 0;
}

/* this is the initial callback used to check mount options and define next
 * actions if necessary */
static int hook_prepare_target(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	const char *tgt;
	char *subdir = NULL;
	int rc = 0;

	assert(cxt);

	tgt = mnt_fs_get_target(cxt->fs);
	if (!tgt)
		return 0;

	if (cxt->action == MNT_ACT_MOUNT
	    && is_subdir_required(cxt, &rc, &subdir)) {

		/* create a global data */
		struct hookset_data *hsd = new_hookset_data(cxt, hs);

		if (!hsd) {
			free(subdir);
			return -ENOMEM;
		}
		hsd->subdir = subdir;

		DBG(HOOK, ul_debugobj(hs, "subdir %s wanted", subdir));

		rc = mnt_context_append_hook(cxt, hs,
				MNT_STAGE_MOUNT_PRE,
				NULL, hook_mount_pre);
	}

	return rc;
}

const struct libmnt_hookset hookset_subdir =
{
	.name = "__subdir",

	.firststage = MNT_STAGE_PREP_TARGET,
	.firstcall = hook_prepare_target,

	.deinit = hookset_deinit
};
