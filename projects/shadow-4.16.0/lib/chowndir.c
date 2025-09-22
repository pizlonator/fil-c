/*
 * SPDX-FileCopyrightText: 1992 - 1993, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2000, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2005, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2010 -     , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <sys/types.h>
#include <sys/stat.h>
#include "prototypes.h"
#include "defines.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int chown_tree_at (int at_fd,
                const char *path,
                uid_t old_uid,
                uid_t new_uid,
                gid_t old_gid,
                gid_t new_gid)
{
	DIR *dir;
	const struct dirent *ent;
	struct stat dir_sb;
	int dir_fd, rc = 0;

	dir_fd = openat (at_fd, path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (dir_fd < 0) {
		return -1;
	}

	dir = fdopendir (dir_fd);
	if (!dir) {
		(void) close (dir_fd);
		return -1;
	}

	/*
	 * Open the directory and read each entry.  Every entry is tested
	 * to see if it is a directory, and if so this routine is called
	 * recursively.  If not, it is checked to see if an ownership
	 * shall be changed.
	 */
	while ((ent = readdir (dir))) {
		uid_t tmpuid = (uid_t) -1;
		gid_t tmpgid = (gid_t) -1;
		struct stat ent_sb;

		/*
		 * Skip the "." and ".." entries
		 */
		if (   (strcmp (ent->d_name, ".") == 0)
		    || (strcmp (ent->d_name, "..") == 0)) {
			continue;
		}

		rc = fstatat (dirfd(dir), ent->d_name, &ent_sb, AT_SYMLINK_NOFOLLOW);
		if (rc < 0) {
			break;
		}

		if (S_ISDIR (ent_sb.st_mode)) {
			/*
			 * Do the entire subdirectory.
			 */
			rc = chown_tree_at (dirfd(dir), ent->d_name, old_uid, new_uid, old_gid, new_gid);
			if (0 != rc) {
				break;
			}
		}

		/*
		 * By default, the IDs are not changed (-1).
		 *
		 * If the file is not owned by the user, the owner is not
		 * changed.
		 *
		 * If the file is not group-owned by the group, the
		 * group-owner is not changed.
		 */
		if (((uid_t) -1 == old_uid) || (ent_sb.st_uid == old_uid)) {
			tmpuid = new_uid;
		}
		if (((gid_t) -1 == old_gid) || (ent_sb.st_gid == old_gid)) {
			tmpgid = new_gid;
		}
		if (((uid_t) -1 != tmpuid) || ((gid_t) -1 != tmpgid)) {
			rc = fchownat (dirfd(dir), ent->d_name, tmpuid, tmpgid, AT_SYMLINK_NOFOLLOW);
			if (0 != rc) {
				break;
			}
		}
	}

	/*
	 * Now do the root of the tree
	 */
	if ((0 == rc) && (fstat (dirfd(dir), &dir_sb) == 0)) {
		uid_t tmpuid = (uid_t) -1;
		gid_t tmpgid = (gid_t) -1;
		if (((uid_t) -1 == old_uid) || (dir_sb.st_uid == old_uid)) {
			tmpuid = new_uid;
		}
		if (((gid_t) -1 == old_gid) || (dir_sb.st_gid == old_gid)) {
			tmpgid = new_gid;
		}
		if (((uid_t) -1 != tmpuid) || ((gid_t) -1 != tmpgid)) {
			rc = fchown (dirfd(dir), tmpuid, tmpgid);
		}
	} else {
		rc = -1;
	}

	(void) closedir (dir);

	return rc;
}

/*
 * chown_tree - change ownership of files in a directory tree
 *
 *	chown_dir() walks a directory tree and changes the ownership
 *	of all files owned by the provided user ID.
 *
 *	Only files owned (resp. group-owned) by old_uid (resp. by old_gid)
 *	will have their ownership (resp. group-ownership) modified, unless
 *	old_uid (resp. old_gid) is set to -1.
 *
 *	new_uid and new_gid can be set to -1 to indicate that no owner or
 *	group-owner shall be changed.
 */
int chown_tree (const char *root,
                uid_t old_uid,
                uid_t new_uid,
                gid_t old_gid,
                gid_t new_gid)
{
	return chown_tree_at (AT_FDCWD, root, old_uid, new_uid, old_gid, new_gid);
}
