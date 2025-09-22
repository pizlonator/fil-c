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
 * SECTION: context
 * @title: Library high-level context
 * @short_description: high-level API to mount/umount devices.
 *
 * <informalexample>
 *   <programlisting>
 *	struct libmnt_context *cxt = mnt_new_context();
 *
 *	mnt_context_set_options(cxt, "aaa,bbb,ccc=CCC");
 *	mnt_context_set_mflags(cxt, MS_NOATIME|MS_NOEXEC);
 *	mnt_context_set_target(cxt, "/mnt/foo");
 *
 *	if (!mnt_context_mount(cxt))
 *		printf("successfully mounted\n");
 *	mnt_free_context(cxt);
 *
 *   </programlisting>
 * </informalexample>
 *
 * This code is similar to:
 *
 *   mount -o aaa,bbb,ccc=CCC,noatime,noexec /mnt/foo
 *
 */

#include "mountP.h"
#include "strutils.h"
#include "namespace.h"
#include "match.h"

#include <sys/wait.h>

#include "mount-api-utils.h"

/**
 * mnt_new_context:
 *
 * Returns: newly allocated mount context
 */
struct libmnt_context *mnt_new_context(void)
{
	struct libmnt_context *cxt;
	uid_t ruid, euid;

	cxt = calloc(1, sizeof(*cxt));
	if (!cxt)
		return NULL;

	ruid = getuid();
	euid = geteuid();

	mnt_context_reset_status(cxt);

	cxt->ns_orig.fd = -1;
	cxt->ns_tgt.fd = -1;
	cxt->ns_cur = &cxt->ns_orig;

	cxt->map_linux = mnt_get_builtin_optmap(MNT_LINUX_MAP);
	cxt->map_userspace = mnt_get_builtin_optmap(MNT_USERSPACE_MAP);

	INIT_LIST_HEAD(&cxt->hooksets_hooks);
	INIT_LIST_HEAD(&cxt->hooksets_datas);

	/* if we're really root and aren't running setuid */
	cxt->restricted = (uid_t) 0 == ruid && ruid == euid ? 0 : 1;

	cxt->noautofs = 0;

	DBG(CXT, ul_debugobj(cxt, "----> allocate %s",
				cxt->restricted ? "[RESTRICTED]" : ""));

	return cxt;
}

/**
 * mnt_free_context:
 * @cxt: mount context
 *
 * Deallocates context struct.
 */
void mnt_free_context(struct libmnt_context *cxt)
{
	if (!cxt)
		return;

	mnt_reset_context(cxt);

	free(cxt->fstype_pattern);
	free(cxt->optstr_pattern);
	free(cxt->tgt_prefix);

	mnt_unref_table(cxt->fstab);
	mnt_unref_cache(cxt->cache);
	mnt_unref_fs(cxt->fs);

	mnt_unref_optlist(cxt->optlist_saved);
	mnt_unref_optlist(cxt->optlist);

	mnt_unref_lock(cxt->lock);
	mnt_free_update(cxt->update);

	mnt_context_set_target_ns(cxt, NULL);

	free(cxt->children);

	DBG(CXT, ul_debugobj(cxt, "free"));
	free(cxt);
}

/**
 * mnt_reset_context:
 * @cxt: mount context
 *
 * Resets all information in the context that is directly related to
 * the latest mount (spec, source, target, mount options, ...).
 *
 * The match patterns, target namespace, prefix, cached fstab, cached canonicalized
 * paths and tags and [e]uid are not reset. You have to use
 *
 *	mnt_context_set_fstab(cxt, NULL);
 *	mnt_context_set_cache(cxt, NULL);
 *	mnt_context_set_fstype_pattern(cxt, NULL);
 *	mnt_context_set_options_pattern(cxt, NULL);
 *	mnt_context_set_target_ns(cxt, NULL);
 *
 * to reset this stuff.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_reset_context(struct libmnt_context *cxt)
{
	int fl;

	if (!cxt)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "<---- reset [status=%d] ---->",
				mnt_context_get_status(cxt)));

	fl = cxt->flags;

	mnt_unref_fs(cxt->fs);
	mnt_unref_table(cxt->mountinfo);
	mnt_unref_table(cxt->utab);
	mnt_unref_optlist(cxt->optlist);

	free(cxt->helper);

	cxt->fs = NULL;
	cxt->mountinfo = NULL;
	cxt->optlist = NULL;
	cxt->utab = NULL;
	cxt->helper = NULL;
	cxt->mountdata = NULL;
	cxt->flags = MNT_FL_DEFAULT;
	cxt->noautofs = 0;
	cxt->has_selinux_opt = 0;

	cxt->map_linux = mnt_get_builtin_optmap(MNT_LINUX_MAP);
	cxt->map_userspace = mnt_get_builtin_optmap(MNT_USERSPACE_MAP);

	mnt_context_reset_status(cxt);
	mnt_context_deinit_hooksets(cxt);

	if (cxt->table_fltrcb)
		mnt_context_set_tabfilter(cxt, NULL, NULL);

	/* restore non-resettable flags */
	cxt->flags |= (fl & MNT_FL_NOMTAB);
	cxt->flags |= (fl & MNT_FL_FAKE);
	cxt->flags |= (fl & MNT_FL_SLOPPY);
	cxt->flags |= (fl & MNT_FL_VERBOSE);
	cxt->flags |= (fl & MNT_FL_NOHELPERS);
	cxt->flags |= (fl & MNT_FL_LOOPDEL);
	cxt->flags |= (fl & MNT_FL_LAZY);
	cxt->flags |= (fl & MNT_FL_FORK);
	cxt->flags |= (fl & MNT_FL_FORCE);
	cxt->flags |= (fl & MNT_FL_NOCANONICALIZE);
	cxt->flags |= (fl & MNT_FL_RDONLY_UMOUNT);
	cxt->flags |= (fl & MNT_FL_RWONLY_MOUNT);
	cxt->flags |= (fl & MNT_FL_NOSWAPMATCH);
	cxt->flags |= (fl & MNT_FL_TABPATHS_CHECKED);

	mnt_context_apply_template(cxt);

	return 0;
}

/*
 * Saves the current context setting (mount options, etc) to make it usable after
 * mnt_reset_context() or by mnt_context_apply_template(). This is usable for
 * example for mnt_context_next_mount() where for the next mount operation we
 * need to restore to the original context setting.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_save_template(struct libmnt_context *cxt)
{
	if (!cxt)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "saving template"));

	/* reset old saved data */
	mnt_unref_optlist(cxt->optlist_saved);
	cxt->optlist_saved = NULL;

	if (cxt->optlist)
		cxt->optlist_saved = mnt_copy_optlist(cxt->optlist);

	return 0;
}

/*
 * Restores context FS setting from previously saved template (see
 * mnt_context_save_template()).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_apply_template(struct libmnt_context *cxt)
{
	if (!cxt)
		return -EINVAL;

	if (cxt->optlist) {
		mnt_unref_optlist(cxt->optlist);
		cxt->optlist = NULL;
	}

	if (cxt->optlist_saved) {
		DBG(CXT, ul_debugobj(cxt, "restoring template"));
		cxt->optlist = mnt_copy_optlist(cxt->optlist_saved);
	}

	return 0;
}

int mnt_context_has_template(struct libmnt_context *cxt)
{
	return cxt && cxt->optlist_saved ? 1 : 0;
}

struct libmnt_context *mnt_copy_context(struct libmnt_context *o)
{
	struct libmnt_context *n;

	n = mnt_new_context();
	if (!n)
		return NULL;

	DBG(CXT, ul_debugobj(n, "<---- clone ---->"));

	n->flags = o->flags;

	if (o->fs) {
		n->fs = mnt_copy_fs(NULL, o->fs);
		if (!n->fs)
			goto failed;
	}

	n->mountinfo = o->mountinfo;
	mnt_ref_table(n->mountinfo);

	n->utab = o->utab;
	mnt_ref_table(n->utab);

	if (strdup_between_structs(n, o, tgt_prefix))
		goto failed;
	if (strdup_between_structs(n, o, helper))
		goto failed;

	n->map_linux = o->map_linux;
	n->map_userspace = o->map_userspace;

	mnt_context_reset_status(n);

	n->table_fltrcb = o->table_fltrcb;
	n->table_fltrcb_data = o->table_fltrcb_data;

	n->noautofs = o->noautofs;
	n->has_selinux_opt = o->has_selinux_opt;

	return n;
failed:
	mnt_free_context(n);
	return NULL;
}

/**
 * mnt_context_reset_status:
 * @cxt: context
 *
 * Resets mount(2) and mount.type statuses, so mnt_context_do_mount() or
 * mnt_context_do_umount() could be again called with the same settings.
 *
 * BE CAREFUL -- after this soft reset the libmount will NOT parse mount
 * options, evaluate permissions or apply stuff from fstab.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_reset_status(struct libmnt_context *cxt)
{
	if (!cxt)
		return -EINVAL;

	reset_syscall_status(cxt);

	cxt->syscall_status = 1;		/* means not called yet */
	cxt->helper_exec_status = 1;
	cxt->helper_status = 0;
	return 0;
}

static int context_init_paths(struct libmnt_context *cxt, int writable)
{
	struct libmnt_ns *ns_old;

	assert(cxt);

	if (!cxt->utab_path) {
		cxt->utab_path = mnt_get_utab_path();
		DBG(CXT, ul_debugobj(cxt, "utab path initialized to: %s", cxt->utab_path));
	}

	if (!writable)
		return 0;		/* only paths wanted */
	if (mnt_context_is_nomtab(cxt))
		return 0;		/* write mode overridden by mount -n */
	if (cxt->flags & MNT_FL_TABPATHS_CHECKED)
		return 0;

	DBG(CXT, ul_debugobj(cxt, "checking for writable tab files"));

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	mnt_has_regular_utab(&cxt->utab_path, &cxt->utab_writable);

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	cxt->flags |= MNT_FL_TABPATHS_CHECKED;
	return 0;
}

int mnt_context_utab_writable(struct libmnt_context *cxt)
{
	assert(cxt);

	context_init_paths(cxt, 1);
	return cxt->utab_writable == 1;
}

const char *mnt_context_get_writable_tabpath(struct libmnt_context *cxt)
{
	assert(cxt);

	return mnt_context_utab_writable(cxt) ? cxt->utab_path : NULL;
}


static int set_flag(struct libmnt_context *cxt, int flag, int enable)
{
	if (!cxt)
		return -EINVAL;
	if (enable) {
		DBG(CXT, ul_debugobj(cxt, "enabling flag %04x", flag));
		cxt->flags |= flag;
	} else {
		DBG(CXT, ul_debugobj(cxt, "disabling flag %04x", flag));
		cxt->flags &= ~flag;
	}
	return 0;
}

/**
 * mnt_context_is_restricted:
 * @cxt: mount context
 *
 * Returns: 0 for an unrestricted mount (user is root), or 1 for non-root mounts
 */
int mnt_context_is_restricted(struct libmnt_context *cxt)
{
	return cxt->restricted;
}

/**
 * mnt_context_force_unrestricted:
 * @cxt: mount context
 *
 * This function is DANGEROURS as it disables all security policies in libmount.
 * Don't use if not sure. It removes "restricted" flag from the context, so
 * libmount will use the current context as for root user.
 *
 * This function is designed for case you have no any suid permissions, so you
 * can depend on kernel.
 *
 * Returns: 0 on success, negative number in case of error.
 *
 * Since: 2.35
 */
int mnt_context_force_unrestricted(struct libmnt_context *cxt)
{
	if (mnt_context_is_restricted(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "force UNRESTRICTED"));
		cxt->restricted = 0;
	}

	return 0;
}

/**
 * mnt_context_set_optsmode
 * @cxt: mount context
 * @mode: MNT_OMODE_* flags
 *
 * Controls how to use mount optionssource and target paths from fstab/mountinfo.
 *
 * @MNT_OMODE_IGNORE: ignore fstab options
 *
 * @MNT_OMODE_APPEND: append fstab options to existing options
 *
 * @MNT_OMODE_PREPEND: prepend fstab options to existing options
 *
 * @MNT_OMODE_REPLACE: replace existing options with options from fstab
 *
 * @MNT_OMODE_FORCE: always read fstab (although source and target are defined)
 *
 * @MNT_OMODE_FSTAB: read from fstab
 *
 * @MNT_OMODE_MTAB: read from mountinfo if fstab not enabled or failed
 *
 * @MNT_OMODE_NOTAB: do not read fstab/mountinfoat all
 *
 * @MNT_OMODE_AUTO: default mode (MNT_OMODE_PREPEND | MNT_OMODE_FSTAB | MNT_OMODE_MTAB)
 *
 * @MNT_OMODE_USER: default for non-root users (MNT_OMODE_REPLACE | MNT_OMODE_FORCE | MNT_OMODE_FSTAB)
 *
 * Notes:
 *
 * - MNT_OMODE_USER is always used if mount context is in restricted mode
 * - MNT_OMODE_AUTO is used if nothing else is defined
 * - the flags are evaluated in this order: MNT_OMODE_NOTAB, MNT_OMODE_FORCE,
 *   MNT_OMODE_FSTAB, MNT_OMODE_MTAB and then the mount options from fstab/mountinfo
 *   are set according to MNT_OMODE_{IGNORE,APPEND,PREPEND,REPLACE}
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_optsmode(struct libmnt_context *cxt, int mode)
{
	if (!cxt)
		return -EINVAL;
	cxt->optsmode = mode;
	return 0;
}

/**
 * mnt_context_get_optsmode
 * @cxt: mount context
 *
 * Returns: MNT_OMODE_* mask or zero.
 */

int mnt_context_get_optsmode(struct libmnt_context *cxt)
{
	return cxt->optsmode;
}

/**
 * mnt_context_disable_canonicalize:
 * @cxt: mount context
 * @disable: TRUE or FALSE
 *
 * Enable/disable paths canonicalization and tags evaluation. The libmount context
 * canonicalizes paths when searching in fstab and when preparing source and target paths
 * for mount(2) syscall.
 *
 * This function has an effect on the private (within context) fstab instance only
 * (see mnt_context_set_fstab()). If you want to use an external fstab then you
 * need to manage your private struct libmnt_cache (see mnt_table_set_cache(fstab,
 * NULL).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_disable_canonicalize(struct libmnt_context *cxt, int disable)
{
	return set_flag(cxt, MNT_FL_NOCANONICALIZE, disable);
}

/**
 * mnt_context_is_nocanonicalize:
 * @cxt: mount context
 *
 * Returns: 1 if no-canonicalize mode is enabled or 0.
 */
int mnt_context_is_nocanonicalize(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_NOCANONICALIZE ? 1 : 0;
}

/**
 * mnt_context_enable_lazy:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable lazy umount (see umount(8) man page, option -l).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_lazy(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_LAZY, enable);
}

/**
 * mnt_context_is_lazy:
 * @cxt: mount context
 *
 * Returns: 1 if lazy umount is enabled or 0
 */
int mnt_context_is_lazy(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_LAZY ? 1 : 0;
}

/**
 * mnt_context_enable_onlyonce:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable only-once mount (check if FS is not already mounted).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_onlyonce(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_ONLYONCE, enable);
}

/**
 * mnt_context_is_onlyonce:
 * @cxt: mount context
 *
 * Returns: 1 if only-once mount is enabled or 0
 */
int mnt_context_is_onlyonce(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_ONLYONCE ? 1 : 0;
}

/**
 * mnt_context_enable_fork:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable fork(2) call in mnt_context_next_mount() (see mount(8) man
 * page, option -F).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_fork(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_FORK, enable);
}

/**
 * mnt_context_is_fork:
 * @cxt: mount context
 *
 * Returns: 1 if fork (mount -F) is enabled or 0
 */
int mnt_context_is_fork(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_FORK ? 1 : 0;
}

/**
 * mnt_context_is_parent:
 * @cxt: mount context
 *
 * Return: 1 if mount -F enabled and the current context is parent, or 0
 */
int mnt_context_is_parent(struct libmnt_context *cxt)
{
	return mnt_context_is_fork(cxt) && cxt->pid == 0;
}

/**
 * mnt_context_is_child:
 * @cxt: mount context
 *
 * Return: 1 f the current context is child, or 0
 */
int mnt_context_is_child(struct libmnt_context *cxt)
{
	/* See mnt_fork_context(), the for fork flag is always disabled
	 * for children to avoid recursive forking.
	 */
	return !mnt_context_is_fork(cxt) && cxt->pid;
}

/**
 * mnt_context_enable_rdonly_umount:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable read-only remount on failed umount(2)
 * (see umount(8) man page, option -r).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_rdonly_umount(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_RDONLY_UMOUNT, enable);
}

/**
 * mnt_context_is_rdonly_umount
 * @cxt: mount context
 *
 * See also mnt_context_enable_rdonly_umount() and umount(8) man page,
 * option -r.
 *
 * Returns: 1 if read-only remount failed umount(2) is enables or 0
 */
int mnt_context_is_rdonly_umount(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_RDONLY_UMOUNT ? 1 : 0;
}

/**
 * mnt_context_enable_rwonly_mount:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Force read-write mount; if enabled libmount will never try MS_RDONLY
 * after failed mount(2) EROFS. (See mount(8) man page, option -w).
 *
 * Since: 2.30
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_rwonly_mount(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_RWONLY_MOUNT, enable);
}

/**
 * mnt_context_is_rwonly_mount
 * @cxt: mount context
 *
 * See also mnt_context_enable_rwonly_mount() and mount(8) man page,
 * option -w.
 *
 * Since: 2.30
 *
 * Returns: 1 if only read-write mount is allowed.
 */
int mnt_context_is_rwonly_mount(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_RWONLY_MOUNT ? 1 : 0;
}

/**
 * mnt_context_forced_rdonly:
 * @cxt: mount context
 *
 * See also mnt_context_enable_rwonly_mount().
 *
 * Since: 2.30
 *
 * Returns: 1 if mounted read-only on write-protected device.
 */
int mnt_context_forced_rdonly(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_FORCED_RDONLY ? 1 : 0;
}

/**
 * mnt_context_disable_helpers:
 * @cxt: mount context
 * @disable: TRUE or FALSE
 *
 * Enable/disable /sbin/[u]mount.* helpers (see mount(8) man page, option -i).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_disable_helpers(struct libmnt_context *cxt, int disable)
{
	return set_flag(cxt, MNT_FL_NOHELPERS, disable);
}

/**
 * mnt_context_is_nohelpers
 * @cxt: mount context
 *
 * Returns: 1 if helpers are disabled (mount -i) or 0
 */
int mnt_context_is_nohelpers(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_NOHELPERS ? 1 : 0;
}


/**
 * mnt_context_enable_sloppy:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Set/unset sloppy mounting (see mount(8) man page, option -s).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_sloppy(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_SLOPPY, enable);
}

/**
 * mnt_context_is_sloppy:
 * @cxt: mount context
 *
 * Returns: 1 if sloppy flag is enabled or 0
 */
int mnt_context_is_sloppy(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_SLOPPY ? 1 : 0;
}

/**
 * mnt_context_enable_fake:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable fake mounting (see mount(8) man page, option -f).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_fake(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_FAKE, enable);
}

/**
 * mnt_context_is_fake:
 * @cxt: mount context
 *
 * Returns: 1 if fake flag is enabled or 0
 */
int mnt_context_is_fake(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_FAKE ? 1 : 0;
}

/**
 * mnt_context_disable_mtab:
 * @cxt: mount context
 * @disable: TRUE or FALSE
 *
 * Disable/enable userspace mount table update (see mount(8) man page,
 * option -n). Originally /etc/mtab, now /run/mount/utab.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_disable_mtab(struct libmnt_context *cxt, int disable)
{
	return set_flag(cxt, MNT_FL_NOMTAB, disable);
}

/**
 * mnt_context_is_nomtab:
 * @cxt: mount context
 *
 * Returns: 1 if no-mtab is enabled or 0
 */
int mnt_context_is_nomtab(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_NOMTAB ? 1 : 0;
}

/**
 * mnt_context_disable_swapmatch:
 * @cxt: mount context
 * @disable: TRUE or FALSE
 *
 * Disable/enable swap between source and target for mount(8) if only one path
 * is specified.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_disable_swapmatch(struct libmnt_context *cxt, int disable)
{
	return set_flag(cxt, MNT_FL_NOSWAPMATCH, disable);
}

/**
 * mnt_context_is_swapmatch:
 * @cxt: mount context
 *
 * Returns: 1 if swap between source and target is allowed (default is 1) or 0.
 */
int mnt_context_is_swapmatch(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_NOSWAPMATCH ? 0 : 1;
}

/**
 * mnt_context_enable_force:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable force umounting (see umount(8) man page, option -f).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_force(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_FORCE, enable);
}

/**
 * mnt_context_is_force
 * @cxt: mount context
 *
 * Returns: 1 if force umounting flag is enabled or 0
 */
int mnt_context_is_force(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_FORCE ? 1 : 0;
}

/**
 * mnt_context_enable_verbose:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable verbose output (TODO: not implemented yet)
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_verbose(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_VERBOSE, enable);
}

/**
 * mnt_context_is_verbose
 * @cxt: mount context
 *
 * Returns: 1 if verbose flag is enabled or 0
 */
int mnt_context_is_verbose(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_VERBOSE ? 1 : 0;
}

/**
 * mnt_context_enable_loopdel:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable the loop delete (destroy) after umount (see umount(8), option -d)
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_loopdel(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_LOOPDEL, enable);
}

/**
 * mnt_context_is_loopdel:
 * @cxt: mount context
 *
 * Returns: 1 if loop device should be deleted after umount (umount -d) or 0.
 */
int mnt_context_is_loopdel(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_LOOPDEL ? 1 : 0;
}

/**
 * mnt_context_set_fs:
 * @cxt: mount context
 * @fs: filesystem description
 *
 * The mount context uses private @fs by default. This function can be used to
 * overwrite the private @fs with an external instance. This function
 * increments @fs reference counter (and decrement reference counter of the
 * old fs).
 *
 * The @fs will be modified by mnt_context_set_{source,target,options,fstype}
 * functions, If the @fs is NULL, then all current FS specific settings (source,
 * target, etc., exclude spec) are reset.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_fs(struct libmnt_context *cxt, struct libmnt_fs *fs)
{
	if (!cxt)
		return -EINVAL;

	if (cxt->fs == fs)
		return 0;

	DBG(CXT, ul_debugobj(cxt, "setting new FS"));

	/* new */
	if (fs) {
		struct libmnt_optlist *ol = mnt_context_get_optlist(cxt);

		if (!ol)
			return -ENOMEM;

		mnt_ref_fs(fs);

		mnt_optlist_set_optstr(ol, mnt_fs_get_options(fs), NULL);
		mnt_fs_follow_optlist(fs, ol);
	}

	/* old */
	if (cxt->fs)
		mnt_fs_follow_optlist(cxt->fs, NULL);
	mnt_unref_fs(cxt->fs);

	cxt->fs = fs;
	return 0;
}

/**
 * mnt_context_get_fs:
 * @cxt: mount context
 *
 * The FS contains the basic description of mountpoint, fs type and so on.
 * Note that the FS is modified by mnt_context_set_{source,target,options,fstype}
 * functions.
 *
 * Returns: pointer to FS description or NULL in case of a calloc() error.
 */
struct libmnt_fs *mnt_context_get_fs(struct libmnt_context *cxt)
{
	if (!cxt)
		return NULL;
	if (!cxt->fs) {
		struct libmnt_optlist *ol = mnt_context_get_optlist(cxt);

		if (!ol)
			return NULL;
		cxt->fs = mnt_new_fs();
		if (!cxt->fs)
			return NULL;

		mnt_fs_follow_optlist(cxt->fs, ol);
	}
	return cxt->fs;
}

/**
 * mnt_context_get_fs_userdata:
 * @cxt: mount context
 *
 * Returns: pointer to userdata or NULL.
 */
void *mnt_context_get_fs_userdata(struct libmnt_context *cxt)
{
	return cxt->fs ? mnt_fs_get_userdata(cxt->fs) : NULL;
}

/**
 * mnt_context_get_fstab_userdata:
 * @cxt: mount context
 *
 * Returns: pointer to userdata or NULL.
 */
void *mnt_context_get_fstab_userdata(struct libmnt_context *cxt)
{
	return cxt->fstab ? mnt_table_get_userdata(cxt->fstab) : NULL;
}

/**
 * mnt_context_get_mtab_userdata:
 * @cxt: mount context
 *
 * The file /etc/mtab is no more used, @context points always to mountinfo
 * (/proc/self/mountinfo). The function uses "mtab" in the name for backward
 * compatibility only.
 *
 * Returns: pointer to userdata or NULL.
 */
void *mnt_context_get_mtab_userdata(struct libmnt_context *cxt)
{
	return cxt->mountinfo ? mnt_table_get_userdata(cxt->mountinfo) : NULL;
}

/**
 * mnt_context_set_source:
 * @cxt: mount context
 * @source: mount source (device, directory, UUID, LABEL, ...)
 *
 * Note that libmount does not interpret "nofail" (MNT_MS_NOFAIL)
 * mount option. The real return code is always returned, when
 * the device does not exist then it's usually MNT_ERR_NOSOURCE
 * from libmount or ENOENT, ENOTDIR, ENOTBLK, ENXIO from mount(2).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_source(struct libmnt_context *cxt, const char *source)
{
	return mnt_fs_set_source(mnt_context_get_fs(cxt), source);
}

/**
 * mnt_context_get_source:
 * @cxt: mount context
 *
 * Returns: returns pointer or NULL in case of error or if not set.
 */
const char *mnt_context_get_source(struct libmnt_context *cxt)
{
	return mnt_fs_get_source(mnt_context_get_fs(cxt));
}

/**
 * mnt_context_set_target:
 * @cxt: mount context
 * @target: mountpoint
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_target(struct libmnt_context *cxt, const char *target)
{
	return mnt_fs_set_target(mnt_context_get_fs(cxt), target);
}

/**
 * mnt_context_get_target:
 * @cxt: mount context
 *
 * Returns: returns pointer or NULL in case of error or if not set.
 */
const char *mnt_context_get_target(struct libmnt_context *cxt)
{
	return mnt_fs_get_target(mnt_context_get_fs(cxt));
}

/**
 * mnt_context_set_target_prefix:
 * @cxt: mount context
 * @path: mountpoint prefix
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_target_prefix(struct libmnt_context *cxt, const char *path)
{
	char *p = NULL;

	if (!cxt)
		return -EINVAL;
	if (path) {
		p = strdup(path);
		if (!p)
			return -ENOMEM;
	}
	free(cxt->tgt_prefix);
	cxt->tgt_prefix = p;

	return 0;
}

/**
 * mnt_context_get_target_prefix:
 * @cxt: mount context
 *
 * Returns: returns pointer or NULL in case of error or if not set.
 */
const char *mnt_context_get_target_prefix(struct libmnt_context *cxt)
{
	return cxt ? cxt->tgt_prefix : NULL;
}


/**
 * mnt_context_set_fstype:
 * @cxt: mount context
 * @fstype: filesystem type
 *
 * Note that the @fstype has to be a FS type. For patterns with
 * comma-separated list of filesystems or for the "nofs" notation, use
 * mnt_context_set_fstype_pattern().
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_fstype(struct libmnt_context *cxt, const char *fstype)
{
	return mnt_fs_set_fstype(mnt_context_get_fs(cxt), fstype);
}

/**
 * mnt_context_get_fstype:
 * @cxt: mount context
 *
 * Returns: pointer or NULL in case of error or if not set.
 */
const char *mnt_context_get_fstype(struct libmnt_context *cxt)
{
	return mnt_fs_get_fstype(mnt_context_get_fs(cxt));
}

struct libmnt_optlist *mnt_context_get_optlist(struct libmnt_context *cxt)
{
	if (!cxt)
		return NULL;
	if (!cxt->optlist) {
		cxt->optlist = mnt_new_optlist();
		if (!cxt->optlist)
			return NULL;
		if (mnt_optlist_register_map(cxt->optlist, cxt->map_linux))
			goto fail;
		if (mnt_optlist_register_map(cxt->optlist, cxt->map_userspace))
			goto fail;
	}

	return cxt->optlist;
fail:
	mnt_unref_optlist(cxt->optlist);
	return NULL;
}

/**
 * mnt_context_set_options:
 * @cxt: mount context
 * @optstr: comma delimited mount options
 *
 * Note that MS_MOVE cannot be specified as "string". It's operation that
 * is no supported in fstab (etc.)
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_options(struct libmnt_context *cxt, const char *optstr)
{
	struct libmnt_optlist *ls = mnt_context_get_optlist(cxt);

	if (!ls)
		return -ENOMEM;
	return mnt_optlist_set_optstr(ls, optstr, NULL);
}

/**
 * mnt_context_append_options:
 * @cxt: mount context
 * @optstr: comma delimited mount options
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_append_options(struct libmnt_context *cxt, const char *optstr)
{
	struct libmnt_optlist *ls = mnt_context_get_optlist(cxt);

	if (!ls)
		return -ENOMEM;
	return mnt_optlist_append_optstr(ls, optstr, NULL);
}

/**
 * mnt_context_get_options:
 * @cxt: mount context
 *
 * This function returns mount options set by mnt_context_set_options(),
 * mnt_context_append_options() or mnt_context_set_mflags();
 *
 * Before v2.39 this function ignored options specified by flags (see
 * mnt_context_set_mflags()) before mnt_context_prepare_mount() call. Now this
 * function always returns all mount options.
 *
 * Returns: pointer or NULL
 */
const char *mnt_context_get_options(struct libmnt_context *cxt)
{
	const char *str = NULL;

	if (cxt->optlist && !mnt_optlist_is_empty(cxt->optlist))
		mnt_optlist_get_optstr(cxt->optlist, &str, NULL, 0);
	return str;
}

/**
 * mnt_context_set_fstype_pattern:
 * @cxt: mount context
 * @pattern: FS name pattern (or NULL to reset the current setting)
 *
 * See mount(8), option -t.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_fstype_pattern(struct libmnt_context *cxt, const char *pattern)
{
	char *p = NULL;

	if (!cxt)
		return -EINVAL;
	if (pattern) {
		p = strdup(pattern);
		if (!p)
			return -ENOMEM;
	}
	free(cxt->fstype_pattern);
	cxt->fstype_pattern = p;
	return 0;
}

/**
 * mnt_context_set_options_pattern:
 * @cxt: mount context
 * @pattern: options pattern (or NULL to reset the current setting)
 *
 * See mount(8), option -O.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_options_pattern(struct libmnt_context *cxt, const char *pattern)
{
	char *p = NULL;

	if (!cxt)
		return -EINVAL;
	if (pattern) {
		p = strdup(pattern);
		if (!p)
			return -ENOMEM;
	}
	free(cxt->optstr_pattern);
	cxt->optstr_pattern = p;
	return 0;
}

/**
 * mnt_context_set_fstab:
 * @cxt: mount context
 * @tb: fstab
 *
 * The mount context reads /etc/fstab to the private struct libmnt_table by default.
 * This function can be used to overwrite the private fstab with an external
 * instance.
 *
 * This function modify the @tb reference counter. This function does not set
 * the cache for the @tb. You have to explicitly call mnt_table_set_cache(tb,
 * mnt_context_get_cache(cxt));
 *
 * The fstab is used read-only and is not modified, it should be possible to
 * share the fstab between more mount contexts (TODO: test it.)
 *
 * If the @tb argument is NULL, then the current private fstab instance is
 * reset.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_fstab(struct libmnt_context *cxt, struct libmnt_table *tb)
{
	if (!cxt)
		return -EINVAL;

	mnt_ref_table(tb);		/* new */
	mnt_unref_table(cxt->fstab);	/* old */

	cxt->fstab = tb;
	return 0;
}

/**
 * mnt_context_get_fstab:
 * @cxt: mount context
 * @tb: returns fstab
 *
 * See also mnt_table_parse_fstab() for more details about fstab.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_fstab(struct libmnt_context *cxt, struct libmnt_table **tb)
{
	struct libmnt_ns *ns_old;

	if (!cxt)
		return -EINVAL;
	if (!cxt->fstab) {
		int rc;

		cxt->fstab = mnt_new_table();
		if (!cxt->fstab)
			return -ENOMEM;
		if (cxt->table_errcb)
			mnt_table_set_parser_errcb(cxt->fstab, cxt->table_errcb);

		ns_old = mnt_context_switch_target_ns(cxt);
		if (!ns_old)
			return -MNT_ERR_NAMESPACE;

		mnt_table_set_cache(cxt->fstab, mnt_context_get_cache(cxt));
		rc = mnt_table_parse_fstab(cxt->fstab, NULL);

		if (!mnt_context_switch_ns(cxt, ns_old))
			return -MNT_ERR_NAMESPACE;

		if (rc)
			return rc;
	}

	if (tb)
		*tb = cxt->fstab;
	return 0;
}

int mnt_context_get_mountinfo(struct libmnt_context *cxt, struct libmnt_table **tb)
{
	int rc = 0;
	struct libmnt_ns *ns_old = NULL;

	if (!cxt)
		return -EINVAL;
	if (!cxt->mountinfo) {
		ns_old = mnt_context_switch_target_ns(cxt);
		if (!ns_old)
			return -MNT_ERR_NAMESPACE;

		context_init_paths(cxt, 0);

		cxt->mountinfo = mnt_new_table();
		if (!cxt->mountinfo) {
			rc = -ENOMEM;
			goto end;
		}

		mnt_table_enable_noautofs(cxt->mountinfo, cxt->noautofs);

		if (cxt->table_errcb)
			mnt_table_set_parser_errcb(cxt->mountinfo, cxt->table_errcb);
		if (cxt->table_fltrcb)
			mnt_table_set_parser_fltrcb(cxt->mountinfo,
					cxt->table_fltrcb,
					cxt->table_fltrcb_data);

		mnt_table_set_cache(cxt->mountinfo, mnt_context_get_cache(cxt));
	}

	/* Read the table; it's empty, because this first mnt_context_get_mountinfo()
	 * call, or because /proc was not accessible in previous calls */
	if (mnt_table_is_empty(cxt->mountinfo)) {
		if (!ns_old) {
			ns_old = mnt_context_switch_target_ns(cxt);
			if (!ns_old)
				return -MNT_ERR_NAMESPACE;
		}

		rc = __mnt_table_parse_mountinfo(cxt->mountinfo, NULL, cxt->utab);
		if (rc)
			goto end;
	}

	if (tb)
		*tb = cxt->mountinfo;

	DBG(CXT, ul_debugobj(cxt, "mountinfo requested [nents=%d]",
				mnt_table_get_nents(cxt->mountinfo)));

end:
	if (ns_old && !mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	return rc;
}

/**
 * mnt_context_get_mtab:
 * @cxt: mount context
 * @tb: returns mtab
 *
 * Parse /proc/self/mountinfo mount table.
 *
 * The file /etc/mtab is no more used, @context points always to mountinfo
 * (/proc/self/mountinfo). The function uses "mtab" in the name for backward
 * compatibility only.
 *
 * See also mnt_table_parse_mtab() for more details about mountinfo. The
 * result will be deallocated by mnt_free_context(@cxt).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_mtab(struct libmnt_context *cxt, struct libmnt_table **tb)
{
	return mnt_context_get_mountinfo(cxt, tb);
}

/*
 * Called by mountinfo parser to filter out entries, non-zero means that
 * an entry has to be filtered out.
 */
static int mountinfo_filter(struct libmnt_fs *fs, void *data)
{
	if (!fs || !data)
		return 0;
	if (mnt_fs_streq_target(fs, data))
		return 0;
	if (mnt_fs_streq_srcpath(fs, data))
		return 0;
	return 1;
}

/*
 * The same like mnt_context_get_mountinfo(), but does not read all mountinfo
 * file, but only entries relevant for @tgt.
 */
int mnt_context_get_mountinfo_for_target(struct libmnt_context *cxt,
				    struct libmnt_table **mountinfo,
				    const char *tgt)
{
	struct stat st;
	struct libmnt_cache *cache = NULL;
	char *cn_tgt = NULL;
	int rc;
	struct libmnt_ns *ns_old;

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	if (mnt_context_is_nocanonicalize(cxt))
		mnt_context_set_tabfilter(cxt, mountinfo_filter, (void *) tgt);

	else if (mnt_safe_stat(tgt, &st) == 0 && S_ISDIR(st.st_mode)) {
		cache = mnt_context_get_cache(cxt);
		cn_tgt = mnt_resolve_path(tgt, cache);
		if (cn_tgt)
			mnt_context_set_tabfilter(cxt, mountinfo_filter, cn_tgt);
	}

	rc = mnt_context_get_mountinfo(cxt, mountinfo);
	mnt_context_set_tabfilter(cxt, NULL, NULL);

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	if (cn_tgt && !cache)
		free(cn_tgt);

	return rc;
}

/*
 * Allows to specify a filter for tab file entries. The filter is called by
 * the table parser. Currently used for utab only.
 */
int mnt_context_set_tabfilter(struct libmnt_context *cxt,
			      int (*fltr)(struct libmnt_fs *, void *),
			      void *data)
{
	if (!cxt)
		return -EINVAL;

	cxt->table_fltrcb = fltr;
	cxt->table_fltrcb_data = data;

	if (cxt->mountinfo)
		mnt_table_set_parser_fltrcb(cxt->mountinfo,
				cxt->table_fltrcb,
				cxt->table_fltrcb_data);

	DBG(CXT, ul_debugobj(cxt, "tabfilter %s", fltr ? "ENABLED!" : "disabled"));
	return 0;
}

/**
 * mnt_context_get_table:
 * @cxt: mount context
 * @filename: e.g. /proc/self/mountinfo
 * @tb: returns the table
 *
 * This function allocates a new table and parses the @file. The parser error
 * callback and cache for tags and paths is set according to the @cxt setting.
 * See also mnt_table_parse_file().
 *
 * It's strongly recommended to use the mnt_context_get_mtab() and
 * mnt_context_get_fstab() functions for mountinfo and fstab files. This function
 * does not care about LIBMOUNT_* env.variables and does not merge userspace
 * options.
 *
 * The result will NOT be deallocated by mnt_free_context(@cxt).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_table(struct libmnt_context *cxt,
			  const char *filename, struct libmnt_table **tb)
{
	int rc;
	struct libmnt_ns *ns_old;

	if (!cxt || !tb)
		return -EINVAL;

	*tb = mnt_new_table();
	if (!*tb)
		return -ENOMEM;

	if (cxt->table_errcb)
		mnt_table_set_parser_errcb(*tb, cxt->table_errcb);

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	rc = mnt_table_parse_file(*tb, filename);

	if (rc) {
		mnt_unref_table(*tb);
		goto end;
	}

	mnt_table_set_cache(*tb, mnt_context_get_cache(cxt));

end:
	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	return rc;
}

/**
 * mnt_context_set_tables_errcb
 * @cxt: mount context
 * @cb: pointer to callback function
 *
 * The error callback is used for all tab files (e.g. mountinfo, fstab)
 * parsed within the context.
 *
 * See also mnt_context_get_mtab(),
 *          mnt_context_get_fstab(),
 *          mnt_table_set_parser_errcb().
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_tables_errcb(struct libmnt_context *cxt,
	int (*cb)(struct libmnt_table *tb, const char *filename, int line))
{
	if (!cxt)
		return -EINVAL;

	if (cxt->mountinfo)
		mnt_table_set_parser_errcb(cxt->mountinfo, cb);
	if (cxt->fstab)
		mnt_table_set_parser_errcb(cxt->fstab, cb);

	cxt->table_errcb = cb;
	return 0;
}

/**
 * mnt_context_set_cache:
 * @cxt: mount context
 * @cache: cache instance or NULL
 *
 * The mount context maintains a private struct libmnt_cache by default. This
 * function can be used to overwrite the private cache with an external instance.
 * This function increments cache reference counter.
 *
 * If the @cache argument is NULL, then the current cache instance is reset.
 * This function apply the cache to fstab and mountinfo instances (if already
 * exists).
 *
 * The old cache instance reference counter is de-incremented.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_cache(struct libmnt_context *cxt, struct libmnt_cache *cache)
{
	if (!cxt)
		return -EINVAL;

	mnt_ref_cache(cache);			/* new */
	mnt_unref_cache(cxt->cache);		/* old */

	cxt->cache = cache;

	if (cxt->mountinfo)
		mnt_table_set_cache(cxt->mountinfo, cache);
	if (cxt->fstab)
		mnt_table_set_cache(cxt->fstab, cache);

	return 0;
}

/**
 * mnt_context_get_cache
 * @cxt: mount context
 *
 * See also mnt_context_set_cache().
 *
 * Returns: pointer to cache or NULL if canonicalization is disabled.
 */
struct libmnt_cache *mnt_context_get_cache(struct libmnt_context *cxt)
{
	if (!cxt || mnt_context_is_nocanonicalize(cxt))
		return NULL;

	if (!cxt->cache) {
		struct libmnt_cache *cache = mnt_new_cache();
		mnt_context_set_cache(cxt, cache);
		mnt_unref_cache(cache);
	}
	return cxt->cache;
}

/**
 * mnt_context_set_passwd_cb:
 * @cxt: mount context
 * @get: callback to get password
 * @release: callback to release (deallocate) password
 *
 * Sets callbacks for encryption password (e.g encrypted loopdev). This
 * function is deprecated (encrypted loops are no longer supported).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_passwd_cb(struct libmnt_context *cxt,
			      char *(*get)(struct libmnt_context *),
			      void (*release)(struct libmnt_context *, char *))
{
	if (!cxt)
		return -EINVAL;
	cxt->pwd_get_cb = get;
	cxt->pwd_release_cb = release;
	return 0;
}

/**
 * mnt_context_get_lock:
 * @cxt: mount context
 *
 * The libmount applications don't have to care about utab locking, but with a
 * small exception: the application has to be able to remove the lock file when
 * interrupted by signal or signals have to be ignored when the lock is locked.
 *
 * The default behavior is to ignore all signals (except SIGALRM and
 * SIGTRAP for utab update) when the lock is locked. If this behavior
 * is unacceptable, then use:
 *
 *	lc = mnt_context_get_lock(cxt);
 *	if (lc)
 *		mnt_lock_block_signals(lc, FALSE);
 *
 * and don't forget to call mnt_unlock_file(lc) before exit.
 *
 * Returns: pointer to lock struct or NULL.
 */
struct libmnt_lock *mnt_context_get_lock(struct libmnt_context *cxt)
{
	/*
	 * DON'T call this function within libmount, it will always allocate
	 * the lock. The mnt_update_* functions are able to allocate the lock
	 * only when utab update is really necessary.
	 */
	if (!cxt || mnt_context_is_nomtab(cxt))
		return NULL;

	if (!cxt->lock) {
		cxt->lock = mnt_new_lock(
				mnt_context_get_writable_tabpath(cxt), 0);
		if (cxt->lock)
			mnt_lock_block_signals(cxt->lock, TRUE);
	}
	return cxt->lock;
}

/**
 * mnt_context_set_mflags:
 * @cxt: mount context
 * @flags: mount(2) flags (MS_* flags)
 *
 * Sets mount flags (see mount(2) man page).
 *
 * Note that order of mount options (strings) and flags matter if you mix
 * mnt_context_append_options() and mnt_context_set_mflags().
 *
 * Be careful if use MS_REC flag -- this is flags is generic for
 * all mask. In this case is better to use options string where
 * mount options are independent and nothign is applied to all options.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_mflags(struct libmnt_context *cxt, unsigned long flags)
{
	struct libmnt_optlist *ls = mnt_context_get_optlist(cxt);

	if (!ls)
		return -ENOMEM;

	return mnt_optlist_set_flags(ls, flags, cxt->map_linux);
}

/**
 * mnt_context_get_mflags:
 * @cxt: mount context
 * @flags: returns MS_* mount flags
 *
 * Converts mount options string to MS_* flags and bitwise-OR the result with
 * the already defined flags (see mnt_context_set_mflags()).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_mflags(struct libmnt_context *cxt, unsigned long *flags)
{
	struct libmnt_optlist *ls = mnt_context_get_optlist(cxt);

	if (!ls)
		return -ENOMEM;

	return mnt_optlist_get_flags(ls, flags, cxt->map_linux, 0);
}

/**
 * mnt_context_set_user_mflags:
 * @cxt: mount context
 * @flags: mount(2) flags (MNT_MS_* flags, e.g. MNT_MS_LOOP)
 *
 * Sets userspace mount flags.
 *
 * See also notes for mnt_context_set_mflags().
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_user_mflags(struct libmnt_context *cxt, unsigned long flags)
{
	struct libmnt_optlist *ls = mnt_context_get_optlist(cxt);

	if (!ls)
		return -ENOMEM;

	return mnt_optlist_set_flags(ls, flags, cxt->map_userspace);
}

/**
 * mnt_context_get_user_mflags:
 * @cxt: mount context
 * @flags: returns mount flags
 *
 * Converts mount options string to MNT_MS_* flags and bitwise-OR the result
 * with the already defined flags (see mnt_context_set_user_mflags()).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_user_mflags(struct libmnt_context *cxt, unsigned long *flags)
{
	struct libmnt_optlist *ls = mnt_context_get_optlist(cxt);

	if (!ls)
		return -ENOMEM;

	return mnt_optlist_get_flags(ls, flags, cxt->map_userspace, 0);
}

/**
 * mnt_context_set_mountdata:
 * @cxt: mount context
 * @data: mount(2) data
 *
 * The mount context generates mountdata from mount options by default. This
 * function can be used to overwrite this behavior, and @data will be used instead
 * of mount options.
 *
 * The libmount does not deallocate the data by mnt_free_context(). Note that
 * NULL is also valid mount data.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_mountdata(struct libmnt_context *cxt, void *data)
{
	if (!cxt)
		return -EINVAL;
	cxt->mountdata = data;
	cxt->flags |= MNT_FL_MOUNTDATA;
	return 0;
}

#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT
int mnt_context_open_tree(struct libmnt_context *cxt, const char *path, unsigned long mflg)
{
	unsigned long oflg = OPEN_TREE_CLOEXEC;
	int rc = 0, fd = -1;

	if (mflg == (unsigned long) -1) {
		rc = mnt_optlist_get_flags(cxt->optlist, &mflg, cxt->map_linux, 0);
		if (rc)
			return rc;
	}
	if (!path) {
		path = mnt_fs_get_target(cxt->fs);
		if (!path)
			return -EINVAL;
	}

	/* Classic -oremount,bind,ro is not bind operation, it's just
	 * VFS flags update only */
	if ((mflg & MS_BIND) && !(mflg & MS_REMOUNT)) {
		oflg |= OPEN_TREE_CLONE;

		if (mnt_optlist_is_rbind(cxt->optlist))
			oflg |= AT_RECURSIVE;
	}

	if (cxt->force_clone)
		oflg |= OPEN_TREE_CLONE;

	DBG(CXT, ul_debugobj(cxt, "open_tree(path=%s%s%s)", path,
				oflg & OPEN_TREE_CLONE ? " clone" : "",
				oflg & AT_RECURSIVE ? " recursive" : ""));
	fd = open_tree(AT_FDCWD, path, oflg);
	set_syscall_status(cxt, "open_tree", fd >= 0);

	return fd;
}
#endif

/*
 * Translates LABEL/UUID/path to mountable path
 */
int mnt_context_prepare_srcpath(struct libmnt_context *cxt)
{
	const char *path = NULL;
	struct libmnt_cache *cache;
	const char *t, *v, *src, *type;
	int rc = 0;
	struct libmnt_ns *ns_old;
	struct libmnt_optlist *ol;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, ul_debugobj(cxt, "--> preparing source path"));

	src = mnt_fs_get_source(cxt->fs);

	if (!src && mnt_context_propagation_only(cxt))
		/* mount --make-{shared,private,...} */
		return mnt_fs_set_source(cxt->fs, "none");

	/* ignore filesystems without source or filesystems
	 * where the source is a quasi-path (//foo/bar)
	 */
	if (!src || mnt_fs_is_netfs(cxt->fs))
		return 0;

	/* ZFS source is always "dataset", not a real path */
	type = mnt_fs_get_fstype(cxt->fs);
	if (type && strcmp(type, "zfs") == 0)
		return 0;

	DBG(CXT, ul_debugobj(cxt, "srcpath '%s'", src));

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	cache = mnt_context_get_cache(cxt);

	if (!mnt_fs_get_tag(cxt->fs, &t, &v)) {
		/*
		 * Source is TAG (evaluate)
		 */
		if (cache)
			path = mnt_resolve_tag(t, v, cache);

		rc = path ? mnt_fs_set_source(cxt->fs, path) : -MNT_ERR_NOSOURCE;

	} else if (cache && !mnt_fs_is_pseudofs(cxt->fs)) {
		/*
		 * Source is PATH (canonicalize)
		 */
		path = mnt_resolve_path(src, cache);
		if (path && strcmp(path, src) != 0)
			rc = mnt_fs_set_source(cxt->fs, path);
	 }

	if (rc) {
		DBG(CXT, ul_debugobj(cxt, "failed to prepare srcpath [rc=%d]", rc));
		goto end;
	}

	if (!path)
		path = src;

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	if (mnt_optlist_is_bind(ol)
	    || mnt_optlist_is_move(ol)
	    || mnt_optlist_is_remount(ol)
	    || mnt_fs_is_pseudofs(cxt->fs)) {
		DBG(CXT, ul_debugobj(cxt, "REMOUNT/BIND/MOVE/pseudo FS source: %s", path));
		goto end;
	}

	rc = mnt_context_call_hooks(cxt, MNT_STAGE_PREP_SOURCE);
	if (rc)
		goto end;

	DBG(CXT, ul_debugobj(cxt, "final srcpath '%s'",
				mnt_fs_get_source(cxt->fs)));

end:
	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;
	return rc;
}

/* Guess type, but not set to cxt->fs, always use free() for the result. It's
 * no error when we're not able to guess a filesystem type. Note that error
 * does not mean that result in @type is NULL.
 */
int mnt_context_guess_srcpath_fstype(struct libmnt_context *cxt, char **type)
{
	int rc = 0;
	struct libmnt_ns *ns_old;
	const char *dev;

	assert(type);
	assert(cxt);

	*type = NULL;

	dev = mnt_fs_get_srcpath(cxt->fs);
	if (!dev)
		return 0;

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	if (access(dev, F_OK) == 0) {
		struct libmnt_cache *cache = mnt_context_get_cache(cxt);
		int ambi = 0;

		*type = mnt_get_fstype(dev, &ambi, cache);
		if (ambi)
			rc = -MNT_ERR_AMBIFS;

		if (cache && *type) {
			*type = strdup(*type);
			if (!*type)
				rc = -ENOMEM;
		}
	} else {
		DBG(CXT, ul_debugobj(cxt, "access(%s) failed [%m]", dev));
		if (strchr(dev, ':') != NULL) {
			*type = strdup("nfs");
			if (!*type)
				rc = -ENOMEM;
		} else if (!strncmp(dev, "//", 2)) {
			*type = strdup("cifs");
			if (!*type)
				rc = -ENOMEM;
		}
	}

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	if (rc == 0 && *type) {
		struct libmnt_optlist *ol = mnt_context_get_optlist(cxt);
		struct libmnt_opt *opt;
		const char *allowed;

		if (!ol)
			return -ENOMEM;

		opt = mnt_optlist_get_named(ol,
				"X-mount.auto-fstypes", cxt->map_userspace);

		if (opt
		    && (allowed = mnt_opt_get_value(opt))
		    && !match_fstype(*type, allowed)) {
			DBG(CXT, ul_debugobj(cxt, "%s is not allowed by auto-fstypes=%s",
						*type, allowed));
			free(*type);
			*type = NULL;
			rc = -MNT_ERR_NOFSTYPE;
		}
	}

	return rc;
}

/*
 * It's usually no error when we're not able to detect the filesystem type -- we
 * will try to use the types from /{etc,proc}/filesystems.
 */
int mnt_context_guess_fstype(struct libmnt_context *cxt)
{
	struct libmnt_optlist *ol;
	char *type;
	int rc = 0;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, ul_debugobj(cxt, "--> preparing fstype"));

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;

	if (mnt_optlist_is_bind(ol)
	    || mnt_optlist_is_move(ol)
	    || mnt_context_propagation_only(cxt))
		goto none;

	type = (char *) mnt_fs_get_fstype(cxt->fs);
	if (type && !strcmp(type, "auto")) {
		mnt_fs_set_fstype(cxt->fs, NULL);
		type = NULL;
	}

	if (type)
		goto done;
	if (mnt_optlist_is_remount(ol))
		goto none;
	if (cxt->fstype_pattern)
		goto done;

	rc = mnt_context_guess_srcpath_fstype(cxt, &type);
	if (rc == 0 && type)
		__mnt_fs_set_fstype_ptr(cxt->fs, type);
	else
		free(type);
done:
	DBG(CXT, ul_debugobj(cxt, "FS type: %s [rc=%d]",
				mnt_fs_get_fstype(cxt->fs), rc));
	return rc;
none:
	return mnt_fs_set_fstype(cxt->fs, "none");
}

/*
 * The default is to use fstype from cxt->fs, this could be overwritten by
 * @type. The @act is MNT_ACT_{MOUNT,UMOUNT}.
 *
 * Returns: 0 on success or negative number in case of error. Note that success
 * does not mean that there is any usable helper, you have to check cxt->helper.
 */
int mnt_context_prepare_helper(struct libmnt_context *cxt, const char *name,
				const char *type)
{
	char search_path[] = FS_SEARCH_PATH;		/* from config.h */
	char *p = NULL, *path;
	struct libmnt_ns *ns_old;
	int rc = 0;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, ul_debugobj(cxt, "checking for helper"));

	if (cxt->helper) {
		free(cxt->helper);
		cxt->helper = NULL;
	}

	if (!type)
		type = mnt_fs_get_fstype(cxt->fs);

	if (type && strchr(type, ','))
		return 0;			/* type is fstype pattern */

	if (mnt_context_is_nohelpers(cxt)
	    || !type
	    || !strcmp(type, "none")
	    || strstr(type, "/..")		/* don't try to smuggle path */
	    || mnt_fs_is_swaparea(cxt->fs))
		return 0;

	ns_old = mnt_context_switch_origin_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	/* Ignore errors when search in $PATH and do not modify @rc
	 */
	path = strtok_r(search_path, ":", &p);
	while (path) {
		char helper[PATH_MAX];
		int len, found = 0;

		len = snprintf(helper, sizeof(helper), "%s/%s.%s",
						path, name, type);
		path = strtok_r(NULL, ":", &p);

		if (len < 0 || (size_t) len >= sizeof(helper))
			continue;

		found = mnt_is_path(helper);
		if (!found && strchr(type, '.')) {
			/* If type ends with ".subtype" try without it */
			char *hs = strrchr(helper, '.');
			if (hs)
				*hs = '\0';
			found = mnt_is_path(helper);
		}

		DBG(CXT, ul_debugobj(cxt, "%-25s ... %s", helper,
					found ? "found" : "not found"));
		if (!found)
			continue;

		/* success */
		rc = strdup_to_struct_member(cxt, helper, helper);
		break;
	}

	if (!mnt_context_switch_ns(cxt, ns_old))
		rc = -MNT_ERR_NAMESPACE;

	/* make sure helper is not set on error */
	if (rc) {
		free(cxt->helper);
		cxt->helper = NULL;
	}
	return rc;
}

/* stop differentiate between options defined by flags and strings */
int mnt_context_merge_mflags(struct libmnt_context *cxt)
{
	struct libmnt_optlist *ls = mnt_context_get_optlist(cxt);

	if (!ls)
		return -ENOMEM;

	/* TODO: optlist returns always flags as merged, so
	 * MNT_FL_MOUNTFLAGS_MERGED is unncessary anymore
	 */
	cxt->flags |= MNT_FL_MOUNTFLAGS_MERGED;
	return mnt_optlist_merge_opts(ls);
}

/*
 * Prepare /run/mount/utab
 */
int mnt_context_prepare_update(struct libmnt_context *cxt)
{
	int rc;
	const char *target, *name;
	unsigned long flags = 0;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->action);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, ul_debugobj(cxt, "--> prepare update"));

	if (mnt_context_propagation_only(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "skip update: only MS_PROPAGATION"));
		return 0;
	}

	target = mnt_fs_get_target(cxt->fs);

	if (cxt->action == MNT_ACT_UMOUNT && target && !strcmp(target, "/")) {
		DBG(CXT, ul_debugobj(cxt, "root umount: setting NOMTAB"));
		mnt_context_disable_mtab(cxt, TRUE);
	}
	if (mnt_context_is_nomtab(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "skip update: NOMTAB flag"));
		return 0;
	}
	name = mnt_context_get_writable_tabpath(cxt);
	if (!name) {
		DBG(CXT, ul_debugobj(cxt, "skip update: no writable destination"));
		return 0;
	}
	/* 0 = success, 1 = not called yet */
	if (cxt->syscall_status != 1 && cxt->syscall_status != 0) {
		DBG(CXT, ul_debugobj(cxt,
				"skip update: syscall failed [status=%d]",
				cxt->syscall_status));
		return 0;
	}

	if (!cxt->update) {
		if (cxt->action == MNT_ACT_UMOUNT && is_file_empty(name)) {
			DBG(CXT, ul_debugobj(cxt, "skip update: umount, no table"));
			return 0;
		}

		cxt->update = mnt_new_update();
		if (!cxt->update)
			return -ENOMEM;

		mnt_update_set_filename(cxt->update, name);
	}

	mnt_context_get_mflags(cxt, &flags);

	if (cxt->action == MNT_ACT_UMOUNT)
		rc = mnt_update_set_fs(cxt->update, flags,
					mnt_context_get_target(cxt), NULL);
	else
		rc = mnt_update_set_fs(cxt->update, flags,
					NULL, cxt->fs);

	if (mnt_update_is_ready(cxt->update)) {
		DBG(CXT, ul_debugobj(cxt, "update is ready"));
		mnt_update_start(cxt->update);
	}
	return rc < 0 ? rc : 0;
}

int mnt_context_update_tabs(struct libmnt_context *cxt)
{
	int rc = 0;
	struct libmnt_ns *ns_old;

	assert(cxt);

	if (mnt_context_is_nomtab(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "don't update: NOMTAB flag"));
		return 0;
	}
	if (!cxt->update || !mnt_update_is_ready(cxt->update)) {
		DBG(CXT, ul_debugobj(cxt, "don't update: no update prepared"));
		return 0;
	}

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	/* check utab update when external helper executed */
	if (mnt_context_helper_executed(cxt)
	    && mnt_context_get_helper_status(cxt) == 0
	    && mnt_context_utab_writable(cxt)) {

		if (mnt_update_already_done(cxt->update)) {
			DBG(CXT, ul_debugobj(cxt, "don't update: error evaluate or already updated"));
			goto emit;
		}
	} else if (cxt->helper) {
		DBG(CXT, ul_debugobj(cxt, "don't update: external helper"));
		goto end;
	}

	if (cxt->syscall_status != 0
	    && !(mnt_context_helper_executed(cxt) &&
		 mnt_context_get_helper_status(cxt) == 0)) {

		DBG(CXT, ul_debugobj(cxt, "don't update: syscall/helper failed/not called"));
		goto end;
	}

	rc = mnt_update_table(cxt->update, cxt->lock);
emit:
	if (rc == 0 && !mnt_context_within_helper(cxt))
		mnt_update_emit_event(cxt->update);

end:
	mnt_update_end(cxt->update);

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;
	return rc;
}

/* apply @fs to @cxt;
 *
 * @mflags are mount flags as specified on command-line -- used only to save
 * MS_RDONLY which is allowed for non-root users.
 */
static int apply_fs(struct libmnt_context *cxt, struct libmnt_fs *fs, unsigned long mflags)
{
	struct libmnt_optlist *ls;
	int rc;



	if (!cxt->optsmode) {
		if (mnt_context_is_restricted(cxt)) {
			DBG(CXT, ul_debugobj(cxt, "force fstab usage for non-root users!"));
			cxt->optsmode = MNT_OMODE_USER;
		} else {
			DBG(CXT, ul_debugobj(cxt, "use default optsmode"));
			cxt->optsmode = MNT_OMODE_AUTO;
		}

	}

	if (!mnt_context_get_fs(cxt))
		return -ENOMEM;

	DBG(CXT, ul_debugobj(cxt, "apply entry:"));
	DBG(CXT, mnt_fs_print_debug(fs, stderr));
	DBG(CXT, ul_debugobj(cxt, "OPTSMODE (opt-part): ignore=%d, append=%d, prepend=%d, replace=%d",
				  cxt->optsmode & MNT_OMODE_IGNORE ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_APPEND ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_PREPEND ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_REPLACE ? 1 : 0));

	/* copy from fs to our FS description
	 */
	rc = mnt_fs_set_source(cxt->fs, mnt_fs_get_source(fs));
	if (!rc)
		rc = mnt_fs_set_target(cxt->fs, mnt_fs_get_target(fs));

	if (!rc && !mnt_fs_get_fstype(cxt->fs))
		rc = mnt_fs_set_fstype(cxt->fs, mnt_fs_get_fstype(fs));

	if (!rc && !mnt_fs_get_root(cxt->fs) && mnt_fs_get_root(fs))
		rc = mnt_fs_set_root(cxt->fs, mnt_fs_get_root(fs));

	if (rc)
		goto done;

	ls = mnt_context_get_optlist(cxt);
	if (!ls) {
		rc = -ENOMEM;
		goto done;
	}

	if (cxt->optsmode & MNT_OMODE_IGNORE)
		;
	else if (cxt->optsmode & MNT_OMODE_REPLACE) {
		rc = mnt_optlist_set_optstr(ls, mnt_fs_get_options(fs), NULL);

		/* mount --read-only for non-root users is allowed */
		if (rc == 0 && (mflags & MS_RDONLY)
		    && mnt_context_is_restricted(cxt)
		    && cxt->optsmode == MNT_OMODE_USER)
			rc = mnt_optlist_append_optstr(ls, "ro", NULL);
	}
	else if (cxt->optsmode & MNT_OMODE_APPEND)
		rc = mnt_optlist_append_optstr(ls, mnt_fs_get_options(fs), NULL);

	else if (cxt->optsmode & MNT_OMODE_PREPEND)
		rc = mnt_optlist_prepend_optstr(ls, mnt_fs_get_options(fs), NULL);

	if (!rc)
		cxt->flags |= MNT_FL_TAB_APPLIED;

done:
	DBG(CXT, ul_debugobj(cxt, "final entry [rc=%d]", rc));
	DBG(CXT, mnt_fs_print_debug(cxt->fs, stderr));

	return rc;
}

static int apply_table(struct libmnt_context *cxt, struct libmnt_table *tb,
		     int direction, unsigned long mflags)
{
	struct libmnt_fs *fs = NULL;
	const char *src, *tgt;

	assert(cxt);
	assert(cxt->fs);

	src = mnt_fs_get_source(cxt->fs);
	tgt = mnt_fs_get_target(cxt->fs);

	if (tgt && src)
		fs = mnt_table_find_pair(tb, src, tgt, direction);
	else {
		if (src)
			fs = mnt_table_find_source(tb, src, direction);
		else if (tgt)
			fs = mnt_table_find_target(tb, tgt, direction);

		if (!fs && mnt_context_is_swapmatch(cxt)) {
			/* swap source and target (if @src is not LABEL/UUID),
			 * for example in
			 *
			 *	mount /foo/bar
			 *
			 * the path could be a mountpoint as well as a source (for
			 * example bind mount, symlink to a device, ...).
			 */
			if (src && !mnt_fs_get_tag(cxt->fs, NULL, NULL))
				fs = mnt_table_find_target(tb, src, direction);
			if (!fs && tgt)
				fs = mnt_table_find_source(tb, tgt, direction);
		}
	}

	if (!fs)
		return -MNT_ERR_NOFSTAB;	/* not found */

	return apply_fs(cxt, fs, mflags);
}

/* apply @fs to @cxt -- use mnt_context_apply_fstab() if not sure
 */
int mnt_context_apply_fs(struct libmnt_context *cxt, struct libmnt_fs *fs)
{
	return apply_fs(cxt, fs, 0);
}

/**
 * mnt_context_apply_fstab:
 * @cxt: mount context
 *
 * This function is optional.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_apply_fstab(struct libmnt_context *cxt)
{
	int rc = -1, isremount = 0, iscmdbind = 0;
	struct libmnt_ns *ns_old;
	struct libmnt_table *tab = NULL;
	const char *src = NULL, *tgt = NULL;
	unsigned long mflags = 0;

	if (!cxt || !cxt->fs)
		return -EINVAL;

	if (mnt_context_tab_applied(cxt)) {	/* already applied */
		DBG(CXT, ul_debugobj(cxt, "fstab already applied -- skip"));
		return 0;
	}

	if (mnt_context_is_restricted(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "force fstab usage for non-root users!"));
		cxt->optsmode = MNT_OMODE_USER;
	} else if (cxt->optsmode == 0) {
		DBG(CXT, ul_debugobj(cxt, "use default optsmode"));
		cxt->optsmode = MNT_OMODE_AUTO;
	} else if (cxt->optsmode & MNT_OMODE_NOTAB) {
		cxt->optsmode &= ~MNT_OMODE_FSTAB;
		cxt->optsmode &= ~MNT_OMODE_MTAB;
		cxt->optsmode &= ~MNT_OMODE_FORCE;
	}

	if (mnt_context_get_mflags(cxt, &mflags) == 0) {
		isremount = !!(mflags & MS_REMOUNT);
		iscmdbind = !!(mflags & MS_BIND);
	}

	if (cxt->fs) {
		src = mnt_fs_get_source(cxt->fs);
		tgt = mnt_fs_get_target(cxt->fs);
	}

	DBG(CXT, ul_debugobj(cxt, "OPTSMODE (file-part): force=%d, fstab=%d, mtab=%d",
				  cxt->optsmode & MNT_OMODE_FORCE ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_FSTAB ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_MTAB ? 1 : 0));

	/* fstab is not required if source and target are specified */
	if (src && tgt && !(cxt->optsmode & MNT_OMODE_FORCE)) {
		DBG(CXT, ul_debugobj(cxt, "fstab not required -- skip"));
		return 0;
	}

	if (!src && tgt
	    && !(cxt->optsmode & MNT_OMODE_FSTAB)
	    && !(cxt->optsmode & MNT_OMODE_MTAB)) {
		DBG(CXT, ul_debugobj(cxt, "only target; fstab/mtab not required "
					  "-- skip, probably MS_PROPAGATION"));
		return 0;
	}

	/* let's initialize cxt->fs */
	ignore_result( mnt_context_get_fs(cxt) );

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	/* try fstab */
	if (cxt->optsmode & MNT_OMODE_FSTAB) {
		DBG(CXT, ul_debugobj(cxt, "trying to apply fstab (src=%s, target=%s)", src, tgt));
		rc = mnt_context_get_fstab(cxt, &tab);
		if (!rc)
			rc = apply_table(cxt, tab, MNT_ITER_FORWARD, mflags);
	}

	/* try mountinfo */
	if (rc < 0 && (cxt->optsmode & MNT_OMODE_MTAB)
	    && (isremount || cxt->action == MNT_ACT_UMOUNT)) {
		DBG(CXT, ul_debugobj(cxt, "trying to apply mountinfo (src=%s, target=%s)", src, tgt));
		if (tgt)
			rc = mnt_context_get_mountinfo_for_target(cxt, &tab, tgt);
		else
			rc = mnt_context_get_mountinfo(cxt, &tab);
		if (!rc)
			rc = apply_table(cxt, tab, MNT_ITER_BACKWARD, mflags);
	}

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	if (rc) {
		if (!mnt_context_is_restricted(cxt)
		    && tgt && !src
		    && isremount) {
			DBG(CXT, ul_debugobj(cxt, "only target; ignore missing mountinfo entry on remount"));
			return 0;
		}

		DBG(CXT, ul_debugobj(cxt, "failed to find entry in fstab/mountinfo [rc=%d]: %m", rc));

		/* force to "not found in fstab/mountinfo" error, the details why
		 * not found are not so important and may be misinterpreted by
		 * applications... */
		rc = -MNT_ERR_NOFSTAB;


	} else if (isremount && !iscmdbind && cxt->optlist) {

		/* ignore "bind" on remount when the flag is read from fstab */
		mnt_optlist_remove_named(cxt->optlist, "bind", NULL);
	}
	return rc;
}

/**
 * mnt_context_tab_applied:
 * @cxt: mount context
 *
 * Returns: 1 if fstab (or mountinfo) has been applied to the context, or 0.
 */
int mnt_context_tab_applied(struct libmnt_context *cxt)
{
	return cxt->flags & MNT_FL_TAB_APPLIED;
}

/*
 * This is not a public function!
 *
 * Returns 1 if *only propagation flags* change is requested.
 */
int mnt_context_propagation_only(struct libmnt_context *cxt)
{
	struct libmnt_optlist *ls;

	if (cxt->action != MNT_ACT_MOUNT)
		return 0;
	if (cxt->mountdata || cxt->fs == NULL)
		return 0;
	if (cxt->fs->fstype && strcmp(cxt->fs->fstype, "none") != 0)
		return 0;
	if (cxt->fs->source && strcmp(cxt->fs->source, "none") != 0)
		return 0;

	ls = mnt_context_get_optlist(cxt);
	return ls ? mnt_optlist_is_propagation_only(ls) : 0;
}

/**
 * mnt_context_get_status:
 * @cxt: mount context
 *
 * Global libmount status.
 *
 * The real exit code of the mount.type helper has to be tested by
 * mnt_context_get_helper_status(). The mnt_context_get_status() only informs
 * that exec() has been successful.
 *
 * Returns: 1 if mount.type or mount(2) syscall has been successfully called.
 */
int mnt_context_get_status(struct libmnt_context *cxt)
{
	return !cxt->syscall_status || !cxt->helper_exec_status;
}

/**
 * mnt_context_helper_executed:
 * @cxt: mount context
 *
 * Returns: 1 if mount.type helper has been executed, or 0.
 */
int mnt_context_helper_executed(struct libmnt_context *cxt)
{
	return cxt->helper_exec_status != 1;
}

/**
 * mnt_context_get_helper_status:
 * @cxt: mount context
 *
 * Return: mount.type helper exit status, result is reliable only if
 *         mnt_context_helper_executed() returns 1.
 */
int mnt_context_get_helper_status(struct libmnt_context *cxt)
{
	return cxt->helper_status;
}

/**
 * mnt_context_syscall_called:
 * @cxt: mount context
 *
 * Returns: 1 if mount(2) syscall has been called, or 0.
 */
int mnt_context_syscall_called(struct libmnt_context *cxt)
{
	return cxt->syscall_status != 1;
}

/**
 * mnt_context_get_syscall_errno:
 * @cxt: mount context
 *
 * The result from this function is reliable only if
 * mnt_context_syscall_called() returns 1.
 *
 * Returns: mount(2) errno if the syscall failed or 0.
 */
int mnt_context_get_syscall_errno(struct libmnt_context *cxt)
{
	if (cxt->syscall_status < 0)
		return -cxt->syscall_status;
	return 0;
}

/**
 * mnt_context_set_syscall_status:
 * @cxt: mount context
 * @status: mount(2) status
 *
 * The @status should be 0 on success, or negative number on error (-errno).
 *
 * This function should only be used if the [u]mount(2) syscall is NOT called by
 * libmount code.
 *
 * Returns: 0 or negative number in case of error.
 */
int mnt_context_set_syscall_status(struct libmnt_context *cxt, int status)
{
	if (!cxt)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "syscall status set to: %d", status));
	cxt->syscall_status = status;
	return 0;
}

/**
 * mnt_context_strerror
 * @cxt: context
 * @buf: buffer
 * @bufsiz: size of the buffer
 *
 * Not implemented, deprecated in favor or mnt_context_get_excode().
 *
 * Returns: 0 or negative number in case of error.
 */
int mnt_context_strerror(struct libmnt_context *cxt __attribute__((__unused__)),
			 char *buf __attribute__((__unused__)),
			 size_t bufsiz __attribute__((__unused__)))
{
	/* TODO: based on cxt->syscall_errno or cxt->helper_status */
	return 0;
}

/**
 * mnt_context_enable_noautofs:
 * @cxt: context
 * @ignore: ignore or don't ignore
 *
 * Enable/disable ignore autofs mount table entries on reading the
 * mount table. Only effective if the "ignore" mount option is being
 * used for autofs mounts (such as if automount(8) has been configured
 * to do so).
 */
int mnt_context_enable_noautofs(struct libmnt_context *cxt, int ignore)
{
	if (!cxt)
		return -EINVAL;
	cxt->noautofs = ignore ? 1 : 0;
	return 0;
}

int mnt_context_get_generic_excode(int rc, char *buf, size_t bufsz, const char *fmt, ...)
{
	va_list va;

	if (rc == 0)
		return MNT_EX_SUCCESS;

	va_start(va, fmt);

	/* we need to support "%m" */
	errno = rc < 0 ? -rc : rc;

	if (buf && bufsz && vsnprintf(buf, bufsz, fmt, va) < 0)
		*buf = '\0';

	switch (errno) {
	case EINVAL:
	case EPERM:
		rc = MNT_EX_USAGE;
		break;
	case ENOMEM:
		rc = MNT_EX_SYSERR;
		break;
	default:
		rc = MNT_EX_FAIL;
		break;
	}
	va_end(va);
	return rc;
}

/**
 * mnt_context_get_excode:
 * @cxt: context
 * @rc: return code of the previous operation
 * @buf: buffer to print error message (optional)
 * @bufsz: size of the buffer
 *
 * This function analyzes context, [u]mount syscall and external helper status
 * and @mntrc and generates unified return code (see MNT_EX_*) as expected
 * from mount(8) or umount(8).
 *
 * If the external helper (e.g. /sbin/mount.type) has been executed than it
 * returns status from wait() of the helper. It's not libmount fail if helper
 * returns some crazy undocumented codes...  See mnt_context_helper_executed()
 * and mnt_context_get_helper_status(). Note that mount(8) and umount(8) utils
 * always return code from helper without extra care about it.
 *
 * The current implementation does not read error message from external
 * helper into @buf.
 *
 * If the argument @buf is not NULL then error message is generated (if
 * anything failed).
 *
 * The @mntrc is usually return code from mnt_context_mount(),
 * mnt_context_umount(), or 'mntrc' as returned by mnt_context_next_mount().
 *
 * Since: 2.30
 *
 * Returns: MNT_EX_* codes.
 */
int mnt_context_get_excode(
			struct libmnt_context *cxt,
			int rc,
			char *buf,
			size_t bufsz)
{
	if (buf) {
		*buf = '\0'; /* for sure */

		if (!cxt->enabled_textdomain) {
			bindtextdomain(LIBMOUNT_TEXTDOMAIN, LOCALEDIR);
			cxt->enabled_textdomain = 1;
		}
	}

	switch (cxt->action) {
	case MNT_ACT_MOUNT:
		rc = mnt_context_get_mount_excode(cxt, rc, buf, bufsz);
		break;
	case MNT_ACT_UMOUNT:
		rc = mnt_context_get_umount_excode(cxt, rc, buf, bufsz);
		break;
	default:
		if (rc)
			rc = mnt_context_get_generic_excode(rc, buf, bufsz,
				_("operation failed: %m"));
		else
			rc = MNT_EX_SUCCESS;
		break;
	}

	DBG(CXT, ul_debugobj(cxt, "excode: rc=%d message=\"%s\"", rc,
				buf ? buf : "<no-message>"));
	return rc;
}

/**
 * mnt_context_init_helper
 * @cxt: mount context
 * @action: MNT_ACT_{UMOUNT,MOUNT}
 * @flags: not used now
 *
 * This function informs libmount that used from [u]mount.type helper.
 *
 * The function also calls mnt_context_disable_helpers() to avoid recursive
 * mount.type helpers calling. It you really want to call another
 * mount.type helper from your helper, then you have to explicitly enable this
 * feature by:
 *
 *	 mnt_context_disable_helpers(cxt, FALSE);
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_init_helper(struct libmnt_context *cxt, int action,
			    int flags __attribute__((__unused__)))
{
	int rc;

	if (!cxt)
		return -EINVAL;

	rc = mnt_context_disable_helpers(cxt, TRUE);
	if (!rc)
		rc = set_flag(cxt, MNT_FL_HELPER, 1);
	if (!rc)
		cxt->action = action;

	DBG(CXT, ul_debugobj(cxt, "initialized for [u]mount.<type> helper [rc=%d]", rc));
	return rc;
}

/*
 * libmount used in /sbin/[u]mount.<type> helper
 */
int mnt_context_within_helper(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_HELPER);
}

/**
 * mnt_context_helper_setopt:
 * @cxt: context
 * @c: getopt() result
 * @arg: getopt() optarg
 *
 * This function applies the [u]mount.type command line option (for example parsed
 * by getopt or getopt_long) to @cxt. All unknown options are ignored and
 * then 1 is returned.
 *
 * Returns: negative number on error, 1 if @c is unknown option, 0 on success.
 */
int mnt_context_helper_setopt(struct libmnt_context *cxt, int c, char *arg)
{
	if (cxt) {
		switch(cxt->action) {
		case MNT_ACT_MOUNT:
			return mnt_context_mount_setopt(cxt, c, arg);
		case MNT_ACT_UMOUNT:
			return mnt_context_umount_setopt(cxt, c, arg);
		}
	}
	return -EINVAL;
}

/**
 * mnt_context_is_fs_mounted:
 * @cxt: context
 * @fs: filesystem
 * @mounted: returns 1 for mounted and 0 for non-mounted filesystems
 *
 * Please, read the mnt_table_is_fs_mounted() description!
 *
 * Returns: 0 on success and negative number in case of error.
 */
int mnt_context_is_fs_mounted(struct libmnt_context *cxt,
			      struct libmnt_fs *fs, int *mounted)
{
	struct libmnt_table *mountinfo, *orig;
	int rc = 0;
	struct libmnt_ns *ns_old;

	if (!cxt || !fs || !mounted)
		return -EINVAL;

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	orig = cxt->mountinfo;
	rc = mnt_context_get_mountinfo(cxt, &mountinfo);
	if (rc == -ENOENT && mnt_fs_streq_target(fs, "/proc")) {
		if (!orig) {
			mnt_unref_table(cxt->mountinfo);
			cxt->mountinfo = NULL;
		}
		*mounted = 0;
		rc = 0;			/* /proc not mounted */

	} else if (rc == 0) {
		*mounted = __mnt_table_is_fs_mounted(mountinfo, fs,
				mnt_context_get_target_prefix(cxt));

	}

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;
	return rc;
}

static int mnt_context_add_child(struct libmnt_context *cxt, pid_t pid)
{
	pid_t *pids;

	if (!cxt)
		return -EINVAL;

	pids = reallocarray(cxt->children, cxt->nchildren + 1, sizeof(pid_t));
	if (!pids)
		return -ENOMEM;

	DBG(CXT, ul_debugobj(cxt, "add new child %d", pid));
	cxt->children = pids;
	cxt->children[cxt->nchildren++] = pid;

	return 0;
}

int mnt_fork_context(struct libmnt_context *cxt)
{
	int rc = 0;
	pid_t pid;

	assert(cxt);
	if (!mnt_context_is_parent(cxt))
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "forking context"));

	DBG_FLUSH;

	pid = fork();

	switch (pid) {
	case -1: /* error */
		DBG(CXT, ul_debugobj(cxt, "fork failed %m"));
		return -errno;

	case 0: /* child */
		cxt->pid = getpid();
		mnt_context_enable_fork(cxt, FALSE);
		DBG(CXT, ul_debugobj(cxt, "child created"));
		break;

	default:
		rc = mnt_context_add_child(cxt, pid);
		break;
	}

	return rc;
}

int mnt_context_wait_for_children(struct libmnt_context *cxt,
				  int *nchildren, int *nerrs)
{
	int i;

	if (!cxt)
		return -EINVAL;

	assert(mnt_context_is_parent(cxt));

	for (i = 0; i < cxt->nchildren; i++) {
		pid_t pid = cxt->children[i];
		int rc = 0, ret = 0;

		if (!pid)
			continue;
		do {
			DBG(CXT, ul_debugobj(cxt,
					"waiting for child (%d/%d): %d",
					i + 1, cxt->nchildren, pid));
			errno = 0;
			rc = waitpid(pid, &ret, 0);

		} while (rc == -1 && errno == EINTR);

		if (nchildren)
			(*nchildren)++;

		if (rc != -1 && nerrs) {
			if (WIFEXITED(ret))
				(*nerrs) += WEXITSTATUS(ret) == 0 ? 0 : 1;
			else
				(*nerrs)++;
		}
		cxt->children[i] = 0;
	}

	cxt->nchildren = 0;
	free(cxt->children);
	cxt->children = NULL;
	return 0;
}

static void close_ns(struct libmnt_ns *ns)
{
	if (ns->fd == -1)
		return;

	close(ns->fd);
	ns->fd = -1;

	mnt_unref_cache(ns->cache);
	ns->cache = NULL;
}

/**
 * mnt_context_set_target_ns:
 * @cxt: mount context
 * @path: path to target namespace or NULL
 *
 * Sets target namespace to namespace represented by @path. If @path is NULL,
 * target namespace is cleared.
 *
 * This function sets errno to ENOSYS and returns error if libmount is
 * compiled without namespaces support.
 *
 * Returns: 0 on success, negative number in case of error.
 *
 * Since: 2.33
 */
int mnt_context_set_target_ns(struct libmnt_context *cxt, const char *path)
{
	if (!cxt)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "Setting %s as target namespace", path));

	/* cleanup only */
	if (!path) {
		close_ns(&cxt->ns_orig);
		close_ns(&cxt->ns_tgt);
		return 0;
	}

#ifdef USE_LIBMOUNT_SUPPORT_NAMESPACES
	int errsv = 0;
	int tmp;

	errno = 0;

	/* open original namespace */
	if (cxt->ns_orig.fd == -1) {
		cxt->ns_orig.fd = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
		if (cxt->ns_orig.fd == -1)
			return -errno;
		cxt->ns_orig.cache = NULL;
	}

	/* open target (wanted) namespace */
	tmp = open(path, O_RDONLY | O_CLOEXEC);
	if (tmp == -1)
		return -errno;

	/* test whether namespace switching works */
	DBG(CXT, ul_debugobj(cxt, "Trying whether namespace is valid"));
	if (setns(tmp, CLONE_NEWNS)
	    || setns(cxt->ns_orig.fd, CLONE_NEWNS)) {
		errsv = errno;
		DBG(CXT, ul_debugobj(cxt, "setns(2) failed [errno=%d %m]", errno));
		goto err;
	}

	close_ns(&cxt->ns_tgt);

	cxt->ns_tgt.fd = tmp;
	cxt->ns_tgt.cache = NULL;

	return 0;
err:
	close(tmp);
	errno = errsv;

#else /* ! USE_LIBMOUNT_SUPPORT_NAMESPACES */
	errno = ENOSYS;
#endif
	return -errno;
}

/**
 * mnt_context_get_target_ns:
 * @cxt: mount context
 *
 * Returns: pointer to target namespace
 *
 * Since: 2.33
 */
struct libmnt_ns *mnt_context_get_target_ns(struct libmnt_context *cxt)
{
	return &cxt->ns_tgt;
}

/**
 * mnt_context_get_origin_ns:
 * @cxt: mount context
 *
 * Returns: pointer to original namespace
 *
 * Since: 2.33
 */
struct libmnt_ns *mnt_context_get_origin_ns(struct libmnt_context *cxt)
{
	return &cxt->ns_orig;
}


/**
 * mnt_context_switch_ns:
 * @cxt: mount context
 * @ns: namespace to switch to
 *
 * Switch to namespace specified by ns
 *
 * Typical usage:
 * <informalexample>
 *   <programlisting>
 *	struct libmnt_ns *ns_old;
 *	ns_old = mnt_context_switch_ns(cxt, mnt_context_get_target_ns(cxt));
 *	... code ...
 *	mnt_context_switch_ns(cxt, ns_old);
 *   </programlisting>
 * </informalexample>
 *
 * Returns: pointer to previous namespace or NULL on error
 *
 * Since: 2.33
 */
struct libmnt_ns *mnt_context_switch_ns(struct libmnt_context *cxt, struct libmnt_ns *ns)
{
	struct libmnt_ns *old = NULL;

	if (!cxt || !ns)
		return NULL;

	/*
	 * If mnt_context_set_target_ns() has never been used than @ns file
	 * descriptor is -1 and this function is noop.
	 */
	old = cxt->ns_cur;
	if (ns == old || ns->fd == -1)
		return old;

#ifdef USE_LIBMOUNT_SUPPORT_NAMESPACES
	/* remember the current cache */
	if (old->cache != cxt->cache) {
		mnt_unref_cache(old->cache);
		old->cache = cxt->cache;
		mnt_ref_cache(old->cache);
	}

	/* switch */
	DBG(CXT, ul_debugobj(cxt, "Switching to %s namespace",
		ns == mnt_context_get_target_ns(cxt) ? "target" :
		ns == mnt_context_get_origin_ns(cxt) ? "original" : "other"));

	if (setns(ns->fd, CLONE_NEWNS)) {
		int errsv = errno;

		DBG(CXT, ul_debugobj(cxt, "setns(2) failed [errno=%d %m]", errno));
		errno = errsv;
		return NULL;
	}

	/* update pointer to the current namespace */
	cxt->ns_cur = ns;

	/* update pointer to the cache */
	mnt_unref_cache(cxt->cache);
	cxt->cache = ns->cache;
	mnt_ref_cache(cxt->cache);
#endif /* USE_LIBMOUNT_SUPPORT_NAMESPACES */

	return old;
}

/**
 * mnt_context_switch_origin_ns:
 * @cxt: mount context
 *
 * Switch to original namespace
 *
 * This is shorthand for
 * <informalexample>
 *   <programlisting>
 *	mnt_context_switch_ns(cxt, mnt_context_get_origin_ns(cxt));
 *   </programlisting>
 * </informalexample>
 *
 * Returns: pointer to previous namespace or NULL on error
 *
 * Since: 2.33
 */
struct libmnt_ns *mnt_context_switch_origin_ns(struct libmnt_context *cxt)
{
	return mnt_context_switch_ns(cxt, mnt_context_get_origin_ns(cxt));
}

/**
 * mnt_context_switch_target_ns:
 * @cxt: mount context
 *
 * Switch to target namespace
 *
 * This is shorthand for
 * <informalexample>
 *   <programlisting>
 *	mnt_context_switch_ns(cxt, mnt_context_get_target_ns(cxt));
 *   </programlisting>
 * </informalexample>
 *
 * Returns: pointer to previous namespace or NULL on error
 *
 * Since: 2.33
 */
struct libmnt_ns *mnt_context_switch_target_ns(struct libmnt_context *cxt)
{
	return mnt_context_switch_ns(cxt, mnt_context_get_target_ns(cxt));
}


#ifdef TEST_PROGRAM

static int test_search_helper(struct libmnt_test *ts __attribute__((unused)),
			      int argc, char *argv[])
{
	struct libmnt_context *cxt;
	const char *type;
	int rc;

	if (argc < 2)
		return -EINVAL;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	type = argv[1];

	mnt_context_get_fs(cxt);		/* just to fill cxt->fs */
	cxt->flags |= MNT_FL_MOUNTFLAGS_MERGED;	/* fake */

	rc = mnt_context_prepare_helper(cxt, "mount", type);
	printf("helper is: %s\n", cxt->helper ? cxt->helper : "not found");

	mnt_free_context(cxt);
	return rc;
}


static struct libmnt_lock *lock;

static void lock_fallback(void)
{
	if (lock)
		mnt_unlock_file(lock);
}

static int test_mount(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	int idx = 1, rc = 0;
	struct libmnt_context *cxt;

	if (argc < 2)
		return -EINVAL;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	if (!strcmp(argv[idx], "-o")) {
		mnt_context_set_options(cxt, argv[idx + 1]);
		idx += 2;
	}
	if (!strcmp(argv[idx], "-t")) {
		/* TODO: use mnt_context_set_fstype_pattern() */
		mnt_context_set_fstype(cxt, argv[idx + 1]);
		idx += 2;
	}

	if (argc == idx + 1)
		/* mount <mountpoint>|<device> */
		mnt_context_set_target(cxt, argv[idx++]);

	else if (argc == idx + 2) {
		/* mount <device> <mountpoint> */
		mnt_context_set_source(cxt, argv[idx++]);
		mnt_context_set_target(cxt, argv[idx++]);
	}

	/* this is unnecessary! -- libmount is able to internally
	 * create and manage the lock
	 */
	lock = mnt_context_get_lock(cxt);
	if (lock)
		atexit(lock_fallback);

	rc = mnt_context_mount(cxt);
	if (rc)
		warn("failed to mount");
	else
		printf("successfully mounted\n");

	lock = NULL;	/* because we use atexit lock_fallback */
	mnt_free_context(cxt);
	return rc;
}

static int test_umount(struct libmnt_test *ts __attribute__((unused)),
		       int argc, char *argv[])
{
	int idx = 1, rc = 0;
	struct libmnt_context *cxt;

	if (argc < 2)
		return -EINVAL;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	if (!strcmp(argv[idx], "-t")) {
		mnt_context_set_fstype(cxt, argv[idx + 1]);
		idx += 2;
	}

	if (!strcmp(argv[idx], "-f")) {
		mnt_context_enable_force(cxt, TRUE);
		idx++;
	}

	if (!strcmp(argv[idx], "-l")) {
		mnt_context_enable_lazy(cxt, TRUE);
		idx++;
	}

	if (!strcmp(argv[idx], "-r")) {
		mnt_context_enable_rdonly_umount(cxt, TRUE);
		idx++;
	}

	if (argc == idx + 1) {
		/* mount <mountpoint>|<device> */
		mnt_context_set_target(cxt, argv[idx++]);
	} else {
		rc = -EINVAL;
		goto err;
	}

	lock = mnt_context_get_lock(cxt);
	if (lock)
		atexit(lock_fallback);

	rc = mnt_context_umount(cxt);
	if (rc)
		printf("failed to umount\n");
	else
		printf("successfully umounted\n");
err:
	lock = NULL;	/* because we use atexit lock_fallback */
	mnt_free_context(cxt);
	return rc;
}

static int test_flags(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	int idx = 1, rc = 0;
	struct libmnt_context *cxt;
	const char *opt = NULL;
	unsigned long flags = 0;

	if (argc < 2)
		return -EINVAL;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	if (!strcmp(argv[idx], "-o")) {
		mnt_context_set_options(cxt, argv[idx + 1]);
		idx += 2;
	}

	if (argc == idx + 1)
		/* mount <mountpoint>|<device> */
		mnt_context_set_target(cxt, argv[idx++]);

	rc = mnt_context_prepare_mount(cxt);
	if (rc)
		printf("failed to prepare mount %s\n", strerror(-rc));

	opt = mnt_fs_get_options(cxt->fs);
	if (opt)
		fprintf(stdout, "options: %s\n", opt);

	mnt_context_get_mflags(cxt, &flags);
	fprintf(stdout, "flags: %08lx\n", flags);

	mnt_free_context(cxt);
	return rc;
}

static int test_cxtsync(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_context *cxt;
	struct libmnt_fs *fs;
	unsigned long flags = 0;
	int rc;

	if (argc != 4)
		return -EINVAL;

	fs = mnt_new_fs();
	if (!fs)
		return -ENOMEM;

	rc = mnt_fs_set_options(fs, argv[1]);
	if (rc)
		return rc;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	rc = mnt_context_set_fs(cxt, fs);
	if (rc)
		return rc;

	rc = mnt_context_append_options(cxt, argv[2]);
	if (rc)
		return rc;

	rc = mnt_fs_append_options(fs, argv[3]);
	if (rc)
		return rc;

	mnt_context_get_mflags(cxt, &flags);

	printf("     fs options: %s\n", mnt_fs_get_options(fs));
	printf("context options: %s\n", mnt_context_get_options(cxt));
	printf(" context mflags: %08lx\n", flags);

	mnt_free_context(cxt);
	return 0;
}

static int test_mountall(struct libmnt_test *ts __attribute__((unused)),
			 int argc, char *argv[])
{
	struct libmnt_context *cxt;
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int mntrc, ignored, idx = 1;

	cxt = mnt_new_context();
	itr = mnt_new_iter(MNT_ITER_FORWARD);

	if (!cxt || !itr)
		return -ENOMEM;

	if (argc > 2) {
		if (argv[idx] && !strcmp(argv[idx], "-O")) {
			mnt_context_set_options_pattern(cxt, argv[idx + 1]);
			idx += 2;
		}
		if (argv[idx] && !strcmp(argv[idx], "-t"))
			mnt_context_set_fstype_pattern(cxt, argv[idx + 1]);
	}

	while (mnt_context_next_mount(cxt, itr, &fs, &mntrc, &ignored) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (ignored == 1)
			printf("%s: ignored: not match\n", tgt);
		else if (ignored == 2)
			printf("%s: ignored: already mounted\n", tgt);

		else if (!mnt_context_get_status(cxt)) {
			if (mntrc > 0) {
				errno = mntrc;
				warn("%s: mount failed", tgt);
			} else
				warnx("%s: mount failed", tgt);
		} else
			printf("%s: successfully mounted\n", tgt);
	}

	mnt_free_context(cxt);
	return 0;
}



int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
	{ "--mount",  test_mount,  "[-o <opts>] [-t <type>] <spec>|<src> <target>" },
	{ "--umount", test_umount, "[-t <type>] [-f][-l][-r] <src>|<target>" },
	{ "--mount-all", test_mountall,  "[-O <pattern>] [-t <pattern] mount all filesystems from fstab" },
	{ "--flags", test_flags,   "[-o <opts>] <spec>" },
	{ "--cxtsync", test_cxtsync, "<fsopts> <cxtopts> <fsopts>" },
	{ "--search-helper", test_search_helper, "<fstype>" },
	{ NULL }};

	umask(S_IWGRP|S_IWOTH);	/* to be compatible with mount(8) */

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
