/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mkfs.bfs - Create SCO BFS filesystem - aeb, 1999-09-07
 *
 * Copyright (C) 1999 Andries E. Brouwe
 *
 * Usage: mkfs.bfs [-N nr-of-inodes] [-V volume-name] [-F fsname] device
 */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "blkdev.h"
#include "c.h"
#include "closestream.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"
#include "bitops.h"
#include "exitcodes.h"

#define BFS_ROOT_INO		2
#define BFS_NAMELEN		14
#define BFS_BLOCKSIZE		512
#define BFS_SUPER_MAGIC		0x1badface

/* superblock - 512 bytes */
struct bfssb {
	uint32_t s_magic;
	uint32_t s_start;	/* byte offset of start of data */
	uint32_t s_end;	/* sizeof(slice)-1 */

	/* for recovery during compaction */
	uint32_t s_from, s_to;	/* src and dest block of current transfer */
	int32_t s_backup_from, s_backup_to;

	/* labels - may well contain garbage */
	char s_fsname[6];
	char s_volume[6];
	char s_pad[472];
};

/* inode - 64 bytes */
struct bfsi {
	uint16_t i_ino;
	unsigned char i_pad1[2];
	uint32_t i_first_block;
	uint32_t i_last_block;
	uint32_t i_bytes_to_end;
	uint32_t i_type;	/* 1: file, 2: the unique dir */
	uint32_t i_mode;
	uint32_t i_uid, i_gid;
	uint32_t i_nlinks;
	uint32_t i_atime, i_mtime, i_ctime;
	unsigned char i_pad2[16];
};

#define BFS_DIR_TYPE	2

/* directory entry - 16 bytes */
struct bfsde {
	uint16_t d_ino;
	char d_name[BFS_NAMELEN];
};

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fprintf(out,
		_("Usage: %s [options] device [block-count]\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Make an SCO bfs filesystem.\n"), out);

	fprintf(out, _("\nOptions:\n"
		       " -N, --inodes=NUM    specify desired number of inodes\n"
		       " -V, --vname=NAME    specify volume name\n"
		       " -F, --fname=NAME    specify file system name\n"
		       " -v, --verbose       explain what is being done\n"
		       " -c                  this option is silently ignored\n"
		       " -l                  this option is silently ignored\n"
		       " --lock[=<mode>]     use exclusive device lock (yes, no or nonblock)\n"
		       ));
	fprintf(out, USAGE_HELP_OPTIONS(21));

	fprintf(out, USAGE_MAN_TAIL("mkfs.bfs(8)"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *device, *volume, *fsname;
	char *lockmode = 0;
	long inodes;
	unsigned long long total_blocks, ino_bytes, ino_blocks, data_blocks;
	unsigned long long user_specified_total_blocks = 0;
	int verbose = 0;
	int fd;
	uint32_t first_block;
	struct bfssb sb;
	struct bfsi ri;
	struct bfsde de;
	struct stat statbuf;
	time_t now;
	int c, i, len;

	enum {
	    VERSION_OPTION = CHAR_MAX + 1,
	    OPT_LOCK
	};
	static const struct option longopts[] = {
		{"inodes", required_argument, NULL, 'N'},
		{"vname", required_argument, NULL, 'V'},
		{"fname", required_argument, NULL, 'F'},
		{"verbose", no_argument, NULL, 'v'},
		{"version", no_argument, NULL, VERSION_OPTION},
		{"help", no_argument, NULL, 'h'},
		{"lock", optional_argument, NULL, OPT_LOCK},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	if (argc < 2) {
		warnx(_("not enough arguments"));
		errtryhelp(EXIT_FAILURE);
	}
	if (argc == 2 && !strcmp(argv[1], "-V"))
		print_version(EXIT_SUCCESS);

	volume = fsname = "      ";	/* is there a default? */
	inodes = 0;

	while ((c = getopt_long(argc, argv, "N:V:F:vhcl", longopts, NULL)) != -1) {
		switch (c) {
		case 'N':
			inodes = strtol_or_err(optarg, _("invalid number of inodes"));
			break;

		case 'V':
			len = strlen(optarg);
			if (len <= 0 || len > 6)
				errx(EXIT_FAILURE, _("volume name too long"));
			volume = xstrdup(optarg);
			break;

		case 'F':
			len = strlen(optarg);
			if (len <= 0 || len > 6)
				errx(EXIT_FAILURE, _("fsname name too long"));
			fsname = xstrdup(optarg);
			break;

		case 'v':
			verbose = 1;
			break;

		case 'c':
		case 'l':
			/* when called via mkfs we may get options c,l,v */
			break;

                case OPT_LOCK:
			lockmode = "1";
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				lockmode = optarg;
			}
			break;

		case VERSION_OPTION:
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind == argc) {
		warnx(_("no device specified"));
		errtryhelp(EXIT_FAILURE);
	}

	device = argv[optind++];

	if (stat(device, &statbuf) < 0)
		err(EXIT_FAILURE, _("stat of %s failed"), device);

	fd = open_blkdev_or_file(&statbuf, device, O_RDWR);
	if (fd < 0)
		err(EXIT_FAILURE, _("cannot open %s"), device);

	if (blkdev_lock(fd, device, lockmode) != 0)
		exit(MKFS_EX_ERROR);

	if (optind == argc - 1)
		user_specified_total_blocks =
			strtou64_or_err(argv[optind], _("invalid block-count"));
	else if (optind != argc) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	if (blkdev_get_sectors(fd, &total_blocks) == -1) {
		if (!user_specified_total_blocks)
			err(EXIT_FAILURE, _("cannot get size of %s"), device);
		total_blocks = user_specified_total_blocks;
	} else if (user_specified_total_blocks) {
		if (user_specified_total_blocks > total_blocks)
			errx(EXIT_FAILURE,
			     _("blocks argument too large, max is %llu"),
			     total_blocks);
		total_blocks = user_specified_total_blocks;
	}

	if (!inodes) {
		/* pick some reasonable default */
		inodes = 8 * (total_blocks / 800);
		if (inodes < 48)
			inodes = 48;
		if (512 < inodes)
			inodes = 512;
	} else {
		/* believe the user */
		if (512 < inodes)
			errx(EXIT_FAILURE, _("too many inodes - max is 512"));
	}

	ino_bytes = inodes * sizeof(struct bfsi);
	ino_blocks = (ino_bytes + BFS_BLOCKSIZE - 1) / BFS_BLOCKSIZE;
	data_blocks = total_blocks - ino_blocks - 1;

	/* mimic the behavior of SCO's mkfs - maybe this limit is needed */
	if (data_blocks < 32)
		errx(EXIT_FAILURE,
		     _("not enough space, need at least %llu blocks"),
		     ino_blocks + 33);

	memset(&sb, 0, sizeof(sb));
	sb.s_magic = cpu_to_le32(BFS_SUPER_MAGIC);
	sb.s_start = cpu_to_le32(ino_bytes + sizeof(struct bfssb));
	sb.s_end = cpu_to_le32(total_blocks * BFS_BLOCKSIZE - 1);
	sb.s_from = sb.s_to = sb.s_backup_from = sb.s_backup_to = -1;
	memcpy(sb.s_fsname, fsname, 6);
	memcpy(sb.s_volume, volume, 6);

	if (verbose) {
		fprintf(stderr, _("Device: %s\n"), device);
		fprintf(stderr, _("Volume: <%-6s>\n"), volume);
		fprintf(stderr, _("FSname: <%-6s>\n"), fsname);
		fprintf(stderr, _("BlockSize: %d\n"), BFS_BLOCKSIZE);
		if (ino_blocks == 1)
			fprintf(stderr, _("Inodes: %ld (in 1 block)\n"),
				inodes);
		else
			fprintf(stderr, _("Inodes: %ld (in %llu blocks)\n"),
				inodes, ino_blocks);
		fprintf(stderr, _("Blocks: %llu\n"), total_blocks);
		fprintf(stderr, _("Inode end: %d, Data end: %d\n"),
			le32_to_cpu(sb.s_start) - 1, le32_to_cpu(sb.s_end));
	}

	if (write(fd, &sb, sizeof(sb)) != sizeof(sb))
		err(EXIT_FAILURE, _("error writing superblock"));

	memset(&ri, 0, sizeof(ri));
	ri.i_ino = cpu_to_le16(BFS_ROOT_INO);
	first_block = 1 + ino_blocks;
	ri.i_first_block = cpu_to_le32(first_block);
	ri.i_last_block = cpu_to_le32(first_block +
	    (inodes * sizeof(de) - 1) / BFS_BLOCKSIZE);
	ri.i_bytes_to_end = cpu_to_le32(first_block * BFS_BLOCKSIZE
	    + 2 * sizeof(struct bfsde) - 1);
	ri.i_type = cpu_to_le32(BFS_DIR_TYPE);
	ri.i_mode = cpu_to_le32(S_IFDIR | 0755);	/* or just 0755 */
	ri.i_uid = cpu_to_le32(0);
	ri.i_gid = cpu_to_le32(1);			/* random */
	ri.i_nlinks = 2;
	time(&now);
	ri.i_atime = cpu_to_le32(now);
	ri.i_mtime = cpu_to_le32(now);
	ri.i_ctime = cpu_to_le32(now);

	if (write(fd, &ri, sizeof(ri)) != sizeof(ri))
		err(EXIT_FAILURE, _("error writing root inode"));

	memset(&ri, 0, sizeof(ri));
	for (i = 1; i < inodes; i++)
		if (write(fd, &ri, sizeof(ri)) != sizeof(ri))
			err(EXIT_FAILURE, _("error writing inode"));

	if (lseek(fd, (1 + ino_blocks) * BFS_BLOCKSIZE, SEEK_SET) == -1)
		err(EXIT_FAILURE, _("seek error"));

	memset(&de, 0, sizeof(de));
	de.d_ino = cpu_to_le16(BFS_ROOT_INO);
	memcpy(de.d_name, ".", 1);
	if (write(fd, &de, sizeof(de)) != sizeof(de))
		err(EXIT_FAILURE, _("error writing . entry"));

	memcpy(de.d_name, "..", 2);
	if (write(fd, &de, sizeof(de)) != sizeof(de))
		err(EXIT_FAILURE, _("error writing .. entry"));

	if (close_fd(fd) != 0)
		err(EXIT_FAILURE, _("error closing %s"), device);

	return EXIT_SUCCESS;
}
