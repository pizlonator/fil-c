/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2022 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2022 Christian Brauner (Microsoft) <brauner@kernel.org>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 *
 * This is X-mount.idmap= implementation.
 *
 * Please, see the comment in libmount/src/hooks.c to understand how hooks work.
 */
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <inttypes.h>

#include "strutils.h"
#include "all-io.h"
#include "namespace.h"
#include "mount-api-utils.h"

#include "mountP.h"

#ifdef HAVE_LINUX_NSFS_H
# include <linux/nsfs.h>
#endif

#if defined(HAVE_MOUNTFD_API) && defined(HAVE_LINUX_MOUNT_H)

typedef enum idmap_type_t {
	ID_TYPE_UID,	/* uidmap entry */
	ID_TYPE_GID,	/* gidmap entry */
	ID_TYPE_UIDGID,	/* uidmap and gidmap entry */
} idmap_type_t;

struct id_map {
	idmap_type_t map_type;
	uint32_t nsid;
	uint32_t hostid;
	uint32_t range;
	struct list_head map_head;
};

struct hook_data {
	int userns_fd;
	struct list_head id_map;
};

static inline struct hook_data *new_hook_data(void)
{
	struct hook_data *hd = calloc(1, sizeof(*hd));

	if (!hd)
		return NULL;

	INIT_LIST_HEAD(&hd->id_map);
	hd->userns_fd = -1;
	return hd;
}

static inline void free_hook_data(struct hook_data *hd)
{
	struct list_head *p, *pnext;
	struct id_map *idmap;

	if (!hd)
		return;

	if (hd->userns_fd >= 0) {
		close(hd->userns_fd);
		hd->userns_fd = -1;
	}

	list_for_each_safe(p, pnext, &hd->id_map) {
		idmap = list_entry(p, struct id_map, map_head);
		list_del(&idmap->map_head);
		free(idmap);
	}
	INIT_LIST_HEAD(&hd->id_map);
	free(hd);
}

static int write_id_mapping(idmap_type_t map_type, pid_t pid, const char *buf,
			    size_t buf_size)
{
	int fd = -1, rc = -1, setgroups_fd = -1;
	char path[PATH_MAX];

	if (geteuid() != 0 && map_type == ID_TYPE_GID) {
		snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);

		setgroups_fd = open(path, O_WRONLY | O_CLOEXEC | O_NOCTTY);
		if (setgroups_fd < 0 && errno != ENOENT)
			goto err;

		if (setgroups_fd >= 0) {
			rc = write_all(setgroups_fd, "deny\n", strlen("deny\n"));
			if (rc)
				goto err;
		}
	}

	snprintf(path, sizeof(path), "/proc/%d/%cid_map", pid,
		 map_type == ID_TYPE_UID ? 'u' : 'g');

	fd = open(path, O_WRONLY | O_CLOEXEC | O_NOCTTY);
	if (fd < 0)
		goto err;

	rc = write_all(fd, buf, buf_size);

err:
	if (fd >= 0)
		close(fd);
	if (setgroups_fd >= 0)
		close(setgroups_fd);

	return rc;
}

static int map_ids(struct list_head *idmap, pid_t pid)
{
	int fill, left;
	char *pos;
	int rc = 0;
	char mapbuf[4096] = {};
	struct list_head *p;

	for (idmap_type_t type = ID_TYPE_UID; type <= ID_TYPE_GID; type++) {
		bool had_entry = false;

		pos = mapbuf;
		list_for_each(p, idmap) {
			struct id_map *map = list_entry(p, struct id_map, map_head);

			/*
			 * If the map type is ID_TYPE_UIDGID we need to include
			 * it in both gid- and uidmap.
			 */
			if (map->map_type != ID_TYPE_UIDGID && map->map_type != type)
				continue;

			had_entry = true;

			left = sizeof(mapbuf) - (pos - mapbuf);
			fill = snprintf(pos, left,
					"%" PRIu32 " %" PRIu32 " %" PRIu32 "\n",
					map->nsid, map->hostid, map->range);
			/*
			 * The kernel only takes <= 4k for writes to
			 * /proc/<pid>/{g,u}id_map
			 */
			if (fill <= 0)
				return errno = EINVAL, -1;

			pos += fill;
		}
		if (!had_entry)
			continue;

		rc = write_id_mapping(type, pid, mapbuf, pos - mapbuf);
		if (rc < 0)
			return -1;

		memset(mapbuf, 0, sizeof(mapbuf));
	}

	return 0;
}

static int wait_for_pid(pid_t pid)
{
	int status, rc;

	do {
		rc = waitpid(pid, &status, 0);
	} while (rc < 0 && errno == EINTR);

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;

	return 0;
}

static int get_userns_fd_from_idmap(struct list_head *idmap)
{
	int fd_userns = -1;
	ssize_t rc = -1;
	char c = '1';
	pid_t pid;
	int sock_fds[2];
	char path[PATH_MAX];

	rc = socketpair(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0, sock_fds);
	if (rc < 0)
		return -errno;

	pid = fork();
	if (pid < 0)
		goto err_close_sock;

	if (pid == 0) {
		close(sock_fds[1]);

		rc = unshare(CLONE_NEWUSER);
		if (rc < 0)
			_exit(EXIT_FAILURE);

		/* Let parent know we're ready to have the idmapping written. */
		rc = write_all(sock_fds[0], &c, 1);
		if (rc)
			_exit(EXIT_FAILURE);

		/* Hang around until the parent has persisted our namespace. */
		rc = read_all(sock_fds[0], &c, 1);
		if (rc != 1)
			_exit(EXIT_FAILURE);

		close(sock_fds[0]);

		_exit(EXIT_SUCCESS);
	}
	close(sock_fds[0]);
	sock_fds[0] = -1;

	/* Wait for child to set up a new namespace. */
	rc = read_all(sock_fds[1], &c, 1);
	if (rc != 1) {
		kill(pid, SIGKILL);
		goto err_wait;
	}

	rc = map_ids(idmap, pid);
	if (rc < 0) {
		kill(pid, SIGKILL);
		goto err_wait;
	}

	snprintf(path, sizeof(path), "/proc/%d/ns/user", pid);
	fd_userns = open(path, O_RDONLY | O_CLOEXEC | O_NOCTTY);

	/* Let child know we've persisted its namespace. */
	(void)write_all(sock_fds[1], &c, 1);

err_wait:
	rc = wait_for_pid(pid);

err_close_sock:
	if (sock_fds[0] > 0)
		close(sock_fds[0]);
	close(sock_fds[1]);

	if (rc < 0 && fd_userns >= 0) {
		close(fd_userns);
		fd_userns = -1;
	}

	return fd_userns;
}

static int open_userns(const char *path)
{

	int userns_fd;

	userns_fd = open(path, O_RDONLY | O_CLOEXEC | O_NOCTTY);
	if (userns_fd < 0)
		return -1;

#if defined(NS_GET_OWNER_UID)
	/*
	 * We use NS_GET_OWNER_UID to verify that this is a user namespace.
	 * This is on a best-effort basis. If this isn't a userns then
	 * mount_setattr() will tell us to go away later.
	 */
	if (ioctl(userns_fd, NS_GET_OWNER_UID, &(uid_t){-1}) < 0) {
		close(userns_fd);
		return -1;
	}
#endif
	return userns_fd;
}

/*
 * Create an idmapped mount based on context target, unmounting the
 * non-idmapped target mount and attaching the detached idmapped mount target.
 */
static int hook_mount_post(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data)
{
	struct hook_data *hd = (struct hook_data *) data;
	struct mount_attr attr = {
		.attr_set	= MOUNT_ATTR_IDMAP,
		.userns_fd	= hd->userns_fd
	};
	const int recursive = mnt_optlist_is_recursive(cxt->optlist);
	const char *target = mnt_fs_get_target(cxt->fs);
	int fd_tree = -1;
	int rc, is_private = 1;

	assert(hd);
	assert(target);
	assert(hd->userns_fd >= 0);

	DBG(HOOK, ul_debugobj(hs, " attaching namespace to %s", target));

	/*
	 * Once a mount has been attached to the filesystem it can't be
	 * idmapped anymore. So create a new detached mount.
	 */
#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT
	{
		struct libmnt_sysapi *api = mnt_context_get_sysapi(cxt);

		if (api && api->fd_tree >= 0) {
			fd_tree = api->fd_tree;
			is_private = 0;
			DBG(HOOK, ul_debugobj(hs, " reuse tree FD"));
		}
	}
#endif
	if (fd_tree < 0)
		fd_tree = open_tree(-1, target,
			    OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC |
			    (recursive ? AT_RECURSIVE : 0));
	if (fd_tree < 0) {
		DBG(HOOK, ul_debugobj(hs, " failed to open tree"));
		return -MNT_ERR_IDMAP;
	}

	/* Attach the idmapping to the mount. */
	rc = mount_setattr(fd_tree, "",
			   AT_EMPTY_PATH | (recursive ? AT_RECURSIVE : 0),
			   &attr, sizeof(attr));
	if (rc < 0) {
		DBG(HOOK, ul_debugobj(hs, " failed to set attributes"));
		goto done;
	}

	/* Attach the idmapped mount. */
	if (is_private) {
		/* Unmount the old, non-idmapped mount we just cloned and idmapped. */
		umount2(target, MNT_DETACH);

		rc = move_mount(fd_tree, "", -1, target, MOVE_MOUNT_F_EMPTY_PATH);
		if (rc)
			DBG(HOOK, ul_debugobj(hs, " failed to set move mount"));
	}
done:
	if (is_private)
		close(fd_tree);
	if (rc < 0)
		return -MNT_ERR_IDMAP;

	return 0;
}

/*
 * Process X-mount.idmap= mount option
 */
static int hook_prepare_options(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct hook_data *hd = NULL;
	struct libmnt_optlist *ol;
	struct libmnt_opt *opt;
	int rc;
	const char *value = NULL;
	char *saveptr = NULL, *tok, *buf = NULL;

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return 0;

	opt = mnt_optlist_get_named(ol, "X-mount.idmap", cxt->map_userspace);
	if (!opt)
		return 0;

	value = mnt_opt_get_value(opt);
	if (value)
		value = skip_blank(value);
	if (!value || !*value)
		return errno = EINVAL, -MNT_ERR_MOUNTOPT;

	hd = new_hook_data();
	if (!hd)
		return -ENOMEM;

	/* Has the user given us a path to a user namespace? */
	if (*value == '/') {
		hd->userns_fd = open_userns(value);
		if (hd->userns_fd < 0)
			goto err;
		goto done;
	}

	buf = strdup(value);
	if (!buf)
		goto err;

	/*
	 * This is an explicit ID-mapping list of the form:
	 * [id-type]:id-mount:id-host:id-range [...]
	 *
	 * We split the list into separate ID-mapping entries. The individual
	 * ID-mapping entries are separated by ' '.
	 *
	 * A long while ago I made the kernel support up to 340 individual
	 * ID-mappings. So users have quite a bit of freedom here.
	 */
	for (tok = strtok_r(buf, " ", &saveptr); tok;
	     tok = strtok_r(NULL, " ", &saveptr)) {
		struct id_map *idmap;
		idmap_type_t map_type;
		uint32_t nsid = UINT_MAX, hostid = UINT_MAX, range = UINT_MAX;

		if (startswith(tok, "b:")) {
			/* b:id-mount:id-host:id-range */
			map_type = ID_TYPE_UIDGID;
			tok += 2;
		} else if (startswith(tok, "g:")) {
			/* g:id-mount:id-host:id-range */
			map_type = ID_TYPE_GID;
			tok += 2;
		} else if (startswith(tok, "u:")) {
			/* u:id-mount:id-host:id-range */
			map_type = ID_TYPE_UID;
			tok += 2;
		} else {
			/*
			 * id-mount:id-host:id-range
			 *
			 * If the user didn't specify it explicitly then they
			 * want this to be both a gid- and uidmap.
			 */
			map_type = ID_TYPE_UIDGID;
		}

		/* id-mount:id-host:id-range */
		rc = sscanf(tok, "%" PRIu32 ":%" PRIu32 ":%" PRIu32, &nsid,
			    &hostid, &range);
		if (rc != 3)
			goto err;

		idmap = calloc(1, sizeof(*idmap));
		if (!idmap)
			goto err;

		idmap->map_type = map_type;
		idmap->nsid = nsid;
		idmap->hostid = hostid;
		idmap->range = range;
		INIT_LIST_HEAD(&idmap->map_head);
		list_add_tail(&idmap->map_head, &hd->id_map);
	}

	hd->userns_fd = get_userns_fd_from_idmap(&hd->id_map);
	if (hd->userns_fd < 0)
		goto err;

done:
	/* define post-mount hook to enter the namespace */
	DBG(HOOK, ul_debugobj(hs, " wanted new user namespace"));
	cxt->force_clone = 1; /* require OPEN_TREE_CLONE */
	rc = mnt_context_append_hook(cxt, hs,
				MNT_STAGE_MOUNT_POST,
				hd, hook_mount_post);
	if (rc < 0)
		goto err;

	free(buf);
	return 0;

err:
	DBG(HOOK, ul_debugobj(hs, " failed to setup idmap"));
	free_hook_data(hd);
	free(buf);
	return -MNT_ERR_MOUNTOPT;
}


/* de-initiallize this module */
static int hookset_deinit(struct libmnt_context *cxt, const struct libmnt_hookset *hs)
{
	void *data;

	DBG(HOOK, ul_debugobj(hs, "deinit '%s'", hs->name));

	/* remove all our hooks and free hook data */
	while (mnt_context_remove_hook(cxt, hs, 0, &data) == 0) {
		if (data)
			free_hook_data((struct hook_data *) data);
		data = NULL;
	}

	return 0;
}

const struct libmnt_hookset hookset_idmap =
{
	.name = "__idmap",

	.firststage = MNT_STAGE_PREP_OPTIONS,
	.firstcall = hook_prepare_options,

	.deinit = hookset_deinit
};

#endif /* HAVE_MOUNTFD_API && HAVE_LINUX_MOUNT_H */
