/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2012 Karel Zak <kzak@redhat.com>
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"
#include "partx.h"
#include "sysfs.h"
#include "strutils.h"
#include "closestream.h"

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s <disk device> <partition number> <length>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Tell the kernel about the new size of a partition.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fprintf(out, USAGE_HELP_OPTIONS(16));
	fprintf(out, USAGE_MAN_TAIL("resizepart(8)"));
	exit(EXIT_SUCCESS);
}

static int get_partition_start(int fd, int partno, uint64_t *start)
{
	struct stat st;
	struct path_cxt *disk = NULL, *part = NULL;
	dev_t devno = 0;
	int rc = -1;

	/*
	 * wholedisk
	 */
	if (fstat(fd, &st) || !S_ISBLK(st.st_mode))
		goto done;
	devno = st.st_rdev;
	disk = ul_new_sysfs_path(devno, NULL, NULL);
	if (!disk)
		goto done;
	/*
	 * partition
	 */
	devno = sysfs_blkdev_partno_to_devno(disk, partno);
	if (!devno)
		goto done;

	part = ul_new_sysfs_path(devno, disk, NULL);
	if (!part)
		goto done;
	if (ul_path_read_u64(part, start, "start"))
		goto done;

	rc = 0;
done:
	ul_unref_path(part);
	ul_unref_path(disk);
	return rc;
}

int main(int argc, char **argv)
{
	int c, fd, partno;
	const char *wholedisk;
	uint64_t start;

	static const struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, '0'},
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (c) {
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	if (argc != 4) {
		warnx(_("not enough arguments"));
		errtryhelp(EXIT_FAILURE);
	}

	wholedisk = argv[1];
	partno = strtou32_or_err(argv[2], _("invalid partition number argument"));

	if ((fd = open(wholedisk, O_RDONLY)) < 0)
		err(EXIT_FAILURE, _("cannot open %s"), wholedisk);

	if (get_partition_start(fd, partno, &start))
		err(EXIT_FAILURE, _("%s: failed to get start of the partition number %s"),
				wholedisk, argv[2]);

	if (partx_resize_partition(fd, partno, start,
			strtou64_or_err(argv[3], _("invalid length argument"))))
		err(EXIT_FAILURE, _("failed to resize partition"));

	if (close_fd(fd) != 0)
		err(EXIT_FAILURE, _("write failed"));

	return 0;
}
