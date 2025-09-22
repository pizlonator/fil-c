/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2012-2023 Karel Zak <kzak@redhat.com>
 *
 * Original implementation from Linux 0.99, without License and copyright in
 * the code. Karel Zak rewrote the code under GPL-2.0-or-later.
 */
#include <stdio.h>
#include <errno.h>
#include <getopt.h>

#ifdef HAVE_SYS_SWAP_H
# include <sys/swap.h>
#endif

#include "nls.h"
#include "c.h"
#include "xalloc.h"
#include "closestream.h"

#include "swapprober.h"
#include "swapon-common.h"

#if !defined(HAVE_SWAPOFF) && defined(SYS_swapoff)
# include <sys/syscall.h>
# define swapoff(path) syscall(SYS_swapoff, path)
#endif

static int verbose;
static int all;

#define QUIET	1
#define CANONIC	1

#define SWAPOFF_EX_OK		0	/* no errors */
#define SWAPOFF_EX_ENOMEM	2	/* swapoff(2) failed due to OOM */
#define SWAPOFF_EX_FAILURE	4	/* swapoff(2) failed due to another reason */
#define SWAPOFF_EX_SYSERR	8	/* non-swaoff() errors */
#define SWAPOFF_EX_USAGE	16	/* usage, permissions or syntax error */
#define SWAPOFF_EX_ALLERR	32	/* --all all failed */
#define SWAPOFF_EX_SOMEOK	64	/* --all some failed some OK */

/*
 * This function works like mnt_resolve_tag(), but it's able to read UUID/LABEL
 * from regular swap files too (according to entries in /proc/swaps). Note that
 * mnt_resolve_tag() and mnt_resolve_spec() works with system visible block
 * devices only.
 */
static char *swapoff_resolve_tag(const char *name, const char *value,
				 struct libmnt_cache *cache)
{
	char *path;
	struct libmnt_table *tb;
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;

	/* this is usual case for block devices (and it's really fast as it uses
	 * udev /dev/disk/by-* symlinks by default */
	path = mnt_resolve_tag(name, value, cache);
	if (path)
		return path;

	/* try regular files from /proc/swaps */
	tb = get_swaps();
	if (!tb)
		return NULL;

	itr = mnt_new_iter(MNT_ITER_BACKWARD);
	if (!itr)
		err(SWAPOFF_EX_SYSERR, _("failed to initialize libmount iterator"));

	while (tb && mnt_table_next_fs(tb, itr, &fs) == 0) {
		blkid_probe pr = NULL;
		const char *src = mnt_fs_get_source(fs);
		const char *type = mnt_fs_get_swaptype(fs);
		const char *data = NULL;

		if (!src || !type || strcmp(type, "file") != 0)
			continue;
		pr = get_swap_prober(src);
		if (!pr)
			continue;
		blkid_probe_lookup_value(pr, name, &data, NULL);
		if (data && strcmp(data, value) == 0)
			path = xstrdup(src);
		blkid_free_probe(pr);
		if (path)
			break;
	}

	mnt_free_iter(itr);
	return path;
}

static int do_swapoff(const char *orig_special, int quiet, int canonic)
{
        const char *special = orig_special;
	int rc = SWAPOFF_EX_OK;

	if (verbose)
		printf(_("swapoff %s\n"), orig_special);

	if (!canonic) {
		char *n, *v;

		special = mnt_resolve_spec(orig_special, mntcache);
		if (!special && blkid_parse_tag_string(orig_special, &n, &v) == 0) {
			special = swapoff_resolve_tag(n, v, mntcache);
			free(n);
			free(v);
		}
		if (!special)
			return cannot_find(orig_special);
	}

	if (swapoff(special) == 0)
		rc = SWAPOFF_EX_OK;	/* success */
	else {
		switch (errno) {
		case EPERM:
			errx(SWAPOFF_EX_USAGE, _("Not superuser."));
			break;
		case ENOMEM:
			warn(_("%s: swapoff failed"), orig_special);
			rc = SWAPOFF_EX_ENOMEM;
			break;
		default:
			if (!quiet)
				warn(_("%s: swapoff failed"), orig_special);
			rc = SWAPOFF_EX_FAILURE;
			break;
		}
	}

	return rc;
}

static int swapoff_by(const char *name, const char *value, int quiet)
{
	const char *special = swapoff_resolve_tag(name, value, mntcache);
	return special ? do_swapoff(special, quiet, CANONIC) : cannot_find(value);
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [<spec>]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Disable devices and files for paging and swapping.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --all              disable all swaps from /proc/swaps\n"
		" -v, --verbose          verbose mode\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(24));

	fputs(_("\nThe <spec> parameter:\n" \
		" -L <label>             LABEL of device to be used\n" \
		" -U <uuid>              UUID of device to be used\n" \
		" LABEL=<label>          LABEL of device to be used\n" \
		" UUID=<uuid>            UUID of device to be used\n" \
		" <device>               name of device to be used\n" \
		" <file>                 name of file to be used\n"), out);

	fprintf(out, USAGE_MAN_TAIL("swapoff(8)"));
	exit(SWAPOFF_EX_OK);
}

static int swapoff_all(void)
{
	int nerrs = 0, nsucc = 0;
	struct libmnt_table *tb;
	struct libmnt_fs *fs;
	struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_BACKWARD);

	if (!itr)
		err(SWAPOFF_EX_SYSERR, _("failed to initialize libmount iterator"));

	/*
	 * In case /proc/swaps exists, unswap stuff listed there.  We are quiet
	 * but report errors in status.  Errors might mean that /proc/swaps
	 * exists as ordinary file, not in procfs.  do_swapoff() exits
	 * immediately on EPERM.
	 */
	tb = get_swaps();

	while (tb && mnt_table_find_next_fs(tb, itr, match_swap, NULL, &fs) == 0) {
		if (do_swapoff(mnt_fs_get_source(fs), QUIET, CANONIC) == SWAPOFF_EX_OK)
			nsucc++;
		else
			nerrs++;
	}

	/*
	 * Unswap stuff mentioned in /etc/fstab.  Probably it was unmounted
	 * already, so errors are not bad.  Doing swapoff -a twice should not
	 * give error messages.
	 */
	tb = get_fstab(NULL);
	mnt_reset_iter(itr, MNT_ITER_FORWARD);

	while (tb && mnt_table_find_next_fs(tb, itr, match_swap, NULL, &fs) == 0) {
		if (!is_active_swap(mnt_fs_get_source(fs)))
			do_swapoff(mnt_fs_get_source(fs), QUIET, !CANONIC);
	}

	mnt_free_iter(itr);

	if (nerrs == 0)
		return SWAPOFF_EX_OK;		/* all success */
	else if (nsucc == 0)
		return SWAPOFF_EX_ALLERR;	/* all failed */

	return SWAPOFF_EX_SOMEOK;		/* some success, some failed */
}

int main(int argc, char *argv[])
{
	int status = 0, c;
	size_t i;

	static const struct option long_opts[] = {
		{ "all",     no_argument, NULL, 'a' },
		{ "help",    no_argument, NULL, 'h' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "ahvVL:U:",
				 long_opts, NULL)) != -1) {
		switch (c) {
		case 'a':		/* all */
			++all;
			break;
		case 'v':		/* be chatty */
			++verbose;
			break;
		case 'L':
			add_label(optarg);
			break;
		case 'U':
			add_uuid(optarg);
			break;

		case 'h':		/* help */
			usage();
		case 'V':		/* version */
			print_version(SWAPOFF_EX_OK);
		default:
			errtryhelp(SWAPOFF_EX_USAGE);
		}
	}
	argv += optind;

	if (!all && !numof_labels() && !numof_uuids() && *argv == NULL) {
		warnx(_("bad usage"));
		errtryhelp(SWAPOFF_EX_USAGE);
	}

	mnt_init_debug(0);
	mntcache = mnt_new_cache();

	for (i = 0; i < numof_labels(); i++)
		status |= swapoff_by("LABEL", get_label(i), !QUIET);

	for (i = 0; i < numof_uuids(); i++)
		status |= swapoff_by("UUID", get_uuid(i), !QUIET);

	while (*argv != NULL)
		status |= do_swapoff(*argv++, !QUIET, !CANONIC);

	if (all)
		status |= swapoff_all();

	free_tables();
	mnt_unref_cache(mntcache);

	return status;
}
