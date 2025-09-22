/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2000 Andries Brouwer
 * Copyright (C) 2023 Karel Zak <kzak@redhat.com>
 *
 * use header info to find size of iso9660 file system
 * output a number - useful in scripts
 *
 * Synopsis:
 *    isosize [-x] [-d <num>] <filename>
 *        where "-x" gives length in sectors and sector size while
 *              without this argument the size is given in bytes
 *        without "-x" gives length in bytes unless "-d <num>" is
 *		given. In the latter case the length in bytes divided
 *		by <num> is given
 *
 *  Version 2.03 2000/12/21
 *     - add "-d <num>" option and use long long to fix things > 2 GB
 *  Version 2.02 2000/10/11
 *     - error messages on IO failures [D. Gilbert]
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "nls.h"
#include "c.h"
#include "strutils.h"
#include "closestream.h"
#include "iso9660.h"

#define ISOSIZE_EXIT_ALLFAILED	32
#define ISOSIZE_EXIT_SOMEOK	64

static int is_iso(int fd)
{
	char label[8];

	if (pread(fd, &label, 8, 0x8000) == -1)
		return 1;
	return memcmp(&label, &"\1CD001\1", 8);
}

static int isosize(int argc, char *filenamep, int xflag, long divisor)
{
	int fd, nsecs, ssize, rc = -1;
	unsigned char volume_space_size[8];
	unsigned char logical_block_size[4];

	if ((fd = open(filenamep, O_RDONLY)) < 0) {
		warn(_("cannot open %s"), filenamep);
		goto done;
	}
	if (is_iso(fd))
		warnx(_("%s: might not be an ISO filesystem"), filenamep);

	if (pread(fd, volume_space_size, sizeof(volume_space_size), 0x8050) != sizeof(volume_space_size) ||
	    pread(fd, logical_block_size, sizeof(logical_block_size), 0x8080) != sizeof(logical_block_size)) {
		if (errno)
			warn(_("read error on %s"), filenamep);
		else
			warnx(_("read error on %s"), filenamep);
		goto done;
	}

	nsecs = isonum_733(volume_space_size, xflag);
	/* isonum_723 returns nowadays always 2048 */
	ssize = isonum_723(logical_block_size, xflag);

	if (1 < argc)
		printf("%s: ", filenamep);
	if (xflag)
		printf(_("sector count: %d, sector size: %d\n"), nsecs, ssize);
	else {
		long long product = nsecs;

		if (divisor == 0)
			printf("%lld\n", product * ssize);
		else if (divisor == ssize)
			printf("%d\n", nsecs);
		else
			printf("%lld\n", (product * ssize) / divisor);
	}

	rc = 0;
done:
	if (fd >= 0)
		close(fd);
	return rc;
}

static void __attribute__((__noreturn__)) usage(void)
{

	fputs(USAGE_HEADER, stdout);
	fprintf(stdout,
		_(" %s [options] <iso9660_image_file> ...\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, stdout);
	fputs(_("Show the length of an ISO-9660 filesystem.\n"), stdout);

	fputs(USAGE_OPTIONS, stdout);
	fputs(_(" -d, --divisor=<number>  divide the amount of bytes by <number>\n"), stdout);
	fputs(_(" -x, --sectors           show sector count and size\n"), stdout);

	fprintf(stdout, USAGE_HELP_OPTIONS(25));
	fprintf(stdout, USAGE_MAN_TAIL("isosize(8)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int j, ct_err = 0, ct, opt, xflag = 0;
	long divisor = 0;

	static const struct option longopts[] = {
		{"divisor", required_argument, NULL, 'd'},
		{"sectors", no_argument,       NULL, 'x'},
		{"version", no_argument,       NULL, 'V'},
		{"help",    no_argument,       NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((opt = getopt_long(argc, argv, "d:xVh", longopts, NULL)) != -1) {
		switch (opt) {
		case 'd':
			divisor =
			    strtol_or_err(optarg,
					  _("invalid divisor argument"));
			break;
		case 'x':
			xflag = 1;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	ct = argc - optind;

	if (ct <= 0) {
		warnx(_("no device specified"));
		errtryhelp(EXIT_FAILURE);
	}

	for (j = optind; j < argc; j++) {
		if (isosize(ct, argv[j], xflag, divisor) != 0)
			ct_err++;
	}

	return	ct == ct_err	? ISOSIZE_EXIT_ALLFAILED :	/* all failed */
		ct_err		? ISOSIZE_EXIT_SOMEOK :		/* some ok */
		EXIT_SUCCESS;					/* all success */
}
