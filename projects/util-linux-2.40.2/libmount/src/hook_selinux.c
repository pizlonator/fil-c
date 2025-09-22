/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2023 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 *
 * Please, see the comment in libmount/src/hooks.c to understand how hooks work.
 */
#ifdef HAVE_LIBSELINUX
#include <selinux/selinux.h>
#include <selinux/context.h>

#include "mountP.h"
#include "fileutils.h"
#include "linux_version.h"

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

static inline int is_option(const char *name, const char *const *names)
{
	const char *const *p;

	for (p = names; p && *p; p++) {
		if (strcmp(name, *p) == 0)
			return 1;
	}
	return 0;
}

/* Converts rootcontext=@target to the real selinxu context
 */
static int hook_selinux_target(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	struct libmnt_optlist *ol;
	struct libmnt_opt *opt;
	const char *tgt, *val;
	char *raw = NULL;
	int rc = 0;

	assert(cxt);

	tgt = mnt_fs_get_target(cxt->fs);
	if (!tgt)
		return 0;
	if (cxt->action != MNT_ACT_MOUNT)
		return 0;
	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -EINVAL;

	opt = mnt_optlist_get_named(ol, "rootcontext", NULL);
	if (!opt)
		return 0;

	val = mnt_opt_get_value(opt);
	if (!val || strcmp(val, "@target") != 0)
		return 0;


	rc = getfilecon_raw(tgt, &raw);
	if (rc <= 0 || !raw) {
		rc = errno ? -errno : -EINVAL;
		DBG(HOOK, ul_debugobj(hs, " SELinux fix @target failed [rc=%d]", rc));
	} else {
		DBG(HOOK, ul_debugobj(hs, " SELinux fix @target to %s", raw));
		rc = 0;	/* getfilecon_raw(3) returns the size of the extended attribute value */
	}
	if (!rc)
		rc = mnt_opt_set_quoted_value(opt, raw);
	if (raw)
		freecon(raw);

	return rc != 0 ? -MNT_ERR_MOUNTOPT : 0;
}

static int hook_prepare_options(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			void *data __attribute__((__unused__)))
{
	int rc = 0, se_fix = 0, se_rem = 0;
	struct libmnt_optlist *ol;

	assert(cxt);

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -EINVAL;

	if (!is_selinux_enabled())
		/* Always remove SELinux garbage if SELinux disabled */
		se_rem = 1;
	else if (mnt_optlist_is_remount(ol))
		/*
		 * Linux kernel < 2.6.39 does not support remount operation
		 * with any selinux specific mount options.
		 *
		 * Kernel 2.6.39 commits:  ff36fe2c845cab2102e4826c1ffa0a6ebf487c65
		 *                         026eb167ae77244458fa4b4b9fc171209c079ba7
		 * fix this odd behavior, so we don't have to care about it in
		 * userspace.
		 */
		se_rem = get_linux_version() < KERNEL_VERSION(2, 6, 39);
	else
		/* For normal mount, contexts are translated */
		se_fix = 1;

	DBG(HOOK, ul_debugobj(hs, " SELinux fix options"));

	/* Fix SELinux contexts */
	if (se_rem || se_fix) {
		static const char *const selinux_options[] = {
			"context",
			"fscontext",
			"defcontext",
			"rootcontext",
			"seclabel",
			NULL
		};
		struct libmnt_iter itr;
		struct libmnt_opt *opt;

		mnt_reset_iter(&itr, MNT_ITER_FORWARD);

		while (mnt_optlist_next_opt(ol, &itr, &opt) == 0) {
			const char *opt_name = mnt_opt_get_name(opt);

			if (!is_option(opt_name, selinux_options))
				continue;
			if (se_rem)
				rc = mnt_optlist_remove_opt(ol, opt);
			else if (se_fix && mnt_opt_has_value(opt)) {
				const char *val = mnt_opt_get_value(opt);
				char *raw = NULL;

				/* @target placeholder is replaced later when target
				 * is already avalable. The mountpoint does not have to exist
				 * yet (for example "-o X-mount.mkdir=" or --target-prefix).
				 */
				if (strcmp(opt_name, "rootcontext") == 0 &&
				    strcmp(val, "@target") == 0) {
					rc = mnt_context_insert_hook(cxt, "__mkdir",
						       hs, MNT_STAGE_PREP_TARGET, NULL,
						       hook_selinux_target);
					continue;
				} else {
					rc = selinux_trans_to_raw_context(val, &raw);
					if (rc == -1 || !raw)
						rc = -EINVAL;
				}
				if (!rc) {
					DBG(HOOK, ul_debugobj(hs, "  %s: %s to %s",
								opt_name, val, raw));
					rc = mnt_opt_set_quoted_value(opt, raw);
				}
				if (raw)
					freecon(raw);

				/* temporary for broken fsconfig() syscall */
				cxt->has_selinux_opt = 1;
			}
			if (rc)
				break;
		}
	}

	return rc != 0 ? -MNT_ERR_MOUNTOPT : 0;
}

const struct libmnt_hookset hookset_selinux =
{
	.name = "__selinux",

	.firststage = MNT_STAGE_PREP_OPTIONS,
	.firstcall = hook_prepare_options,

	.deinit = hookset_deinit
};

#endif /* HAVE_LIBSELINUX */
