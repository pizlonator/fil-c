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
 * This is fsconfig/fsopen based mount.
 *
 * Please, see the comment in libmount/src/hooks.c to understand how hooks work.
 *
 * Operations: functions and STAGE, all is prepared in hook_prepare():
 *
 * mount:
 *	- fsopen	PRE
 *	- fsmount	MOUNT
 *	- mount_setattr MOUNT (VFS flags)
 *	- mount_move	POST
 *	- mount_setattr POST (propagation)
 *
 * remount:
 *	- open_tree	PRE
 *	- fsconfig      MOUNT (FS reconfigure)
 *	- mount_setattr	MOUNT (VFS flags)
 *	- mount_setattr POST (propagation)
 *
 * propagation-only:
 *	- open_tree	PRE
 *	- mount_setattr	POST (propagation)
 *
 * move:
 *	- open_tree	PRE
 *	- mount_move	POST
 *
 * bind:
 *	- open_tree	PRE (clone)
 *	- mount_setattr MOUNT (VFS flags)
 *	- mount_move	POST
 */

#include "mountP.h"
#include "fileutils.h"	/* statx() fallback */
#include "strutils.h"
#include "mount-api-utils.h"
#include "linux_version.h"

#include <inttypes.h>

#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT

#define get_sysapi(_cxt) mnt_context_get_sysapi(_cxt)

static void close_sysapi_fds(struct libmnt_sysapi *api)
{
	if (api->fd_fs >= 0)
		close(api->fd_fs);
	if (api->fd_tree >= 0)
		close(api->fd_tree);

	api->fd_tree = api->fd_fs = -1;
}

static void save_fd_messages(struct libmnt_context *cxt, int fd)
{
	uint8_t buf[BUFSIZ];
	int rc;

	free(cxt->syscall_errmsg);
	cxt->syscall_errmsg = NULL;

	while ((rc = read(fd, buf, sizeof(buf) - 1)) != -1) {

		if (rc == 0)
			continue;
		if (buf[rc - 1] == '\n')
			buf[--rc] = '\0';
		else
			buf[rc] = '\0';

		DBG(CXT, ul_debug("message from kernel: \"%*s\"", rc, buf));

		if (rc < 3 || strncmp((char *) buf, "e ", 2) != 0)
			continue;
		if (cxt->syscall_errmsg)
			strappend(&cxt->syscall_errmsg, "; ");

		strappend(&cxt->syscall_errmsg, ((char *) buf) + 2);
	}
}

static void hookset_set_syscall_status(struct libmnt_context *cxt,
				       const char *name, int x)
{
	struct libmnt_sysapi *api;

	set_syscall_status(cxt, name, x);

	if (!x) {
		api = get_sysapi(cxt);
		if (api && api->fd_fs >= 0)
			save_fd_messages(cxt, api->fd_fs);
	}
}

/*
 * This hookset uses 'struct libmnt_sysapi' (mountP.h) as hookset data.
 */
static void free_hookset_data(	struct libmnt_context *cxt,
				const struct libmnt_hookset *hs)
{
	struct libmnt_sysapi *api = mnt_context_get_hookset_data(cxt, hs);

	if (!api)
		return;

	close_sysapi_fds(api);

	free(api);
	mnt_context_set_hookset_data(cxt, hs, NULL);
}

/* global data, used by all callbacks */
static struct libmnt_sysapi *new_hookset_data(
				struct libmnt_context *cxt,
				const struct libmnt_hookset *hs)
{
	struct libmnt_sysapi *api = calloc(1, sizeof(struct libmnt_sysapi));

	if (!api)
		return NULL;
	api->fd_fs = api->fd_tree = -1;

	if (mnt_context_set_hookset_data(cxt, hs, api) != 0) {
		/* probably ENOMEM problem */
		free(api);
		api = NULL;
	}
	return api;
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

static inline int fsconfig_set_value(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			int fd,
			const char *name, const char *value)
{
	int rc;
	char *s = NULL;

	/* "\," is a way to use comma in values, let's remove \ escape */
	if (value && strstr(value, "\\,")) {
		char *x, *p;

		s = strdup(value);
		if (!s)
			return -EINVAL;
		for (x = p = s; *x; p++, x++) {
			if (*x == '\\' && *(x + 1) == ',')
				x++;
			*p = *x;
		}
		*p = '\0';
		value = s;
	}

	DBG(HOOK, ul_debugobj(hs, "  fsconfig(name=\"%s\" value=\"%s\")", name,
				value ? : ""));
	if (value) {
		rc = fsconfig(fd, FSCONFIG_SET_STRING, name, value, 0);
		free(s);
	} else
		rc = fsconfig(fd, FSCONFIG_SET_FLAG, name, NULL, 0);

	hookset_set_syscall_status(cxt, "fsconfig", rc == 0);
	return rc;
}

static int configure_superblock(struct libmnt_context *cxt,
				const struct libmnt_hookset *hs,
				int fd, int force_rwro)
{
	struct libmnt_optlist *ol;
	struct libmnt_iter itr;
	struct libmnt_opt *opt;
	int rc = 0, has_rwro = 0;

	DBG(HOOK, ul_debugobj(hs, " config FS"));

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_optlist_next_opt(ol, &itr, &opt) == 0) {
		const char *name = mnt_opt_get_name(opt);
		const char *value = mnt_opt_get_value(opt);
		const struct libmnt_optmap *ent = mnt_opt_get_mapent(opt);
		const int is_linux = ent && mnt_opt_get_map(opt) == cxt->map_linux;

		if (is_linux && ent->id == MS_RDONLY) {
			/* Use ro/rw for superblock (for backward compatibility) */
			value = NULL;
			has_rwro = 1;

		} else if (is_linux && ent->mask & MNT_SUPERBLOCK) {
			/* Use some old MS_* (VFS) flags as superblock flags */
			;

		} else if (!name || mnt_opt_get_map(opt) || mnt_opt_is_external(opt))
			/* Ignore VFS flags, userspace and external options */
			continue;

		if (!value && mnt_opt_is_sepnodata(opt))
			value = "";	/* force use the value as string */

		rc = fsconfig_set_value(cxt, hs, fd, name, value);
		if (rc != 0)
			goto done;
	}

	if (force_rwro && !has_rwro)
		rc = fsconfig_set_value(cxt, hs, fd, "rw", NULL);

done:
	DBG(HOOK, ul_debugobj(hs, " config done [rc=%d]", rc));
	return rc != 0 && errno ? -errno : rc;
}

static int open_fs_configuration_context(struct libmnt_context *cxt,
					 struct libmnt_sysapi *api,
					 const char *type)
{
	DBG(HOOK, ul_debug(" new FS '%s'", type));

	if (!type)
		return -EINVAL;

	DBG(HOOK, ul_debug(" fsopen(%s)", type));

	api->fd_fs = fsopen(type, FSOPEN_CLOEXEC);
	hookset_set_syscall_status(cxt, "fsopen", api->fd_fs >= 0);
	if (api->fd_fs < 0)
		return -errno;
	api->is_new_fs = 1;
	return api->fd_fs;
}

static int hook_create_mount(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct libmnt_sysapi *api;
	const char *src;
	int rc = 0;

	assert(cxt);

	if (mnt_context_helper_executed(cxt))
		return 0;

	assert(cxt->fs);

	api = get_sysapi(cxt);
	assert(api);

	if (api->fd_fs < 0) {
		const char *type = mnt_fs_get_fstype(cxt->fs);

		rc = open_fs_configuration_context(cxt, api, type);
		if (rc < 0) {
			rc = api->fd_fs;
			goto done;
		}
	}

	src = mnt_fs_get_srcpath(cxt->fs);
	if (!src)
		return -EINVAL;

	DBG(HOOK, ul_debugobj(hs, "init FS"));

	rc = fsconfig(api->fd_fs, FSCONFIG_SET_STRING, "source", src, 0);
	hookset_set_syscall_status(cxt, "fsconfig", rc == 0);

	if (!rc)
		rc = configure_superblock(cxt, hs, api->fd_fs, 0);
	if (!rc) {
		DBG(HOOK, ul_debugobj(hs, "create FS"));
		rc = fsconfig(api->fd_fs, FSCONFIG_CMD_CREATE, NULL, NULL, 0);
		hookset_set_syscall_status(cxt, "fsconfig", rc == 0);
	}

	if (!rc) {
		api->fd_tree = fsmount(api->fd_fs, FSMOUNT_CLOEXEC, 0);
		hookset_set_syscall_status(cxt, "fsmount", api->fd_tree >= 0);
		if (api->fd_tree < 0)
			rc = -errno;
	}

	if (rc)
		/* cleanup after fail (libmount may only try the FS type) */
		close_sysapi_fds(api);

#if defined(HAVE_STATX) && defined(HAVE_STRUCT_STATX) && defined(HAVE_STRUCT_STATX_STX_MNT_ID)
	if (!rc && cxt->fs) {
		struct statx st;

		rc = statx(api->fd_tree, "", AT_EMPTY_PATH, STATX_MNT_ID, &st);
		if (rc == 0) {
			cxt->fs->id = (int) st.stx_mnt_id;
			if (cxt->update) {
				struct libmnt_fs *fs = mnt_update_get_fs(cxt->update);
				if (fs)
					fs->id = cxt->fs->id;
			}
		}
	}
#endif

done:
	DBG(HOOK, ul_debugobj(hs, "create FS done [rc=%d, id=%d]", rc, cxt->fs ? cxt->fs->id : -1));
	return rc;
}

static int hook_reconfigure_mount(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct libmnt_sysapi *api;
	int rc = 0;

	assert(cxt);

	if (mnt_context_helper_executed(cxt))
		return 0;

	api = get_sysapi(cxt);
	assert(api);
	assert(api->fd_tree >= 0);

	if (api->fd_fs < 0) {
		api->fd_fs = fspick(api->fd_tree, "", FSPICK_EMPTY_PATH |
						      FSPICK_NO_AUTOMOUNT);
		hookset_set_syscall_status(cxt, "fspick", api->fd_fs >= 0);
		if (api->fd_fs < 0)
			return -errno;
	}

	rc = configure_superblock(cxt, hs, api->fd_fs, 1);
	if (!rc) {
		DBG(HOOK, ul_debugobj(hs, "re-configurate FS"));
		rc = fsconfig(api->fd_fs, FSCONFIG_CMD_RECONFIGURE, NULL, NULL, 0);
		hookset_set_syscall_status(cxt, "fsconfig", rc == 0);
	}

	DBG(HOOK, ul_debugobj(hs, "reconf FS done [rc=%d]", rc));
	return rc;
}

static int set_vfsflags(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			uint64_t set, uint64_t clr, int recursive)
{
	struct libmnt_sysapi *api;
	struct mount_attr attr = { .attr_clr = 0 };
	unsigned int callflags = AT_EMPTY_PATH;
	int rc;

	api = get_sysapi(cxt);
	assert(api);

	/* fallback only; necessary when init_sysapi() during preparation
	 * cannot open the tree -- for example when we call /sbin/mount.<type> */
	if (api->fd_tree < 0 && mnt_fs_get_target(cxt->fs)) {
		rc = api->fd_tree = mnt_context_open_tree(cxt, NULL, (unsigned long) -1);
		if (rc < 0)
			return rc;
		rc = 0;
	}

	if (recursive)
		callflags |= AT_RECURSIVE;

	DBG(HOOK, ul_debugobj(hs,
			"mount_setattr(set=0x%08" PRIx64" clr=0x%08" PRIx64")", set, clr));
	attr.attr_set = set;
	attr.attr_clr = clr;

	errno = 0;
	rc = mount_setattr(api->fd_tree, "", callflags, &attr, sizeof(attr));
	hookset_set_syscall_status(cxt, "mount_setattr", rc == 0);

	if (rc && errno == EINVAL)
		return -MNT_ERR_APPLYFLAGS;

	return rc == 0 ? 0 : -errno;
}

static int hook_set_vfsflags(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct libmnt_optlist *ol;
	uint64_t set = 0, clr = 0;
	int rc = 0;

	if (mnt_context_helper_executed(cxt))
		return 0;

	DBG(HOOK, ul_debugobj(hs, "setting VFS flags"));

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	/* normal flags */
	rc = mnt_optlist_get_attrs(ol, &set, &clr, MNT_OL_NOREC);
	if (!rc && (set || clr))
		rc = set_vfsflags(cxt, hs, set, clr, 0);

	/* recursive flags */
	set = clr = 0;
	if (!rc)
		rc = mnt_optlist_get_attrs(ol, &set, &clr, MNT_OL_REC);
	if (!rc && (set || clr))
		rc = set_vfsflags(cxt, hs, set, clr, 1);

	return rc;
}

static int hook_set_propagation(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct libmnt_sysapi *api;
	struct libmnt_optlist *ol;
	struct libmnt_iter itr;
	struct libmnt_opt *opt;
	int rc = 0;

	DBG(HOOK, ul_debugobj(hs, "setting propagation"));

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	api = get_sysapi(cxt);
	assert(api);

	/* fallback only; necessary when init_sysapi() during preparation
	 * cannot open the tree -- for example when we call /sbin/mount.<type> */
	if (api->fd_tree < 0 && mnt_fs_get_target(cxt->fs)) {
		rc = api->fd_tree = mnt_context_open_tree(cxt, NULL, (unsigned long) -1);
		if (rc < 0)
			goto done;
		rc = 0;
	}

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while (mnt_optlist_next_opt(ol, &itr, &opt) == 0) {
		const struct libmnt_optmap *map = mnt_opt_get_map(opt);
		const struct libmnt_optmap *ent = mnt_opt_get_mapent(opt);
		struct mount_attr attr = { .attr_clr = 0 };
		unsigned int flgs = AT_EMPTY_PATH;

		if (cxt->map_linux != map)
			continue;
		if (mnt_opt_is_external(opt))
			continue;
		if (!ent || !ent->id || !(ent->id & MS_PROPAGATION))
			continue;

		attr.propagation = ent->id & MS_PROPAGATION;
		if (ent->id & MS_REC)
			flgs |= AT_RECURSIVE;

		DBG(HOOK, ul_debugobj(hs,
			"mount_setattr(propagation=0x%08" PRIx64")",
			(uint64_t) attr.propagation));

		rc = mount_setattr(api->fd_tree, "", flgs, &attr, sizeof(attr));
		hookset_set_syscall_status(cxt, "mount_setattr", rc == 0);

		if (rc && errno == EINVAL)
			return -MNT_ERR_APPLYFLAGS;
		if (rc != 0)
			break;
	}
done:
	return rc == 0 ? 0 : -errno;
}

static int hook_attach_target(struct libmnt_context *cxt,
		const struct libmnt_hookset *hs,
		void *data __attribute__((__unused__)))
{
	struct libmnt_sysapi *api;
	const char *target;
	int rc = 0;

	if (mnt_context_helper_executed(cxt))
		return 0;

	target = mnt_fs_get_target(cxt->fs);
	if (!target)
		return -EINVAL;

	api = get_sysapi(cxt);
	assert(api);
	assert(api->fd_tree >= 0);

	DBG(HOOK, ul_debugobj(hs, "move_mount(to=%s)", target));

	/* umount old target if we created a clone */
	if (cxt->force_clone
	    && !api->is_new_fs
	    && !mnt_optlist_is_bind(cxt->optlist)) {

		DBG(HOOK, ul_debugobj(hs, "remove expired target"));
		umount2(target, MNT_DETACH);
	}

	rc = move_mount(api->fd_tree, "", AT_FDCWD, target, MOVE_MOUNT_F_EMPTY_PATH);
	hookset_set_syscall_status(cxt, "move_mount", rc == 0);

	return rc == 0 ? 0 : -errno;
}

static inline int fsopen_is_supported(void)
{
	int dummy, rc = 1;

	errno = 0;
	dummy = fsopen(NULL, FSOPEN_CLOEXEC);

	if (errno == ENOSYS)
		rc = 0;
	if (dummy >= 0)
		close(dummy);
	return rc;
}

static inline int mount_setattr_is_supported(void)
{
	int rc;

	errno = 0;
	rc = mount_setattr(-1, NULL, 0, NULL, 0);
	return !(rc == -1 && errno == ENOSYS);
}

/*
 * open_tree() and fsopen()
 */
static int init_sysapi(struct libmnt_context *cxt,
		       const struct libmnt_hookset *hs,
		       unsigned long flags)
{
	struct libmnt_sysapi *api;
	const char *path = NULL;

	assert(cxt);
	assert(hs);

	DBG(HOOK, ul_debugobj(hs, "initialize API fds"));

	/* A) tree based operation -- the tree is mount point */
	if ((flags & MS_REMOUNT)
	    || mnt_context_propagation_only(cxt)) {
		DBG(HOOK, ul_debugobj(hs, " REMOUNT/propagation"));
		path = mnt_fs_get_target(cxt->fs);
		if (!path)
			return -EINVAL;

	/* B) tree based operation -- the tree is mount source */
	} else if ((flags & MS_BIND)
	    || (flags & MS_MOVE)) {
		DBG(HOOK, ul_debugobj(hs, " BIND/MOVE"));
		path = mnt_fs_get_srcpath(cxt->fs);
		if (!path)
			return -EINVAL;
	}

	api = new_hookset_data(cxt, hs);
	if (!api)
		return -ENOMEM;

	if (path) {
		api->fd_tree = mnt_context_open_tree(cxt, path, flags);
		if (api->fd_tree < 0)
			goto fail;

	/* C) FS based operation
	 *
	 *  Note, fstype is optinal and may be specified later if mount by
	 *  list of FS types (mount -t foo,bar,ext4). In this case fsopen()
	 *  is called later in hook_create_mount(). */
	} else {
		const char *type = mnt_fs_get_fstype(cxt->fs);
		int rc = 0;

		/* fsopen() to create a superblock */
		if (cxt->helper == NULL && type && !strchr(type, ','))
			rc = open_fs_configuration_context(cxt, api, type);

		/* dummy fsopen() to test if API is available */
		else if (!fsopen_is_supported()) {
			errno = ENOSYS;
			rc = -errno;
			hookset_set_syscall_status(cxt, "fsopen", rc == 0);
		}
		if (rc < 0)
			goto fail;
	}

	return 0;
fail:
	DBG(HOOK, ul_debugobj(hs, "init fs/tree failed [errno=%d %m]", errno));
	return -errno;
}

static int force_classic_mount(struct libmnt_context *cxt)
{
	const char *env = getenv("LIBMOUNT_FORCE_MOUNT2");

	if (env) {
		if (strcmp(env, "always") == 0)
			return 1;
		if (strcmp(env, "never") == 0)
			return 0;
	}

	/* "auto" (default) -- try to be smart */

	/* For external /sbin/mount.<type> helpers we use the new API only for
	 * propagation setting. In this case, the usability of mount_setattr()
	 * will be verified later */
	if (cxt->helper)
		return 0;

	/*
	 * The current kernel btrfs driver does not completely implement
	 * fsconfig() as it does not work with selinux stuff.
	 *
	 * Don't use the new mount API in this situation. Let's hope this issue
	 * is temporary.
	 */
	{
		const char *type = mnt_fs_get_fstype(cxt->fs);

		if (type && strcmp(type, "btrfs") == 0 && cxt->has_selinux_opt)
			return 1;
	}

	return 0;
}


/*
 * Analyze library context and register hook to call mount-like syscalls.
 *
 * Note that this function interprets classic MS_* flags by new Linux mount FD
 * based API.
 *
 * Returns: 0 on success, <0 on error, >0 on recover-able error
 */
static int hook_prepare(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct libmnt_optlist *ol;
	unsigned long flags = 0;
	uint64_t set = 0, clr = 0;
	int rc = 0;

	assert(cxt);
	assert(hs == &hookset_mount);

	if (force_classic_mount(cxt)) {
		DBG(HOOK, ul_debugobj(hs, "new API disabled"));
		return 0;
	}

	DBG(HOOK, ul_debugobj(hs, "prepare mount"));

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	/* classic MS_* flags (include oprations like MS_REMOUNT, etc) */
	rc = mnt_optlist_get_flags(ol, &flags, cxt->map_linux, 0);

	/* MOUNT_ATTR_* flags for mount_setattr() */
	if (!rc)
		rc = mnt_optlist_get_attrs(ol, &set, &clr, 0);

	/* open_tree() or fsopen() */
	if (!rc) {
		rc = init_sysapi(cxt, hs, flags);
		if (rc && cxt->syscall_status == -ENOSYS)
			goto enosys;
	}

	/* check mutually exclusive operations */
	if (!rc && (flags & MS_BIND) && (flags & MS_MOVE))
		return -EINVAL;
	if (!rc && (flags & MS_MOVE) && (flags & MS_REMOUNT))
		return -EINVAL;

	/* classic remount (note -oremount,bind,ro is not superblock reconfiguration) */
	if (!rc
	    && cxt->helper == NULL
	    && (flags & MS_REMOUNT)
	    && !(flags & MS_BIND))
		rc = mnt_context_append_hook(cxt, hs, MNT_STAGE_MOUNT, NULL,
					hook_reconfigure_mount);

	/* create a new FS instance */
	else if (!rc
	    && cxt->helper == NULL
	    && !(flags & MS_BIND)
	    && !(flags & MS_MOVE)
	    && !(flags & MS_REMOUNT)
	    && !mnt_context_propagation_only(cxt))
		rc = mnt_context_append_hook(cxt, hs, MNT_STAGE_MOUNT, NULL,
					hook_create_mount);

	/* call mount_setattr() */
	if (!rc
	    && cxt->helper == NULL
	    && (set != 0 || clr != 0 || (flags & MS_REMOUNT))) {
		/*
		 * mount_setattr() supported, but not usable for remount
		 * https://github.com/torvalds/linux/commit/dd8b477f9a3d8edb136207acb3652e1a34a661b7
		 */
		if (get_linux_version() < KERNEL_VERSION(5, 14, 0))
			goto enosys;

		if (!mount_setattr_is_supported())
			goto enosys;

		rc = mnt_context_append_hook(cxt, hs, MNT_STAGE_MOUNT, NULL,
					hook_set_vfsflags);
	}

	/* call move_mount() to attach target */
	if (!rc
	    && cxt->helper == NULL
	    && (cxt->force_clone ||
			(!(flags & MS_REMOUNT) && !mnt_context_propagation_only(cxt))))
		rc = mnt_context_append_hook(cxt, hs, MNT_STAGE_MOUNT_POST, NULL,
					hook_attach_target);

	/* set propagation (has to be attached to VFS) */
	if (!rc && mnt_optlist_get_propagation(ol)) {
		if (!mount_setattr_is_supported())
			goto enosys;

		rc = mnt_context_append_hook(cxt, hs, MNT_STAGE_MOUNT_POST, NULL,
					hook_set_propagation);
	}

	DBG(HOOK, ul_debugobj(hs, "prepare mount done [rc=%d]", rc));
	return rc;

enosys:
	/* we need to recover from this error, so hook_mount_legacy.c
	 * can try to continue */
	DBG(HOOK, ul_debugobj(hs, "failed to init new API"));
	reset_syscall_status(cxt);
	hookset_deinit(cxt, hs);
	return 1;
}

const struct libmnt_hookset hookset_mount =
{
	.name = "__mount",

	.firststage = MNT_STAGE_PREP,
	.firstcall = hook_prepare,

	.deinit = hookset_deinit
};
#endif /* USE_LIBMOUNT_MOUNTFD_SUPPORT */
