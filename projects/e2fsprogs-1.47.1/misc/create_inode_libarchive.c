/*
 * create_inode_libarchive.c --- create an inode from libarchive input
 *
 * Copyright (C) 2023 Johannes Schauer Marin Rodrigues <josch@debian.org>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU library
 * General Public License, version 2.
 * %End-Header%
 */

#define _LARGEFILE64_SOURCE 1
#define _GNU_SOURCE 1

#include "config.h"
#include <ext2fs/ext2_types.h>
#include "create_inode.h"
#include "create_inode_libarchive.h"
#include "support/nls-enable.h"

#ifdef HAVE_ARCHIVE_H

/* 64KiB is the minimum blksize to best minimize system call overhead. */
//#define COPY_FILE_BUFLEN 65536
//#define COPY_FILE_BUFLEN 1048576
#define COPY_FILE_BUFLEN 16777216

#include <archive.h>
#include <archive_entry.h>
#include <libgen.h>
#include <locale.h>

static const char *(*dl_archive_entry_hardlink)(struct archive_entry *);
static const char *(*dl_archive_entry_pathname)(struct archive_entry *);
static const struct stat *(*dl_archive_entry_stat)(struct archive_entry *);
static const char *(*dl_archive_entry_symlink)(struct archive_entry *);
static int (*dl_archive_entry_xattr_count)(struct archive_entry *);
static int (*dl_archive_entry_xattr_next)(struct archive_entry *, const char **,
					  const void **, size_t *);
static int (*dl_archive_entry_xattr_reset)(struct archive_entry *);
static const char *(*dl_archive_error_string)(struct archive *);
static int (*dl_archive_read_close)(struct archive *);
static la_ssize_t (*dl_archive_read_data)(struct archive *, void *, size_t);
static int (*dl_archive_read_free)(struct archive *);
static struct archive *(*dl_archive_read_new)(void);
static int (*dl_archive_read_next_header)(struct archive *,
					  struct archive_entry **);
static int (*dl_archive_read_open_filename)(struct archive *,
					    const char *filename, size_t);
static int (*dl_archive_read_support_filter_all)(struct archive *);
static int (*dl_archive_read_support_format_all)(struct archive *);

#ifdef CONFIG_DLOPEN_LIBARCHIVE
#include <dlfcn.h>

static void *libarchive_handle;

#if defined(__FreeBSD__)
#define LIBARCHIVE_SO "libarchive.so.7"
#else
#define LIBARCHIVE_SO "libarchive.so.13"
#endif

static int libarchive_available(void)
{
	if (!libarchive_handle) {
		libarchive_handle = dlopen(LIBARCHIVE_SO, RTLD_NOW);
		if (!libarchive_handle)
			return 0;

		dl_archive_entry_hardlink =
			(const char *(*)(struct archive_entry *))dlsym(
				libarchive_handle, "archive_entry_hardlink");
		if (!dl_archive_entry_hardlink)
			return 0;
		dl_archive_entry_pathname =
			(const char *(*)(struct archive_entry *))dlsym(
				libarchive_handle, "archive_entry_pathname");
		if (!dl_archive_entry_pathname)
			return 0;
		dl_archive_entry_stat =
			(const struct stat *(*)(struct archive_entry *))dlsym(
				libarchive_handle, "archive_entry_stat");
		if (!dl_archive_entry_stat)
			return 0;
		dl_archive_entry_symlink =
			(const char *(*)(struct archive_entry *))dlsym(
				libarchive_handle, "archive_entry_symlink");
		if (!dl_archive_entry_symlink)
			return 0;
		dl_archive_entry_xattr_count =
			(int (*)(struct archive_entry *))dlsym(
				libarchive_handle, "archive_entry_xattr_count");
		if (!dl_archive_entry_xattr_count)
			return 0;
		dl_archive_entry_xattr_next = (int (*)(
			struct archive_entry *, const char **, const void **,
			size_t *))dlsym(libarchive_handle,
					"archive_entry_xattr_next");
		if (!dl_archive_entry_xattr_next)
			return 0;
		dl_archive_entry_xattr_reset =
			(int (*)(struct archive_entry *))dlsym(
				libarchive_handle, "archive_entry_xattr_reset");
		if (!dl_archive_entry_xattr_reset)
			return 0;
		dl_archive_error_string =
			(const char *(*)(struct archive *))dlsym(
				libarchive_handle, "archive_error_string");
		if (!dl_archive_error_string)
			return 0;
		dl_archive_read_close = (int (*)(struct archive *))dlsym(
			libarchive_handle, "archive_read_close");
		if (!dl_archive_read_close)
			return 0;
		dl_archive_read_data =
			(la_ssize_t(*)(struct archive *, void *, size_t))dlsym(
				libarchive_handle, "archive_read_data");
		if (!dl_archive_read_data)
			return 0;
		dl_archive_read_free = (int (*)(struct archive *))dlsym(
			libarchive_handle, "archive_read_free");
		if (!dl_archive_read_free)
			return 0;
		dl_archive_read_new = (struct archive * (*)(void))
			dlsym(libarchive_handle, "archive_read_new");
		if (!dl_archive_read_new)
			return 0;
		dl_archive_read_next_header = (int (*)(struct archive *,
						       struct archive_entry **))
			dlsym(libarchive_handle, "archive_read_next_header");
		if (!dl_archive_read_next_header)
			return 0;
		dl_archive_read_open_filename =
			(int (*)(struct archive *, const char *filename,
				 size_t))dlsym(libarchive_handle,
					       "archive_read_open_filename");
		if (!dl_archive_read_open_filename)
			return 0;
		dl_archive_read_support_filter_all =
			(int (*)(struct archive *))dlsym(
				libarchive_handle,
				"archive_read_support_filter_all");
		if (!dl_archive_read_support_filter_all)
			return 0;
		dl_archive_read_support_format_all =
			(int (*)(struct archive *))dlsym(
				libarchive_handle,
				"archive_read_support_format_all");
		if (!dl_archive_read_support_format_all)
			return 0;
	}

	return 1;
}
#else
static int libarchive_available(void)
{
	dl_archive_entry_hardlink = archive_entry_hardlink;
	dl_archive_entry_pathname = archive_entry_pathname;
	dl_archive_entry_stat = archive_entry_stat;
	dl_archive_entry_symlink = archive_entry_symlink;
	dl_archive_entry_xattr_count = archive_entry_xattr_count;
	dl_archive_entry_xattr_next = archive_entry_xattr_next;
	dl_archive_entry_xattr_reset = archive_entry_xattr_reset;
	dl_archive_error_string = archive_error_string;
	dl_archive_read_close = archive_read_close;
	dl_archive_read_data = archive_read_data;
	dl_archive_read_free = archive_read_free;
	dl_archive_read_new = archive_read_new;
	dl_archive_read_next_header = archive_read_next_header;
	dl_archive_read_open_filename = archive_read_open_filename;
	dl_archive_read_support_filter_all = archive_read_support_filter_all;
	dl_archive_read_support_format_all = archive_read_support_format_all;

	return 1;
}
#endif

static errcode_t __find_path(ext2_filsys fs, ext2_ino_t root, const char *name,
			     ext2_ino_t *inode)
{
	errcode_t retval = 0;
	ext2_ino_t tmpino;
	char *p, *n, *n2 = strdup(name);

	if (n2 == NULL) {
		retval = errno;
		goto out;
	}
	n = n2;
	*inode = root;
	/* any number of leading slashes */
	while (*n == '/')
		n++;
	while (*n) {
		/* replace the next slash by a NULL, if any */
		if ((p = strchr(n, '/')))
			(*p) = 0;
		/* find the inode of the next component */
		retval = ext2fs_lookup(fs, *inode, n, strlen(n), 0, &tmpino);
		if (retval)
			goto out;
		*inode = tmpino;
		/* continue the search at the character after the slash */
		if (p)
			n = p + 1;
		else
			break;
	}

out:
	free(n2);
	return retval;
}

/* Rounds quantity up to a multiple of size. size should be a power of 2 */
static inline unsigned int __round_up(unsigned int quantity, unsigned int size)
{
	return (quantity + (size - 1)) & ~(size - 1);
}

static int remove_inode(ext2_filsys fs, ext2_ino_t ino)
{
	errcode_t ret = 0;
	struct ext2_inode_large inode;

	memset(&inode, 0, sizeof(inode));
	ret = ext2fs_read_inode_full(fs, ino, (struct ext2_inode *)&inode,
				     sizeof(inode));
	if (ret)
		goto out;

	switch (inode.i_links_count) {
	case 0:
		return 0; /* XXX: already done? */
	case 1:
		inode.i_links_count--;
		ext2fs_set_dtime(fs, EXT2_INODE(&inode));
		break;
	default:
		inode.i_links_count--;
	}

	if (inode.i_links_count)
		goto write_out;

	/* Nobody holds this file; free its blocks! */
	ret = ext2fs_free_ext_attr(fs, ino, &inode);
	if (ret)
		goto write_out;

	if (ext2fs_inode_has_valid_blocks2(fs, (struct ext2_inode *)&inode)) {
		ret = ext2fs_punch(fs, ino, (struct ext2_inode *)&inode, NULL,
				   0, ~0ULL);
		if (ret)
			goto write_out;
	}

	ext2fs_inode_alloc_stats2(fs, ino, -1, LINUX_S_ISDIR(inode.i_mode));

write_out:
	ret = ext2fs_write_inode_full(fs, ino, (struct ext2_inode *)&inode,
				      sizeof(inode));
	if (ret)
		goto out;
out:
	return ret;
}

static errcode_t copy_file_chunk_tar(ext2_filsys fs, struct archive *archive,
				     ext2_file_t e2_file, off_t start,
				     off_t end, char *buf, char *zerobuf)
{
	off_t off, bpos;
	ssize_t got, blen;
	unsigned int written;
	char *ptr;
	errcode_t err = 0;

	for (off = start; off < end; off += COPY_FILE_BUFLEN) {
		got = dl_archive_read_data(archive, buf, COPY_FILE_BUFLEN);
		if (got < 0) {
			err = errno;
			goto fail;
		}
		for (bpos = 0, ptr = buf; bpos < got; bpos += fs->blocksize) {
			blen = fs->blocksize;
			if (blen > got - bpos)
				blen = got - bpos;
			if (memcmp(ptr, zerobuf, blen) == 0) {
				ptr += blen;
				continue;
			}
			err = ext2fs_file_llseek(e2_file, off + bpos,
						 EXT2_SEEK_SET, NULL);
			if (err)
				goto fail;
			while (blen > 0) {
				err = ext2fs_file_write(e2_file, ptr, blen,
							&written);
				if (err)
					goto fail;
				if (written == 0) {
					err = EIO;
					goto fail;
				}
				blen -= written;
				ptr += written;
			}
		}
	}
fail:
	return err;
}
static errcode_t copy_file_tar(ext2_filsys fs, struct archive *archive,
			       const struct stat *statbuf, ext2_ino_t ino)
{
	ext2_file_t e2_file;
	char *buf = NULL, *zerobuf = NULL;
	errcode_t err, close_err;

	err = ext2fs_file_open(fs, ino, EXT2_FILE_WRITE, &e2_file);
	if (err)
		return err;

	err = ext2fs_get_mem(COPY_FILE_BUFLEN, &buf);
	if (err)
		goto out;

	err = ext2fs_get_memzero(fs->blocksize, &zerobuf);
	if (err)
		goto out;

	err = copy_file_chunk_tar(fs, archive, e2_file, 0, statbuf->st_size,
				  buf, zerobuf);
out:
	ext2fs_free_mem(&zerobuf);
	ext2fs_free_mem(&buf);
	close_err = ext2fs_file_close(e2_file);
	if (err == 0)
		err = close_err;
	return err;
}

static errcode_t do_write_internal_tar(ext2_filsys fs, ext2_ino_t cwd,
				       struct archive *archive,
				       const char *dest,
				       const struct stat *statbuf)
{
	ext2_ino_t newfile;
	errcode_t retval;
	struct ext2_inode inode;

	retval = ext2fs_new_inode(fs, cwd, 010755, 0, &newfile);
	if (retval)
		goto out;
#ifdef DEBUGFS
	printf("Allocated inode: %u\n", newfile);
#endif
	retval = ext2fs_link(fs, cwd, dest, newfile, EXT2_FT_REG_FILE);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		retval = ext2fs_expand_dir(fs, cwd);
		if (retval)
			goto out;
		retval = ext2fs_link(fs, cwd, dest, newfile, EXT2_FT_REG_FILE);
	}
	if (retval)
		goto out;
	if (ext2fs_test_inode_bitmap2(fs->inode_map, newfile))
		com_err(__func__, 0, "Warning: inode already set");
	ext2fs_inode_alloc_stats2(fs, newfile, 1, 0);
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = (statbuf->st_mode & ~S_IFMT) | LINUX_S_IFREG;
	inode.i_atime = inode.i_ctime = inode.i_mtime = fs->now ? fs->now :
								  time(0);
	inode.i_links_count = 1;
	retval = ext2fs_inode_size_set(fs, &inode, statbuf->st_size);
	if (retval)
		goto out;
	if (ext2fs_has_feature_inline_data(fs->super)) {
		inode.i_flags |= EXT4_INLINE_DATA_FL;
	} else if (ext2fs_has_feature_extents(fs->super)) {
		ext2_extent_handle_t handle;

		inode.i_flags &= ~EXT4_EXTENTS_FL;
		retval = ext2fs_extent_open2(fs, newfile, &inode, &handle);
		if (retval)
			goto out;
		ext2fs_extent_free(handle);
	}

	retval = ext2fs_write_new_inode(fs, newfile, &inode);
	if (retval)
		goto out;
	if (inode.i_flags & EXT4_INLINE_DATA_FL) {
		retval = ext2fs_inline_data_init(fs, newfile);
		if (retval)
			goto out;
	}
	if (LINUX_S_ISREG(inode.i_mode)) {
		retval = copy_file_tar(fs, archive, statbuf, newfile);
		if (retval)
			goto out;
	}
out:
	return retval;
}

static errcode_t set_inode_xattr_tar(ext2_filsys fs, ext2_ino_t ino,
				     struct archive_entry *entry)
{
	errcode_t retval, close_retval;
	struct ext2_xattr_handle *handle;
	ssize_t size;
	const char *name;
	const void *value;
	size_t value_size;

	if (no_copy_xattrs)
		return 0;

	size = dl_archive_entry_xattr_count(entry);
	if (size == 0)
		return 0;

	retval = ext2fs_xattrs_open(fs, ino, &handle);
	if (retval) {
		if (retval == EXT2_ET_MISSING_EA_FEATURE)
			return 0;
		com_err(__func__, retval, _("while opening inode %u"), ino);
		return retval;
	}

	retval = ext2fs_xattrs_read(handle);
	if (retval) {
		com_err(__func__, retval,
			_("while reading xattrs for inode %u"), ino);
		goto out;
	}

	dl_archive_entry_xattr_reset(entry);
	while (dl_archive_entry_xattr_next(entry, &name, &value, &value_size) ==
	       ARCHIVE_OK) {
		if (strcmp(name, "security.capability") != 0)
			continue;

		retval = ext2fs_xattr_set(handle, name, value, value_size);
		if (retval) {
			com_err(__func__, retval,
				_("while writing attribute \"%s\" to inode %u"),
				name, ino);
			break;
		}
	}
out:
	close_retval = ext2fs_xattrs_close(&handle);
	if (close_retval) {
		com_err(__func__, retval, _("while closing inode %u"), ino);
		retval = retval ? retval : close_retval;
	}
	return retval;
}

static errcode_t handle_entry(ext2_filsys fs, ext2_ino_t root_ino,
			      ext2_ino_t root, ext2_ino_t dirinode, char *name,
			      struct archive *a, struct archive_entry *entry,
			      const struct stat *st)
{
	errcode_t retval = 0;
	char *ln_target;
	ext2_ino_t tmpino;

	switch (st->st_mode & S_IFMT) {
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		retval = do_mknod_internal(fs, dirinode, name, st->st_mode,
					   st->st_rdev);
		if (retval) {
			com_err(__func__, retval,
				_("while creating special file "
				  "\"%s\""),
				name);
			return 1;
		}
		break;
	case S_IFLNK:
		ln_target = calloc(
			1, __round_up(strlen(dl_archive_entry_symlink(entry)),
				      1024));
		strcpy(ln_target, dl_archive_entry_symlink(entry));
		retval = do_symlink_internal(fs, dirinode, name, ln_target,
					     root);
		free(ln_target);
		if (retval) {
			com_err(__func__, retval,
				_("while writing symlink\"%s\""), name);
			return 1;
		}
		break;
	case S_IFREG:
		retval = do_write_internal_tar(fs, dirinode, a, name, st);
		if (retval) {
			com_err(__func__, retval,
				_("while writing file \"%s\""), name);
			return 1;
		}
		break;
	case S_IFDIR:
		retval = do_mkdir_internal(fs, dirinode, name, root);
		if (retval) {
			com_err(__func__, retval, _("while making dir \"%s\""),
				name);
			return 1;
		}
		break;
	default:
		if (dl_archive_entry_hardlink(entry) != NULL) {
			if ((retval = __find_path(
				     fs, root_ino,
				     dl_archive_entry_hardlink(entry),
				     &tmpino))) {
				com_err(__func__, retval,
					_("cannot find hardlink destination \"%s\" "
					  "to create \"%s\""),
					dl_archive_entry_hardlink(entry), name);
				return 1;
			}
			retval = add_link(fs, dirinode, tmpino, name);
			if (retval) {
				com_err(__func__, retval, "while linking %s",
					name);
				return 1;
			}
		} else {
			com_err(__func__, 0, _("ignoring entry \"%s\""),
				dl_archive_entry_pathname(entry));
		}
	}
	return 0;
}
#endif

errcode_t __populate_fs_from_tar(ext2_filsys fs, ext2_ino_t root_ino,
				 const char *source_tar, ext2_ino_t root,
				 struct hdlinks_s *hdlinks EXT2FS_ATTR((unused)),
				 struct file_info *target,
				 struct fs_ops_callbacks *fs_callbacks)
{
#ifndef HAVE_ARCHIVE_H
	com_err(__func__, 0,
		_("you need to compile e2fsprogs with libarchive to "
		  "be able to process tarballs"));
	return 1;
#else
	char *path2, *path3, *dir, *name;
	unsigned int dir_exists;
	struct archive *a;
	struct archive_entry *entry;
	errcode_t retval = 0;
	locale_t archive_locale;
	locale_t old_locale;
	ext2_ino_t dirinode, tmpino;
	const struct stat *st;

	if (!libarchive_available()) {
		com_err(__func__, 0,
			_("you need libarchive to be able to process tarballs"));
		return 1;
	}

	archive_locale = newlocale(LC_CTYPE_MASK, "", (locale_t)0);
	old_locale = uselocale(archive_locale);
	a = dl_archive_read_new();
	if (a == NULL) {
		retval = 1;
		com_err(__func__, retval, _("while creating archive reader"));
		goto out;
	}
	if (dl_archive_read_support_filter_all(a) != ARCHIVE_OK) {
		retval = 1;
		com_err(__func__, retval, _("while enabling decompression"));
		goto out;
	}
	if (dl_archive_read_support_format_all(a) != ARCHIVE_OK) {
		retval = 1;
		com_err(__func__, retval, _("while enabling reader formats"));
		goto out;
	}

	if ((retval = dl_archive_read_open_filename(a, source_tar, 4096))) {
		com_err(__func__, retval, _("while opening \"%s\""),
			dl_archive_error_string(a));
		goto out;
	}

	for (;;) {
		retval = dl_archive_read_next_header(a, &entry);
		if (retval == ARCHIVE_EOF) {
			retval = 0;
			break;
		}
		if (retval != ARCHIVE_OK) {
			com_err(__func__, retval,
				_("cannot read archive header: \"%s\""),
				dl_archive_error_string(a));
			goto out;
		}
		path2 = strdup(dl_archive_entry_pathname(entry));
		path3 = strdup(dl_archive_entry_pathname(entry));
		name = basename(path2);
		dir = dirname(path3);
		if ((retval = __find_path(fs, root_ino, dir, &dirinode))) {
			com_err(__func__, retval,
				_("cannot find directory \"%s\" to create \"%s\""),
				dir, name);
			goto out;
		}

		/*
		 * Did we already create this file as the result of a repeated entry
		 * in the tarball? Delete the existing one (except if it is a
		 * directory) so that it can be re-created by handle_entry().
		 */
		dir_exists = 0;
		st = dl_archive_entry_stat(entry);
		retval = ext2fs_namei(fs, root, dirinode, name, &tmpino);
		if (!retval) {
			if ((st->st_mode & S_IFMT) == S_IFDIR) {
				dir_exists = 1;
			} else {
				retval = ext2fs_unlink(fs, dirinode, name,
						       tmpino, 0);
				if (retval) {
					com_err(__func__, retval,
						_("failed to unlink \"%s/%s\""),
						dir, name);
					goto out;
				}
				retval = remove_inode(fs, tmpino);
				if (retval) {
					com_err(__func__, retval,
						_("failed to remove inode of \"%s/%s\""),
						dir, name);
					goto out;
				}
			}
		}

		/*
		 * Create files, directories, symlinks etc referenced by this archive
		 * entry unless this is an already existing directory
		 */
		if (!dir_exists) {
			retval = handle_entry(fs, root_ino, root, dirinode,
					      name, a, entry, st);
			if (retval)
				goto out;
			retval =
				ext2fs_namei(fs, root, dirinode, name, &tmpino);
			if (retval) {
				com_err(__func__, retval,
					_("while looking up \"%s\""), name);
				goto out;
			}
		}

		/* set uid, gid, mode and time for the new (or re-created) inode */
		retval = set_inode_extra(fs, tmpino, st);
		if (retval) {
			com_err(__func__, retval,
				_("while setting inode for \"%s\""), name);
			goto out;
		}

		retval = set_inode_xattr_tar(fs, tmpino, entry);
		if (retval) {
			com_err(__func__, retval,
				_("while setting xattrs for \"%s\""), name);
			goto out;
		}

		if (fs_callbacks && fs_callbacks->end_create_new_inode) {
			retval = fs_callbacks->end_create_new_inode(
				fs, target->path, name, dirinode, root,
				st->st_mode & S_IFMT);
			if (retval)
				goto out;
		}

		free(path2);
		free(path3);
	}

out:
	dl_archive_read_close(a);
	dl_archive_read_free(a);
	uselocale(old_locale);
	freelocale(archive_locale);
	return retval;
#endif
}
