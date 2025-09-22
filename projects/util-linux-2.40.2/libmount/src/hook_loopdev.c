/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2011-2022 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * Please, see the comment in libmount/src/hooks.c to understand how hooks work.
 */
#include <blkid.h>
#include <stdbool.h>

#include "mountP.h"
#include "loopdev.h"
#include "strutils.h"
#include "linux_version.h"

struct hook_data {
	int loopdev_fd;
};

/* de-initiallize this module */
static int hookset_deinit(struct libmnt_context *cxt, const struct libmnt_hookset *hs)
{
	void *data;

	DBG(HOOK, ul_debugobj(hs, "deinit '%s'", hs->name));

	/* remove all our hooks */
	while (mnt_context_remove_hook(cxt, hs, 0, &data) == 0) {
		free(data);
		data = NULL;
	}

	return 0;
}

static inline struct hook_data *new_hook_data(void)
{
	struct hook_data *hd = calloc(1, sizeof(*hd));

	if (!hd)
		return NULL;

	hd->loopdev_fd =  -1;
	return hd;
}

/* Check if there already exists a mounted loop device on the mountpoint node
 * with the same parameters.
 */
static int __attribute__((nonnull))
is_mounted_same_loopfile(struct libmnt_context *cxt,
				    const char *target,
				    const char *backing_file,
				    uint64_t offset)
{
	struct libmnt_table *tb = NULL;
	struct libmnt_iter itr;
	struct libmnt_fs *fs;
	struct libmnt_cache *cache;
	const char *bf;
	int rc = 0;
	struct libmnt_ns *ns_old;
	unsigned long flags = 0;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (mnt_context_get_mountinfo(cxt, &tb))
		return 0;

	ns_old = mnt_context_switch_target_ns(cxt);
	if (!ns_old)
		return -MNT_ERR_NAMESPACE;

	DBG(LOOP, ul_debugobj(cxt, "checking if %s mounted on %s",
				backing_file, target));

	rc = mnt_context_get_user_mflags(cxt, &flags);
	if (rc)
		return 0;

	cache = mnt_context_get_cache(cxt);
	mnt_reset_iter(&itr, MNT_ITER_BACKWARD);

	bf = cache ? mnt_resolve_path(backing_file, cache) : backing_file;

	/* Search for a mountpoint node in mountinfo, proceed if any of these have the
	 * loop option set or the device is a loop device
	 */
	while (rc == 0 && mnt_table_next_fs(tb, &itr, &fs) == 0) {
		const char *src = mnt_fs_get_source(fs);
		const char *opts = mnt_fs_get_user_options(fs);
		char *val;
		size_t len;

		if (!src || !mnt_fs_match_target(fs, target, cache))
			continue;

		rc = 0;

		if (strncmp(src, "/dev/loop", 9) == 0) {
			rc = loopdev_is_used((char *) src, bf, offset, 0, LOOPDEV_FL_OFFSET);

		} else if (opts && (flags & MNT_MS_LOOP) &&
		    mnt_optstr_get_option(opts, "loop", &val, &len) == 0 && val) {

			val = strndup(val, len);
			rc = loopdev_is_used((char *) val, bf, offset, 0, LOOPDEV_FL_OFFSET);
			free(val);
		}
	}
	if (rc)
		DBG(LOOP, ul_debugobj(cxt, "%s already mounted", backing_file));

	if (!mnt_context_switch_ns(cxt, ns_old))
		return -MNT_ERR_NAMESPACE;
	return rc;
}

static int setup_loopdev(struct libmnt_context *cxt,
			 struct libmnt_optlist *ol, struct hook_data *hd)
{
	const char *backing_file, *loopdev = NULL;
	struct loopdev_cxt lc;
	int rc = 0, lo_flags = 0;
	uint64_t offset = 0, sizelimit = 0;
	bool reuse = FALSE;
	struct libmnt_opt *opt, *loopopt = NULL;

	backing_file = mnt_fs_get_srcpath(cxt->fs);
	if (!backing_file)
		return -EINVAL;

	DBG(LOOP, ul_debugobj(cxt, "trying to setup device for %s", backing_file));

	if (mnt_optlist_is_rdonly(ol)) {
		DBG(LOOP, ul_debugobj(cxt, "enabling READ-ONLY flag"));
		lo_flags |= LO_FLAGS_READ_ONLY;
	}

	/*
	 * loop=
	 */
	if (!rc)
		loopopt = mnt_optlist_get_opt(ol, MNT_MS_LOOP, cxt->map_userspace);

	/*
	 * offset=
	 */
	if (!rc && (opt = mnt_optlist_get_opt(ol, MNT_MS_OFFSET, cxt->map_userspace))
	    && mnt_opt_has_value(opt)) {
		if (strtosize(mnt_opt_get_value(opt), &offset)) {
			DBG(LOOP, ul_debugobj(cxt, "failed to parse offset="));
			rc = -MNT_ERR_MOUNTOPT;
		}
	}

	/*
	 * sizelimit=
	 */
	if (!rc && (opt = mnt_optlist_get_opt(ol, MNT_MS_SIZELIMIT, cxt->map_userspace))
	    && mnt_opt_has_value(opt)) {
		if (strtosize(mnt_opt_get_value(opt), &sizelimit)) {
			DBG(LOOP, ul_debugobj(cxt, "failed to parse sizelimit="));
			rc = -MNT_ERR_MOUNTOPT;
		}
	}

	/*
	 * encryption=
	 */
	if (!rc && mnt_optlist_get_opt(ol, MNT_MS_ENCRYPTION, cxt->map_userspace)) {
		DBG(LOOP, ul_debugobj(cxt, "encryption no longer supported"));
		rc = -MNT_ERR_MOUNTOPT;
	}

	if (!rc && is_mounted_same_loopfile(cxt,
				mnt_context_get_target(cxt),
				backing_file, offset))
		rc = -EBUSY;

	if (rc)
		goto done_no_deinit;

	/* It is possible to mount the same file more times. If we set more
	 * than one loop device referring to the same file, kernel has no
	 * mechanism to detect it. To prevent data corruption, the same loop
	 * device has to be recycled.
	*/
	if (backing_file) {
		rc = loopcxt_init(&lc, 0);
		if (rc)
			goto done_no_deinit;

		rc = loopcxt_find_overlap(&lc, backing_file, offset, sizelimit);
		switch (rc) {
		case 0: /* not found */
			DBG(LOOP, ul_debugobj(cxt, "not found overlapping loopdev"));
			loopcxt_deinit(&lc);
			break;

		case 1:	/* overlap */
			DBG(LOOP, ul_debugobj(cxt, "overlapping %s detected",
						loopcxt_get_device(&lc)));
			rc = -MNT_ERR_LOOPOVERLAP;
			goto done;

		case 2: /* overlap -- full size and offset match (reuse) */
		{
			uint32_t lc_encrypt_type = 0;

			DBG(LOOP, ul_debugobj(cxt, "re-using existing loop device %s",
				loopcxt_get_device(&lc)));

			/* Open loop device to block device autoclear... */
			if (loopcxt_get_fd(&lc) < 0) {
				DBG(LOOP, ul_debugobj(cxt, "failed to get loopdev FD"));
				rc = -errno;
				goto done;
			}

			/*
			 * Now that we certainly have the loop device open,
			 * verify the loop device was not autocleared in the
			 * mean time.
			 */
			if (!loopcxt_get_info(&lc)) {
				DBG(LOOP, ul_debugobj(cxt, "lost race with %s teardown",
						loopcxt_get_device(&lc)));
				loopcxt_deinit(&lc);
				break;
			}

			/* Once a loop is initialized RO, there is no
			 * way to change its parameters. */
			if (loopcxt_is_readonly(&lc)
			    && !(lo_flags & LO_FLAGS_READ_ONLY)) {
				DBG(LOOP, ul_debugobj(cxt, "%s is read-only",
						loopcxt_get_device(&lc)));
				rc = -EROFS;
				goto done;
			}

			/* This is no more supported, but check to be safe. */
			if (loopcxt_get_encrypt_type(&lc, &lc_encrypt_type) == 0
			    && lc_encrypt_type != LO_CRYPT_NONE) {
				DBG(LOOP, ul_debugobj(cxt, "encryption no longer supported for device %s",
					loopcxt_get_device(&lc)));
				rc = -MNT_ERR_LOOPOVERLAP;
				goto done;
			}
			rc = 0;
			/* loop= used with argument. Conflict will occur. */
			if (mnt_opt_has_value(loopopt)) {
				rc = -MNT_ERR_LOOPOVERLAP;
				goto done;
			} else {
				reuse = TRUE;
				goto success;
			}
		}
		default: /* error */
			goto done;
		}
	}

	DBG(LOOP, ul_debugobj(cxt, "not found; create a new loop device"));
	rc = loopcxt_init(&lc, 0);
	if (rc)
		goto done_no_deinit;
	if (mnt_opt_has_value(loopopt)) {
		rc = loopcxt_set_device(&lc, mnt_opt_get_value(loopopt));
		if (rc == 0)
			loopdev = loopcxt_get_device(&lc);
	}
	if (rc)
		goto done;

	/* since 2.6.37 we don't have to store backing filename to mountinfo
	 * because kernel provides the name in /sys.
	 */
	if (get_linux_version() >= KERNEL_VERSION(2, 6, 37)) {
		DBG(LOOP, ul_debugobj(cxt, "enabling AUTOCLEAR flag"));
		lo_flags |= LO_FLAGS_AUTOCLEAR;
	}

	do {
		/* found free device */
		if (!loopdev) {
			rc = loopcxt_find_unused(&lc);
			if (rc)
				goto done;
			DBG(LOOP, ul_debugobj(cxt, "trying to use %s",
						loopcxt_get_device(&lc)));
		}

		/* set device attributes
		 * -- note that loopcxt_find_unused() resets "lc"
		 */
		rc = loopcxt_set_backing_file(&lc, backing_file);

		if (!rc && offset)
			rc = loopcxt_set_offset(&lc, offset);
		if (!rc && sizelimit)
			rc = loopcxt_set_sizelimit(&lc, sizelimit);
		if (!rc)
			loopcxt_set_flags(&lc, lo_flags);
		if (rc) {
			DBG(LOOP, ul_debugobj(cxt, "failed to set loop attributes"));
			goto done;
		}

		/* setup the device */
		rc = loopcxt_setup_device(&lc);
		if (!rc)
			break;		/* success */

		if (loopdev || rc != -EBUSY) {
			DBG(LOOP, ul_debugobj(cxt, "failed to setup device"));
			rc = -MNT_ERR_LOOPDEV;
			goto done;
		}
		DBG(LOOP, ul_debugobj(cxt, "device stolen...trying again"));
	} while (1);

success:
	if (!rc)
		rc = mnt_fs_set_source(cxt->fs, loopcxt_get_device(&lc));

	if (!rc) {
		if (loopopt && (reuse || loopcxt_is_autoclear(&lc))) {
			/*
			 * autoclear flag accepted by the kernel, don't store
			 * the "loop=" option to utab.
			 */
			DBG(LOOP, ul_debugobj(cxt, "removing unnecessary loop= from utab"));
			mnt_optlist_remove_opt(ol, loopopt);
			loopopt = NULL;
		}

		if (!mnt_optlist_is_rdonly(ol) && loopcxt_is_readonly(&lc))
			/*
			 * mount planned read-write, but loopdev is read-only,
			 * let's fix mount options...
			 */
			mnt_optlist_append_flags(ol, MS_RDONLY, cxt->map_linux);

		/*
		 * We have to keep the device open until mount(1), otherwise it
		 * will be auto-cleared by kernel. However we don't want to
		 * keep writeable fd as kernel wants to block all writers to
		 * the device being mounted (in the more hardened
		 * configurations). So grab read-only fd instead.
		 */
		hd->loopdev_fd = open(lc.device, O_RDONLY | O_CLOEXEC);
		if (hd->loopdev_fd < 0) {
			DBG(LOOP,
			    ul_debugobj(cxt, "failed to reopen loopdev FD"));
			rc = -errno;
		}
	}
done:
	loopcxt_deinit(&lc);
done_no_deinit:
	return rc;
}

static int delete_loopdev(struct libmnt_context *cxt, struct hook_data *hd)
{
	const char *src;
	int rc;

	assert(cxt);
	assert(cxt->fs);

	src = mnt_fs_get_srcpath(cxt->fs);
	if (!src)
		return -EINVAL;

	if (hd && hd->loopdev_fd > -1) {
		close(hd->loopdev_fd);
		hd->loopdev_fd = -1;
	}

	rc = loopdev_delete(src);	/* see lib/loopdev.c */

	DBG(LOOP, ul_debugobj(cxt, "deleted [rc=%d]", rc));
	return rc;
}

/* Now used by umount until context_umount.c will use hooks toosee  */
int mnt_context_delete_loopdev(struct libmnt_context *cxt)
{
	return delete_loopdev(cxt, NULL);
}

static int is_loopdev_required(struct libmnt_context *cxt, struct libmnt_optlist *ol)
{
	const char *src, *type;
	unsigned long flags = 0;

	if (cxt->action != MNT_ACT_MOUNT)
		return 0;
	if (!cxt->fs)
		return 0;
	if (mnt_optlist_is_bind(ol)
	    || mnt_optlist_is_move(ol)
	    || mnt_context_propagation_only(cxt))
		return 0;

	src = mnt_fs_get_srcpath(cxt->fs);
	if (!src)
		return 0;		/* backing file not set */

	/* userspace flags */
	if (mnt_context_get_user_mflags(cxt, &flags))
		return 0;

	if (flags & (MNT_MS_LOOP | MNT_MS_OFFSET | MNT_MS_SIZELIMIT)) {
		DBG(LOOP, ul_debugobj(cxt, "loopdev specific options detected"));
		return 1;
	}

	/* Automatically create a loop device from a regular file if a
	 * filesystem is not specified or the filesystem is known for libblkid
	 * (these filesystems work with block devices only). The file size
	 * should be at least 1KiB, otherwise we will create an empty loopdev with
	 * no mountable filesystem...
	 *
	 * Note that there is no restriction (on kernel side) that would prevent a regular
	 * file as a mount(2) source argument. A filesystem that is able to mount
	 * regular files could be implemented.
	 */
	type = mnt_fs_get_fstype(cxt->fs);

	if (mnt_fs_is_regularfs(cxt->fs) &&
	    (!type || strcmp(type, "auto") == 0 || blkid_known_fstype(type))) {
		struct stat st;

		if (stat(src, &st) == 0 && S_ISREG(st.st_mode) &&
		    st.st_size > 1024) {

			DBG(LOOP, ul_debugobj(cxt, "automatically enabling loop= option"));
			mnt_optlist_append_flags(ol, MNT_MS_LOOP, cxt->map_userspace);
			return 1;
		}
	}

	return 0;
}

/* call after mount(2) */
static int hook_cleanup_loopdev(
			struct libmnt_context *cxt,
			const struct libmnt_hookset *hs __attribute__((__unused__)),
			void *data)
{
	struct hook_data *hd = (struct hook_data *) data;

	if (!hd || hd->loopdev_fd < 0)
		return 0;

	if (mnt_context_get_status(cxt) == 0) {
		/*
		 * mount(2) failed, delete loopdev
		 */
		delete_loopdev(cxt, hd);

	} else {
		/*
		 * mount(2) success, close the device
		 */
		DBG(LOOP, ul_debugobj(cxt, "closing FD"));
		close(hd->loopdev_fd);
		hd->loopdev_fd = -1;
	}

	return 0;
}

/* call to prepare mount source */
static int hook_prepare_loopdev(
                        struct libmnt_context *cxt,
                        const struct libmnt_hookset *hs,
                        void *data __attribute__((__unused__)))
{
	struct libmnt_optlist *ol;
	struct hook_data *hd;
	int rc;

	assert(cxt);

	ol = mnt_context_get_optlist(cxt);
	if (!ol)
		return -ENOMEM;
	if (!is_loopdev_required(cxt, ol))
		return 0;
	hd = new_hook_data();
	if (!hd)
		return -ENOMEM;

	rc = setup_loopdev(cxt, ol, hd);
	if (!rc)
		rc = mnt_context_append_hook(cxt, hs,
				MNT_STAGE_MOUNT_POST,
				hd, hook_cleanup_loopdev);
	if (rc) {
		delete_loopdev(cxt, hd);
		free(hd);
	}
	return rc;
}


const struct libmnt_hookset hookset_loopdev =
{
        .name = "__loopdev",

        .firststage = MNT_STAGE_PREP_SOURCE,
        .firstcall = hook_prepare_loopdev,

        .deinit = hookset_deinit
};
