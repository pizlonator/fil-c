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
 * Please, see the comment in libmount/src/hooks.c to understand how hooks work.
 */
#include "mountP.h"
#include "fileutils.h"

static int hookset_deinit(struct libmnt_context *cxt, const struct libmnt_hookset *hs)
{
	void *data = NULL;

	DBG(HOOK, ul_debugobj(hs, "deinit '%s'", hs->name));

	/* remove all our hooks and free hook data */
	while (mnt_context_remove_hook(cxt, hs, 0, &data) == 0) {
		if (data)
			free(data);
		data = NULL;
	}

	return 0;
}

static int is_mkdir_required(struct libmnt_context *cxt, const char *tgt, mode_t *mode, int *rc)
{
	struct libmnt_optlist *ol;
	struct libmnt_opt *opt;
	const char *mstr = NULL;

	assert(cxt);
	assert(cxt->map_userspace);
	assert(tgt);
	assert(mode);
	assert(rc);

	*mode = 0;
	*rc = 0;

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	opt = mnt_optlist_get_named(ol, "X-mount.mkdir", cxt->map_userspace);
	if (!opt)
		opt = mnt_optlist_get_named(ol, "x-mount.mkdir", cxt->map_userspace);
	if (!opt)
		return 0;

	if (mnt_is_path(tgt))
		return 0;

	mstr = mnt_opt_get_value(opt);

	if (mstr && *mstr) {
		char *end = NULL;

		if (*mstr == '"')
			mstr++;

		errno = 0;
		*mode = strtol(mstr, &end, 8);

		if (errno || !end || !(*end == '"' || *end == '\0')) {
			DBG(HOOK, ul_debug("failed to parse mkdir mode '%s'", mstr));
			*rc = -MNT_ERR_MOUNTOPT;
			return 0;
		}
	}

	if (!*mode)
		*mode = S_IRWXU |			/* 0755 */
		       S_IRGRP | S_IXGRP |
		       S_IROTH | S_IXOTH;

	DBG(HOOK, ul_debug("mkdir %s (%o) wanted", tgt, *mode));

	return 1;
}

static int hook_prepare_target(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	int rc = 0;
	mode_t mode = 0;
	const char *tgt;

	assert(cxt);

	tgt = mnt_fs_get_target(cxt->fs);
	if (!tgt)
		return 0;

	if (cxt->action == MNT_ACT_MOUNT
	    && is_mkdir_required(cxt, tgt, &mode, &rc)) {

		struct libmnt_cache *cache;

		/* supported only for root or non-suid mount(8) */
		if (!mnt_context_is_restricted(cxt)) {
			rc = ul_mkdir_p(tgt, mode);
			if (rc)
				DBG(HOOK, ul_debugobj(hs, "mkdir %s failed: %m", tgt));
		} else
			rc = -EPERM;

		if (rc == 0) {
			cache = mnt_context_get_cache(cxt);
			if (cache) {
				char *path = mnt_resolve_path(tgt, cache);
				if (path && strcmp(path, tgt) != 0)
					rc = mnt_fs_set_target(cxt->fs, path);
			}
		}
	}

	return rc;
}

const struct libmnt_hookset hookset_mkdir =
{
	.name = "__mkdir",

	.firststage = MNT_STAGE_PREP_TARGET,
	.firstcall = hook_prepare_target,

	.deinit = hookset_deinit
};
