/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2011 Red Hat, Inc. All rights reserved.
 * Written by Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mountpoint(1) - see if a directory is a mountpoint
 *
 * This is libmount based reimplementation of the mountpoint(1)
 * from sysvinit project.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libmount.h>

#include "nls.h"
#include "xalloc.h"
#include "c.h"
#include "closestream.h"
#include "pathnames.h"

#define MOUNTPOINT_EXIT_NOMNT	32

struct mountpoint_control {
	char *path;
	dev_t dev;
	struct stat st;
	unsigned int
		dev_devno:1,
		fs_devno:1,
		nofollow:1,
		quiet:1;
};

static int dir_to_device(struct mountpoint_control *ctl)
{
	struct libmnt_table *tb = mnt_new_table_from_file(_PATH_PROC_MOUNTINFO);
	struct libmnt_fs *fs;
	struct libmnt_cache *cache;
	int rc = -1;

	if (!tb) {
		/*
		 * Fallback. Traditional way to detect mountpoints. This way
		 * is independent on /proc, but not able to detect bind mounts.
		 */
		struct stat pst;
		char buf[PATH_MAX], *cn;
		int len;

		cn = mnt_resolve_path(ctl->path, NULL);	/* canonicalize */

		len = snprintf(buf, sizeof(buf), "%s/..", cn ? cn : ctl->path);
		free(cn);

		if (len < 0 || (size_t) len >= sizeof(buf))
			return -1;
		if (stat(buf, &pst) !=0)
			return -1;

		if (ctl->st.st_dev != pst.st_dev || ctl->st.st_ino == pst.st_ino) {
			ctl->dev = ctl->st.st_dev;
			return 0;
		}

		return -1;
	}

	/* to canonicalize all necessary paths */
	cache = mnt_new_cache();
	mnt_table_set_cache(tb, cache);
	mnt_unref_cache(cache);

	fs = mnt_table_find_target(tb, ctl->path, MNT_ITER_BACKWARD);
	if (fs && mnt_fs_get_target(fs)) {
		ctl->dev = mnt_fs_get_devno(fs);
		rc = 0;
	}

	mnt_unref_table(tb);
	return rc;
}

static int print_devno(const struct mountpoint_control *ctl)
{
	if (!S_ISBLK(ctl->st.st_mode)) {
		if (!ctl->quiet)
			warnx(_("%s: not a block device"), ctl->path);
		return -1;
	}
	printf("%u:%u\n", major(ctl->st.st_rdev), minor(ctl->st.st_rdev));
	return 0;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %1$s [-qd] /path/to/directory\n"
		" %1$s -x /dev/device\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Check whether a directory or file is a mountpoint.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -q, --quiet        quiet mode - don't print anything\n"
		"     --nofollow     do not follow symlink\n"
		" -d, --fs-devno     print maj:min device number of the filesystem\n"
		" -x, --devno        print maj:min device number of the block device\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(20));
	fprintf(out, USAGE_MAN_TAIL("mountpoint(1)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int c;
	struct mountpoint_control ctl = { NULL };

	enum {
		OPT_NOFOLLOW = CHAR_MAX + 1
	};

	static const struct option longopts[] = {
		{ "quiet",    no_argument, NULL, 'q' },
		{ "nofollow", no_argument, NULL, OPT_NOFOLLOW },
		{ "fs-devno", no_argument, NULL, 'd' },
		{ "devno",    no_argument, NULL, 'x' },
		{ "help",     no_argument, NULL, 'h' },
		{ "version",  no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	mnt_init_debug(0);

	while ((c = getopt_long(argc, argv, "qdxhV", longopts, NULL)) != -1) {

		switch(c) {
		case 'q':
			ctl.quiet = 1;
			break;
		case OPT_NOFOLLOW:
			ctl.nofollow = 1;
			break;
		case 'd':
			ctl.fs_devno = 1;
			break;
		case 'x':
			ctl.dev_devno = 1;
			break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (optind + 1 != argc) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}
	if (ctl.nofollow && ctl.dev_devno)
		errx(EXIT_FAILURE, _("%s and %s are mutually exclusive"),
		     "--devno", "--nofollow");

	ctl.path = argv[optind];
	c = ctl.nofollow ? lstat(ctl.path, &ctl.st) : stat(ctl.path, &ctl.st);
	if (c) {
		if (!ctl.quiet)
			err(EXIT_FAILURE, "%s", ctl.path);
		return EXIT_FAILURE;
	}
	if (ctl.dev_devno)
		return print_devno(&ctl) ? MOUNTPOINT_EXIT_NOMNT : EXIT_SUCCESS;

	if ((ctl.nofollow && S_ISLNK(ctl.st.st_mode)) || dir_to_device(&ctl)) {
		if (!ctl.quiet)
			printf(_("%s is not a mountpoint\n"), ctl.path);
		return MOUNTPOINT_EXIT_NOMNT;
	}
	if (ctl.fs_devno)
		printf("%u:%u\n", major(ctl.dev), minor(ctl.dev));
	else if (!ctl.quiet)
		printf(_("%s is a mountpoint\n"), ctl.path);

	return EXIT_SUCCESS;
}
