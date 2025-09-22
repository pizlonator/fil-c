/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Original implementation from Ted Ts'o; losetup was part of mount.
 *
 * Copyright (C) 2011-2023 Karel Zak <kzak@redhat.com>
 *
 * losetup.c - setup and control loop devices
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <getopt.h>

#include <libsmartcols.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "loopdev.h"
#include "closestream.h"
#include "optutils.h"
#include "xalloc.h"
#include "canonicalize.h"
#include "pathnames.h"

enum {
	A_CREATE = 1,		/* setup a new device */
	A_DELETE,		/* delete given device(s) */
	A_DELETE_ALL,		/* delete all devices */
	A_SHOW,			/* list devices */
	A_SHOW_ONE,		/* print info about one device */
	A_FIND_FREE,		/* find first unused */
	A_SET_CAPACITY,		/* set device capacity */
	A_SET_DIRECT_IO,	/* set accessing backing file by direct io */
	A_SET_BLOCKSIZE,	/* set logical block size of the loop device */
};

enum {
	COL_NAME = 0,
	COL_AUTOCLR,
	COL_BACK_FILE,
	COL_BACK_INO,
	COL_BACK_MAJMIN,
	COL_BACK_MAJ,
	COL_BACK_MIN,
	COL_MAJMIN,
	COL_MAJ,
	COL_MIN,
	COL_OFFSET,
	COL_PARTSCAN,
	COL_REF,
	COL_RO,
	COL_SIZELIMIT,
	COL_DIO,
	COL_LOGSEC,
};

/* basic output flags */
static int no_headings;
static int raw;
static int json;

struct colinfo {
	const char * const name;
	double whint;
	int flags;
	const char *help;

	int json_type;	/* default is string */
};

static const struct colinfo infos[] = {
	[COL_AUTOCLR]     = { "AUTOCLEAR",    1, SCOLS_FL_RIGHT, N_("autoclear flag set"), SCOLS_JSON_BOOLEAN},
	[COL_BACK_FILE]   = { "BACK-FILE",  0.3, SCOLS_FL_NOEXTREMES, N_("device backing file")},
	[COL_BACK_INO]    = { "BACK-INO",     4, SCOLS_FL_RIGHT, N_("backing file inode number"), SCOLS_JSON_NUMBER},
	[COL_BACK_MAJMIN] = { "BACK-MAJ:MIN", 6, 0, N_("backing file major:minor device number")},
	[COL_BACK_MAJ]    = { "BACK-MAJ",     6, 0, N_("backing file major device number")},
	[COL_BACK_MIN]    = { "BACK-MIN",     6, 0, N_("backing file minor device number")},
	[COL_NAME]        = { "NAME",      0.25, 0, N_("loop device name")},
	[COL_OFFSET]      = { "OFFSET",       5, SCOLS_FL_RIGHT, N_("offset from the beginning"), SCOLS_JSON_NUMBER},
	[COL_PARTSCAN]    = { "PARTSCAN",     1, SCOLS_FL_RIGHT, N_("partscan flag set"), SCOLS_JSON_BOOLEAN},
	[COL_REF]         = { "REF",        0.1, 0, N_("loop device reference string")},
	[COL_RO]          = { "RO",           1, SCOLS_FL_RIGHT, N_("read-only device"), SCOLS_JSON_BOOLEAN},
	[COL_SIZELIMIT]   = { "SIZELIMIT",    5, SCOLS_FL_RIGHT, N_("size limit of the file in bytes"), SCOLS_JSON_NUMBER},
	[COL_MAJMIN]      = { "MAJ:MIN",      3, 0, N_("loop device major:minor number")},
	[COL_MAJ]         = { "MAJ",          1, SCOLS_FL_RIGHT, N_("loop device major number"), SCOLS_JSON_NUMBER},
	[COL_MIN]         = { "MIN",          1, SCOLS_FL_RIGHT, N_("loop device minor number"), SCOLS_JSON_NUMBER},
	[COL_DIO]         = { "DIO",          1, SCOLS_FL_RIGHT, N_("access backing file with direct-io"), SCOLS_JSON_BOOLEAN},
	[COL_LOGSEC]      = { "LOG-SEC",      4, SCOLS_FL_RIGHT, N_("logical sector size in bytes"), SCOLS_JSON_NUMBER},
};

static int columns[ARRAY_SIZE(infos) * 2] = {-1};
static size_t ncolumns;

static int get_column_id(int num)
{
	assert(num >= 0);
	assert((size_t) num < ncolumns);
	assert(columns[num] < (int) ARRAY_SIZE(infos));
	return columns[num];
}

static const struct colinfo *get_column_info(int num)
{
	return &infos[ get_column_id(num) ];
}

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

static int printf_loopdev(struct loopdev_cxt *lc)
{
	uint64_t x;
	dev_t dev = 0;
	ino_t ino = 0;
	char *fname;
	uint32_t type;

	fname = loopcxt_get_backing_file(lc);
	if (!fname)
		return -EINVAL;

	if (loopcxt_get_backing_devno(lc, &dev) == 0)
		loopcxt_get_backing_inode(lc, &ino);

	if (!dev && !ino) {
		/*
		 * Probably non-root user (no permissions to
		 * call LOOP_GET_STATUS ioctls).
		 */
		printf("%s%s: []: (%s)",
			loopcxt_get_device(lc),
			loopcxt_is_lost(lc) ? " (lost)" : "",
			fname);

		if (loopcxt_get_offset(lc, &x) == 0 && x)
				printf(_(", offset %ju"), x);
		if (loopcxt_get_sizelimit(lc, &x) == 0 && x)
				printf(_(", sizelimit %ju"), x);

		goto done;
	}

	printf("%s%s: [%04jd]:%ju (%s)",
		loopcxt_get_device(lc),
		loopcxt_is_lost(lc) ? " (lost)" : "",
		(intmax_t) dev, (uintmax_t) ino, fname);

	if (loopcxt_get_offset(lc, &x) == 0 && x)
			printf(_(", offset %ju"), x);
	if (loopcxt_get_sizelimit(lc, &x) == 0 && x)
			printf(_(", sizelimit %ju"), x);

	if (loopcxt_get_encrypt_type(lc, &type) == 0) {
		const char *e = loopcxt_get_crypt_name(lc);

		if ((!e || !*e) && type == 1)
			e = "XOR";
		if (e && *e)
			printf(_(", encryption %s (type %u)"), e, type);
	}

done:
	free(fname);
	printf("\n");
	return 0;
}

static int show_all_loops(struct loopdev_cxt *lc, const char *file,
			  uint64_t offset, int flags)
{
	struct stat sbuf, *st = &sbuf;
	char *cn_file = NULL;

	if (loopcxt_init_iterator(lc, LOOPITER_FL_USED))
		return -1;

	if (!file || stat(file, st))
		st = NULL;

	while (loopcxt_next(lc) == 0) {
		if (file) {
			int used;
			const char *bf = cn_file ? cn_file : file;

			used = loopcxt_is_used(lc, st, bf, offset, 0, flags);
			if (!used && !cn_file) {
				bf = cn_file = canonicalize_path(file);
				used = loopcxt_is_used(lc, st, bf, offset, 0, flags);
			}
			if (!used)
				continue;
		}
		printf_loopdev(lc);
	}
	loopcxt_deinit_iterator(lc);
	free(cn_file);
	return 0;
}

static void warn_lost(struct loopdev_cxt *lc)
{
	dev_t devno = loopcxt_get_devno(lc);

	if (devno <= 0)
		return;

	warnx(("device node %s (%u:%u) is lost. You may use mknod(1) to recover it."),
			loopcxt_get_device(lc), major(devno), minor(devno));
}

static int delete_loop(struct loopdev_cxt *lc)
{
	if (loopcxt_delete_device(lc)) {
		warn(_("%s: detach failed"), loopcxt_get_device(lc));
		if (loopcxt_is_lost(lc))
			warn_lost(lc);
	} else
		return 0;

	return -1;
}

static int delete_all_loops(struct loopdev_cxt *lc)
{
	int res = 0;

	if (loopcxt_init_iterator(lc, LOOPITER_FL_USED))
		return -1;

	while (loopcxt_next(lc) == 0)
		res += delete_loop(lc);

	loopcxt_deinit_iterator(lc);
	return res;
}

static int set_scols_data(struct loopdev_cxt *lc, struct libscols_line *ln)
{
	size_t i;

	for (i = 0; i < ncolumns; i++) {
		const char *p = NULL;			/* external data */
		char *np = NULL;			/* allocated here */
		uint64_t x = 0;
		int rc = 0;

		switch(get_column_id(i)) {
		case COL_NAME:
			p = loopcxt_get_device(lc);
			if (loopcxt_is_lost(lc)) {
				xasprintf(&np, "%s (lost)", p);
				p = NULL;
			}
			break;
		case COL_BACK_FILE:
			np = loopcxt_get_backing_file(lc);
			break;
		case COL_OFFSET:
			if (loopcxt_get_offset(lc, &x) == 0)
				xasprintf(&np, "%jd", x);
			break;
		case COL_SIZELIMIT:
			if (loopcxt_get_sizelimit(lc, &x) == 0)
				xasprintf(&np, "%jd", x);
			break;
		case COL_BACK_MAJMIN:
		{
			dev_t dev = 0;
			if (loopcxt_get_backing_devno(lc, &dev) == 0 && dev)
				xasprintf(&np, raw || json ? "%u:%u" : "%8u:%-3u",
						major(dev), minor(dev));
			break;
		}
		case COL_BACK_MAJ:
		{
			dev_t dev = 0;
			if (loopcxt_get_backing_devno(lc, &dev) == 0 && dev)
				xasprintf(&np, "%u", major(dev));
			break;
		}
		case COL_BACK_MIN:
		{
			dev_t dev = 0;
			if (loopcxt_get_backing_devno(lc, &dev) == 0 && dev)
				xasprintf(&np, "%u", minor(dev));
			break;
		}
		case COL_MAJMIN:
		{
			dev_t dev = loopcxt_get_devno(lc);
			if (dev)
				xasprintf(&np, raw || json ? "%u:%u" :"%3u:%-3u",
						major(dev), minor(dev));
			break;
		}
		case COL_MAJ: {
			dev_t dev = loopcxt_get_devno(lc);
			if (dev)
				xasprintf(&np, "%u", major(dev));
			break;
		}
		case COL_MIN: {
			dev_t dev = loopcxt_get_devno(lc);
			if (dev)
				xasprintf(&np, "%u", minor(dev));
			break;
		}
		case COL_BACK_INO:
		{
			ino_t ino = 0;
			if (loopcxt_get_backing_inode(lc, &ino) == 0 && ino)
				xasprintf(&np, "%ju", ino);
			break;
		}
		case COL_AUTOCLR:
			p = loopcxt_is_autoclear(lc) ? "1" : "0";
			break;
		case COL_RO:
			p = loopcxt_is_readonly(lc) ? "1" : "0";
			break;
		case COL_DIO:
			p = loopcxt_is_dio(lc) ? "1" : "0";
			break;
		case COL_PARTSCAN:
			p = loopcxt_is_partscan(lc) ? "1" : "0";
			break;
		case COL_LOGSEC:
			if (loopcxt_get_blocksize(lc, &x) == 0)
				xasprintf(&np, "%jd", x);
			break;
		case COL_REF:
			np = loopcxt_get_refname(lc);
			break;
		default:
			return -EINVAL;
		}


		if (p)
			rc = scols_line_set_data(ln, i, p);	/* calls strdup() */
		else if (np)
			rc = scols_line_refer_data(ln, i, np);	/* only refers */

		if (rc)
			err(EXIT_FAILURE, _("failed to add output data"));
	}

	return 0;
}

static int show_table(struct loopdev_cxt *lc,
		      const char *file,
		      uint64_t offset,
		      int flags)
{
	struct stat sbuf, *st = &sbuf;
	struct libscols_table *tb;
	struct libscols_line *ln;
	int rc = 0;
	size_t i;

	scols_init_debug(0);

	if (!(tb = scols_new_table()))
		err(EXIT_FAILURE, _("failed to allocate output table"));
	scols_table_enable_raw(tb, raw);
	scols_table_enable_json(tb, json);
	scols_table_enable_noheadings(tb, no_headings);

	if (json)
		scols_table_set_name(tb, "loopdevices");

	for (i = 0; i < ncolumns; i++) {
		const struct colinfo *ci = get_column_info(i);
		struct libscols_column *cl;

		cl = scols_table_new_column(tb, ci->name, ci->whint, ci->flags);
		if (!cl)
			err(EXIT_FAILURE, _("failed to allocate output column"));
		if (json)
			scols_column_set_json_type(cl, ci->json_type);
	}

	/* only one loopdev requested (already assigned to loopdev_cxt) */
	if (loopcxt_get_device(lc)) {
		ln = scols_table_new_line(tb, NULL);
		if (!ln)
			err(EXIT_FAILURE, _("failed to allocate output line"));
		rc = set_scols_data(lc, ln);

	/* list all loopdevs */
	} else {
		char *cn_file = NULL;

		rc = loopcxt_init_iterator(lc, LOOPITER_FL_USED);
		if (rc)
			goto done;
		if (!file || stat(file, st))
			st = NULL;

		while (loopcxt_next(lc) == 0) {
			if (file) {
				int used;
				const char *bf = cn_file ? cn_file : file;

				used = loopcxt_is_used(lc, st, bf, offset, 0, flags);
				if (!used && !cn_file) {
					bf = cn_file = canonicalize_path(file);
					used = loopcxt_is_used(lc, st, bf, offset, 0, flags);
				}
				if (!used)
					continue;
			}

			ln = scols_table_new_line(tb, NULL);
			if (!ln)
				err(EXIT_FAILURE, _("failed to allocate output line"));
			rc = set_scols_data(lc, ln);
			if (rc)
				break;
		}

		loopcxt_deinit_iterator(lc);
		free(cn_file);
	}
done:
	if (rc == 0)
		rc = scols_print_table(tb);
	scols_unref_table(tb);
	return rc;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);

	fprintf(out,
	      _(" %1$s [options] [<loopdev>]\n"
		" %1$s [options] -f | <loopdev> <file>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Set up and control loop devices.\n"), out);

	/* commands */
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --all                     list all used devices\n"), out);
	fputs(_(" -d, --detach <loopdev>...     detach one or more devices\n"), out);
	fputs(_(" -D, --detach-all              detach all used devices\n"), out);
	fputs(_(" -f, --find                    find first unused device\n"), out);
	fputs(_(" -c, --set-capacity <loopdev>  resize the device\n"), out);
	fputs(_(" -j, --associated <file>       list all devices associated with <file>\n"), out);
	fputs(_(" -L, --nooverlap               avoid possible conflict between devices\n"), out);

	/* commands options */
	fputs(USAGE_SEPARATOR, out);
	fputs(_(" -o, --offset <num>            start at offset <num> into file\n"), out);
	fputs(_("     --sizelimit <num>         device is limited to <num> bytes of the file\n"), out);
	fputs(_(" -b, --sector-size <num>       set the logical sector size to <num>\n"), out);
	fputs(_(" -P, --partscan                create a partitioned loop device\n"), out);
	fputs(_(" -r, --read-only               set up a read-only loop device\n"), out);
	fputs(_("     --direct-io[=<on|off>]    open backing file with O_DIRECT\n"), out);
	fputs(_("     --loop-ref <string>       loop device reference\n"), out);
	fputs(_("     --show                    print device name after setup (with -f)\n"), out);
	fputs(_(" -v, --verbose                 verbose mode\n"), out);

	/* output options */
	fputs(USAGE_SEPARATOR, out);
	fputs(_(" -J, --json                    use JSON --list output format\n"), out);
	fputs(_(" -l, --list                    list info about all or specified (default)\n"), out);
	fputs(_(" -n, --noheadings              don't print headings for --list output\n"), out);
	fputs(_(" -O, --output <cols>           specify columns to output for --list\n"), out);
	fputs(_("     --output-all              output all columns\n"), out);
	fputs(_("     --raw                     use raw --list output format\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(31));

	fputs(USAGE_COLUMNS, out);
	for (i = 0; i < ARRAY_SIZE(infos); i++)
		fprintf(out, " %12s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(out, USAGE_MAN_TAIL("losetup(8)"));

	exit(EXIT_SUCCESS);
}

static void warn_size(const char *filename, uint64_t size, uint64_t offset, int flags)
{
	struct stat st;

	if (!size) {
		if (stat(filename, &st) || S_ISBLK(st.st_mode))
			return;
		size = st.st_size;

		if (flags & LOOPDEV_FL_OFFSET)
			size -= offset;
	}

	if (size < 512)
		warnx(_("%s: Warning: file is smaller than 512 bytes; the loop device "
			"may be useless or invisible for system tools."),
			filename);
	else if (size % 512)
		warnx(_("%s: Warning: file does not end on a 512-byte sector boundary; "
			"the remaining end of the file will be ignored."),
			filename);
}

static int find_unused(struct loopdev_cxt *lc)
{
	int rc;

	rc = loopcxt_find_unused(lc);
	if (!rc)
		return 0;

	if (access(_PATH_DEV_LOOPCTL, F_OK) == 0 &&
			access(_PATH_DEV_LOOPCTL, W_OK) != 0)
		;
	else
		errno = -rc;

	warn(_("cannot find an unused loop device"));

	return rc;
}

static int create_loop(struct loopdev_cxt *lc,
		       int nooverlap, int lo_flags, int flags,
		       const char *file, const char *refname,
		       uint64_t offset, uint64_t sizelimit,
		       uint64_t blocksize)
{
	int hasdev = loopcxt_has_device(lc);
	int rc = 0, ntries = 0;

	/* losetup --find --noverlap file.img */
	if (!hasdev && nooverlap) {
		rc = loopcxt_find_overlap(lc, file, offset, sizelimit);
		switch (rc) {
		case 0: /* not found */
			break;

		case 1:	/* overlap */
			loopcxt_deinit(lc);
			errx(EXIT_FAILURE, _("%s: overlapping loop device exists"), file);

		case 2: /* overlap -- full size and offset match (reuse) */
		{
			uint32_t lc_encrypt_type;

			/* Once a loop is initialized RO, there is no
			 * way to change its parameters. */
			if (loopcxt_is_readonly(lc)
			    && !(lo_flags & LO_FLAGS_READ_ONLY)) {
				loopcxt_deinit(lc);
				errx(EXIT_FAILURE, _("%s: overlapping read-only loop device exists"), file);
			}

			/* This is no more supported, but check to be safe. */
			if (loopcxt_get_encrypt_type(lc, &lc_encrypt_type) == 0
			    && lc_encrypt_type != LO_CRYPT_NONE) {
				loopcxt_deinit(lc);
				errx(EXIT_FAILURE, _("%s: overlapping encrypted loop device exists"), file);
			}

			lc->config.info.lo_flags &= ~LO_FLAGS_AUTOCLEAR;
			if (loopcxt_ioctl_status(lc)) {
				loopcxt_deinit(lc);
				errx(EXIT_FAILURE, _("%s: failed to re-use loop device"), file);
			}
			return 0;	/* success, re-use */
		}
		default: /* error */
			loopcxt_deinit(lc);
			errx(EXIT_FAILURE, _("failed to inspect loop devices"));
			return -errno;
		}
	}

	if (hasdev)
		loopcxt_add_device(lc);

	/* losetup --noverlap /dev/loopN file.img */
	if (hasdev && nooverlap) {
		struct loopdev_cxt lc2;

		if (loopcxt_init(&lc2, 0)) {
			loopcxt_deinit(lc);
			err(EXIT_FAILURE, _("failed to initialize loopcxt"));
		}
		rc = loopcxt_find_overlap(&lc2, file, offset, sizelimit);
		loopcxt_deinit(&lc2);

		if (rc) {
			loopcxt_deinit(lc);
			if (rc > 0)
				errx(EXIT_FAILURE, _("%s: overlapping loop device exists"), file);
			err(EXIT_FAILURE, _("%s: failed to check for conflicting loop devices"), file);
		}
	}

	/* Create a new device */
	do {
		const char *errpre;

		/* Note that loopcxt_{find_unused,set_device}() resets
		 * loopcxt struct.
		 */
		if (!hasdev && (rc = find_unused(lc)))
			break;
		if (flags & LOOPDEV_FL_OFFSET)
			loopcxt_set_offset(lc, offset);
		if (flags & LOOPDEV_FL_SIZELIMIT)
			loopcxt_set_sizelimit(lc, sizelimit);
		if (lo_flags)
			loopcxt_set_flags(lc, lo_flags);
		if (blocksize > 0)
			loopcxt_set_blocksize(lc, blocksize);
		if (refname && (rc = loopcxt_set_refname(lc, refname))) {
			warnx(_("cannot set loop reference string"));
			break;
		}
		if ((rc = loopcxt_set_backing_file(lc, file))) {
			warn(_("%s: failed to use backing file"), file);
			break;
		}
		errno = 0;
		rc = loopcxt_setup_device(lc);
		if (rc == 0)
			break;			/* success */

		if ((errno == EBUSY || errno == EAGAIN) && !hasdev && ntries < 64) {
			xusleep(200000);
			ntries++;
			continue;
		}

		/* errors */
		errpre = hasdev && lc->fd < 0 ?
				 loopcxt_get_device(lc) : file;
		warn(_("%s: failed to set up loop device"), errpre);
		break;
	} while (hasdev == 0);

	return rc;
}

int main(int argc, char **argv)
{
	struct loopdev_cxt lc;
	int act = 0, flags = 0, no_overlap = 0, c;
	char *file = NULL, *refname = NULL;
	uint64_t offset = 0, sizelimit = 0, blocksize = 0;
	int res = 0, showdev = 0, lo_flags = 0;
	char *outarg = NULL;
	int list = 0;
	unsigned long use_dio = 0, set_dio = 0, set_blocksize = 0;

	enum {
		OPT_SIZELIMIT = CHAR_MAX + 1,
		OPT_SHOW,
		OPT_RAW,
		OPT_REF,
		OPT_DIO,
		OPT_OUTPUT_ALL
	};
	static const struct option longopts[] = {
		{ "all",          no_argument,       NULL, 'a'           },
		{ "set-capacity", required_argument, NULL, 'c'           },
		{ "detach",       required_argument, NULL, 'd'           },
		{ "detach-all",   no_argument,       NULL, 'D'           },
		{ "find",         no_argument,       NULL, 'f'           },
		{ "nooverlap",    no_argument,       NULL, 'L'           },
		{ "help",         no_argument,       NULL, 'h'           },
		{ "associated",   required_argument, NULL, 'j'           },
		{ "json",         no_argument,       NULL, 'J'           },
		{ "list",         no_argument,       NULL, 'l'           },
		{ "sector-size",  required_argument, NULL, 'b'      },
		{ "noheadings",   no_argument,       NULL, 'n'           },
		{ "offset",       required_argument, NULL, 'o'           },
		{ "output",       required_argument, NULL, 'O'           },
		{ "output-all",   no_argument,       NULL, OPT_OUTPUT_ALL },
		{ "sizelimit",    required_argument, NULL, OPT_SIZELIMIT },
		{ "partscan",     no_argument,       NULL, 'P'           },
		{ "read-only",    no_argument,       NULL, 'r'           },
		{ "direct-io",    optional_argument, NULL, OPT_DIO       },
		{ "raw",          no_argument,       NULL, OPT_RAW       },
		{ "loop-ref",     required_argument, NULL, OPT_REF,      },
		{ "show",         no_argument,       NULL, OPT_SHOW      },
		{ "verbose",      no_argument,       NULL, 'v'           },
		{ "version",      no_argument,       NULL, 'V'           },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'D','a','c','d','f','j' },
		{ 'D','c','d','f','l' },
		{ 'D','c','d','f','O' },
		{ 'J',OPT_RAW },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	if (loopcxt_init(&lc, 0))
		err(EXIT_FAILURE, _("failed to initialize loopcxt"));

	while ((c = getopt_long(argc, argv, "ab:c:d:Dfhj:JlLno:O:PrvV",
				longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch (c) {
		case 'a':
			act = A_SHOW;
			break;
		case 'b':
			set_blocksize = 1;
			blocksize = strtosize_or_err(optarg, _("failed to parse logical block size"));
			break;
		case 'c':
			act = A_SET_CAPACITY;
			if (loopcxt_set_device(&lc, optarg))
				err(EXIT_FAILURE, _("%s: failed to use device"),
						optarg);
			break;
		case 'r':
			lo_flags |= LO_FLAGS_READ_ONLY;
			break;
		case OPT_REF:
			refname = optarg;
			break;
		case 'd':
			act = A_DELETE;
			if (loopcxt_set_device(&lc, optarg))
				err(EXIT_FAILURE, _("%s: failed to use device"),
						optarg);
			break;
		case 'D':
			act = A_DELETE_ALL;
			break;
		case 'f':
			act = A_FIND_FREE;
			break;
		case 'J':
			json = 1;
			break;
		case 'j':
			act = A_SHOW;
			file = optarg;
			break;
		case 'l':
			list = 1;
			break;
		case 'L':
			no_overlap = 1;
			break;
		case 'n':
			no_headings = 1;
			break;
		case OPT_RAW:
			raw = 1;
			break;
		case 'o':
			offset = strtosize_or_err(optarg, _("failed to parse offset"));
			flags |= LOOPDEV_FL_OFFSET;
			break;
		case 'O':
			outarg = optarg;
			list = 1;
			break;
		case OPT_OUTPUT_ALL:
			list = 1;
			for (ncolumns = 0; ncolumns < ARRAY_SIZE(infos); ncolumns++)
				columns[ncolumns] = ncolumns;
			break;
		case 'P':
			lo_flags |= LO_FLAGS_PARTSCAN;
			break;
		case OPT_SHOW:
			showdev = 1;
			break;
		case OPT_DIO:
			use_dio = set_dio = 1;
			if (optarg)
				use_dio = parse_switch(optarg, _("argument error"), "on", "off", NULL);
			if (use_dio)
				lo_flags |= LO_FLAGS_DIRECT_IO;
			break;
		case 'v':
			break;
		case OPT_SIZELIMIT:			/* --sizelimit */
			sizelimit = strtosize_or_err(optarg, _("failed to parse size"));
			flags |= LOOPDEV_FL_SIZELIMIT;
                        break;

		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	ul_path_init_debug();
	ul_sysfs_init_debug();

	/* default is --list --all */
	if (argc == 1) {
		act = A_SHOW;
		list = 1;
	}

	if (!act && argc == 2 && (raw || json)) {
		act = A_SHOW;
		list = 1;
	}

	/* default --list output columns */
	if (list && !ncolumns) {
		columns[ncolumns++] = COL_NAME;
		columns[ncolumns++] = COL_SIZELIMIT;
		columns[ncolumns++] = COL_OFFSET;
		columns[ncolumns++] = COL_AUTOCLR;
		columns[ncolumns++] = COL_RO;
		columns[ncolumns++] = COL_BACK_FILE;
		columns[ncolumns++] = COL_DIO;
		columns[ncolumns++] = COL_LOGSEC;
	}

	if (act == A_FIND_FREE && optind < argc) {
		/*
		 * losetup -f <backing_file>
		 */
		act = A_CREATE;
		file = argv[optind++];

		if (optind < argc)
			errx(EXIT_FAILURE, _("unexpected arguments"));
	}

	if (list && !act && optind == argc)
		/*
		 * losetup --list	defaults to --all
		 */
		act = A_SHOW;

	if (!act && optind + 1 == argc) {
		/*
		 * losetup [--list] <device>
		 * OR
		 * losetup {--direct-io[=off]|--logical-blocksize=size}... <device>
		 */
		if (set_dio) {
			act = A_SET_DIRECT_IO;
			lo_flags &= ~LO_FLAGS_DIRECT_IO;
		} else if (set_blocksize)
			act = A_SET_BLOCKSIZE;
		else
			act = A_SHOW_ONE;

		if (loopcxt_set_device(&lc, argv[optind]))
			err(EXIT_FAILURE, _("%s: failed to use device"),
					argv[optind]);
		optind++;
	}
	if (!act) {
		/*
		 * losetup <loopdev> <backing_file>
		 */
		act = A_CREATE;

		if (optind >= argc)
			errx(EXIT_FAILURE, _("no loop device specified"));
		/* don't use is_loopdev() here, the device does not have exist yet */
		if (loopcxt_set_device(&lc, argv[optind]))
			err(EXIT_FAILURE, _("%s: failed to use device"),
					argv[optind]);
		optind++;

		if (optind >= argc)
			errx(EXIT_FAILURE, _("no file specified"));
		file = argv[optind++];
	}

	if (act != A_CREATE &&
	    (sizelimit || lo_flags || showdev))
		errx(EXIT_FAILURE,
			_("the options %s are allowed during loop device setup only"),
			"--{sizelimit,partscan,read-only,show}");

	if ((flags & LOOPDEV_FL_OFFSET) &&
	    act != A_CREATE && (act != A_SHOW || !file))
		errx(EXIT_FAILURE, _("the option --offset is not allowed in this context"));

	if (outarg && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
					 &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	switch (act) {
	case A_CREATE:
		res = create_loop(&lc, no_overlap, lo_flags, flags, file, refname,
				  offset, sizelimit, blocksize);
		if (res == 0) {
			if (showdev)
				printf("%s\n", loopcxt_get_device(&lc));
			warn_size(file, sizelimit, offset, flags);
		}
		break;
	case A_DELETE:
		res = delete_loop(&lc);
		while (optind < argc) {
			if (loopcxt_set_device(&lc, argv[optind]))
				warn(_("%s: failed to use device"),
						argv[optind]);
			optind++;
			res += delete_loop(&lc);
		}
		break;
	case A_DELETE_ALL:
		res = delete_all_loops(&lc);
		break;
	case A_FIND_FREE:
		res = find_unused(&lc);
		if (!res)
			printf("%s%s\n", loopcxt_get_device(&lc),
				loopcxt_is_lost(&lc) ? " (lost)" : "");
		break;
	case A_SHOW:
		if (list)
			res = show_table(&lc, file, offset, flags);
		else
			res = show_all_loops(&lc, file, offset, flags);
		break;
	case A_SHOW_ONE:
		if (list)
			res = show_table(&lc, NULL, 0, 0);
		else
			res = printf_loopdev(&lc);
		if (res)
			warn("%s", loopcxt_get_device(&lc));
		break;
	case A_SET_CAPACITY:
		res = loopcxt_ioctl_capacity(&lc);
		if (res)
			warn(_("%s: set capacity failed"),
			        loopcxt_get_device(&lc));
		break;
	case A_SET_DIRECT_IO:
		res = loopcxt_ioctl_dio(&lc, use_dio);
		if (res)
			warn(_("%s: set direct io failed"),
			        loopcxt_get_device(&lc));
		break;
	case A_SET_BLOCKSIZE:
		res = loopcxt_ioctl_blocksize(&lc, blocksize);
		if (res)
			warn(_("%s: set logical block size failed"),
			        loopcxt_get_device(&lc));
		break;
	default:
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
		break;
	}

	if (res && (act == A_SET_CAPACITY
		    || act == A_CREATE
		    || act == A_SET_DIRECT_IO
		    || act == A_SET_BLOCKSIZE)
	    && loopcxt_is_lost(&lc))
		warn_lost(&lc);

	loopcxt_deinit(&lc);
	return res ? EXIT_FAILURE : EXIT_SUCCESS;
}
