/*
 * switchroot.c - switch to new root directory and start init.
 *
 * Copyright 2002-2009 Red Hat, Inc.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Peter Jones <pjones@redhat.com>
 *	Jeremy Katz <katzj@redhat.com>
 */
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <getopt.h>

#include "c.h"
#include "nls.h"
#include "closestream.h"
#include "statfs_magic.h"

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#ifndef MNT_DETACH
#define MNT_DETACH       0x00000002	/* Just detach from the tree */
#endif

/* remove all files/directories below dirName -- don't cross mountpoints */
static int recursiveRemove(int fd)
{
	struct stat rb;
	DIR *dir;
	int rc = -1;
	int dfd;

	if (!(dir = fdopendir(fd))) {
		warn(_("failed to open directory"));
		goto done;
	}

	/* fdopendir() precludes us from continuing to use the input fd */
	dfd = dirfd(dir);
	if (fstat(dfd, &rb)) {
		warn(_("stat failed"));
		goto done;
	}

	while(1) {
		struct dirent *d;
		int isdir = 0;

		errno = 0;
		if (!(d = readdir(dir))) {
			if (errno) {
				warn(_("failed to read directory"));
				goto done;
			}
			break;	/* end of directory */
		}

		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
#ifdef _DIRENT_HAVE_D_TYPE
		if (d->d_type == DT_DIR || d->d_type == DT_UNKNOWN)
#endif
		{
			struct stat sb;

			if (fstatat(dfd, d->d_name, &sb, AT_SYMLINK_NOFOLLOW)) {
				warn(_("stat of %s failed"), d->d_name);
				continue;
			}

			/* skip if device is not the same */
			if (sb.st_dev != rb.st_dev)
				continue;

			/* remove subdirectories */
			if (S_ISDIR(sb.st_mode)) {
				int cfd;

				cfd = openat(dfd, d->d_name, O_RDONLY);
				if (cfd >= 0)
					recursiveRemove(cfd);	/* it closes cfd too */
				isdir = 1;
			}
		}

		if (unlinkat(dfd, d->d_name, isdir ? AT_REMOVEDIR : 0))
			warn(_("failed to unlink %s"), d->d_name);
	}

	rc = 0;	/* success */
done:
	if (dir)
		closedir(dir);
	else
		close(fd);
	return rc;
}

static int switchroot(const char *newroot)
{
	/*  Don't try to unmount the old "/", there's no way to do it. */
	const char *umounts[] = { "/dev", "/proc", "/sys", "/run", NULL };
	int i;
	int cfd = -1;
	struct stat newroot_stat, oldroot_stat, sb;

	if (stat("/", &oldroot_stat) != 0) {
		warn(_("stat of %s failed"), "/");
		return -1;
	}

	if (stat(newroot, &newroot_stat) != 0) {
		warn(_("stat of %s failed"), newroot);
		return -1;
	}

	for (i = 0; umounts[i] != NULL; i++) {
		char newmount[PATH_MAX];

		snprintf(newmount, sizeof(newmount), "%s%s", newroot, umounts[i]);

		if ((stat(umounts[i], &sb) == 0) && sb.st_dev == oldroot_stat.st_dev) {
			/* mount point to move seems to be a normal directory or stat failed */
			continue;
		}

		if ((stat(newmount, &sb) != 0) || (sb.st_dev != newroot_stat.st_dev)) {
			/* mount point seems to be mounted already or stat failed */
			umount2(umounts[i], MNT_DETACH);
			continue;
		}

		if (mount(umounts[i], newmount, NULL, MS_MOVE, NULL) < 0) {
			warn(_("failed to mount moving %s to %s"),
				umounts[i], newmount);
			warnx(_("forcing unmount of %s"), umounts[i]);
			umount2(umounts[i], MNT_FORCE);
		}
	}

	if (chdir(newroot)) {
		warn(_("failed to change directory to %s"), newroot);
		return -1;
	}

	cfd = open("/", O_RDONLY);
	if (cfd < 0) {
		warn(_("cannot open %s"), "/");
		goto fail;
	}

	if (mount(newroot, "/", NULL, MS_MOVE, NULL) < 0) {
		warn(_("failed to mount moving %s to /"), newroot);
		goto fail;
	}

	if (chroot(".")) {
		warn(_("failed to change root"));
		goto fail;
	}

	if (chdir("/")) {
		warn(_("cannot change directory to %s"), "/");
		goto fail;
	}

	switch (fork()) {
	case 0: /* child */
	{
		struct statfs stfs;

		if (fstatfs(cfd, &stfs) == 0 &&
		    (F_TYPE_EQUAL(stfs.f_type, STATFS_RAMFS_MAGIC) ||
		     F_TYPE_EQUAL(stfs.f_type, STATFS_TMPFS_MAGIC)))
			recursiveRemove(cfd);
		else {
			warn(_("old root filesystem is not an initramfs"));
			close(cfd);
		}
		exit(EXIT_SUCCESS);
	}
	case -1: /* error */
		break;

	default: /* parent */
		close(cfd);
		return 0;
	}

fail:
	if (cfd >= 0)
		close(cfd);
	return -1;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *output = stdout;
	fputs(USAGE_HEADER, output);
	fprintf(output, _(" %s [options] <newrootdir> <init> <args to init>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, output);
	fputs(_("Switch to another filesystem as the root of the mount tree.\n"), output);

	fputs(USAGE_OPTIONS, output);
	fprintf(output, USAGE_HELP_OPTIONS(16));
	fprintf(output, USAGE_MAN_TAIL("switch_root(8)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	char *newroot, *init, **initargs;
	int c;
	static const struct option longopts[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "+Vh", longopts, NULL)) != -1)
		switch (c) {
		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	if (argc < 3) {
		warnx(_("not enough arguments"));
		errtryhelp(EXIT_FAILURE);
	}

	newroot = argv[1];
	init = argv[2];
	initargs = &argv[2];

	if (!*newroot || !*init) {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}

	if (switchroot(newroot))
		errx(EXIT_FAILURE, _("failed. Sorry."));

	if (access(init, X_OK))
		warn(_("cannot access %s"), init);

	execv(init, initargs);
	errexec(init);
}
