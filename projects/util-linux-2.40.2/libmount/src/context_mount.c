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
 * SECTION: context-mount
 * @title: Mount context
 * @short_description: high-level API to mount operation.
 */
#include <sys/wait.h>
#include <sys/mount.h>

#include "mountP.h"
#include "strutils.h"

#if defined(HAVE_SMACK)
static int is_option(const char *name, const char *const *names)
{
	const char *const *p;

	for (p = names; p && *p; p++) {
		if (strcmp(name, *p) == 0)
			return 1;
	}
	return 0;
}
#endif /* HAVE_SMACK */

/*
 * this has to be called after mnt_context_evaluate_permissions()
 */
static int fix_optstr(struct libmnt_context *cxt)
{
	struct libmnt_optlist *ol;
	struct libmnt_opt *opt;
	struct libmnt_ns *ns_old;
	const char *val;
	int rc = 0;

	assert(cxt);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (cxt->flags & MNT_FL_MOUNTOPTS_FIXED)
		return 0;

	DBG(CXT, ul_debugobj(cxt, "--> preparing options"));

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -EINVAL;

	ns_old = mnt_context_switch_origin_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	/* Fix user (convert "user" to "user=username") */
	if (mnt_context_is_restricted(cxt)) {
		opt = mnt_optlist_get_opt(ol, MNT_MS_USER, cxt->map_userspace);
		if (opt) {
			char *name = mnt_get_username(getuid());

			if (!name)
				rc = -ENOMEM;
			else {
				rc = mnt_opt_set_value(opt, name);
				free(name);
			}
			if (rc)
				goto done;
		}
	}

	/* Fix UID */
	opt = mnt_optlist_get_named(ol, "uid", NULL);
	if (opt && (val = mnt_opt_get_value(opt)) && !isdigit_string(val)) {
		uid_t id;

		if (strcmp(val, "useruid") == 0)	/* UID of the current user */
			id = getuid();
		else
			rc = mnt_get_uid(val, &id);	/* UID for the username */
		if (!rc)
			rc = mnt_opt_set_u64value(opt, id);
		if (rc)
			goto done;
	}

	/* Fix GID */
	opt = mnt_optlist_get_named(ol, "gid", NULL);
	if (opt && (val = mnt_opt_get_value(opt)) && !isdigit_string(val)) {
		gid_t id;

		if (strcmp(val, "usergid") == 0)	/* UID of the current user */
			id = getgid();
		else
			rc = mnt_get_gid(val, &id);	/* UID for the groupname */
		if (!rc)
			rc = mnt_opt_set_u64value(opt, id);
		if (rc)
			goto done;
	}

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

#ifdef HAVE_SMACK
	/* Fix Smack */
	if (access("/sys/fs/smackfs", F_OK) != 0) {
		struct libmnt_iter itr;

		static const char *const smack_options[] = {
			"smackfsdef",
			"smackfsfloor",
			"smackfshat",
			"smackfsroot",
			"smackfstransmute",
			NULL,
		};
		mnt_reset_iter(&itr, MNT_ITER_FORWARD);

		while (mnt_optlist_next_opt(ol, &itr, &opt) == 0) {
			if (!is_option(mnt_opt_get_name(opt), smack_options))
				continue;
			rc = mnt_optlist_remove_opt(ol, opt);
			if (rc)
				goto done;
		}
	}
#endif
	rc = mnt_context_call_hooks(cxt, MNT_STAGE_PREP_OPTIONS);
done:
	DBG(CXT, ul_debugobj(cxt, "<-- preparing options done [rc=%d]", rc));
	cxt->flags |= MNT_FL_MOUNTOPTS_FIXED;

	if (rc)
		rc = -MNT_ERR_MOUNTOPT;
	return rc;
}

/*
 * this has to be called before fix_optstr()
 *
 * Note that user=<name> may be used by some filesystems as a filesystem
 * specific option (e.g. cifs). Yes, developers of such filesystems have
 * allocated pretty hot place in hell...
 */
static int evaluate_permissions(struct libmnt_context *cxt)
{
	struct libmnt_optlist *ol;
	struct libmnt_opt *opt = NULL;

	unsigned long user_flags = 0;	/* userspace mount flags */
	int rc;

	assert(cxt);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!cxt->fs)
		return 0;

	DBG(CXT, ul_debugobj(cxt, "mount: evaluating permissions"));

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -EINVAL;

	/* get userspace mount flags (user[=<name>] etc.*/
	rc = mnt_optlist_get_flags(ol, &user_flags, cxt->map_userspace, 0);
	if (rc)
		return rc;

	/*
	* Ignore user=<name> (if <name> is set). Let's keep it hidden
	* for normal library operations, but visible for /sbin/mount.<type>
	* helpers.
	*/
	if (user_flags & MNT_MS_USER
	    && (opt = mnt_optlist_get_opt(ol, MNT_MS_USER, cxt->map_userspace))
	    && mnt_opt_has_value(opt)) {
		DBG(CXT, ul_debugobj(cxt, "perms: user=<name> detected, ignore"));

		cxt->flags |= MNT_FL_SAVED_USER;

		mnt_opt_set_external(opt, 1);
		user_flags &= ~MNT_MS_USER;
	}

	if (!mnt_context_is_restricted(cxt)) {
		/*
		 * superuser mount
		 *
		 * Let's convert user, users, owenr and groups to MS_* flags
		 * to be compatible with non-root execution.
		 *
		 * The old deprecated way is to use mnt_optstr_get_flags().
		 */
		if (user_flags & (MNT_MS_OWNER | MNT_MS_GROUP))
			rc = mnt_optlist_remove_flags(ol,
					MNT_MS_OWNER | MNT_MS_GROUP, cxt->map_userspace);

		if (!rc && (user_flags & MNT_MS_OWNER))
			rc = mnt_optlist_insert_flags(ol,
					MS_OWNERSECURE, cxt->map_linux,
					MNT_MS_OWNER, cxt->map_userspace);

		if (!rc && (user_flags & MNT_MS_GROUP))
			rc = mnt_optlist_insert_flags(ol,
					MS_OWNERSECURE, cxt->map_linux,
					MNT_MS_GROUP, cxt->map_userspace);

		if (!rc && (user_flags & MNT_MS_USER)
		    && (opt = mnt_optlist_get_opt(ol, MNT_MS_USER, cxt->map_userspace))
		    && !mnt_opt_has_value(opt))
			rc = mnt_optlist_insert_flags(ol, MS_SECURE, cxt->map_linux,
					MNT_MS_USER, cxt->map_userspace);

		if (!rc && (user_flags & MNT_MS_USERS))
			rc = mnt_optlist_insert_flags(ol, MS_SECURE, cxt->map_linux,
					MNT_MS_USERS, cxt->map_userspace);

		DBG(CXT, ul_debugobj(cxt, "perms: superuser [rc=%d]", rc));
		if (rc)
			return rc;

		if (user_flags & (MNT_MS_OWNER | MNT_MS_GROUP |
				  MNT_MS_USER | MNT_MS_USERS))
			mnt_optlist_merge_opts(ol);
	} else {

		/*
		 * user mount
		 */
		if (!mnt_context_tab_applied(cxt))
		{
			DBG(CXT, ul_debugobj(cxt, "perms: fstab not applied, ignore user mount"));
			return -EPERM;
		}

		/*
		 * Insert MS_SECURE between system flags on position where is MNT_MS_USER
		 */
		if ((user_flags & MNT_MS_USER)
		    && (rc = mnt_optlist_insert_flags(ol, MS_SECURE, cxt->map_linux,
						    MNT_MS_USER, cxt->map_userspace)))
			return rc;

		if ((user_flags & MNT_MS_USERS)
		    && (rc = mnt_optlist_insert_flags(ol, MS_SECURE, cxt->map_linux,
						    MNT_MS_USERS, cxt->map_userspace)))
			return rc;

		/*
		 * MS_OWNER: Allow owners to mount when fstab contains the
		 * owner option.  Note that this should never be used in a high
		 * security environment, but may be useful to give people at
		 * the console the possibility of mounting a floppy.  MS_GROUP:
		 * Allow members of device group to mount. (Martin Dickopp)
		 */
		if (user_flags & (MNT_MS_OWNER | MNT_MS_GROUP)) {
			struct stat sb;
			struct libmnt_cache *cache = NULL;
			char *xsrc = NULL;
			const char *srcpath = mnt_fs_get_srcpath(cxt->fs);

			DBG(CXT, ul_debugobj(cxt, "perms: owner/group"));

			if (!srcpath) {					/* Ah... source is TAG */
				cache = mnt_context_get_cache(cxt);
				xsrc = mnt_resolve_spec(
						mnt_context_get_source(cxt),
						cache);
				srcpath = xsrc;
			}
			if (!srcpath) {
				DBG(CXT, ul_debugobj(cxt, "perms: src undefined"));
				return -EPERM;
			}

			if (strncmp(srcpath, "/dev/", 5) == 0 &&
			    stat(srcpath, &sb) == 0 &&
			    (((user_flags & MNT_MS_OWNER) && getuid() == sb.st_uid) ||
			     ((user_flags & MNT_MS_GROUP) && mnt_in_group(sb.st_gid)))) {

				/* insert MS_OWNERSECURE between system flags */
				if (user_flags & MNT_MS_OWNER)
					mnt_optlist_insert_flags(ol,
							MS_OWNERSECURE, cxt->map_linux,
							MNT_MS_OWNER, cxt->map_userspace);
				if (user_flags & MNT_MS_GROUP)
					mnt_optlist_insert_flags(ol,
							MS_OWNERSECURE, cxt->map_linux,
							MNT_MS_GROUP, cxt->map_userspace);

				/* continue as like "user" was specified */
				user_flags |= MNT_MS_USER;
				mnt_optlist_append_flags(ol, MNT_MS_USER, cxt->map_userspace);
			}

			if (!cache)
				free(xsrc);
		}

		if (!(user_flags & (MNT_MS_USER | MNT_MS_USERS))) {
			DBG(CXT, ul_debugobj(cxt, "perms: evaluation ends with -EPERMS [flags=0x%08lx]", user_flags));
			return -EPERM;
		}

		/* we have modified some flags (noexec, ...), let's cleanup the
		 * options to remove duplicate stuff etc.*/
		mnt_optlist_merge_opts(ol);
	}

	return 0;
}

/*
 * mnt_context_helper_setopt() backend
 *
 * This function applies the mount.type command line option (for example parsed
 * by getopt() or getopt_long()) to @cxt. All unknown options are ignored and
 * then 1 is returned.
 *
 * Returns: negative number on error, 1 if @c is unknown option, 0 on success.
 */
int mnt_context_mount_setopt(struct libmnt_context *cxt, int c, char *arg)
{
	int rc = -EINVAL;

	assert(cxt);
	assert(cxt->action == MNT_ACT_MOUNT);

	switch(c) {
	case 'f':
		rc = mnt_context_enable_fake(cxt, TRUE);
		break;
	case 'n':
		rc = mnt_context_disable_mtab(cxt, TRUE);
		break;
	case 'r':
		rc = mnt_context_append_options(cxt, "ro");
		break;
	case 'v':
		rc = mnt_context_enable_verbose(cxt, TRUE);
		break;
	case 'w':
		rc = mnt_context_append_options(cxt, "rw");
		break;
	case 'o':
		if (arg)
			rc = mnt_context_append_options(cxt, arg);
		break;
	case 's':
		rc = mnt_context_enable_sloppy(cxt, TRUE);
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

static int exec_helper(struct libmnt_context *cxt)
{
	struct libmnt_ns *ns_tgt = mnt_context_get_target_ns(cxt);
	char *namespace = NULL;
	int rc;
	pid_t pid;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, ul_debugobj(cxt, "mount: executing helper %s", cxt->helper));

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
		const char *args[14], *type;
		struct libmnt_optlist *ol = mnt_context_get_optlist(cxt);
		struct libmnt_opt *opt;
		const char *o = NULL;
		int i = 0;

		if (!ol)
			_exit(EXIT_FAILURE);

		/* Call helper with original user=<name> (aka "saved user")
		 * or remove the username at all.
		 */
		opt = mnt_optlist_get_opt(ol, MNT_MS_USER, cxt->map_userspace);
		if (opt && !(cxt->flags & MNT_FL_SAVED_USER))
			mnt_opt_set_value(opt, NULL);

		if (mnt_optlist_get_optstr(ol, &o, NULL, MNT_OL_FLTR_HELPERS))
			_exit(EXIT_FAILURE);

		if (drop_permissions() != 0)
			_exit(EXIT_FAILURE);

		if (!mnt_context_switch_origin_ns(cxt))
			_exit(EXIT_FAILURE);

		type = mnt_fs_get_fstype(cxt->fs);

		args[i++] = cxt->helper;		/* 1 */
		args[i++] = mnt_fs_get_srcpath(cxt->fs);/* 2 */
		args[i++] = mnt_fs_get_target(cxt->fs);	/* 3 */

		if (mnt_context_is_sloppy(cxt))
			args[i++] = "-s";		/* 4 */
		if (mnt_context_is_fake(cxt))
			args[i++] = "-f";		/* 5 */
		if (mnt_context_is_nomtab(cxt))
			args[i++] = "-n";		/* 6 */
		if (mnt_context_is_verbose(cxt))
			args[i++] = "-v";		/* 7 */
		if (o) {
			args[i++] = "-o";		/* 8 */
			args[i++] = o;			/* 9 */
		}
		if (type
		    && strchr(type, '.')
		    && !endswith(cxt->helper, type)) {
			args[i++] = "-t";		/* 10 */
			args[i++] = type;		/* 11 */
		}
		if (namespace) {
			args[i++] = "-N";		/* 11 */
			args[i++] = namespace;		/* 12 */
		}
		args[i] = NULL;				/* 13 */
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
 * The default is to use fstype from cxt->fs, this could be overwritten by
 * @try_type argument. If @try_type is specified then mount with MS_SILENT.
 *
 * Returns: 0 on success,
 *         >0 in case of mount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
static int do_mount(struct libmnt_context *cxt, const char *try_type)
{
	int rc = 0;
	char *org_type = NULL;
	struct libmnt_optlist *ol = NULL;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	mnt_context_reset_status(cxt);

	if (try_type) {
		rc = mnt_context_prepare_helper(cxt, "mount", try_type);
		if (rc)
			return rc;
	}

	if (cxt->helper)
		return exec_helper(cxt);

	if (try_type) {
		ol = mnt_context_get_optlist(cxt);
		assert(ol);

		mnt_optlist_append_flags(ol, MS_SILENT, cxt->map_linux);
		if (mnt_fs_get_fstype(cxt->fs)) {
			org_type = strdup(mnt_fs_get_fstype(cxt->fs));
			if (!org_type) {
				rc = -ENOMEM;
				goto done;
			}
		}
		mnt_fs_set_fstype(cxt->fs, try_type);
	}


	/*
	 * mount(2) or others syscalls
	 */
	if (!rc)
		rc = mnt_context_call_hooks(cxt, MNT_STAGE_MOUNT);

	if (rc == 0 && mnt_context_is_fake(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "FAKE (-f) set status=0"));
		cxt->syscall_status = 0;
	}

	if (org_type && rc != 0) {
		__mnt_fs_set_fstype_ptr(cxt->fs, org_type);
		org_type  = NULL;
	}

	if (rc == 0 && try_type && cxt->update) {
		struct libmnt_fs *fs = mnt_update_get_fs(cxt->update);
		if (fs)
			rc = mnt_fs_set_fstype(fs, try_type);
	}

done:
	if (try_type && ol)
		mnt_optlist_remove_flags(ol, MS_SILENT, cxt->map_linux);
	free(org_type);
	return rc;
}

static int is_success_status(struct libmnt_context *cxt)
{
	if (mnt_context_helper_executed(cxt))
		return mnt_context_get_helper_status(cxt) == 0;

	if (mnt_context_syscall_called(cxt))
		return mnt_context_get_status(cxt) == 1;

	return 0;
}

/* try mount(2) for all items in comma separated list of the filesystem @types */
static int do_mount_by_types(struct libmnt_context *cxt, const char *types)
{
	int rc = -EINVAL;
	char *p, *p0;

	assert(cxt);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, ul_debugobj(cxt, "trying to mount by FS list '%s'", types));

	p0 = p = strdup(types);
	if (!p)
		return -ENOMEM;
	do {
		char *autotype = NULL;
		char *end = strchr(p, ',');

		if (end)
			*end = '\0';

		DBG(CXT, ul_debugobj(cxt, "-->trying '%s'", p));

		/* Let's support things like "udf,iso9660,auto" */
		if (strcmp(p, "auto") == 0) {
			rc = mnt_context_guess_srcpath_fstype(cxt, &autotype);
			if (rc) {
				DBG(CXT, ul_debugobj(cxt, "failed to guess FS type [rc=%d]", rc));
				free(p0);
				free(autotype);
				return rc;
			}
			p = autotype;
			DBG(CXT, ul_debugobj(cxt, "   --> '%s'", p));
		}

		if (p)
			rc = do_mount(cxt, p);
		p = end ? end + 1 : NULL;
		free(autotype);
	} while (!is_success_status(cxt) && p);

	free(p0);
	return rc;
}


static int do_mount_by_pattern(struct libmnt_context *cxt, const char *pattern)
{
	int neg = pattern && strncmp(pattern, "no", 2) == 0;
	int rc = -EINVAL;
	char **filesystems, **fp;
	struct libmnt_ns *ns_old;

	assert(cxt);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	/*
	 * Use the pattern as list of the filesystems
	 */
	if (!neg && pattern) {
		DBG(CXT, ul_debugobj(cxt, "use FS pattern as FS list"));
		return do_mount_by_types(cxt, pattern);
	}

	DBG(CXT, ul_debugobj(cxt, "trying to mount by FS pattern '%s'", pattern));

	/*
	 * Apply pattern to /etc/filesystems and /proc/filesystems
	 */
	ns_old = mnt_context_switch_origin_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;
	rc = mnt_get_filesystems(&filesystems, neg ? pattern : NULL);
	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;
	if (rc)
		return rc;

	if (filesystems == NULL)
		return -MNT_ERR_NOFSTYPE;

	for (fp = filesystems; *fp; fp++) {
		DBG(CXT, ul_debugobj(cxt, " ##### trying '%s'", *fp));
		rc = do_mount(cxt, *fp);
		if (is_success_status(cxt))
			break;
		if (mnt_context_get_syscall_errno(cxt) != EINVAL &&
		    mnt_context_get_syscall_errno(cxt) != ENODEV)
			break;
	}
	mnt_free_filesystems(filesystems);
	return rc;
}

static int prepare_target(struct libmnt_context *cxt)
{
	const char *tgt, *prefix;
	int rc = 0;
	struct libmnt_ns *ns_old;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, ul_debugobj(cxt, "--> preparing target path"));

	tgt = mnt_fs_get_target(cxt->fs);
	if (!tgt)
		return 0;

	/* apply prefix */
	prefix = mnt_context_get_target_prefix(cxt);
	if (prefix) {
		const char *p = *tgt == '/' ? tgt + 1 : tgt;

		if (!*p)
			/* target is "/", use "/prefix" */
			rc = mnt_fs_set_target(cxt->fs, prefix);
		else {
			char *path = NULL;

			if (asprintf(&path, "%s/%s", prefix, p) <= 0)
				rc = -ENOMEM;
			else {
				rc = mnt_fs_set_target(cxt->fs, path);
				free(path);
			}
		}
		if (rc)
			return rc;
		tgt = mnt_fs_get_target(cxt->fs);
	}

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	/* canonicalize the path */
	if (rc == 0) {
		struct libmnt_cache *cache = mnt_context_get_cache(cxt);

		if (cache) {
			char *path = mnt_resolve_path(tgt, cache);
			if (path && strcmp(path, tgt) != 0)
				rc = mnt_fs_set_target(cxt->fs, path);
		}
	}

	if (rc == 0)
		rc = mnt_context_call_hooks(cxt, MNT_STAGE_PREP_TARGET);

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	DBG(CXT, ul_debugobj(cxt, "final target '%s' [rc=%d]",
				mnt_fs_get_target(cxt->fs), rc));
	return rc;
}

/**
 * mnt_context_prepare_mount:
 * @cxt: context
 *
 * Prepare context for mounting, unnecessary for mnt_context_mount().
 *
 * Returns: negative number on error, zero on success
 */
int mnt_context_prepare_mount(struct libmnt_context *cxt)
{
	int rc = -EINVAL;
	struct libmnt_ns *ns_old;

	if (!cxt || !cxt->fs || mnt_fs_is_swaparea(cxt->fs))
		return -EINVAL;
	if (!mnt_fs_get_source(cxt->fs) && !mnt_fs_get_target(cxt->fs))
		return -EINVAL;
	if (cxt->flags & MNT_FL_PREPARED)
		return 0;

	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);

	cxt->action = MNT_ACT_MOUNT;

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	DBG(CXT, ul_debugobj(cxt, "mount: preparing"));

	rc = mnt_context_apply_fstab(cxt);
	if (!rc)
		rc = mnt_context_merge_mflags(cxt);
	if (!rc && cxt->fs && cxt->optlist)
		rc = mnt_fs_follow_optlist(cxt->fs, cxt->optlist);
	if (!rc)
		rc = evaluate_permissions(cxt);
	if (!rc)
		rc = fix_optstr(cxt);
	if (!rc)
		rc = mnt_context_prepare_srcpath(cxt);
	if (!rc)
		rc = mnt_context_guess_fstype(cxt);
	if (!rc)
		rc = prepare_target(cxt);
	if (!rc)
		rc = mnt_context_prepare_helper(cxt, "mount", NULL);

	if (!rc && mnt_context_is_onlyonce(cxt)) {
		int mounted = 0;
		rc = mnt_context_is_fs_mounted(cxt, cxt->fs, &mounted);
		if (rc == 0 && mounted == 1) {
			rc = -MNT_ERR_ONLYONCE;
			goto end;
		}
	}

	if (!rc)
		rc = mnt_context_call_hooks(cxt, MNT_STAGE_PREP);

	if (rc) {
		DBG(CXT, ul_debugobj(cxt, "mount: preparing failed"));
		goto end;
	}

	cxt->flags |= MNT_FL_PREPARED;

end:
	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	return rc;
}

/**
 * mnt_context_do_mount
 * @cxt: context
 *
 * Call mount(2) or mount.type helper. Unnecessary for mnt_context_mount().
 *
 * Note that this function could be called only once. If you want to mount
 * another source or target, then you have to call mnt_reset_context().
 *
 * If you want to call mount(2) for the same source and target with different
 * mount flags or fstype, then call mnt_context_reset_status() and then try
 * again mnt_context_do_mount().
 *
 * WARNING: non-zero return code does not mean that mount(2) syscall or
 *          mount.type helper wasn't successfully called.
 *
 * Check mnt_context_get_status() after error! See mnt_context_mount() for more
 * details about errors and warnings.
 *
 * Returns: 0 on success;
 *         >0 in case of mount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
int mnt_context_do_mount(struct libmnt_context *cxt)
{
	const char *type;
	int res = 0, rc = 0;
	struct libmnt_ns *ns_old;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));
	assert((cxt->flags & MNT_FL_PREPARED));
	assert((cxt->action == MNT_ACT_MOUNT));

	DBG(CXT, ul_debugobj(cxt, "mount: do mount"));

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	/* before mount stage */
	rc = mnt_context_call_hooks(cxt, MNT_STAGE_MOUNT_PRE);
	if (rc)
		return rc;

	/* mount stage */
	type = mnt_fs_get_fstype(cxt->fs);
	if (type) {
		if (strchr(type, ','))
			/* this only happens if fstab contains a list of filesystems */
			res = do_mount_by_types(cxt, type);
		else
			res = do_mount(cxt, NULL);
	} else
		res = do_mount_by_pattern(cxt, cxt->fstype_pattern);

	/* after mount stage */
	if (res == 0) {
		rc = mnt_context_call_hooks(cxt, MNT_STAGE_MOUNT_POST);
		if (rc)
			return rc;
	}

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;

	DBG(CXT, ul_debugobj(cxt, "mnt_context_do_mount() done [rc=%d]", res));
	return res;
}

/*
 * Returns mountinfo FS entry of context source patch if the source is already
 * mounted. This function is used for "already mounted" message or to get FS of
 * re-used loop device.
 */
static struct libmnt_fs *get_already_mounted_source(struct libmnt_context *cxt)
{
	const char *src;
	struct libmnt_table *tb;

	assert(cxt);

	src = mnt_fs_get_srcpath(cxt->fs);

	if (src && mnt_context_get_mountinfo(cxt, &tb) == 0) {
		struct libmnt_iter itr;
		struct libmnt_fs *fs;

		mnt_reset_iter(&itr, MNT_ITER_FORWARD);
		while (mnt_table_next_fs(tb, &itr, &fs) == 0) {
			const char *s = mnt_fs_get_srcpath(fs),
				   *t = mnt_fs_get_target(fs);

			if (t && s && mnt_fs_streq_srcpath(fs, src))
				return fs;
		}
	}
	return NULL;
}

/*
 * Checks if source filesystem superblock is already ro-mounted. Note that we
 * care about FS superblock as VFS node is irrelevant here.
 */
static int is_source_already_rdonly(struct libmnt_context *cxt)
{
	struct libmnt_fs *fs = get_already_mounted_source(cxt);
	const char *opts = fs ? mnt_fs_get_fs_options(fs) : NULL;

	return opts && mnt_optstr_get_option(opts, "ro", NULL, NULL) == 0;
}

/**
 * mnt_context_finalize_mount:
 * @cxt: context
 *
 * Mtab update, etc. Unnecessary for mnt_context_mount(), but should be called
 * after mnt_context_do_mount(). See also mnt_context_set_syscall_status().
 *
 * Returns: negative number on error, 0 on success.
 */
int mnt_context_finalize_mount(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));
	assert((cxt->flags & MNT_FL_PREPARED));

	rc = mnt_context_prepare_update(cxt);
	if (!rc)
		rc = mnt_context_update_tabs(cxt);
	return rc;
}

/**
 * mnt_context_mount:
 * @cxt: mount context
 *
 * High-level, mounts the filesystem by mount(2) or fork()+exec(/sbin/mount.type).
 *
 * This is similar to:
 *
 *	mnt_context_prepare_mount(cxt);
 *	mnt_context_do_mount(cxt);
 *	mnt_context_finalize_mount(cxt);
 *
 * See also mnt_context_disable_helpers().
 *
 * Note that this function should be called only once. If you want to mount with
 * different settings, then you have to call mnt_reset_context(). It's NOT enough
 * to call mnt_context_reset_status(). If you want to call this function more than
 * once, the whole context has to be reset.
 *
 * WARNING: non-zero return code does not mean that mount(2) syscall or
 *          mount.type helper wasn't successfully called.
 *
 * Always use mnt_context_get_status():
 *
 * <informalexample>
 *   <programlisting>
 *       rc = mnt_context_mount(cxt);
 *
 *       if (mnt_context_helper_executed(cxt))
 *               return mnt_context_get_helper_status(cxt);
 *       if (rc == 0 && mnt_context_get_status(cxt) == 1)
 *               return MNT_EX_SUCCESS;
 *       return MNT_EX_FAIL;
 *   </programlisting>
 * </informalexample>
 *
 * or mnt_context_get_excode() to generate mount(8) compatible error
 * or warning message:
 *
 * <informalexample>
 *   <programlisting>
 *       rc = mnt_context_mount(cxt);
 *       rc = mnt_context_get_excode(cxt, rc, buf, sizeof(buf));
 *       if (buf)
 *               warnx(_("%s: %s"), mnt_context_get_target(cxt), buf);
 *	 return rc;   // MNT_EX_*
 *   </programlisting>
 * </informalexample>
 *
 * Returns: 0 on success;
 *         >0 in case of mount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
int mnt_context_mount(struct libmnt_context *cxt)
{
	int rc;
	struct libmnt_ns *ns_old;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

again:
	rc = mnt_context_prepare_mount(cxt);
	if (!rc)
		rc = mnt_context_prepare_update(cxt);
	if (!rc)
		rc = mnt_context_do_mount(cxt);
	if (!rc)
		rc = mnt_context_update_tabs(cxt);

	/*
	 * Read-only device or already read-only mounted FS.
	 * Try mount the filesystem read-only.
	 */
	if ((rc == -EROFS && !mnt_context_syscall_called(cxt))	/* before syscall; rdonly loopdev */
	     || mnt_context_get_syscall_errno(cxt) == EROFS	/* syscall failed with EROFS */
	     || mnt_context_get_syscall_errno(cxt) == EACCES	/* syscall failed with EACCES */
	     || (mnt_context_get_syscall_errno(cxt) == EBUSY	/* already ro-mounted FS */
		 && is_source_already_rdonly(cxt)))
	{
		unsigned long mflags = 0;

		mnt_context_get_mflags(cxt, &mflags);

		if (!(mflags & MS_RDONLY)			/* not yet RDONLY */
		    && !(mflags & MS_REMOUNT)			/* not remount */
		    && !(mflags & MS_BIND)			/* not bin mount */
		    && !mnt_context_is_rwonly_mount(cxt)) {	/* no explicit read-write */

			assert(!(cxt->flags & MNT_FL_FORCED_RDONLY));
			DBG(CXT, ul_debugobj(cxt, "write-protected source, trying RDONLY."));

			mnt_context_reset_status(cxt);
			mnt_context_set_mflags(cxt, mflags | MS_RDONLY);
			cxt->flags |= MNT_FL_FORCED_RDONLY;
			goto again;
		}
	}

	if (rc == 0)
		rc = mnt_context_call_hooks(cxt, MNT_STAGE_POST);

	mnt_context_deinit_hooksets(cxt);

	if (!mnt_context_switch_ns(cxt, ns_old))
		rc = -MNT_ERR_NAMESPACE;

	DBG(CXT, ul_debugobj(cxt, "mnt_context_mount() done [rc=%d]", rc));
	return rc;
}

/**
 * mnt_context_next_mount:
 * @cxt: context
 * @itr: iterator
 * @fs: returns the current filesystem
 * @mntrc: returns the return code from mnt_context_mount()
 * @ignored: returns 1 for non-matching and 2 for already mounted filesystems
 *
 * This function tries to mount the next filesystem from fstab (as returned by
 * mnt_context_get_fstab()). See also mnt_context_set_fstab().
 *
 * You can filter out filesystems by:
 *	mnt_context_set_options_pattern() to simulate mount -a -O pattern
 *	mnt_context_set_fstype_pattern()  to simulate mount -a -t pattern
 *
 * If the filesystem is already mounted or does not match defined criteria,
 * then the mnt_context_next_mount() function returns zero, but the @ignored is
 * non-zero. Note that the root filesystem and filesystems with "noauto" option
 * are always ignored.
 *
 * If mount(2) syscall or mount.type helper failed, then the
 * mnt_context_next_mount() function returns zero, but the @mntrc is non-zero.
 * Use also mnt_context_get_status() to check if the filesystem was
 * successfully mounted.
 *
 * See mnt_context_mount() for more details about errors and warnings.
 *
 * Returns: 0 on success,
 *         <0 in case of error (!= mount(2) errors)
 *          1 at the end of the list.
 */
int mnt_context_next_mount(struct libmnt_context *cxt,
			   struct libmnt_iter *itr,
			   struct libmnt_fs **fs,
			   int *mntrc,
			   int *ignored)
{
	struct libmnt_table *fstab, *mountinfo;
	const char *o, *tgt;
	int rc, mounted = 0;

	if (ignored)
		*ignored = 0;
	if (mntrc)
		*mntrc = 0;

	if (!cxt || !fs || !itr)
		return -EINVAL;

	/* ingore --onlyonce, it's default behavior for --all */
	mnt_context_enable_onlyonce(cxt, 0);

	rc = mnt_context_get_fstab(cxt, &fstab);
	if (rc)
		return rc;

	rc = mnt_table_next_fs(fstab, itr, fs);
	if (rc != 0)
		return rc;	/* more filesystems (or error) */

	o = mnt_fs_get_user_options(*fs);
	tgt = mnt_fs_get_target(*fs);

	DBG(CXT, ul_debugobj(cxt, "next-mount: trying %s", tgt));

	/*  ignore swap */
	if (mnt_fs_is_swaparea(*fs) ||

	/* ignore root filesystem */
	   (tgt && (strcmp(tgt, "/") == 0 || strcmp(tgt, "root") == 0)) ||

	/* ignore noauto filesystems */
	   (o && mnt_optstr_get_option(o, "noauto", NULL, NULL) == 0) ||

	/* ignore filesystems which don't match options patterns */
	   (cxt->fstype_pattern && !mnt_fs_match_fstype(*fs,
					cxt->fstype_pattern)) ||

	/* ignore filesystems which don't match type patterns */
	   (cxt->optstr_pattern && !mnt_fs_match_options(*fs,
					cxt->optstr_pattern))) {
		if (ignored)
			*ignored = 1;
		DBG(CXT, ul_debugobj(cxt, "next-mount: not-match "
				"[fstype: %s, t-pattern: %s, options: %s, O-pattern: %s]",
				mnt_fs_get_fstype(*fs),
				cxt->fstype_pattern,
				mnt_fs_get_options(*fs),
				cxt->optstr_pattern));
		return 0;
	}

	/* ignore already mounted filesystems */
	rc = mnt_context_is_fs_mounted(cxt, *fs, &mounted);
	if (rc) {
		if (mnt_table_is_empty(cxt->mountinfo)) {
			DBG(CXT, ul_debugobj(cxt, "next-mount: no mount table [rc=%d], ignore", rc));
			rc = 0;
			if (ignored)
				*ignored = 1;
		}
		return rc;
	}
	if (mounted) {
		if (ignored)
			*ignored = 2;
		return 0;
	}

	/* Save mount options, etc. -- this is effective for the first
	 * mnt_context_next_mount() call only. Make sure that cxt has not set
	 * source, target or fstype.
	 */
	if (!mnt_context_has_template(cxt)) {
		mnt_context_set_source(cxt, NULL);
		mnt_context_set_target(cxt, NULL);
		mnt_context_set_fstype(cxt, NULL);
		mnt_context_save_template(cxt);
	}

	/* reset context, but protect mountinfo */
	mountinfo = cxt->mountinfo;
	cxt->mountinfo = NULL;
	mnt_reset_context(cxt);
	cxt->mountinfo = mountinfo;

	if (mnt_context_is_fork(cxt)) {
		rc = mnt_fork_context(cxt);
		if (rc)
			return rc;		/* fork error */

		if (mnt_context_is_parent(cxt)) {
			return 0;		/* parent */
		}
	}

	/*
	 * child or non-forked
	 */

	/* copy stuff from fstab to context */
	rc = mnt_context_apply_fs(cxt, *fs);
	if (!rc) {
		/*
		 * "-t <pattern>" is used to filter out fstab entries, but for ordinary
		 * mount operation -t means "-t <type>". We have to zeroize the pattern
		 * to avoid misinterpretation.
		 */
		char *pattern = cxt->fstype_pattern;
		cxt->fstype_pattern = NULL;

		rc = mnt_context_mount(cxt);

		cxt->fstype_pattern = pattern;

		if (mntrc)
			*mntrc = rc;
	}

	if (mnt_context_is_child(cxt)) {
		DBG(CXT, ul_debugobj(cxt, "next-mount: child exit [rc=%d]", rc));
		DBG_FLUSH;
		_exit(rc);
	}
	return 0;
}


/**
 * mnt_context_next_remount:
 * @cxt: context
 * @itr: iterator
 * @fs: returns the current filesystem
 * @mntrc: returns the return code from mnt_context_mount()
 * @ignored: returns 1 for non-matching
 *
 * This function tries to remount the next mounted filesystem (as returned by
 * mnt_context_get_mtab()).
 *
 * You can filter out filesystems by:
 *	mnt_context_set_options_pattern() to simulate mount -a -O pattern
 *	mnt_context_set_fstype_pattern()  to simulate mount -a -t pattern
 *
 * If the filesystem does not match defined criteria, then the
 * mnt_context_next_remount() function returns zero, but the @ignored is
 * non-zero.
 *
 * IMPORTANT -- the mount operation is performed in the current context.
 * The context is reset before the next mount (see mnt_reset_context()).
 * The context setting related to the filesystem (e.g. mount options,
 * etc.) are protected.

 * If mount(2) syscall or mount.type helper failed, then the
 * mnt_context_next_mount() function returns zero, but the @mntrc is non-zero.
 * Use also mnt_context_get_status() to check if the filesystem was
 * successfully mounted.
 *
 * See mnt_context_mount() for more details about errors and warnings.
 *
 * Returns: 0 on success,
 *         <0 in case of error (!= mount(2) errors)
 *          1 at the end of the list.
 *
 * Since: 2.34
 */
int mnt_context_next_remount(struct libmnt_context *cxt,
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
	if (rc)
		return rc;

	rc = mnt_table_next_fs(mountinfo, itr, fs);
	if (rc != 0)
		return rc;	/* more filesystems (or error) */

	tgt = mnt_fs_get_target(*fs);

	DBG(CXT, ul_debugobj(cxt, "next-remount: trying %s", tgt));

	/* ignore filesystems which don't match options patterns */
	if ((cxt->fstype_pattern && !mnt_fs_match_fstype(*fs,
					cxt->fstype_pattern)) ||

	/* ignore filesystems which don't match type patterns */
	   (cxt->optstr_pattern && !mnt_fs_match_options(*fs,
					cxt->optstr_pattern))) {
		if (ignored)
			*ignored = 1;
		DBG(CXT, ul_debugobj(cxt, "next-remount: not-match "
				"[fstype: %s, t-pattern: %s, options: %s, O-pattern: %s]",
				mnt_fs_get_fstype(*fs),
				cxt->fstype_pattern,
				mnt_fs_get_options(*fs),
				cxt->optstr_pattern));
		return 0;
	}

	/* Save mount options, etc. -- this is effective for the first
	 * mnt_context_next_remount() call only. Make sure that cxt has not set
	 * source, target or fstype.
	 */
	if (!mnt_context_has_template(cxt)) {
		mnt_context_set_source(cxt, NULL);
		mnt_context_set_target(cxt, NULL);
		mnt_context_set_fstype(cxt, NULL);
		mnt_context_save_template(cxt);
	}

	/* restore original, but protect mountinfo */
	cxt->mountinfo = NULL;
	mnt_reset_context(cxt);
	cxt->mountinfo = mountinfo;

	rc = mnt_context_set_target(cxt, tgt);
	if (!rc) {
		/*
		 * "-t <pattern>" is used to filter out fstab entries, but for ordinary
		 * mount operation -t means "-t <type>". We have to zeroize the pattern
		 * to avoid misinterpretation.
		 */
		char *pattern = cxt->fstype_pattern;
		cxt->fstype_pattern = NULL;

		rc = mnt_context_mount(cxt);

		cxt->fstype_pattern = pattern;

		if (mntrc)
			*mntrc = rc;
		rc = 0;
	}

	return rc;
}

/*
 * Returns 1 if @dir parent is shared
 */
static int is_shared_tree(struct libmnt_context *cxt, const char *dir)
{
	struct libmnt_table *tb = NULL;
	struct libmnt_fs *fs;
	unsigned long mflags = 0;
	char *mnt = NULL, *p;
	int rc = 0;
	struct libmnt_ns *ns_old;

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	if (!dir)
		return 0;
	if (mnt_context_get_mountinfo(cxt, &tb) || !tb)
		goto done;

	mnt = strdup(dir);
	if (!mnt)
		goto done;
	p = strrchr(mnt, '/');
	if (!p)
		goto done;
	if (p > mnt)
		*p = '\0';
	fs = mnt_table_find_mountpoint(tb, mnt, MNT_ITER_BACKWARD);

	rc = fs && mnt_fs_is_kernel(fs)
		&& mnt_fs_get_propagation(fs, &mflags) == 0
		&& (mflags & MS_SHARED);
done:
	free(mnt);
	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;
	return rc;
}

int mnt_context_get_mount_excode(
			struct libmnt_context *cxt,
			int rc,
			char *buf,
			size_t bufsz)
{
	int syserr;
	struct stat st;
	unsigned long uflags = 0, mflags = 0;

	int restricted = mnt_context_is_restricted(cxt);
	const char *tgt = mnt_context_get_target(cxt);
	const char *src = mnt_context_get_source(cxt);

	if (mnt_context_helper_executed(cxt)) {
		/*
		 * /sbin/mount.<type> called, return status
		 */
		if (rc == -MNT_ERR_APPLYFLAGS && buf)
			snprintf(buf, bufsz, _("WARNING: failed to apply propagation flags"));

		return mnt_context_get_helper_status(cxt);
	}

	if (rc == 0 && mnt_context_get_status(cxt) == 1) {
		/*
		 * Libmount success && syscall success.
		 */
		if (buf && mnt_context_forced_rdonly(cxt))
			snprintf(buf, bufsz, _("WARNING: source write-protected, mounted read-only"));
		return MNT_EX_SUCCESS;
	}

	mnt_context_get_mflags(cxt, &mflags);		/* mount(2) flags */
	mnt_context_get_user_mflags(cxt, &uflags);	/* userspace flags */

	if (!mnt_context_syscall_called(cxt)) {
		/*
		 * libmount errors (extra library checks)
		 */
		switch (rc) {
		case -EPERM:
			if (buf)
				snprintf(buf, bufsz, _("operation permitted for root only"));
			return MNT_EX_USAGE;
		case -EBUSY:
			if (buf)
				snprintf(buf, bufsz, _("%s is already mounted"), src);
			return MNT_EX_USAGE;
		case -MNT_ERR_NOFSTAB:
			if (!buf)
				return MNT_EX_USAGE;
			if (mnt_context_is_swapmatch(cxt))
				snprintf(buf, bufsz, _("can't find in %s"),
						mnt_get_fstab_path());
			else if (tgt)
				snprintf(buf, bufsz, _("can't find mount point in %s"),
						mnt_get_fstab_path());
			else if (src)
				snprintf(buf, bufsz, _("can't find mount source %s in %s"),
						src, mnt_get_fstab_path());
			return MNT_EX_USAGE;
		case -MNT_ERR_AMBIFS:
			if (buf)
				snprintf(buf, bufsz, _("more filesystems detected on %s; use -t <type> or wipefs(8)"), src);
			return MNT_EX_USAGE;
		case -MNT_ERR_NOFSTYPE:
			if (buf)
				snprintf(buf, bufsz, restricted ?
						_("failed to determine filesystem type") :
						_("no valid filesystem type specified"));
			return MNT_EX_USAGE;
		case -MNT_ERR_NOSOURCE:
			if (uflags & MNT_MS_NOFAIL)
				return MNT_EX_SUCCESS;
			if (buf) {
				if (src)
					snprintf(buf, bufsz, _("can't find %s"), src);
				else
					snprintf(buf, bufsz, _("no mount source specified"));
			}
			return MNT_EX_USAGE;
		case -MNT_ERR_MOUNTOPT:
			if (buf) {
				const char *opts = mnt_context_get_options(cxt);

				if (!opts)
					opts = "";
				if (opts)
					snprintf(buf, bufsz, errno ?
						_("failed to parse mount options '%s': %m") :
						_("failed to parse mount options '%s'"), opts);
				else
					snprintf(buf, bufsz, errno ?
						_("failed to parse mount options: %m") :
						_("failed to parse mount options"));
			}
			return MNT_EX_USAGE;
		case -MNT_ERR_LOOPDEV:
			if (buf)
				snprintf(buf, bufsz, _("failed to setup loop device for %s"), src);
			return MNT_EX_FAIL;
		case -MNT_ERR_LOOPOVERLAP:
			if (buf)
				snprintf(buf, bufsz, _("overlapping loop device exists for %s"), src);
			return MNT_EX_FAIL;
		case -MNT_ERR_LOCK:
			if (buf)
				snprintf(buf, bufsz, _("locking failed"));
			return MNT_EX_FILEIO;
		case -MNT_ERR_NAMESPACE:
			if (buf)
				snprintf(buf, bufsz, _("failed to switch namespace"));
			return MNT_EX_SYSERR;
		case -MNT_ERR_ONLYONCE:
			if (buf)
				snprintf(buf, bufsz, _("filesystem already mounted"));
			return MNT_EX_FAIL;
		default:
			return mnt_context_get_generic_excode(rc, buf, bufsz, _("mount failed: %m"));
		}

	} else if (mnt_context_get_syscall_errno(cxt) == 0) {
		/*
		 * mount(2) syscall success, but something else failed
		 * (probably error in utab processing).
		 */
		if (rc == -MNT_ERR_APPLYFLAGS) {
			if (buf)
				snprintf(buf, bufsz, _("filesystem was mounted, but failed to apply flags"));
			return MNT_EX_USAGE;
		}

		if (rc == -MNT_ERR_LOCK) {
			if (buf)
				snprintf(buf, bufsz, _("filesystem was mounted, but failed to update userspace mount table"));
			return MNT_EX_FILEIO;
		}

		if (rc == -MNT_ERR_NAMESPACE) {
			if (buf)
				snprintf(buf, bufsz, _("filesystem was mounted, but failed to switch namespace back"));
			return MNT_EX_SYSERR;
		}

		if (rc == -MNT_ERR_CHOWN) {
			if (buf)
				snprintf(buf, bufsz, _("filesystem was mounted, but failed to change ownership: %m"));
			return MNT_EX_SYSERR;
		}

		if (rc == -MNT_ERR_CHMOD) {
			if (buf)
				snprintf(buf, bufsz, _("filesystem was mounted, but failed to change mode: %m"));
			return MNT_EX_SYSERR;
		}

		if (rc == -MNT_ERR_IDMAP) {
			if (buf)
				snprintf(buf, bufsz, _("filesystem was mounted, but failed to attach idmapping"));
			return MNT_EX_SYSERR;
		}

		if (rc < 0)
			return mnt_context_get_generic_excode(rc, buf, bufsz,
				_("filesystem was mounted, but any subsequent operation failed: %m"));

		return MNT_EX_SOFTWARE;	/* internal error */

	}

	/*
	 * mount(2) and other mount related syscalls errors
	 */
	syserr = mnt_context_get_syscall_errno(cxt);

	if (buf && cxt->syscall_errmsg) {
		snprintf(buf, bufsz, _("%s system call failed: %s"),
					cxt->syscall_name ? : "mount",
					cxt->syscall_errmsg);
		return MNT_EX_FAIL;
	}

	switch(syserr) {
	case EPERM:
		if (!buf)
			break;
		if (geteuid() == 0) {

			if (mnt_safe_stat(tgt, &st) == 0
			    && ((mflags & MS_BIND && S_ISREG(st.st_mode))
				|| S_ISDIR(st.st_mode)))
				snprintf(buf, bufsz, _("permission denied"));
			else
				snprintf(buf, bufsz, _("mount point is not a directory"));
		} else
			snprintf(buf, bufsz, _("must be superuser to use mount"));
		break;

	case EBUSY:
		if (!buf)
			break;
		if (mflags & MS_REMOUNT) {
			snprintf(buf, bufsz, _("mount point is busy"));
			break;
		}
		if (src) {
			struct libmnt_fs *fs = get_already_mounted_source(cxt);

			if (fs && mnt_fs_get_target(fs))
				snprintf(buf, bufsz, _("%s already mounted on %s"),
						src, mnt_fs_get_target(fs));
		}
		if (!*buf)
			snprintf(buf, bufsz, _("%s already mounted or mount point busy"), src);
		break;
	case ENOENT:
		if (tgt && mnt_safe_lstat(tgt, &st)) {
			if (buf)
				snprintf(buf, bufsz, _("mount point does not exist"));
		} else if (tgt && mnt_safe_stat(tgt, &st)) {
			if (buf)
				snprintf(buf, bufsz, _("mount point is a symbolic link to nowhere"));
		} else if (src && !mnt_is_path(src)) {
			if (uflags & MNT_MS_NOFAIL)
				return MNT_EX_SUCCESS;
			if (buf)
				snprintf(buf, bufsz, _("special device %s does not exist"), src);
		} else
			goto generic_error;
		break;

	case ENOTDIR:
		if (mnt_safe_stat(tgt, &st) || ! S_ISDIR(st.st_mode)) {
			if (buf)
				snprintf(buf, bufsz, _("mount point is not a directory"));
		} else if (src && !mnt_is_path(src)) {
			if (uflags & MNT_MS_NOFAIL)
				return MNT_EX_SUCCESS;
			if (buf)
				snprintf(buf, bufsz, _("special device %s does not exist "
					 "(a path prefix is not a directory)"), src);
		} else
			goto generic_error;
		break;

	case EINVAL:
		if (!buf)
			break;
		if (mflags & MS_REMOUNT)
			snprintf(buf, bufsz, _("mount point not mounted or bad option"));
		else if (rc == -MNT_ERR_APPLYFLAGS)
			snprintf(buf, bufsz, _("not mount point or bad option"));
		else if ((mflags & MS_MOVE) && is_shared_tree(cxt, src))
			snprintf(buf, bufsz,
				_("bad option; moving a mount "
				  "residing under a shared mount is unsupported"));
		else if (mnt_fs_is_netfs(mnt_context_get_fs(cxt)))
			snprintf(buf, bufsz,
				_("bad option; for several filesystems (e.g. nfs, cifs) "
				  "you might need a /sbin/mount.<type> helper program"));
		else
			snprintf(buf, bufsz,
				_("wrong fs type, bad option, bad superblock on %s, "
				  "missing codepage or helper program, or other error"),
				src);
		break;

	case EMFILE:
		if (buf)
			snprintf(buf, bufsz, _("mount table full"));
		break;

	case EIO:
		if (buf)
			snprintf(buf, bufsz, _("can't read superblock on %s"), src);
		break;

	case ENODEV:
		if (!buf)
			break;
		if (mnt_context_get_fstype(cxt))
			snprintf(buf, bufsz, _("unknown filesystem type '%s'"),
					mnt_context_get_fstype(cxt));
		else
			snprintf(buf, bufsz, _("unknown filesystem type"));
		break;

	case ENOTBLK:
		if (uflags & MNT_MS_NOFAIL)
			return MNT_EX_SUCCESS;
		if (!buf)
			break;
		if (src && mnt_safe_stat(src, &st))
			snprintf(buf, bufsz, _("%s is not a block device, and stat(2) fails?"), src);
		else if (src && S_ISBLK(st.st_mode))
			snprintf(buf, bufsz,
				_("the kernel does not recognize %s as a block device; "
				  "maybe \"modprobe driver\" is necessary"), src);
		else if (src && S_ISREG(st.st_mode))
			snprintf(buf, bufsz, _("%s is not a block device; try \"-o loop\""), src);
		else
			snprintf(buf, bufsz, _("%s is not a block device"), src);
		break;

	case ENXIO:
		if (uflags & MNT_MS_NOFAIL)
			return MNT_EX_SUCCESS;
		if (buf)
			snprintf(buf, bufsz, _("%s is not a valid block device"), src);
		break;

	case EACCES:
	case EROFS:
		if (!buf)
			break;
		if (mflags & MS_RDONLY)
			snprintf(buf, bufsz, _("cannot mount %s read-only"), src);
		else if (mnt_context_is_rwonly_mount(cxt))
			snprintf(buf, bufsz, _("%s is write-protected but explicit read-write mode requested"), src);
		else if (mflags & MS_REMOUNT)
			snprintf(buf, bufsz, _("cannot remount %s read-write, is write-protected"), src);
		else if (mflags & MS_BIND)
			snprintf(buf, bufsz, _("bind %s failed"), src);
		else
			goto generic_error;
		break;

	case ENOMEDIUM:
		if (uflags & MNT_MS_NOFAIL)
			return MNT_EX_SUCCESS;
		if (buf)
			snprintf(buf, bufsz, _("no medium found on %s"), src);
		break;

	case EBADMSG:
		/* Bad CRC for classic filesystems (e.g. extN or XFS) */
		if (buf && src && mnt_safe_stat(src, &st) == 0
		    && (S_ISBLK(st.st_mode) || S_ISREG(st.st_mode))) {
			snprintf(buf, bufsz, _("cannot mount; probably corrupted filesystem on %s"), src);
			break;
		}
		/* fallthrough */

	default:
	generic_error:
		if (buf) {
			errno = syserr;
			snprintf(buf, bufsz, _("%s system call failed: %m"),
					cxt->syscall_name ? : "mount");
		}
		break;
	}

	return MNT_EX_FAIL;
}

#ifdef TEST_PROGRAM

static int test_perms(struct libmnt_test *ts __attribute__((unused)),
		      int argc, char *argv[])
{
	struct libmnt_context *cxt;
	struct libmnt_optlist *ls;
	int rc;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	cxt->restricted = 1;		/* emulate suid mount(8) */
	mnt_context_get_fs(cxt);	/* due to assert() in evaluate_permissions() */

	if (argc < 2) {
		 warn("missing fstab options");
		 return -EPERM;
	}
	if (argc == 3 && strcmp(argv[2], "--root") == 0)
		cxt->restricted = 0;

	ls = mnt_context_get_optlist(cxt);
	if (!ls)
		return -ENOMEM;
	rc = mnt_optlist_set_optstr(ls, argv[1], NULL);
	if (rc) {
		warn("cannot apply fstab options");
		return rc;
	}
	cxt->flags |= MNT_FL_TAB_APPLIED;	/* emulate mnt_context_apply_fstab() */

	mnt_context_merge_mflags(cxt);

	rc = evaluate_permissions(cxt);
	if (rc) {
		warn("evaluate permission failed [rc=%d]", rc);
		return rc;
	}
	printf("user can mount\n");

	mnt_free_context(cxt);
	return 0;
}

static int test_fixopts(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	struct libmnt_context *cxt;
	struct libmnt_optlist *ls;
	unsigned long flags = 0;
	const char *p;
	int rc;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	cxt->restricted = 1;		/* emulate suid mount(8) */
	mnt_context_get_fs(cxt);	/* to fill fs->*_optstr */

	if (argc < 2) {
		 warn("missing fstab options");
		 return -EPERM;
	}
	if (argc == 3 && strcmp(argv[2], "--root") == 0)
		cxt->restricted = 0;

	ls = mnt_context_get_optlist(cxt);
	if (!ls)
		return -ENOMEM;
	rc = mnt_optlist_set_optstr(ls, argv[1], NULL);
	if (rc) {
		warn("cannot apply fstab options");
		return rc;
	}
	cxt->flags |= MNT_FL_TAB_APPLIED;	/* emulate mnt_context_apply_fstab() */

	mnt_context_merge_mflags(cxt);

	rc = evaluate_permissions(cxt);
	if (rc) {
		warn("evaluate permission failed [rc=%d]", rc);
		return rc;
	}
	rc = fix_optstr(cxt);
	if (rc) {
		warn("fix options failed [rc=%d]", rc);
		return rc;
	}

	mnt_optlist_get_optstr(ls, &p, NULL, 0);

	mnt_optlist_get_flags(ls, &flags, cxt->map_linux, 0);
	printf("options (dfl): '%s' [mount flags: %08lx]\n", p, flags);

	mnt_optlist_get_optstr(ls, &p, NULL, MNT_OL_FLTR_ALL);
	printf("options (ex.): '%s'\n", p);

	mnt_free_context(cxt);
	return 0;
}

int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
	{ "--perms",		test_perms,    "<fstab-options> [--root]" },
	{ "--fix-options",	test_fixopts,  "<fstab-options> [--root]" },

	{ NULL }};

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
