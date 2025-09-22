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
 * The "hookset" is a set of callbacks (hooks) that implement some functionality.
 * The library defines stages where hooks are called (e.g. when preparing source, post
 * mount(2), etc.). An arbitrary hook can, on the fly, define another hook for the
 * arbitrary stage. The first hook from the hookset which goes to the game is a
 * "firstcall" (defined in struct libmnt_hookset). This first hook controls
 * what will happen in the next stages (usually nothing).
 *
 * The library supports two kinds of data for hooksets:
 *
 * - global data;  accessible for all callbacks. Makes sense for complex
 *   hooksets with more callbacks in more stages. Usually implemented by
 *   locally defined 'struct hookset_data' in hook_*.c.
 *
 * - per-hook data; acessible for specific callback
 *   Usually implemented by locally defined 'struct hook_data' in hook_*.c.
 */
#include "mountP.h"
#include "mount-api-utils.h"

/* built-in hooksets */
static const struct libmnt_hookset *hooksets[] =
{
#ifdef __linux__
	&hookset_loopdev,
#ifdef HAVE_CRYPTSETUP
	&hookset_veritydev,
#endif
	&hookset_mkdir,
#ifdef HAVE_LIBSELINUX
	&hookset_selinux,
#endif
	&hookset_subdir,
#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT
	&hookset_mount,
#endif
	&hookset_mount_legacy,
#if defined(HAVE_MOUNTFD_API) && defined(HAVE_LINUX_MOUNT_H)
	&hookset_idmap,
#endif
	&hookset_owner
#endif
};

/* hooksets data (this is global list of hookset data) */
struct hookset_data {
	const struct libmnt_hookset *hookset;
	void *data;

	struct list_head	datas;
};

/* individial callback */
struct hookset_hook {
	const struct libmnt_hookset *hookset;
	int stage;
	void *data;
	const char *after;

	int (*func)(struct libmnt_context *, const struct libmnt_hookset *, void *);

	struct list_head	hooks;
	unsigned int		executed : 1;
};

static const char *stagenames[] = {
	/* prepare */
	[MNT_STAGE_PREP_SOURCE] = "prep-source",
	[MNT_STAGE_PREP_TARGET] = "prep-target",
	[MNT_STAGE_PREP_OPTIONS] = "prep-options",
	[MNT_STAGE_PREP] = "prep",

	/* mount */
	[MNT_STAGE_MOUNT_PRE] = "pre-mount",
	[MNT_STAGE_MOUNT] = "mount",
	[MNT_STAGE_MOUNT_POST] = "post-mount",

	/* post */
	[MNT_STAGE_POST] = "post",
};

static int call_depend_hooks(struct libmnt_context *cxt, const char *name, int stage);


int mnt_context_deinit_hooksets(struct libmnt_context *cxt)
{
	size_t i;
	int rc = 0;

	assert(cxt);

	if (list_empty(&cxt->hooksets_datas) &&
	    list_empty(&cxt->hooksets_hooks))
		return 0;

	for (i = 0; i <  ARRAY_SIZE(hooksets); i++) {
		const struct libmnt_hookset *hs = hooksets[i];

		rc += hs->deinit(cxt, hs);
	}

	assert(list_empty(&cxt->hooksets_datas));
	assert(list_empty(&cxt->hooksets_hooks));

	INIT_LIST_HEAD(&cxt->hooksets_datas);
	INIT_LIST_HEAD(&cxt->hooksets_hooks);

	return rc;
}

const struct libmnt_hookset *mnt_context_get_hookset(
			struct libmnt_context *cxt, const char *name)
{
	size_t i;

	assert(cxt);
	assert(name);

	for (i = 0; i <  ARRAY_SIZE(hooksets); i++) {
		const struct libmnt_hookset *hs = hooksets[i];

		if (strcmp(name, hs->name) == 0)
			return hs;
	}

	return NULL;
}

static struct hookset_data *get_hookset_data(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs)
{
	struct list_head *p;

	assert(cxt);
	assert(hs);

	list_for_each(p, &cxt->hooksets_datas) {
		struct hookset_data *x = list_entry(p, struct hookset_data, datas);

		if (x->hookset == hs)
			return x;
	}
	return 0;
}

int mnt_context_set_hookset_data(struct libmnt_context *cxt,
				 const struct libmnt_hookset *hs,
				 void *data)
{
	struct hookset_data *hd = NULL;

	hd = get_hookset_data(cxt, hs);

	/* deallocate old data */
	if (data == NULL) {
		if (hd) {
			DBG(CXT, ul_debugobj(cxt, " free '%s' data", hs->name));
			list_del(&hd->datas);
			free(hd);
		}
		return 0;
	}

	/* create and append new data */
	if (!hd) {
		hd = calloc(1, sizeof(*hd));
		if (!hd)
			return -ENOMEM;

		DBG(CXT, ul_debugobj(cxt, " alloc '%s' data", hs->name));
		INIT_LIST_HEAD(&hd->datas);
		hd->hookset = hs;
		list_add_tail(&hd->datas, &cxt->hooksets_datas);

	}
	hd->data = data;
	return 0;
}

void *mnt_context_get_hookset_data(struct libmnt_context *cxt,
				   const struct libmnt_hookset *hs)
{
	struct hookset_data *hd = get_hookset_data(cxt, hs);

	return hd ? hd->data : NULL;
}

static int append_hook(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			int stage,
			void *data,
			int (*func)(struct libmnt_context *,
				    const struct libmnt_hookset *,
				    void *),
			const char *after)
{
	struct hookset_hook *hook;

	assert(cxt);
	assert(hs);
	assert(stage);

	hook = calloc(1, sizeof(*hook));
	if (!hook)
		return -ENOMEM;

	DBG(CXT, ul_debugobj(cxt, " appending %s hook from %s",
				stagenames[stage], hs->name));

	INIT_LIST_HEAD(&hook->hooks);

	hook->hookset = hs;
	hook->data = data;
	hook->func = func;
	hook->stage = stage;
	hook->after = after;

	list_add_tail(&hook->hooks, &cxt->hooksets_hooks);
	return 0;
}

int mnt_context_append_hook(struct libmnt_context *cxt,
			const struct libmnt_hookset *hs,
			int stage,
			void *data,
			int (*func)(struct libmnt_context *,
				    const struct libmnt_hookset *,
				    void *))
{
	return append_hook(cxt, hs, stage, data, func, NULL);
}

int mnt_context_insert_hook(struct libmnt_context *cxt,
			const char *after,
			const struct libmnt_hookset *hs,
			int stage,
			void *data,
			int (*func)(struct libmnt_context *,
				    const struct libmnt_hookset *,
				    void *))
{
	return append_hook(cxt, hs, stage, data, func, after);
}

static struct hookset_hook *get_hookset_hook(struct libmnt_context *cxt,
					     const struct libmnt_hookset *hs,
					     int stage,
					     void *data)
{
	struct list_head *p, *next;

	assert(cxt);

	list_for_each_safe(p, next, &cxt->hooksets_hooks) {
		struct hookset_hook *x = list_entry(p, struct hookset_hook, hooks);

		if (hs && x->hookset != hs)
			continue;
		if (stage && x->stage != stage)
			continue;
		if (data && x->data != data)
			continue;
		return x;
	}

	return NULL;
}

int mnt_context_remove_hook(struct libmnt_context *cxt,
			    const struct libmnt_hookset *hs,
			    int stage,
			    void **data)
{
	struct hookset_hook *hook;

	assert(cxt);

	hook = get_hookset_hook(cxt, hs, stage, NULL);
	if (hook) {
		DBG(CXT, ul_debugobj(cxt, " removing %s hook from %s",
			stagenames[hook->stage], hook->hookset->name));

		if (data)
			*data = hook->data;

		list_del(&hook->hooks);
		free(hook);
		return 0;
	}

	return 1;
}

int mnt_context_has_hook(struct libmnt_context *cxt,
			 const struct libmnt_hookset *hs,
			 int stage,
			 void *data)
{
	return get_hookset_hook(cxt, hs, stage, data) ? 1 : 0;
}

static int call_hook(struct libmnt_context *cxt, struct hookset_hook *hook)
{
	int rc = 0;

	if (mnt_context_is_fake(cxt))
		DBG(CXT, ul_debugobj(cxt, " FAKE call"));
	else
		rc = hook->func(cxt, hook->hookset, hook->data);

	hook->executed = 1;
	if (!rc)
		rc = call_depend_hooks(cxt, hook->hookset->name, hook->stage);
	return rc;
}

static int call_depend_hooks(struct libmnt_context *cxt, const char *name, int stage)
{
	struct list_head *p = NULL, *next = NULL;
	int rc = 0;

	list_for_each_safe(p, next, &cxt->hooksets_hooks) {
		struct hookset_hook *x = list_entry(p, struct hookset_hook, hooks);

		if (x->stage != stage || x->executed ||
		    x->after == NULL || strcmp(x->after, name) != 0)
			continue;

		DBG(CXT, ul_debugobj(cxt, "calling %s [after]", x->hookset->name));
		rc = call_hook(cxt, x);
		if (rc)
			break;
	}

	return rc;
}

int mnt_context_call_hooks(struct libmnt_context *cxt, int stage)
{
	struct list_head *p = NULL, *next = NULL;
	size_t i;
	int rc = 0;

	DBG(CXT, ul_debugobj(cxt, "---> stage:%s", stagenames[stage]));

	/* call initial hooks */
	for (i = 0; i <  ARRAY_SIZE(hooksets); i++) {
		const struct libmnt_hookset *hs = hooksets[i];

		if (hs->firststage != stage)
			continue;

		DBG(CXT, ul_debugobj(cxt, "calling %s [first]", hs->name));

		if (mnt_context_is_fake(cxt))
			DBG(CXT, ul_debugobj(cxt, " FAKE call"));
		else
			rc = hs->firstcall(cxt, hs, NULL);
		if (!rc)
			rc = call_depend_hooks(cxt, hs->name, stage);
		if (rc < 0)
			goto done;
	}

	/* call already active hooks */
	list_for_each_safe(p, next, &cxt->hooksets_hooks) {
		struct hookset_hook *x = list_entry(p, struct hookset_hook, hooks);

		if (x->stage != stage || x->executed)
			continue;

		DBG(CXT, ul_debugobj(cxt, "calling %s [active]", x->hookset->name));
		rc = call_hook(cxt, x);
		if (rc < 0)
			goto done;
	}

done:
	/* zeroize status */
	p = next = NULL;
	list_for_each_safe(p, next, &cxt->hooksets_hooks) {
		struct hookset_hook *x = list_entry(p, struct hookset_hook, hooks);

		if (x->stage != stage)
			continue;
		x->executed = 0;
	}

	DBG(CXT, ul_debugobj(cxt, "<--- stage:%s [rc=%d status=%d]",
				stagenames[stage], rc, cxt->syscall_status));
	return rc;
}
