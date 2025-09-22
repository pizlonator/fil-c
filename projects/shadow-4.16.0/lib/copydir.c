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

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>

#include "alloc.h"
#include "attr.h"
#include "prototypes.h"
#include "defines.h"
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif				/* WITH_SELINUX */
#if defined(WITH_ACL) || defined(WITH_ATTR)
#include <stdarg.h>
#include <attr/error_context.h>
#endif				/* WITH_ACL || WITH_ATTR */
#ifdef WITH_ACL
#include <acl/libacl.h>
#endif				/* WITH_ACL */
#ifdef WITH_ATTR
#include <attr/libattr.h>
#endif				/* WITH_ATTR */
#include "shadowlog.h"
#include "string/sprintf.h"


static /*@null@*/const char *src_orig;
static /*@null@*/const char *dst_orig;

struct link_name {
	dev_t ln_dev;
	ino_t ln_ino;
	nlink_t ln_count;
	char *ln_name;
	/*@dependent@*/struct link_name *ln_next;
};
static /*@exposed@*/struct link_name *links;

struct path_info {
	const char *full_path;
	int dirfd;
	const char *name;
};

static int copy_entry (const struct path_info *src, const struct path_info *dst,
                       bool reset_selinux,
                       uid_t old_uid, uid_t new_uid,
                       gid_t old_gid, gid_t new_gid);
static int copy_dir (const struct path_info *src, const struct path_info *dst,
                     bool reset_selinux,
                     const struct stat *statp, const struct timespec mt[],
                     uid_t old_uid, uid_t new_uid,
                     gid_t old_gid, gid_t new_gid);
static /*@null@*/char *readlink_malloc (const char *filename);
static int copy_symlink (const struct path_info *src, const struct path_info *dst,
                         MAYBE_UNUSED bool reset_selinux,
                         const struct stat *statp, const struct timespec mt[],
                         uid_t old_uid, uid_t new_uid,
                         gid_t old_gid, gid_t new_gid);
static int copy_hardlink (const struct path_info *dst,
                          MAYBE_UNUSED bool reset_selinux,
                          struct link_name *lp);
static int copy_special (const struct path_info *src, const struct path_info *dst,
                         bool reset_selinux,
                         const struct stat *statp, const struct timespec mt[],
                         uid_t old_uid, uid_t new_uid,
                         gid_t old_gid, gid_t new_gid);
static int copy_file (const struct path_info *src, const struct path_info *dst,
                      bool reset_selinux,
                      const struct stat *statp, const struct timespec mt[],
                      uid_t old_uid, uid_t new_uid,
                      gid_t old_gid, gid_t new_gid);
static int chownat_if_needed (const struct path_info *dst, const struct stat *statp,
                            uid_t old_uid, uid_t new_uid,
                            gid_t old_gid, gid_t new_gid);
static int fchown_if_needed (int fdst, const struct stat *statp,
                             uid_t old_uid, uid_t new_uid,
                             gid_t old_gid, gid_t new_gid);

#if defined(WITH_ACL) || defined(WITH_ATTR)
/*
 * error_acl - format the error messages for the ACL and EQ libraries.
 */
format_attr(printf, 2, 3)
static void error_acl (MAYBE_UNUSED struct error_context *ctx, const char *fmt, ...)
{
	va_list ap;
	FILE *shadow_logfd = log_get_logfd();

	/* ignore the case when destination does not support ACLs
	 * or extended attributes */
	if (ENOTSUP == errno) {
		errno = 0;
		return;
	}

	va_start (ap, fmt);
	(void) fprintf (shadow_logfd, _("%s: "), log_get_progname());
	if (vfprintf (shadow_logfd, fmt, ap) != 0) {
		(void) fputs (_(": "), shadow_logfd);
	}
	(void) fprintf (shadow_logfd, "%s\n", strerror (errno));
	va_end (ap);
}

static struct error_context ctx = {
	error_acl, NULL, NULL
};
#endif				/* WITH_ACL || WITH_ATTR */

#ifdef WITH_ACL
static int perm_copy_path(const struct path_info *src,
						  const struct path_info *dst,
						  struct error_context *errctx)
{
	int src_fd, dst_fd, ret;

	src_fd = openat(src->dirfd, src->name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (src_fd < 0) {
		return -1;
	}

	dst_fd = openat(dst->dirfd, dst->name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (dst_fd < 0) {
		(void) close (src_fd);
		return -1;
	}

	ret = perm_copy_fd(src->full_path, src_fd, dst->full_path, dst_fd, errctx);
	(void) close (src_fd);
	(void) close (dst_fd);
	return ret;
}
#endif				/* WITH_ACL */

#ifdef WITH_ATTR
static int attr_copy_path(const struct path_info *src,
						  const struct path_info *dst,
						  int (*callback) (const char *, struct error_context *),
						  struct error_context *errctx)
{
	int src_fd, dst_fd, ret;

	src_fd = openat(src->dirfd, src->name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (src_fd < 0) {
		return -1;
	}

	dst_fd = openat(dst->dirfd, dst->name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
	if (dst_fd < 0) {
		(void) close (src_fd);
		return -1;
	}

	ret = attr_copy_fd(src->full_path, src_fd, dst->full_path, dst_fd, callback, errctx);
	(void) close (src_fd);
	(void) close (dst_fd);
	return ret;
}
#endif				/* WITH_ATTR */

/*
 * remove_link - delete a link from the linked list
 */
static void remove_link (/*@only@*/struct link_name *ln)
{
	struct link_name *lp;

	if (links == ln) {
		links = ln->ln_next;
		free (ln->ln_name);
		free (ln);
		return;
	}
	for (lp = links; NULL !=lp; lp = lp->ln_next) {
		if (lp->ln_next == ln) {
			break;
		}
	}

	if (NULL == lp) {
		free (ln->ln_name);
		free (ln);
		return;
	}

	lp->ln_next = lp->ln_next->ln_next;
	free (ln->ln_name);
	free (ln);
}

/*
 * check_link - see if a file is really a link
 */

static /*@exposed@*/ /*@null@*/struct link_name *check_link (const char *name, const struct stat *sb)
{
	struct link_name  *lp;

	/* copy_tree () must be the entry point */
	assert (NULL != src_orig);
	assert (NULL != dst_orig);

	for (lp = links; NULL != lp; lp = lp->ln_next) {
		if ((lp->ln_dev == sb->st_dev) && (lp->ln_ino == sb->st_ino)) {
			return lp;
		}
	}

	if (sb->st_nlink == 1) {
		return NULL;
	}

	lp = XMALLOC(1, struct link_name);
	lp->ln_dev = sb->st_dev;
	lp->ln_ino = sb->st_ino;
	lp->ln_count = sb->st_nlink;
	xasprintf(&lp->ln_name, "%s%s", dst_orig, name + strlen(src_orig));
	lp->ln_next = links;
	links = lp;

	return NULL;
}

static int copy_tree_impl (const struct path_info *src, const struct path_info *dst,
               bool copy_root, bool reset_selinux,
               uid_t old_uid, uid_t new_uid,
               gid_t old_gid, gid_t new_gid)
{
	int dst_fd, src_fd, err = 0;
	bool set_orig = false;
	const struct dirent *ent;
	DIR *dir;

	if (copy_root) {
		struct stat sb;

		if (   fstatat (dst->dirfd, dst->name, &sb, 0) == 0
		    || errno != ENOENT) {
			return -1;
		}

		if (fstatat (src->dirfd, src->name, &sb, AT_SYMLINK_NOFOLLOW) == -1) {
			return -1;
		}

		if (!S_ISDIR (sb.st_mode)) {
			fprintf (log_get_logfd(),
			         "%s: %s is not a directory",
			         log_get_progname(), src->full_path);
			return -1;
		}

		return copy_entry (src, dst, reset_selinux,
		                   old_uid, new_uid, old_gid, new_gid);
	}

	/*
	 * Make certain both directories exist.  This routine is called
	 * after the home directory is created, or recursively after the
	 * target is created.  It assumes the target directory exists.
	 */

	src_fd = openat (src->dirfd, src->name, O_DIRECTORY | O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (src_fd < 0) {
		return -1;
	}

	dst_fd = openat (dst->dirfd, dst->name, O_DIRECTORY | O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
	if (dst_fd < 0) {
		(void) close (src_fd);
		return -1;
	}

	/*
	 * Open the source directory and read each entry.  Every file
	 * entry in the directory is copied with the UID and GID set
	 * to the provided values.  As an added security feature only
	 * regular files (and directories ...) are copied, and no file
	 * is made set-ID.
	 */
	dir = fdopendir (src_fd);
	if (NULL == dir) {
		(void) close (src_fd);
		(void) close (dst_fd);
		return -1;
	}

	if (src_orig == NULL) {
		src_orig = src->full_path;
		dst_orig = dst->full_path;
		set_orig = true;
	}
	while ((0 == err) && (ent = readdir (dir)) != NULL) {
		char              *src_name = NULL;
		char              *dst_name;
		struct path_info  src_entry, dst_entry;
		/*
		 * Skip the "." and ".." entries
		 */
		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
		{
			continue;
		}

		if (asprintf(&src_name, "%s/%s", src->full_path, ent->d_name) == -1)
		{
			err = -1;
			continue;
		}
		if (asprintf(&dst_name, "%s/%s", dst->full_path, ent->d_name) == -1)
		{
			err = -1;
			goto skip;
		}

		src_entry.full_path = src_name;
		src_entry.dirfd = dirfd(dir);
		src_entry.name = ent->d_name;

		dst_entry.full_path = dst_name;
		dst_entry.dirfd = dst_fd;
		dst_entry.name = ent->d_name;

		err = copy_entry(&src_entry, &dst_entry, reset_selinux,
				 old_uid, new_uid, old_gid, new_gid);

		free(dst_name);
skip:
		free(src_name);
	}
	(void) closedir (dir);
	(void) close (dst_fd);

	if (set_orig) {
		src_orig = NULL;
		dst_orig = NULL;
		/* FIXME: clean links
		 * Since there can be hardlinks elsewhere on the device,
		 * we cannot check that all the hardlinks were found:
		assert (NULL == links);
		 */
	}

#ifdef WITH_SELINUX
	/* Reset SELinux to create files with default contexts.
	 * Note that the context is only reset on exit of copy_tree (it is
	 * assumed that the program would quit without needing a restored
	 * context if copy_tree failed previously), and that copy_tree can
	 * be called recursively (hence the context is set on the
	 * sub-functions of copy_entry).
	 */
	if (reset_selinux_file_context () != 0) {
		err = -1;
	}
#endif				/* WITH_SELINUX */

	return err;
}

/*
 * copy_entry - copy the entry of a directory
 *
 *	Copy the entry src to dst.
 *	Depending on the type of entry, this function will forward the
 *	request to copy_dir(), copy_symlink(), copy_hardlink(),
 *	copy_special(), or copy_file().
 *
 *	The access and modification time will not be modified.
 *
 *	The permissions will be set to new_uid/new_gid.
 *
 *	If new_uid (resp. new_gid) is equal to -1, the user (resp. group) will
 *	not be modified.
 *
 *	Only the files owned (resp. group-owned) by old_uid (resp.
 *	old_gid) will be modified, unless old_uid (resp. old_gid) is set
 *	to -1.
 */
static int copy_entry (const struct path_info *src, const struct path_info *dst,
                       bool reset_selinux,
                       uid_t old_uid, uid_t new_uid,
                       gid_t old_gid, gid_t new_gid)
{
	int err = 0;
	struct stat sb;
	struct stat tmp_sb;
	struct link_name *lp;
	struct timespec mt[2];

	if (fstatat(src->dirfd, src->name, &sb, AT_SYMLINK_NOFOLLOW) == -1) {
		/* If we cannot stat the file, do not care. */
		return 0;
	}

	mt[0].tv_sec  = sb.st_atim.tv_sec;
	mt[0].tv_nsec = sb.st_atim.tv_nsec;

	mt[1].tv_sec  = sb.st_mtim.tv_sec;
	mt[1].tv_nsec = sb.st_mtim.tv_nsec;

	if (S_ISDIR (sb.st_mode)) {
		err = copy_dir (src, dst, reset_selinux, &sb, mt,
				old_uid, new_uid, old_gid, new_gid);
	}

	/*
	* If the destination already exists do nothing.
	* This is after the copy_dir above to still iterate into subdirectories.
	*/
	if (fstatat(dst->dirfd, dst->name, &tmp_sb, AT_SYMLINK_NOFOLLOW) != -1) {
		return err;
	}

	/*
	* Copy any symbolic links
	*/

	else if (S_ISLNK (sb.st_mode)) {
		err = copy_symlink (src, dst, reset_selinux, &sb, mt,
				    old_uid, new_uid, old_gid, new_gid);
	}

	/*
	* See if this is a previously copied link
	*/

	else if ((lp = check_link (src->full_path, &sb)) != NULL) {
		err = copy_hardlink (dst, reset_selinux, lp);
	}

	/*
	* Deal with FIFOs and special files.  The user really
	* shouldn't have any of these, but it seems like it
	* would be nice to copy everything ...
	*/

	else if (!S_ISREG (sb.st_mode)) {
		err = copy_special (src, dst, reset_selinux, &sb, mt,
				    old_uid, new_uid, old_gid, new_gid);
	}

	/*
	* Create the new file and copy the contents.  The new
	* file will be owned by the provided UID and GID values.
	*/

	else {
		err = copy_file (src, dst, reset_selinux, &sb, mt,
				 old_uid, new_uid, old_gid, new_gid);
	}

	return err;
}

/*
 * copy_dir - copy a directory
 *
 *	Copy a directory (recursively) from src to dst.
 *
 *	statp, mt, old_uid, new_uid, old_gid, and new_gid are used to set
 *	the access and modification and the access rights.
 *
 *	Return 0 on success, -1 on error.
 */
static int copy_dir (const struct path_info *src, const struct path_info *dst,
                     bool reset_selinux,
                     const struct stat *statp, const struct timespec mt[],
                     uid_t old_uid, uid_t new_uid,
                     gid_t old_gid, gid_t new_gid)
{
	int err = 0;
	struct stat dst_sb;

	/*
	 * Create a new target directory, make it owned by
	 * the user and then recursively copy that directory.
	 */

#ifdef WITH_SELINUX
	if (set_selinux_file_context (dst->full_path, S_IFDIR) != 0) {
		return -1;
	}
#endif				/* WITH_SELINUX */
        /*
         * If the destination is already a directory, don't change it
         * but copy into it (recursively).
        */
        if (fstatat(dst->dirfd, dst->name, &dst_sb, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(dst_sb.st_mode)) {
            return (copy_tree_impl (src, dst, false, reset_selinux,
                           old_uid, new_uid, old_gid, new_gid) != 0);
        }

	if (   (mkdirat (dst->dirfd, dst->name, 0700) != 0)
	    || (chownat_if_needed (dst, statp,
	                         old_uid, new_uid, old_gid, new_gid) != 0)
	    || (fchmodat (dst->dirfd, dst->name, statp->st_mode & 07777, AT_SYMLINK_NOFOLLOW) != 0)
#ifdef WITH_ACL
	    || (   (perm_copy_path (src, dst, &ctx) != 0)
	        && (errno != 0))
#endif				/* WITH_ACL */
#ifdef WITH_ATTR
	/*
	 * If the third parameter is NULL, all extended attributes
	 * except those that define Access Control Lists are copied.
	 * ACLs are excluded by default because copying them between
	 * file systems with and without ACL support needs some
	 * additional logic so that no unexpected permissions result.
	 */
	    || (   !reset_selinux
	        && (attr_copy_path (src, dst, NULL, &ctx) != 0)
	        && (errno != 0))
#endif				/* WITH_ATTR */
	    || (copy_tree_impl (src, dst, false, reset_selinux,
	                   old_uid, new_uid, old_gid, new_gid) != 0)
	    || (utimensat (dst->dirfd, dst->name, mt, AT_SYMLINK_NOFOLLOW) != 0)) {
		err = -1;
	}

	return err;
}

/*
 * readlink_malloc - wrapper for readlink
 *
 * return NULL on error.
 * The return string shall be freed by the caller.
 */
static /*@null@*/char *readlink_malloc (const char *filename)
{
	size_t size = 1024;

	while (true) {
		ssize_t nchars;
		char *buffer = MALLOC(size, char);
		if (NULL == buffer) {
			return NULL;
		}

		nchars = readlink (filename, buffer, size);

		if (nchars < 0) {
			free(buffer);
			return NULL;
		}

		if ((size_t) nchars < size) { /* The buffer was large enough */
			/* readlink does not nul-terminate */
			buffer[nchars] = '\0';
			return buffer;
		}

		/* Try again with a bigger buffer */
		free (buffer);
		size *= 2;
	}
}

/*
 * copy_symlink - copy a symlink
 *
 *	Copy a symlink from src to dst.
 *
 *	statp, mt, old_uid, new_uid, old_gid, and new_gid are used to set
 *	the access and modification and the access rights.
 *
 *	Return 0 on success, -1 on error.
 */
static int copy_symlink (const struct path_info *src, const struct path_info *dst,
                         MAYBE_UNUSED bool reset_selinux,
                         const struct stat *statp, const struct timespec mt[],
                         uid_t old_uid, uid_t new_uid,
                         gid_t old_gid, gid_t new_gid)
{
	char *oldlink;

	/* copy_tree () must be the entry point */
	assert (NULL != src_orig);
	assert (NULL != dst_orig);

	/*
	 * Get the name of the file which the link points
	 * to.  If that name begins with the original
	 * source directory name, that part of the link
	 * name will be replaced with the original
	 * destination directory name.
	 */

	oldlink = readlink_malloc (src->full_path);
	if (NULL == oldlink) {
		return -1;
	}

	/* If src was a link to an entry of the src_orig directory itself,
	 * create a link to the corresponding entry in the dst_orig
	 * directory.
	 */
	if (strncmp(oldlink, src_orig, strlen(src_orig)) == 0) {
		char  *dummy;

		xasprintf(&dummy, "%s%s", dst_orig, oldlink + strlen(src_orig));
		free(oldlink);
		oldlink = dummy;
	}

#ifdef WITH_SELINUX
	if (set_selinux_file_context (dst->full_path, S_IFLNK) != 0) {
		free (oldlink);
		return -1;
	}
#endif				/* WITH_SELINUX */
	if (   (symlinkat (oldlink, dst->dirfd, dst->name) != 0)
	    || (chownat_if_needed (dst, statp,
	                          old_uid, new_uid, old_gid, new_gid) != 0)) {
		/* FIXME: there are no modes on symlinks, right?
		 *        ACL could be copied, but this would be much more
		 *        complex than calling perm_copy_file.
		 *        Ditto for Extended Attributes.
		 *        We currently only document that ACL and Extended
		 *        Attributes are not copied.
		 */
		free (oldlink);
		return -1;
	}
	free (oldlink);

	if (utimensat (dst->dirfd, dst->name, mt, AT_SYMLINK_NOFOLLOW) != 0) {
		return -1;
	}

	return 0;
}

/*
 * copy_hardlink - copy a hardlink
 *
 *	Copy a hardlink from src to dst.
 *
 *	Return 0 on success, -1 on error.
 */
static int copy_hardlink (const struct path_info *dst,
                          MAYBE_UNUSED bool reset_selinux,
                          struct link_name *lp)
{
	/* FIXME: selinux, ACL, Extended Attributes needed? */

	if (linkat (AT_FDCWD, lp->ln_name, dst->dirfd, dst->name, 0) != 0) {
		return -1;
	}

	/* If the file could be unlinked, decrement the links counter,
	 * and forget about this link if it was the last reference */
	lp->ln_count--;
	if (lp->ln_count <= 0) {
		remove_link (lp);
	}

	return 0;
}


/*
 * copy_special - copy a special file
 *
 *	Copy a special file from src to dst.
 *
 *	statp, mt, old_uid, new_uid, old_gid, and new_gid are used to set
 *	the access and modification and the access rights.
 *
 *	Return 0 on success, -1 on error.
 */
static int
copy_special(const struct path_info *src, const struct path_info *dst,
             bool reset_selinux,
             const struct stat *statp, const struct timespec mt[],
             uid_t old_uid, uid_t new_uid,
             gid_t old_gid, gid_t new_gid)
{
#if defined(WITH_SELINUX)
	if (set_selinux_file_context(dst->full_path, statp->st_mode & S_IFMT) != 0)
		return -1;
#endif

	if (mknodat(dst->dirfd, dst->name, statp->st_mode & ~07777U, statp->st_rdev) == -1)
		return -1;

	if (chownat_if_needed(dst, statp, old_uid, new_uid, old_gid, new_gid) == -1)
		return -1;

	if (fchmodat(dst->dirfd, dst->name, statp->st_mode & 07777, AT_SYMLINK_NOFOLLOW) == -1)
		return -1;

#if defined(WITH_ACL)
	if (perm_copy_path(src, dst, &ctx) == -1 && errno != 0)
		return -1;
#endif

#if defined(WITH_ATTR)
	/*
	 * If the third parameter is NULL, all extended attributes
	 * except those that define Access Control Lists are copied.
	 * ACLs are excluded by default because copying them between
	 * file systems with and without ACL support needs some
	 * additional logic so that no unexpected permissions result.
	 */
	if (!reset_selinux) {
		if (attr_copy_path(src, dst, NULL, &ctx) == -1 && errno != 0)
			return -1;
	}
#endif

	if (utimensat(dst->dirfd, dst->name, mt, AT_SYMLINK_NOFOLLOW) == -1)
		return -1;

	return 0;
}

/*
 * copy_file - copy a file
 *
 *	Copy a file from src to dst.
 *
 *	statp, mt, old_uid, new_uid, old_gid, and new_gid are used to set
 *	the access and modification and the access rights.
 *
 *	Return 0 on success, -1 on error.
 */
static int copy_file (const struct path_info *src, const struct path_info *dst,
                      bool reset_selinux,
                      const struct stat *statp, const struct timespec mt[],
                      uid_t old_uid, uid_t new_uid,
                      gid_t old_gid, gid_t new_gid)
{
	int err = 0;
	int ifd;
	int ofd;

	ifd = openat (src->dirfd, src->name, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
	if (ifd < 0) {
		return -1;
	}
#ifdef WITH_SELINUX
	if (set_selinux_file_context (dst->full_path, S_IFREG) != 0) {
		(void) close (ifd);
		return -1;
	}
#endif				/* WITH_SELINUX */
	ofd = openat (dst->dirfd, dst->name, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (   (ofd < 0)
	    || (fchown_if_needed (ofd, statp,
	                          old_uid, new_uid, old_gid, new_gid) != 0)
	    || (fchmod (ofd, statp->st_mode & 07777) != 0)
#ifdef WITH_ACL
	    || (   (perm_copy_fd (src->full_path, ifd, dst->full_path, ofd, &ctx) != 0)
	        && (errno != 0))
#endif				/* WITH_ACL */
#ifdef WITH_ATTR
	/*
	 * If the third parameter is NULL, all extended attributes
	 * except those that define Access Control Lists are copied.
	 * ACLs are excluded by default because copying them between
	 * file systems with and without ACL support needs some
	 * additional logic so that no unexpected permissions result.
	 */
	    || (   !reset_selinux
	        && (attr_copy_fd (src->full_path, ifd, dst->full_path, ofd, NULL, &ctx) != 0)
	        && (errno != 0))
#endif				/* WITH_ATTR */
	   ) {
		if (ofd >= 0) {
			(void) close (ofd);
		}
		(void) close (ifd);
		return -1;
	}

	while (true) {
		char buf[8192];
		ssize_t cnt;

		cnt = read (ifd, buf, sizeof buf);
		if (cnt < 0) {
			if (errno == EINTR) {
				continue;
			}
			(void) close (ofd);
			(void) close (ifd);
			return -1;
		}
		if (cnt == 0) {
			break;
		}

		if (write_full(ofd, buf, cnt) == -1) {
			(void) close (ofd);
			(void) close (ifd);
			return -1;
		}
	}

	(void) close (ifd);
	if (close (ofd) != 0 && errno != EINTR) {
		return -1;
	}

	if (utimensat (dst->dirfd, dst->name, mt, AT_SYMLINK_NOFOLLOW) != 0) {
		return -1;
	}

	return err;
}

#define def_chown_if_needed(chown_function, type_dst)                  \
static int chown_function ## _if_needed (type_dst dst,                 \
                                         const struct stat *statp,     \
                                         uid_t old_uid, uid_t new_uid, \
                                         gid_t old_gid, gid_t new_gid) \
{                                                                      \
	uid_t tmpuid = (uid_t) -1;                                     \
	gid_t tmpgid = (gid_t) -1;                                     \
                                                                       \
	/* Use new_uid if old_uid is set to -1 or if the file was      \
	 * owned by the user. */                                       \
	if (((uid_t) -1 == old_uid) || (statp->st_uid == old_uid)) {   \
		tmpuid = new_uid;                                      \
	}                                                              \
	/* Otherwise, or if new_uid was set to -1, we keep the same    \
	 * owner. */                                                   \
	if ((uid_t) -1 == tmpuid) {                                    \
		tmpuid = statp->st_uid;                                \
	}                                                              \
                                                                       \
	if (((gid_t) -1 == old_gid) || (statp->st_gid == old_gid)) {   \
		tmpgid = new_gid;                                      \
	}                                                              \
	if ((gid_t) -1 == tmpgid) {                                    \
		tmpgid = statp->st_gid;                                \
	}                                                              \
                                                                       \
	return chown_function (dst, tmpuid, tmpgid);                   \
}

def_chown_if_needed (fchown, int)

static int chownat_if_needed (const struct path_info *dst,
							  const struct stat *statp,
                              uid_t old_uid, uid_t new_uid,
                              gid_t old_gid, gid_t new_gid)
{
	uid_t tmpuid = (uid_t) -1;
	gid_t tmpgid = (gid_t) -1;

	/* Use new_uid if old_uid is set to -1 or if the file was
	 * owned by the user. */
	if (((uid_t) -1 == old_uid) || (statp->st_uid == old_uid)) {
		tmpuid = new_uid;
	}
	/* Otherwise, or if new_uid was set to -1, we keep the same
	 * owner. */
	if ((uid_t) -1 == tmpuid) {
		tmpuid = statp->st_uid;
	}

	if (((gid_t) -1 == old_gid) || (statp->st_gid == old_gid)) {
		tmpgid = new_gid;
	}
	if ((gid_t) -1 == tmpgid) {
		tmpgid = statp->st_gid;
	}

	return fchownat (dst->dirfd, dst->name, tmpuid, tmpgid, AT_SYMLINK_NOFOLLOW);
}

/*
 * copy_tree - copy files in a directory tree
 *
 *	copy_tree() walks a directory tree and copies ordinary files
 *	as it goes.
 *
 *	When reset_selinux is enabled, extended attributes (and thus
 *	SELinux attributes) are not copied.
 *
 *	old_uid and new_uid are used to set the ownership of the copied
 *	files. Unless old_uid is set to -1, only the files owned by
 *	old_uid have their ownership changed to new_uid. In addition, if
 *	new_uid is set to -1, no ownership will be changed.
 *
 *	The same logic applies for the group-ownership and
 *	old_gid/new_gid.
 */
int copy_tree (const char *src_root, const char *dst_root,
               bool copy_root, bool reset_selinux,
               uid_t old_uid, uid_t new_uid,
               gid_t old_gid, gid_t new_gid)
{
	const struct path_info src = {
		.full_path = src_root,
		.dirfd = AT_FDCWD,
		.name = src_root
	};
	const struct path_info dst = {
		.full_path = dst_root,
		.dirfd = AT_FDCWD,
		.name = dst_root
	};

	return copy_tree_impl(&src, &dst, copy_root, reset_selinux,
						  old_uid, new_uid, old_gid, new_gid);
}
