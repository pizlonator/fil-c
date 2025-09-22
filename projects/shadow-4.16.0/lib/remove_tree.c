/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 2001, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2003 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2010, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ident "$Id$"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "prototypes.h"
#include "defines.h"

static int remove_tree_at (int at_fd, const char *path, bool remove_root)
{
	DIR *dir;
	const struct dirent *ent;
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
	 * Open the source directory and delete each entry.
	 */
	while ((ent = readdir (dir))) {
		struct stat ent_sb;

		/*
		 * Skip the "." and ".." entries
		 */
		if (strcmp (ent->d_name, ".") == 0 ||
		    strcmp (ent->d_name, "..") == 0) {
			continue;
		}

		rc = fstatat (dirfd(dir), ent->d_name, &ent_sb, AT_SYMLINK_NOFOLLOW);
		if (rc < 0) {
			break;
		}

		if (S_ISDIR (ent_sb.st_mode)) {
			/*
			 * Recursively delete this directory.
			 */
			if (remove_tree_at (dirfd(dir), ent->d_name, true) != 0) {
				rc = -1;
				break;
			}
		} else {
			/*
			 * Delete the file.
			 */
			if (unlinkat (dirfd(dir), ent->d_name, 0) != 0) {
				rc = -1;
				break;
			}
		}
	}

	(void) closedir (dir);

	if (remove_root && (0 == rc)) {
		if (unlinkat (at_fd, path, AT_REMOVEDIR) != 0) {
			rc = -1;
		}
	}

	return rc;
}

/*
 * remove_tree - delete a directory tree
 *
 *	remove_tree() walks a directory tree and deletes all the files
 *	and directories.
 *	At the end, it deletes the root directory itself.
 */
int remove_tree (const char *root, bool remove_root)
{
	return remove_tree_at (AT_FDCWD, root, remove_root);
}
