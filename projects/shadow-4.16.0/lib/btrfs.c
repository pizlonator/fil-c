#include <linux/btrfs_tree.h>
#include <linux/magic.h>
#include <sys/statfs.h>
#include <stdbool.h>

#include "prototypes.h"

static bool path_exists(const char *p)
{
	struct stat sb;

	return stat(p, &sb) == 0;
}

static const char *btrfs_cmd(void)
{
	const char *const btrfs_paths[] = {"/sbin/btrfs",
		"/bin/btrfs", "/usr/sbin/btrfs", "/usr/bin/btrfs", NULL};
	const char *p;
	int i;

	for (i = 0, p = btrfs_paths[i]; p; i++, p = btrfs_paths[i])
		if (path_exists(p))
			return p;

	return NULL;
}

static int run_btrfs_subvolume_cmd(const char *subcmd, const char *arg1, const char *arg2)
{
	int status = 0;
	const char *cmd = btrfs_cmd();
	const char *argv[] = {
		"btrfs",
		"subvolume",
		subcmd,
		arg1,
		arg2,
		NULL
	};

	if (!cmd || access(cmd, X_OK)) {
		return 1;
	}

	if (run_command(cmd, argv, NULL, &status))
		return -1;
	return status;
}


int btrfs_create_subvolume(const char *path)
{
	return run_btrfs_subvolume_cmd("create", path, NULL);
}


int btrfs_remove_subvolume(const char *path)
{
	return run_btrfs_subvolume_cmd("delete", "-C", path);
}


/* Adapted from btrfsprogs */
/*
 * This intentionally duplicates btrfs_util_is_subvolume_fd() instead of opening
 * a file descriptor and calling it, because fstat() and fstatfs() don't accept
 * file descriptors opened with O_PATH on old kernels (before v3.6 and before
 * v3.12, respectively), but stat() and statfs() can be called on a path that
 * the user doesn't have read or write permissions to.
 *
 * returns:
 *   1 - btrfs subvolume
 *   0 - not btrfs subvolume
 *  -1 - error
 */
int btrfs_is_subvolume(const char *path)
{
	struct stat st;
	int ret;

	ret = is_btrfs(path);
	if (ret <= 0)
		return ret;

	ret = stat(path, &st);
	if (ret == -1)
		return -1;

	if (st.st_ino != BTRFS_FIRST_FREE_OBJECTID || !S_ISDIR(st.st_mode)) {
		return 0;
	}

	return 1;
}


/* Adapted from btrfsprogs */
int is_btrfs(const char *path)
{
	struct statfs sfs;
	int ret;

	ret = statfs(path, &sfs);
	if (ret == -1)
		return -1;

	return sfs.f_type == BTRFS_SUPER_MAGIC;
}

