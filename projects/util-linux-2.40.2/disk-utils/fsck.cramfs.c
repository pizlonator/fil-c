/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * cramfsck - check a cramfs file system
 *
 * Copyright (C) 2000-2002 Transmeta Corporation
 *               2005 Adrian Bunk
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * 1999/12/03: Linus Torvalds (cramfs tester and unarchive program)
 * 2000/06/03: Daniel Quinlan (CRC and length checking program)
 * 2000/06/04: Daniel Quinlan (merged programs, added options, support
 *                            for special files, preserve permissions and
 *                            ownership, cramfs superblock v2, bogus mode
 *                            test, pathname length test, etc.)
 * 2000/06/06: Daniel Quinlan (support for holes, pretty-printing,
 *                            symlink size test)
 * 2000/07/11: Daniel Quinlan (file length tests, start at offset 0 or 512,
 *                            fsck-compatible exit codes)
 * 2000/07/15: Daniel Quinlan (initial support for block devices)
 * 2002/01/10: Daniel Quinlan (additional checks, test more return codes,
 *                            use read if mmap fails, standardize messages)
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>

/* We don't use our include/crc32.h, but crc32 from zlib!
 *
 * The zlib implementation performs pre/post-conditioning. The util-linux
 * imlemenation requires post-conditioning (xor) in the applications.
 */
#include <zlib.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "c.h"
#include "cramfs.h"
#include "nls.h"
#include "blkdev.h"
#include "exitcodes.h"
#include "strutils.h"
#include "closestream.h"

#define XALLOC_EXIT_CODE FSCK_EX_ERROR
#include "xalloc.h"

static int fd;			/* ROM image file descriptor */
static char *filename;		/* ROM image filename */
static struct cramfs_super super;	/* just find the cramfs superblock once */
static int cramfs_is_big_endian = 0;	/* source is big endian */
static int opt_verbose = 0;	/* 1 = verbose (-v), 2+ = very verbose (-vv) */
static int opt_extract = 0;	/* extract cramfs (-x) */
static char *extract_dir = "";		/* optional extraction directory (-x) */

#define PAD_SIZE 512

static uid_t euid;		/* effective UID */

/* (cramfs_super + start) <= start_dir < end_dir <= start_data <= end_data */
static unsigned long start_dir = ~0UL;	/* start of first non-root inode */
static unsigned long end_dir = 0;	/* end of the directory structure */
static unsigned long start_data = ~0UL;	/* start of the data (256 MB = max) */
static unsigned long end_data = 0;	/* end of the data */


/* Guarantee access to at least 2 * blksize at a time */
#define CRAMFS_ROMBUFFER_BITS	13
#define CRAMFS_ROMBUFFERSIZE	(1 << CRAMFS_ROMBUFFER_BITS)
#define CRAMFS_ROMBUFFERMASK	(CRAMFS_ROMBUFFERSIZE - 1)

/* Defaults, updated in main() according to block size */
static size_t rombufbits = CRAMFS_ROMBUFFER_BITS;
static size_t rombufsize = CRAMFS_ROMBUFFERSIZE;
static size_t rombufmask = CRAMFS_ROMBUFFERMASK;

static char *read_buffer;
static unsigned long read_buffer_block = ~0UL;

static z_stream stream;

/* Prototypes */
static void expand_fs(char *, struct cramfs_inode *);

static char *outbuffer;

static size_t blksize = 0;

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out,
		_(" %s [options] <file>\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Check and repair a compressed ROM filesystem.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a                       for compatibility only, ignored\n"), out);
	fputs(_(" -v, --verbose            be more verbose\n"), out);
	fputs(_(" -y                       for compatibility only, ignored\n"), out);
	fputs(_(" -b, --blocksize <size>   use this blocksize, defaults to page size\n"), out);
	fputs(_("     --extract[=<dir>]    test uncompression, optionally extract into <dir>\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(26));

	fprintf(out, USAGE_MAN_TAIL("fsck.cramfs(8)"));
	exit(FSCK_EX_OK);
}

static int get_superblock_endianness(uint32_t magic)
{
	if (magic == CRAMFS_MAGIC) {
		cramfs_is_big_endian = HOST_IS_BIG_ENDIAN;
		return 0;
	}

	if (magic ==
		   u32_toggle_endianness(!HOST_IS_BIG_ENDIAN, CRAMFS_MAGIC)) {
		cramfs_is_big_endian = !HOST_IS_BIG_ENDIAN;
		return 0;
	}

	return -1;
}

static void test_super(int *start)
{
	struct stat st;
	unsigned long long length;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		err(FSCK_EX_ERROR, _("cannot open %s"), filename);

	/* find the physical size of the file or block device */
	if (fstat(fd, &st) < 0)
		err(FSCK_EX_ERROR, _("stat of %s failed"), filename);

	if (S_ISBLK(st.st_mode)) {
		if (blkdev_get_size(fd, &length))
			err(FSCK_EX_ERROR,
			    _("ioctl failed: unable to determine device size: %s"),
			    filename);
	} else if (S_ISREG(st.st_mode))
		length = st.st_size;
	else
		errx(FSCK_EX_ERROR, _("not a block device or file: %s"), filename);

	if (length < sizeof(struct cramfs_super))
		errx(FSCK_EX_UNCORRECTED, _("file length too short"));

	/* find superblock */
	if (read(fd, &super, sizeof(super)) != sizeof(super))
		err(FSCK_EX_ERROR, _("cannot read %s"), filename);
	if (get_superblock_endianness(super.magic) != -1)
		*start = 0;
	else if (length >= (PAD_SIZE + sizeof(super))) {
		if (lseek(fd, PAD_SIZE, SEEK_SET) == (off_t) -1)
			err(FSCK_EX_ERROR, _("seek on %s failed"), filename);
		if (read(fd, &super, sizeof(super)) != sizeof(super))
			err(FSCK_EX_ERROR, _("cannot read %s"), filename);
		if (get_superblock_endianness(super.magic) != -1)
			*start = PAD_SIZE;
		else
			errx(FSCK_EX_UNCORRECTED, _("superblock magic not found"));
	} else
		errx(FSCK_EX_UNCORRECTED, _("superblock magic not found"));

	if (opt_verbose)
		printf(_("cramfs endianness is %s\n"),
		       cramfs_is_big_endian ? _("big") : _("little"));

	super_toggle_endianness(cramfs_is_big_endian, &super);
	if (super.flags & ~CRAMFS_SUPPORTED_FLAGS)
		errx(FSCK_EX_ERROR, _("unsupported filesystem features"));

	/* What are valid superblock sizes? */
	if (super.size < *start + sizeof(struct cramfs_super))
		errx(FSCK_EX_UNCORRECTED, _("superblock size (%d) too small"),
		     super.size);

	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2) {
		if (super.fsid.files == 0)
			errx(FSCK_EX_UNCORRECTED, _("zero file count"));
		if (length < super.size)
			errx(FSCK_EX_UNCORRECTED, _("file length too short"));
		else if (length > super.size)
			warnx(_("file extends past end of filesystem"));
	} else
		warnx(_("old cramfs format"));
}

static void test_crc(int start)
{
	void *buf;
	uint32_t crc;

	if (!(super.flags & CRAMFS_FLAG_FSID_VERSION_2)) {
		warnx(_("unable to test CRC: old cramfs format"));
		return;
	}

	crc = crc32(0L, NULL, 0);

	buf =
	    mmap(NULL, super.size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (buf == MAP_FAILED) {
		buf =
		    mmap(NULL, super.size, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (buf != MAP_FAILED) {
			ssize_t tmp;
			if (lseek(fd, 0, SEEK_SET) == (off_t) -1)
				err(FSCK_EX_ERROR, _("seek on %s failed"), filename);
			tmp = read(fd, buf, super.size);
			if (tmp < 0)
				err(FSCK_EX_ERROR, _("cannot read %s"), filename);
			if (tmp != (ssize_t) super.size)
				errx(FSCK_EX_ERROR, _("failed to read %"PRIu32" bytes from file %s"),
					super.size, filename);
		}
	}
	if (buf != MAP_FAILED) {
		((struct cramfs_super *)((unsigned char *) buf + start))->fsid.crc =
		    crc32(0L, NULL, 0);
		crc = crc32(crc, (unsigned char *) buf + start, super.size - start);
		munmap(buf, super.size);
	} else {
		int retval;
		size_t length = 0;

		buf = xmalloc(4096);
		if (lseek(fd, start, SEEK_SET) == (off_t) -1)
			err(FSCK_EX_ERROR, _("seek on %s failed"), filename);
		for (;;) {
			retval = read(fd, buf, 4096);
			if (retval < 0)
				err(FSCK_EX_ERROR, _("cannot read %s"), filename);
			else if (retval == 0)
				break;
			if (length == 0)
				((struct cramfs_super *)buf)->fsid.crc =
				    crc32(0L, NULL, 0);
			length += retval;
			if (length > (super.size - start)) {
				crc = crc32(crc, buf,
					  retval - (length -
						    (super.size - start)));
				break;
			}
			crc = crc32(crc, buf, retval);
		}
		free(buf);
	}

	if (crc != super.fsid.crc)
		errx(FSCK_EX_UNCORRECTED, _("crc error"));
}

static void print_node(char type, struct cramfs_inode *i, char *name)
{
	char info[10];

	if (S_ISCHR(i->mode) || (S_ISBLK(i->mode)))
		/* major/minor numbers can be as high as 2^12 or 4096 */
		snprintf(info, 10, "%4d,%4d", major(i->size), minor(i->size));
	else
		/* size be as high as 2^24 or 16777216 */
		snprintf(info, 10, "%9d", i->size);

	printf("%c %04o %s %5d:%-3d %s\n",
	       type, i->mode & ~S_IFMT, info, i->uid, i->gid,
	       !*name && type == 'd' ? "/" : name);
}

/*
 * Create a fake "blocked" access
 */
static void *romfs_read(unsigned long offset)
{
	unsigned int block = offset >> rombufbits;
	if (block != read_buffer_block) {
		ssize_t x;

		read_buffer_block = block;
		if (lseek(fd, block << rombufbits, SEEK_SET) == (off_t) -1)
			warn(_("seek failed"));

		x = read(fd, read_buffer, rombufsize * 2);
		if (x < 0)
			warn(_("read romfs failed"));
	}
	return read_buffer + (offset & rombufmask);
}

static struct cramfs_inode *cramfs_iget(struct cramfs_inode *i)
{
	struct cramfs_inode *inode = xmalloc(sizeof(struct cramfs_inode));

	inode_to_host(cramfs_is_big_endian, i, inode);
	return inode;
}

static struct cramfs_inode *iget(unsigned int ino)
{
	return cramfs_iget(romfs_read(ino));
}

static void iput(struct cramfs_inode *inode)
{
	free(inode);
}

/*
 * Return the offset of the root directory
 */
static struct cramfs_inode *read_super(void)
{
	struct cramfs_inode *root = cramfs_iget(&super.root);
	unsigned long offset = root->offset << 2;

	if (!S_ISDIR(root->mode))
		errx(FSCK_EX_UNCORRECTED, _("root inode is not directory"));
	if (!(super.flags & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) &&
	    ((offset != sizeof(struct cramfs_super)) &&
	     (offset != PAD_SIZE + sizeof(struct cramfs_super)))) {
		errx(FSCK_EX_UNCORRECTED, _("bad root offset (%lu)"), offset);
	}
	return root;
}

static int uncompress_block(void *src, size_t len)
{
	int err;

	stream.next_in = src;
	stream.avail_in = len;

	stream.next_out = (unsigned char *)outbuffer;
	stream.avail_out = blksize * 2;

	inflateReset(&stream);

	if (len > blksize * 2)
		errx(FSCK_EX_UNCORRECTED, _("data block too large"));

	err = inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END)
		errx(FSCK_EX_UNCORRECTED, _("decompression error: %s"),
		     zError(err));
	return stream.total_out;
}

#ifndef HAVE_LCHOWN
#define lchown chown
#endif

static void do_uncompress(char *path, int outfd, unsigned long offset,
			  unsigned long size)
{
	unsigned long curr = offset + 4 * ((size + blksize - 1) / blksize);

	do {
		unsigned long out = blksize;
		unsigned long next = u32_toggle_endianness(cramfs_is_big_endian,
							   *(uint32_t *)
							   romfs_read(offset));

		if (next > end_data)
			end_data = next;

		offset += 4;
		if (curr == next) {
			if (opt_verbose > 1)
				printf(_("  hole at %lu (%zu)\n"), curr,
				       blksize);
			if (size < blksize)
				out = size;
			memset(outbuffer, 0x00, out);
		} else {
			if (opt_verbose > 1)
				printf(_("  uncompressing block at %lu to %lu (%lu)\n"),
				       curr, next, next - curr);
			out = uncompress_block(romfs_read(curr), next - curr);
		}
		if (size >= blksize) {
			if (out != blksize)
				errx(FSCK_EX_UNCORRECTED,
				     _("non-block (%ld) bytes"), out);
		} else {
			if (out != size)
				errx(FSCK_EX_UNCORRECTED,
				     _("non-size (%ld vs %ld) bytes"), out,
				     size);
		}
		size -= out;
		if (*extract_dir != '\0' && write(outfd, outbuffer, out) < 0)
			err(FSCK_EX_ERROR, _("write failed: %s"), path);
		curr = next;
	} while (size);
}
static void change_file_status(char *path, struct cramfs_inode *i)
{
	const struct timeval epoch[] = { {0,0}, {0,0} };

	if (euid == 0) {
		if (lchown(path, i->uid, i->gid) < 0)
			err(FSCK_EX_ERROR, _("lchown failed: %s"), path);
		if (S_ISLNK(i->mode))
			return;
		if (((S_ISUID | S_ISGID) & i->mode) && chmod(path, i->mode) < 0)
			err(FSCK_EX_ERROR, _("chmod failed: %s"), path);
	}
	if (S_ISLNK(i->mode))
		return;
	if (utimes(path, epoch) < 0)
		err(FSCK_EX_ERROR, _("utimes failed: %s"), path);
}

static int is_dangerous_filename(char *name, int len)
{
	return (len == 1 && name[0] == '.') ||
	       (len == 2 && name[0] == '.' && name[1] == '.');
}

static void __attribute__((__noreturn__))
	errx_path(const char *mesg, const char *name, size_t namelen)
{
	char buf[PATH_MAX] = { 0 };

	namelen = min(namelen, sizeof(buf) - 1);
	memcpy(buf, name, namelen);

	errx(FSCK_EX_UNCORRECTED, "%s: %s", mesg, buf);
}

static void do_directory(char *path, struct cramfs_inode *i)
{
	int pathlen = strlen(path);
	int count = i->size;
	unsigned long offset = i->offset << 2;
	char *newpath = xmalloc(pathlen + 256);

	if (offset == 0 && count != 0)
		errx(FSCK_EX_UNCORRECTED,
		     _("directory inode has zero offset and non-zero size: %s"),
		     path);

	if (offset != 0 && offset < start_dir)
		start_dir = offset;

	/* TODO: Do we need to check end_dir for empty case? */
	memcpy(newpath, path, pathlen);
	newpath[pathlen] = '/';
	pathlen++;
	if (opt_verbose)
		print_node('d', i, path);

	if (*extract_dir != '\0') {
		if (mkdir(path, i->mode) < 0)
			err(FSCK_EX_ERROR, _("mkdir failed: %s"), path);
		change_file_status(path, i);
	}
	while (count > 0) {
		struct cramfs_inode *child = iget(offset);
		char *name;
		int size;
		int newlen = child->namelen << 2;

		size = sizeof(struct cramfs_inode) + newlen;
		count -= size;

		offset += sizeof(struct cramfs_inode);
		name = romfs_read(offset);

		if (memchr(name, '/', newlen) != NULL)
			errx_path(_("illegal filename"), name, newlen);
		if (*extract_dir != '\0' && is_dangerous_filename(name, newlen))
			errx_path(_("dangerous filename"), name, newlen);
		memcpy(newpath + pathlen, name, newlen);
		newpath[pathlen + newlen] = 0;
		if (newlen == 0)
			errx(FSCK_EX_UNCORRECTED, _("filename length is zero"));
		if ((pathlen + newlen) - strlen(newpath) > 3)
			errx(FSCK_EX_UNCORRECTED, _("bad filename length"));
		expand_fs(newpath, child);

		offset += newlen;

		if (offset <= start_dir)
			errx(FSCK_EX_UNCORRECTED, _("bad inode offset"));
		if (offset > end_dir)
			end_dir = offset;
		iput(child);	/* free(child) */
	}
	free(newpath);
}

static void do_file(char *path, struct cramfs_inode *i)
{
	unsigned long offset = i->offset << 2;
	int outfd = 0;

	if (offset == 0 && i->size != 0)
		errx(FSCK_EX_UNCORRECTED,
		     _("file inode has zero offset and non-zero size"));
	if (i->size == 0 && offset != 0)
		errx(FSCK_EX_UNCORRECTED,
		     _("file inode has zero size and non-zero offset"));
	if (offset != 0 && offset < start_data)
		start_data = offset;
	if (opt_verbose)
		print_node('f', i, path);
	if (*extract_dir != '\0') {
		outfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, i->mode);
		if (outfd < 0)
			err(FSCK_EX_ERROR, _("cannot open %s"), path);
	}
	if (i->size)
		do_uncompress(path, outfd, offset, i->size);
	if ( *extract_dir != '\0') {
		if (close_fd(outfd) != 0)
			err(FSCK_EX_ERROR, _("write failed: %s"), path);
		change_file_status(path, i);
	}
}

static void do_symlink(char *path, struct cramfs_inode *i)
{
	unsigned long offset = i->offset << 2;
	unsigned long curr = offset + 4;
	unsigned long next =
	    u32_toggle_endianness(cramfs_is_big_endian,
				  *(uint32_t *) romfs_read(offset));
	unsigned long size;

	if (offset == 0)
		errx(FSCK_EX_UNCORRECTED, _("symbolic link has zero offset"));
	if (i->size == 0)
		errx(FSCK_EX_UNCORRECTED, _("symbolic link has zero size"));

	if (offset < start_data)
		start_data = offset;
	if (next > end_data)
		end_data = next;

	size = uncompress_block(romfs_read(curr), next - curr);
	if (size != i->size)
		errx(FSCK_EX_UNCORRECTED, _("size error in symlink: %s"), path);
	outbuffer[size] = 0;
	if (opt_verbose) {
		char *str;

		xasprintf(&str, "%s -> %s", path, outbuffer);
		print_node('l', i, str);
		if (opt_verbose > 1)
			printf(_("  uncompressing block at %lu to %lu (%lu)\n"),
			       curr, next, next - curr);
		free(str);
	}
	if (*extract_dir != '\0') {
		if (symlink(outbuffer, path) < 0)
			err(FSCK_EX_ERROR, _("symlink failed: %s"), path);
		change_file_status(path, i);
	}
}

static void do_special_inode(char *path, struct cramfs_inode *i)
{
	dev_t devtype = 0;
	char type;

	if (i->offset)
		/* no need to shift offset */
		errx(FSCK_EX_UNCORRECTED,
		     _("special file has non-zero offset: %s"), path);

	if (S_ISCHR(i->mode)) {
		devtype = i->size;
		type = 'c';
	} else if (S_ISBLK(i->mode)) {
		devtype = i->size;
		type = 'b';
	} else if (S_ISFIFO(i->mode)) {
		if (i->size != 0)
			errx(FSCK_EX_UNCORRECTED, _("fifo has non-zero size: %s"),
			     path);
		type = 'p';
	} else if (S_ISSOCK(i->mode)) {
		if (i->size != 0)
			errx(FSCK_EX_UNCORRECTED,
			     _("socket has non-zero size: %s"), path);
		type = 's';
	} else {
		errx(FSCK_EX_UNCORRECTED, _("bogus mode: %s (%o)"), path, i->mode);
		return;		/* not reached */
	}

	if (opt_verbose)
		print_node(type, i, path);

	if (*extract_dir != '\0') {
		if (mknod(path, i->mode, devtype) < 0)
			err(FSCK_EX_ERROR, _("mknod failed: %s"), path);
		change_file_status(path, i);
	}
}

static void expand_fs(char *path, struct cramfs_inode *inode)
{
	if (S_ISDIR(inode->mode))
		do_directory(path, inode);
	else if (S_ISREG(inode->mode))
		do_file(path, inode);
	else if (S_ISLNK(inode->mode))
		do_symlink(path, inode);
	else
		do_special_inode(path, inode);
}

static void test_fs(int start)
{
	struct cramfs_inode *root;

	root = read_super();
	umask(0);
	euid = geteuid();
	stream.next_in = NULL;
	stream.avail_in = 0;
	inflateInit(&stream);
	expand_fs(extract_dir, root);
	inflateEnd(&stream);
	if (start_data != ~0UL) {
		if (start_data < (sizeof(struct cramfs_super) + start))
			errx(FSCK_EX_UNCORRECTED,
			     _("directory data start (%lu) < sizeof(struct cramfs_super) + start (%zu)"),
			     start_data, sizeof(struct cramfs_super) + start);
		if (end_dir != start_data)
			errx(FSCK_EX_UNCORRECTED,
			     _("directory data end (%lu) != file data start (%lu)"),
			     end_dir, start_data);
	}
	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2 && end_data > super.size)
		errx(FSCK_EX_UNCORRECTED, _("invalid file data offset"));

	iput(root);		/* free(root) */
}

int main(int argc, char **argv)
{
	int c;			/* for getopt */
	int start = 0;

	static const struct option longopts[] = {
		{"verbose",   no_argument,       NULL, 'v'},
		{"version",   no_argument,       NULL, 'V'},
		{"help",      no_argument,       NULL, 'h'},
		{"blocksize", required_argument, NULL, 'b'},
		{"extract",   optional_argument, NULL, 'x'},
		{NULL, 0, NULL, 0},
	};

	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	strutils_set_exitcode(FSCK_EX_USAGE);

	/* command line options */
	while ((c = getopt_long(argc, argv, "ayvVhb:", longopts, NULL)) != EOF)
		switch (c) {
		case 'a':		/* ignore */
		case 'y':
			break;
		case 'h':
			usage();
			break;
		case 'V':
			print_version(FSCK_EX_OK);
		case 'x':
			opt_extract = 1;
			if(optarg)
				extract_dir = optarg;
			break;
		case 'v':
			opt_verbose++;
			break;
		case 'b':
			blksize = strtou32_or_err(optarg, _("invalid blocksize argument"));
			break;
		default:
			errtryhelp(FSCK_EX_USAGE);
		}

	if ((argc - optind) != 1){
		warnx(_("bad usage"));
		errtryhelp(FSCK_EX_USAGE);
	}
	filename = argv[optind];

	test_super(&start);
	test_crc(start);

	if (opt_extract) {
		size_t bufsize = 0;

		if (blksize == 0)
			blksize = getpagesize();

		/* re-calculate according to blksize */
		bufsize = rombufsize = blksize * 2;
		rombufbits = 0;
		while (bufsize >>= 1)
			rombufbits++;
		rombufmask = rombufsize - 1;

		outbuffer = xmalloc(blksize * 2);
		read_buffer = xmalloc(rombufsize * 2);
		test_fs(start);
	}

	if (opt_verbose)
		printf(_("%s: OK\n"), filename);

	exit(FSCK_EX_OK);
}
