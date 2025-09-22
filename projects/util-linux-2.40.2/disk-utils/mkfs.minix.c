/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * mkfs.minix.c - make a linux (minix) file-system.
 *
 * (C) 1991 Linus Torvalds. This file may be redistributed as per
 * the Linux copyright.
*
 * DD.MM.YY
 *
 * 24.11.91  -	Time began. Used the fsck sources to get started.
 *
 * 25.11.91  -	Corrected some bugs. Added support for ".badblocks"
 *		The algorithm for ".badblocks" is a bit weird, but
 *		it should work. Oh, well.
 *
 * 25.01.92  -	Added the -l option for getting the list of bad blocks
 *		out of a named file. (Dave Rivers, rivers@ponds.uucp)
 *
 * 28.02.92  -	Added %-information when using -c.
 *
 * 28.02.93  -	Added support for other namelengths than the original
 *		14 characters so that I can test the new kernel routines..
 *
 * 09.10.93  -	Make exit status conform to that required by fsutil
 *		(Rik Faith, faith@cs.unc.edu)
 *
 * 31.10.93  -	Added inode request feature, for backup floppies: use
 *		32 inodes, for a news partition use more.
 *		(Scott Heavner, sdh@po.cwru.edu)
 *
 * 03.01.94  -	Added support for file system valid flag.
 *		(Dr. Wettstein, greg%wind.uucp@plains.nodak.edu)
 *
 * 30.10.94  -  Added support for v2 filesystem
 *		(Andreas Schwab, schwab@issan.informatik.uni-dortmund.de)
 *
 * 09.11.94  -	Added test to prevent overwrite of mounted fs adapted
 *		from Theodore Ts'o's (tytso@athena.mit.edu) mke2fs
 *		program.  (Daniel Quinlan, quinlan@yggdrasil.com)
 *
 * 03.20.95  -	Clear first 512 bytes of filesystem to make certain that
 *		the filesystem is not misidentified as a MS-DOS FAT filesystem.
 *		(Daniel Quinlan, quinlan@yggdrasil.com)
 *
 * 02.07.96  -  Added small patch from Russell King to make the program a
 *		good deal more portable (janl@math.uio.no)
 *
 * 06.29.11  -  Overall cleanups for util-linux and v3 support
 *              Davidlohr Bueso <dave@gnu.org>
 *
 * 06.20.15  -  Do not infinite loop or crash on large devices
 *              Joshua Hudson <joshudson@gmail.com>
 *
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/stat.h>
#include <getopt.h>
#include <err.h>

#include "blkdev.h"
#include "minix_programs.h"
#include "nls.h"
#include "pathnames.h"
#include "bitops.h"
#include "exitcodes.h"
#include "strutils.h"
#include "all-io.h"
#include "closestream.h"
#include "ismounted.h"

#define XALLOC_EXIT_CODE MKFS_EX_ERROR
#include "xalloc.h"

#define MINIX_ROOT_INO 1
#define MINIX_BAD_INO 2

#define TEST_BUFFER_BLOCKS 16
#define MAX_GOOD_BLOCKS 512

#define MINIX_MAX_INODES 65535

#define DEFAULT_FS_VERSION 1

/*
 * Global variables used in minix_programs.h inline functions
 */
int fs_version = DEFAULT_FS_VERSION;
char *super_block_buffer;

static char *inode_buffer = NULL;

#define Inode (((struct minix_inode *) inode_buffer) - 1)
#define Inode2 (((struct minix2_inode *) inode_buffer) - 1)

struct fs_control {
	char *device_name;		/* device on a Minix file system is created */
	int device_fd;			/* open file descriptor of the device */
	char *lockmode;			/* as specified by --lock */
	unsigned long long fs_blocks;	/* device block count for the file system */
	int fs_used_blocks;		/* used blocks on a device */
	int fs_bad_blocks;		/* number of bad blocks found from device */
	uint16_t fs_namelen;		/* maximum length of filenames */
	size_t fs_dirsize;		/* maximum size of directory */
	unsigned long fs_inodes;	/* number of inodes */
	int fs_magic;			/* file system magic number */
	unsigned int
	 check_blocks:1;		/* check for bad blocks */
};

static char root_block[MINIX_BLOCK_SIZE];
static char boot_block_buffer[512];
static unsigned short good_blocks_table[MAX_GOOD_BLOCKS];

static char *inode_map;
static char *zone_map;

#define zone_in_use(x) (isset(zone_map,(x)-get_first_zone()+1) != 0)

#define mark_inode(x) (setbit(inode_map,(x)))
#define unmark_inode(x) (clrbit(inode_map,(x)))

#define mark_zone(x) (setbit(zone_map,(x)-get_first_zone()+1))
#define unmark_zone(x) (clrbit(zone_map,(x)-get_first_zone()+1))

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] /dev/name [blocks]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -1                      use Minix version 1\n"), out);
	fputs(_(" -2, -v                  use Minix version 2\n"), out);
	fputs(_(" -3                      use Minix version 3\n"), out);
	fputs(_(" -n, --namelength <num>  maximum length of filenames\n"), out);
	fputs(_(" -i, --inodes <num>      number of inodes for the filesystem\n"), out);
	fputs(_(" -c, --check             check the device for bad blocks\n"), out);
	fputs(_(" -l, --badblocks <file>  list of bad blocks from file\n"), out);
	fprintf(out, _(
		"     --lock[=<mode>]     use exclusive device lock (%s, %s or %s)\n"), "yes", "no", "nonblock");
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(25));
	fprintf(out, USAGE_MAN_TAIL("mkfs.minix(8)"));
	exit(MKFS_EX_OK);
}

#ifdef TEST_SCRIPT
static inline time_t mkfs_minix_time(time_t *t)
{
	const char *str = getenv("MKFS_MINIX_TEST_SECOND_SINCE_EPOCH");
	uint64_t sec;

	if (str && sscanf(str, "%"SCNd64, &sec) == 1)
		return sec;
	return time(t);
}
#else				/* !TEST_SCRIPT */
# define mkfs_minix_time(x) time(x)
#endif

static void super_set_state(void)
{
	switch (fs_version) {
	case 1:
	case 2:
		Super.s_state |= MINIX_VALID_FS;
		Super.s_state &= ~MINIX_ERROR_FS;
		break;
	default: /* v3 */
		break;
	}
}

static void write_tables(const struct fs_control *ctl) {
	unsigned long imaps = get_nimaps();
	unsigned long zmaps = get_nzmaps();
	size_t buffsz = get_inode_buffer_size();

	/* Mark the super block valid. */
	super_set_state();

	if (lseek(ctl->device_fd, 0, SEEK_SET))
		err(MKFS_EX_ERROR, _("%s: seek to boot block failed "
				   " in write_tables"), ctl->device_name);
	if (write_all(ctl->device_fd, boot_block_buffer, 512))
		err(MKFS_EX_ERROR, _("%s: unable to clear boot sector"), ctl->device_name);
	if (MINIX_BLOCK_SIZE != lseek(ctl->device_fd, MINIX_BLOCK_SIZE, SEEK_SET))
		err(MKFS_EX_ERROR, _("%s: seek failed in write_tables"), ctl->device_name);

	if (write_all(ctl->device_fd, super_block_buffer, MINIX_BLOCK_SIZE))
		err(MKFS_EX_ERROR, _("%s: unable to write super-block"), ctl->device_name);

	if (write_all(ctl->device_fd, inode_map, imaps * MINIX_BLOCK_SIZE))
		err(MKFS_EX_ERROR, _("%s: unable to write inode map"), ctl->device_name);

	if (write_all(ctl->device_fd, zone_map, zmaps * MINIX_BLOCK_SIZE))
		err(MKFS_EX_ERROR, _("%s: unable to write zone map"), ctl->device_name);

	if (write_all(ctl->device_fd, inode_buffer, buffsz))
		err(MKFS_EX_ERROR, _("%s: unable to write inodes"), ctl->device_name);
}

static void write_block(const struct fs_control *ctl, int blk, char * buffer) {
	if (blk * MINIX_BLOCK_SIZE != lseek(ctl->device_fd, blk * MINIX_BLOCK_SIZE, SEEK_SET))
		errx(MKFS_EX_ERROR, _("%s: seek failed in write_block"), ctl->device_name);

	if (write_all(ctl->device_fd, buffer, MINIX_BLOCK_SIZE))
		errx(MKFS_EX_ERROR, _("%s: write failed in write_block"), ctl->device_name);
}

static int get_free_block(struct fs_control *ctl) {
	unsigned int blk;
	unsigned int zones = get_nzones();
	unsigned int first_zone = get_first_zone();

	if (ctl->fs_used_blocks + 1 >= MAX_GOOD_BLOCKS)
		errx(MKFS_EX_ERROR, _("%s: too many bad blocks"), ctl->device_name);
	if (ctl->fs_used_blocks)
		blk = good_blocks_table[ctl->fs_used_blocks - 1] + 1;
	else
		blk = first_zone;
	while (blk < zones && zone_in_use(blk))
		blk++;
	if (blk >= zones)
		errx(MKFS_EX_ERROR, _("%s: not enough good blocks"), ctl->device_name);
	good_blocks_table[ctl->fs_used_blocks] = blk;
	ctl->fs_used_blocks++;
	return blk;
}

static void mark_good_blocks(const struct fs_control *ctl) {
	int blk;

	for (blk=0 ; blk < ctl->fs_used_blocks ; blk++)
		mark_zone(good_blocks_table[blk]);
}

static inline int next(unsigned long zone) {
	unsigned long zones = get_nzones();
	unsigned long first_zone = get_first_zone();

	if (!zone)
		zone = first_zone-1;
	while (++zone < zones)
		if (zone_in_use(zone))
			return zone;
	return 0;
}

static void make_bad_inode_v1(struct fs_control *ctl)
{
	struct minix_inode * inode = &Inode[MINIX_BAD_INO];
	int i,j,zone;
	int ind=0,dind=0;
	unsigned short ind_block[MINIX_BLOCK_SIZE>>1];
	unsigned short dind_block[MINIX_BLOCK_SIZE>>1];

#define NEXT_BAD (zone = next(zone))

	if (!ctl->fs_bad_blocks)
		return;
	mark_inode(MINIX_BAD_INO);
	inode->i_nlinks = 1;
	inode->i_time = mkfs_minix_time(NULL);
	inode->i_mode = S_IFREG + 0000;
	inode->i_size = ctl->fs_bad_blocks * MINIX_BLOCK_SIZE;
	zone = next(0);
	for (i=0 ; i<7 ; i++) {
		inode->i_zone[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[7] = ind = get_free_block(ctl);
	memset(ind_block,0,MINIX_BLOCK_SIZE);
	for (i=0 ; i<512 ; i++) {
		ind_block[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[8] = dind = get_free_block(ctl);
	memset(dind_block,0,MINIX_BLOCK_SIZE);
	for (i=0 ; i<512 ; i++) {
		write_block(ctl, ind,(char *) ind_block);
		dind_block[i] = ind = get_free_block(ctl);
		memset(ind_block,0,MINIX_BLOCK_SIZE);
		for (j=0 ; j<512 ; j++) {
			ind_block[j] = zone;
			if (!NEXT_BAD)
				goto end_bad;
		}
	}
	errx(MKFS_EX_ERROR, _("%s: too many bad blocks"), ctl->device_name);
end_bad:
	if (ind)
		write_block(ctl, ind, (char *) ind_block);
	if (dind)
		write_block(ctl, dind, (char *) dind_block);
}

static void make_bad_inode_v2_v3 (struct fs_control *ctl)
{
	struct minix2_inode *inode = &Inode2[MINIX_BAD_INO];
	int i, j, zone;
	int ind = 0, dind = 0;
	unsigned long ind_block[MINIX_BLOCK_SIZE >> 2];
	unsigned long dind_block[MINIX_BLOCK_SIZE >> 2];

	if (!ctl->fs_bad_blocks)
		return;
	mark_inode (MINIX_BAD_INO);
	inode->i_nlinks = 1;
	inode->i_atime = inode->i_mtime = inode->i_ctime = mkfs_minix_time(NULL);
	inode->i_mode = S_IFREG + 0000;
	inode->i_size = ctl->fs_bad_blocks * MINIX_BLOCK_SIZE;
	zone = next (0);
	for (i = 0; i < 7; i++) {
		inode->i_zone[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[7] = ind = get_free_block (ctl);
	memset (ind_block, 0, MINIX_BLOCK_SIZE);
	for (i = 0; i < 256; i++) {
		ind_block[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[8] = dind = get_free_block (ctl);
	memset (dind_block, 0, MINIX_BLOCK_SIZE);
	for (i = 0; i < 256; i++) {
		write_block (ctl, ind, (char *) ind_block);
		dind_block[i] = ind = get_free_block (ctl);
		memset (ind_block, 0, MINIX_BLOCK_SIZE);
		for (j = 0; j < 256; j++) {
			ind_block[j] = zone;
			if (!NEXT_BAD)
				goto end_bad;
		}
	}
	/* Could make triple indirect block here */
	errx(MKFS_EX_ERROR, _("%s: too many bad blocks"), ctl->device_name);
 end_bad:
	if (ind)
		write_block (ctl, ind, (char *) ind_block);
	if (dind)
		write_block (ctl, dind, (char *) dind_block);
}

static void make_bad_inode(struct fs_control *ctl)
{
	if (fs_version < 2) {
		make_bad_inode_v1(ctl);
		return;
	}
	make_bad_inode_v2_v3(ctl);
}

static void make_root_inode_v1(struct fs_control *ctl) {
	struct minix_inode * inode = &Inode[MINIX_ROOT_INO];

	mark_inode(MINIX_ROOT_INO);
	inode->i_zone[0] = get_free_block(ctl);
	inode->i_nlinks = 2;
	inode->i_time = mkfs_minix_time(NULL);
	if (ctl->fs_bad_blocks)
		inode->i_size = 3 * ctl->fs_dirsize;
	else {
		memset(&root_block[2 * ctl->fs_dirsize], 0, ctl->fs_dirsize);
		inode->i_size = 2 * ctl->fs_dirsize;
	}
	inode->i_mode = S_IFDIR + 0755;
	inode->i_uid = getuid();
	if (inode->i_uid)
		inode->i_gid = getgid();
	write_block(ctl, inode->i_zone[0],root_block);
}

static void make_root_inode_v2_v3 (struct fs_control *ctl) {
	struct minix2_inode *inode = &Inode2[MINIX_ROOT_INO];

	mark_inode (MINIX_ROOT_INO);
	inode->i_zone[0] = get_free_block (ctl);
	inode->i_nlinks = 2;
	inode->i_atime = inode->i_mtime = inode->i_ctime = mkfs_minix_time(NULL);

	if (ctl->fs_bad_blocks)
		inode->i_size = 3 * ctl->fs_dirsize;
	else {
		memset(&root_block[2 * ctl->fs_dirsize], 0, ctl->fs_dirsize);
		inode->i_size = 2 * ctl->fs_dirsize;
	}

	inode->i_mode = S_IFDIR + 0755;
	inode->i_uid = getuid();
	if (inode->i_uid)
		inode->i_gid = getgid();
	write_block (ctl, inode->i_zone[0], root_block);
}

static void make_root_inode(struct fs_control *ctl)
{
	char *tmp = root_block;

	if (fs_version == 3) {
		*(uint32_t *) tmp = 1;
		strcpy(tmp + 4, ".");
		tmp += ctl->fs_dirsize;
		*(uint32_t *) tmp = 1;
		strcpy(tmp + 4, "..");
		tmp += ctl->fs_dirsize;
		*(uint32_t *) tmp = 2;
		strcpy(tmp + 4, ".badblocks");
	} else {
		*(uint16_t *) tmp = 1;
		strcpy(tmp + 2, ".");
		tmp += ctl->fs_dirsize;
		*(uint16_t *) tmp = 1;
		strcpy(tmp + 2, "..");
		tmp += ctl->fs_dirsize;
		*(uint16_t *) tmp = 2;
		strcpy(tmp + 2, ".badblocks");
	}
	if (fs_version < 2) {
		make_root_inode_v1(ctl);
		return;
	}
	make_root_inode_v2_v3(ctl);
}

static void super_set_nzones(const struct fs_control *ctl)
{
	switch (fs_version) {
	case 3:
		Super3.s_zones = ctl->fs_blocks;
		break;
	case 2:
		Super.s_zones = ctl->fs_blocks;
		break;
	default: /* v1 */
		Super.s_nzones = ctl->fs_blocks;
		break;
	}
}

static void super_init_maxsize(void)
{
	switch (fs_version) {
	case 3:
		Super3.s_max_size = 2147483647L;
		break;
	case 2:
		Super.s_max_size =  0x7fffffff;
		break;
	default: /* v1 */
		Super.s_max_size = (7+512+512*512)*1024;
		break;
	}
}

static void super_set_map_blocks(const struct fs_control *ctl, unsigned long inodes)
{
	switch (fs_version) {
	case 3:
		Super3.s_imap_blocks = UPPER(inodes + 1, BITS_PER_BLOCK);
		Super3.s_zmap_blocks = UPPER(ctl->fs_blocks - (1 + get_nimaps() + inode_blocks()),
					     BITS_PER_BLOCK + 1);
		Super3.s_firstdatazone = first_zone_data();
		break;
	default:
		Super.s_imap_blocks = UPPER(inodes + 1, BITS_PER_BLOCK);
		Super.s_zmap_blocks = UPPER(ctl->fs_blocks - (1 + get_nimaps() + inode_blocks()),
					     BITS_PER_BLOCK + 1);
		Super.s_firstdatazone = first_zone_data();
		break;
	}
}

static void super_set_magic(const struct fs_control *ctl)
{
	switch (fs_version) {
	case 3:
		Super3.s_magic = ctl->fs_magic;
		break;
	default:
		Super.s_magic = ctl->fs_magic;
		break;
	}
}

static void setup_tables(const struct fs_control *ctl) {
	unsigned long inodes, zmaps, imaps, zones, i;

	super_block_buffer = xcalloc(1, MINIX_BLOCK_SIZE);

	memset(boot_block_buffer,0,512);
	super_set_magic(ctl);

	if (fs_version == 3) {
		Super3.s_log_zone_size = 0;
		Super3.s_blocksize = MINIX_BLOCK_SIZE;
	}
	else {
		Super.s_log_zone_size = 0;
	}

	super_init_maxsize();
	super_set_nzones(ctl);
	zones = get_nzones();

	/* some magic nrs: 1 inode / 3 blocks for smaller filesystems,
	 * for one inode / 16 blocks for large ones. mkfs will eventually
	 * crab about too far when getting close to the maximum size. */
	if (ctl->fs_inodes == 0)
		if (2048 * 1024 < ctl->fs_blocks)	/* 2GB */
			inodes = ctl->fs_blocks / 16;
		else if (512 * 1024 < ctl->fs_blocks)	/* 0.5GB */
			inodes = ctl->fs_blocks / 8;
		else
			inodes = ctl->fs_blocks / 3;
	else
		inodes = ctl->fs_inodes;
	/* Round up inode count to fill block size */
	if (fs_version == 2 || fs_version == 3)
		inodes = ((inodes + MINIX2_INODES_PER_BLOCK - 1) &
			  ~(MINIX2_INODES_PER_BLOCK - 1));
	else
		inodes = ((inodes + MINIX_INODES_PER_BLOCK - 1) &
			  ~(MINIX_INODES_PER_BLOCK - 1));

	if (fs_version == 3)
		Super3.s_ninodes = inodes;
	else {
		if (inodes > MINIX_MAX_INODES)
			inodes = MINIX_MAX_INODES;
		Super.s_ninodes = inodes;
	}
	super_set_map_blocks(ctl, inodes);
	if (MINIX_MAX_INODES < first_zone_data())
		errx(MKFS_EX_ERROR,
		     _("First data block at %jd, which is too far (max %d).\n"
		       "Try specifying fewer inodes by passing --inodes <num>"),
		     (intmax_t)first_zone_data(),
		     MINIX_MAX_INODES);
	imaps = get_nimaps();
	zmaps = get_nzmaps();

	inode_map = xmalloc(imaps * MINIX_BLOCK_SIZE);
	zone_map = xmalloc(zmaps * MINIX_BLOCK_SIZE);
	memset(inode_map,0xff,imaps * MINIX_BLOCK_SIZE);
	memset(zone_map,0xff,zmaps * MINIX_BLOCK_SIZE);

	for (i = get_first_zone() ; i<zones ; i++)
		unmark_zone(i);
	for (i = MINIX_ROOT_INO ; i<=inodes; i++)
		unmark_inode(i);

	inode_buffer = xmalloc(get_inode_buffer_size());
	memset(inode_buffer,0, get_inode_buffer_size());

	printf(P_("%lu inode\n", "%lu inodes\n", inodes), inodes);
	printf(P_("%lu block\n", "%lu blocks\n", zones), zones);
	printf(_("Firstdatazone=%jd (%jd)\n"),
		(intmax_t)get_first_zone(), (intmax_t)first_zone_data());
	printf(_("Zonesize=%zu\n"), (size_t) MINIX_BLOCK_SIZE << get_zone_size());
	printf(_("Maxsize=%zu\n\n"),get_max_size());
}

/*
 * Perform a test of a block; return the number of
 * blocks readable/writable.
 */
static size_t do_check(const struct fs_control *ctl, char * buffer, int try, unsigned int current_block) {
	ssize_t got;

	/* Seek to the correct loc. */
	if (lseek(ctl->device_fd, current_block * MINIX_BLOCK_SIZE, SEEK_SET) !=
		       current_block * MINIX_BLOCK_SIZE )
		err(MKFS_EX_ERROR, _("%s: seek failed during testing of blocks"),
				ctl->device_name);

	/* Try the read */
	got = read(ctl->device_fd, buffer, try * MINIX_BLOCK_SIZE);
	if (got < 0) got = 0;
	if (got & (MINIX_BLOCK_SIZE - 1 )) {
		printf(_("Weird values in do_check: probably bugs\n"));
	}
	got /= MINIX_BLOCK_SIZE;
	return got;
}

static unsigned int currently_testing = 0;

static void alarm_intr(int alnum __attribute__ ((__unused__))) {
	unsigned long zones = get_nzones();

	if (currently_testing >= zones)
		return;
	signal(SIGALRM,alarm_intr);
	alarm(5);
	if (!currently_testing)
		return;
	printf("%d ...", currently_testing);
	fflush(stdout);
}

static void check_blocks(struct fs_control *ctl) {
	size_t try, got;
	static char buffer[MINIX_BLOCK_SIZE * TEST_BUFFER_BLOCKS];
	unsigned long zones = get_nzones();
	unsigned long first_zone = get_first_zone();

	currently_testing=0;
	signal(SIGALRM,alarm_intr);
	alarm(5);
	while (currently_testing < zones) {
		if (lseek(ctl->device_fd, currently_testing * MINIX_BLOCK_SIZE,SEEK_SET) !=
		    currently_testing*MINIX_BLOCK_SIZE)
			errx(MKFS_EX_ERROR, _("%s: seek failed in check_blocks"),
					ctl->device_name);
		try = TEST_BUFFER_BLOCKS;
		if (currently_testing + try > zones)
			try = zones-currently_testing;
		got = do_check(ctl, buffer, try, currently_testing);
		currently_testing += got;
		if (got == try)
			continue;
		if (currently_testing < first_zone)
			errx(MKFS_EX_ERROR, _("%s: bad blocks before data-area: "
					"cannot make fs"), ctl->device_name);
		mark_zone(currently_testing);
		ctl->fs_bad_blocks++;
		currently_testing++;
	}
	if (ctl->fs_bad_blocks > 0)
		printf(P_("%d bad block\n", "%d bad blocks\n", ctl->fs_bad_blocks), ctl->fs_bad_blocks);
}

static void get_list_blocks(struct fs_control *ctl, char *filename) {
	FILE *listfile;
	unsigned long blockno;

	listfile = fopen(filename,"r");
	if (listfile == NULL)
		err(MKFS_EX_ERROR, _("%s: can't open file of bad blocks"),
				ctl->device_name);

	while (!feof(listfile)) {
		if (fscanf(listfile,"%lu\n", &blockno) != 1) {
			printf(_("badblock number input error on line %d\n"), ctl->fs_bad_blocks + 1);
			errx(MKFS_EX_ERROR, _("%s: cannot read badblocks file"),
					ctl->device_name);
		}
		mark_zone(blockno);
		ctl->fs_bad_blocks++;
	}
	fclose(listfile);

	if (ctl->fs_bad_blocks > 0)
		printf(P_("%d bad block\n", "%d bad blocks\n", ctl->fs_bad_blocks), ctl->fs_bad_blocks);
}

static int find_super_magic(const struct fs_control *ctl)
{
	switch (fs_version) {
	case 1:
		if (ctl->fs_namelen == 14)
			return MINIX_SUPER_MAGIC;
		return MINIX_SUPER_MAGIC2;
	case 2:
		if (ctl->fs_namelen == 14)
			return MINIX2_SUPER_MAGIC;
		return MINIX2_SUPER_MAGIC2;
	case 3:
		return MINIX3_SUPER_MAGIC;
	default:
		abort();
	}
}

static void determine_device_blocks(struct fs_control *ctl, const struct stat *statbuf)
{
	unsigned long long dev_blocks = 0;

	if (S_ISBLK(statbuf->st_mode)) {
		int sectorsize;

		if (blkdev_get_sector_size(ctl->device_fd, &sectorsize) == -1)
			sectorsize = DEFAULT_SECTOR_SIZE;	/* kernel < 2.3.3 */
		if (MINIX_BLOCK_SIZE < sectorsize)
			errx(MKFS_EX_ERROR, _("block size smaller than physical "
					      "sector size of %s"), ctl->device_name);
		if (blkdev_get_size(ctl->device_fd, &dev_blocks) == -1)
			errx(MKFS_EX_ERROR, _("cannot determine size of %s"), ctl->device_name);
		dev_blocks /= MINIX_BLOCK_SIZE;
	} else if (!S_ISBLK(statbuf->st_mode))
		dev_blocks = statbuf->st_size / MINIX_BLOCK_SIZE;
	if (!ctl->fs_blocks)
		ctl->fs_blocks = dev_blocks;
	else if (dev_blocks < ctl->fs_blocks)
		errx(MKFS_EX_ERROR,
		     _("%s: requested blocks (%llu) exceeds available (%llu) blocks\n"),
		     ctl->device_name, ctl->fs_blocks, dev_blocks);
	if (ctl->fs_blocks < 10)
		errx(MKFS_EX_ERROR, _("%s: number of blocks too small"), ctl->device_name);
	if (fs_version == 1 && ctl->fs_blocks > MINIX_MAX_INODES)
		ctl->fs_blocks = MINIX_MAX_INODES;
	if (ctl->fs_blocks > (4 + ((MINIX_MAX_INODES - 4) * BITS_PER_BLOCK)))
		ctl->fs_blocks = 4 + ((MINIX_MAX_INODES - 4) * BITS_PER_BLOCK);	/* Utter maximum: Clip. */
}

static void check_user_instructions(struct fs_control *ctl)
{
	switch (fs_version) {
	case 1:
	case 2:
		if (ctl->fs_namelen == 14 || ctl->fs_namelen == 30)
			ctl->fs_dirsize = ctl->fs_namelen + 2;
		else
			errx(MKFS_EX_ERROR, _("unsupported name length: %d"), ctl->fs_namelen);
		break;
	case 3:
		if (ctl->fs_namelen == 60)
			ctl->fs_dirsize = ctl->fs_namelen + 4;
		else
			errx(MKFS_EX_ERROR, _("unsupported name length: %d"), ctl->fs_namelen);
		break;
	default:
		errx(MKFS_EX_ERROR, _("unsupported minix file system version: %d"), fs_version);
	}
	ctl->fs_magic = find_super_magic(ctl);
}

int main(int argc, char ** argv)
{
	struct fs_control ctl = {
		.fs_namelen = 30,	/* keep in sync with DEFAULT_FS_VERSION */
		0
	};
	int i;
	struct stat statbuf;
	char * listfile = NULL;
	enum {
		OPT_LOCK = CHAR_MAX + 1
	};
	static const struct option longopts[] = {
		{"namelength", required_argument, NULL, 'n'},
		{"inodes", required_argument, NULL, 'i'},
		{"check", no_argument, NULL, 'c'},
		{"badblocks", required_argument, NULL, 'l'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"lock",optional_argument, NULL, OPT_LOCK},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	strutils_set_exitcode(MKFS_EX_USAGE);

	while ((i = getopt_long(argc, argv, "1v23n:i:cl:Vh", longopts, NULL)) != -1)
		switch (i) {
		case '1':
			fs_version = 1;
			break;
		case 'v': /* kept for backwards compatibility */
			warnx(_("-v is ambiguous, use '-2' instead"));
			/* fallthrough */
		case '2':
			fs_version = 2;
			break;
		case '3':
			fs_version = 3;
			ctl.fs_namelen = 60;
			break;
		case 'n':
			ctl.fs_namelen = strtou16_or_err(optarg,
					_("failed to parse maximum length of filenames"));
			break;
		case 'i':
			ctl.fs_inodes = strtoul_or_err(optarg,
					_("failed to parse number of inodes"));
			break;
		case 'c':
			ctl.check_blocks = 1;
			break;
		case 'l':
			listfile = optarg;
			break;
		case OPT_LOCK:
			ctl.lockmode = "1";
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				ctl.lockmode = optarg;
			}
			break;
		case 'V':
			print_version(MKFS_EX_OK);
		case 'h':
			usage();
		default:
			errtryhelp(MKFS_EX_USAGE);
		}
	argc -= optind;
	argv += optind;
	if (argc > 0) {
		ctl.device_name = argv[0];
		argc--;
		argv++;
	}
	if (argc > 0)
		ctl.fs_blocks = strtoul_or_err(argv[0], _("failed to parse number of blocks"));

	if (!ctl.device_name) {
		warnx(_("no device specified"));
		errtryhelp(MKFS_EX_USAGE);
	}
	check_user_instructions(&ctl);
	if (is_mounted(ctl.device_name))
		errx(MKFS_EX_ERROR, _("%s is mounted; will not make a filesystem here!"),
			ctl.device_name);
	if (stat(ctl.device_name, &statbuf) < 0)
		err(MKFS_EX_ERROR, _("stat of %s failed"), ctl.device_name);
	ctl.device_fd = open_blkdev_or_file(&statbuf, ctl.device_name, O_RDWR);
	if (ctl.device_fd < 0)
		err(MKFS_EX_ERROR, _("cannot open %s"), ctl.device_name);
	if (blkdev_lock(ctl.device_fd, ctl.device_name, ctl.lockmode) != 0)
		exit(MKFS_EX_ERROR);
	determine_device_blocks(&ctl, &statbuf);
	setup_tables(&ctl);
	if (ctl.check_blocks)
		check_blocks(&ctl);
	else if (listfile)
		get_list_blocks(&ctl, listfile);

	make_root_inode(&ctl);
	make_bad_inode(&ctl);

	mark_good_blocks(&ctl);
	write_tables(&ctl);
	if (close_fd(ctl.device_fd) != 0)
		err(MKFS_EX_ERROR, _("write failed"));

	return MKFS_EX_OK;
}
