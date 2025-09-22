/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 1992 Werner Almesberger
 *
 * fdformat.c  -  Low-level formats a floppy disk - Werner Almesberger
 */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/fd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "c.h"
#include "blkdev.h"
#include "strutils.h"
#include "closestream.h"
#include "nls.h"
#include "xalloc.h"

#define SECTOR_SIZE 512

static struct floppy_struct param;


static void format_begin(int ctrl)
{
	if (ioctl(ctrl, FDFMTBEG, NULL) < 0)
		err(EXIT_FAILURE, "ioctl: FDFMTBEG");
}

static void format_end(int ctrl)
{
	if (ioctl(ctrl, FDFMTEND, NULL) < 0)
		err(EXIT_FAILURE, "ioctl: FDFMTEND");
}

static void format_track_head(int ctrl, struct format_descr *descr)
{
	if (ioctl(ctrl, FDFMTTRK, (long) descr) < 0)
		err(EXIT_FAILURE, "ioctl: FDFMTTRK");
}

static void seek_track_head(int ctrl, struct format_descr *descr)
{
	lseek(ctrl, ((off_t) descr->track * param.head + descr->head)
			* param.sect * SECTOR_SIZE, SEEK_SET);
}

static void format_disk(int ctrl, unsigned int track_from, unsigned int track_to)
{
	struct format_descr current;

	printf(_("Formatting ... "));
	fflush(stdout);

	format_begin(ctrl);

	for (current.track = track_from; current.track <= track_to; current.track++) {
		for (current.head = 0; current.head < param.head; current.head++) {
			printf("%3u/%u\b\b\b\b\b", current.track, current.head);
			fflush(stdout);
			format_track_head(ctrl, &current);
		}
	}

	format_end(ctrl);

	printf("     \b\b\b\b\b%s", _("done\n"));
}

static void verify_disk(int ctrl, unsigned int track_from, unsigned int track_to, unsigned int repair)
{
	unsigned char *data;
	struct format_descr current;
	int track_size, count;
	unsigned int retries_left;

	track_size = param.sect * SECTOR_SIZE;
	data = xmalloc(track_size);
	printf(_("Verifying ... "));
	fflush(stdout);

	current.track = track_from;
	current.head = 0;
	seek_track_head (ctrl, &current);

	for (current.track = track_from; current.track <= track_to; current.track++) {
		for (current.head = 0; current.head < param.head; current.head++) {
			int read_bytes;

			printf("%3u\b\b\b", current.track);
			fflush(stdout);

			retries_left = repair;
			do {
				read_bytes = read(ctrl, data, track_size);
				if (read_bytes != track_size) {
					if (retries_left) {
						format_begin(ctrl);
						format_track_head(ctrl, &current);
						format_end(ctrl);
						seek_track_head (ctrl, &current);
						retries_left--;
						if (retries_left)
							continue;
					}
					if (read_bytes < 0)
						perror(_("Read: "));
					fprintf(stderr,
						_("Problem reading track/head %u/%u,"
						  " expected %d, read %d\n"),
						current.track, current.head, track_size, read_bytes);
					free(data);
					exit(EXIT_FAILURE);
				}
				for (count = 0; count < track_size; count++)
					if (data[count] != FD_FILL_BYTE) {
						if (retries_left) {
							format_begin(ctrl);
							format_track_head(ctrl, &current);
							format_end(ctrl);
							seek_track_head (ctrl, &current);
							retries_left--;
							if (retries_left)
								continue;
						}
						printf(_("bad data in track/head %u/%u\n"
							 "Continuing ... "), current.track, current.head);
						fflush(stdout);
						break;
					}
				break;
			} while (retries_left);
		}
	}

	free(data);
	printf(_("done\n"));
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] <device>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Do a low-level formatting of a floppy disk.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -f, --from <N>    start at the track N (default 0)\n"), out);
	fputs(_(" -t, --to <N>      stop at the track N\n"), out);
	fputs(_(" -r, --repair <N>  try to repair tracks failed during\n"
                "                     the verification (max N retries)\n"), out);
	fputs(_(" -n, --no-verify   disable the verification after the format\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(19));
	fprintf(out, USAGE_MAN_TAIL("fdformat(8)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int ch;
	int ctrl;
	int verify = 1;
	unsigned int repair = 0;
	unsigned int track_from = 0;
	unsigned int track_to = 0;
	int has_user_defined_track_to = 0;
	struct stat st;

	static const struct option longopts[] = {
		{"from", required_argument, NULL, 'f'},
		{"to", required_argument, NULL, 't'},
		{"repair", required_argument, NULL, 'r'},
		{"no-verify", no_argument, NULL, 'n'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((ch = getopt_long(argc, argv, "f:t:r:nVh", longopts, NULL)) != -1)
		switch (ch) {
		case 'f':
			track_from = strtou32_or_err(optarg, _("invalid argument - from"));
			break;
		case 't':
			has_user_defined_track_to = 1;
			track_to = strtou32_or_err(optarg, _("invalid argument - to"));
			break;
		case 'r':
			repair = strtou32_or_err(optarg, _("invalid argument - repair"));
			break;
		case 'n':
			verify = 0;
			break;
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		warnx(_("no device specified"));
		errtryhelp(EXIT_FAILURE);
	}
	if (stat(argv[0], &st) < 0)
		err(EXIT_FAILURE, _("stat of %s failed"), argv[0]);
	if (!S_ISBLK(st.st_mode))
		/* do not test major - perhaps this was a USB floppy */
		errx(EXIT_FAILURE, _("%s: not a block device"), argv[0]);
	ctrl = open_blkdev_or_file(&st, argv[0], O_RDWR);
	if (ctrl < 0)
		err(EXIT_FAILURE, _("cannot open %s"), argv[0]);
	if (ioctl(ctrl, FDGETPRM, (long) &param) < 0)
		err(EXIT_FAILURE, _("could not determine current format type"));

	printf(_("%s-sided, %d tracks, %d sec/track. Total capacity %d kB.\n"),
		(param.head == 2) ? _("Double") : _("Single"),
		param.track, param.sect, param.size >> 1);

	if (!has_user_defined_track_to)
		track_to = param.track - 1;

	if (track_from >= param.track)
		err(EXIT_FAILURE, _("user defined start track exceeds the medium specific maximum"));
	if (track_to >= param.track)
		err(EXIT_FAILURE, _("user defined end track exceeds the medium specific maximum"));
	if (track_from > track_to)
		err(EXIT_FAILURE, _("user defined start track exceeds the user defined end track"));

	format_disk(ctrl, track_from, track_to);

	if (verify)
		verify_disk(ctrl, track_from, track_to, repair);

	if (close_fd(ctrl) != 0)
		err(EXIT_FAILURE, _("close failed"));

	return EXIT_SUCCESS;
}
