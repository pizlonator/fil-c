/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 * partx: tell the kernel about your disk's partitions
 * [This is not an fdisk - adding and removing partitions
 * is not a change of the disk, but just telling the kernel
 * about presence and numbering of on-disk partitions.]
 *
 * aeb, 2000-03-21 -- sah is 42 now
 *
 * Copyright (C) 2010 Davidlohr Bueso <dave@gnu.org>
 *      Rewritten to use libblkid for util-linux
 *      based on ideas from Karel Zak <kzak@redhat.com>
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>

#include <blkid.h>
#include <libsmartcols.h>

#include "c.h"
#include "pathnames.h"
#include "nls.h"
#include "blkdev.h"
#include "strutils.h"
#include "xalloc.h"
#include "partx.h"
#include "sysfs.h"
#include "loopdev.h"
#include "closestream.h"
#include "optutils.h"

/* this is the default upper limit, could be modified by --nr */
#define SLICES_MAX	256

/* basic table settings */
enum {
	PARTX_RAW =		(1 << 0),
	PARTX_NOHEADINGS =	(1 << 1),
	PARTX_EXPORT =		(1 << 2),
};

/* all the columns (-o option) */
enum {
	COL_PARTNO,
	COL_START,
	COL_END,
	COL_SECTORS,
	COL_SIZE,
	COL_NAME,
	COL_UUID,
	COL_TYPE,
	COL_FLAGS,
	COL_SCHEME,
};

#define ACT_ERROR "--{add,delete,show,list,raw,pairs}"
enum {
	ACT_NONE,
	ACT_LIST,
	ACT_SHOW,
	ACT_ADD,
	ACT_UPD,
	ACT_DELETE
};

enum {
	FL_BYTES = (1 << 1)
};

/* column names */
struct colinfo {
	const char * const	name;	/* header */
	double			whint;	/* width hint (N < 1 is in percent of termwidth) */
	int			flags;	/* SCOLS_FL_* */
	const char		*help;
};

/* columns descriptions */
static const struct colinfo infos[] = {
	[COL_PARTNO]   = { "NR",    0.25, SCOLS_FL_RIGHT, N_("partition number") },
	[COL_START]    = { "START",   0.30, SCOLS_FL_RIGHT, N_("start of the partition in sectors") },
	[COL_END]      = { "END",     0.30, SCOLS_FL_RIGHT, N_("end of the partition in sectors") },
	[COL_SECTORS]  = { "SECTORS", 0.30, SCOLS_FL_RIGHT, N_("number of sectors") },
	[COL_SIZE]     = { "SIZE",    0.30, SCOLS_FL_RIGHT, N_("human readable size") },
	[COL_NAME]     = { "NAME",    0.30, SCOLS_FL_TRUNC, N_("partition name") },
	[COL_UUID]     = { "UUID",    36, 0, N_("partition UUID")},
	[COL_SCHEME]   = { "SCHEME",  0.1, SCOLS_FL_TRUNC, N_("partition table type (dos, gpt, ...)")},
	[COL_FLAGS]    = { "FLAGS",   0.1, SCOLS_FL_TRUNC, N_("partition flags")},
	[COL_TYPE]     = { "TYPE",    1, SCOLS_FL_RIGHT, N_("partition type (a string, a UUID, or hex)")},
};

#define NCOLS ARRAY_SIZE(infos)

/* array with IDs of enabled columns */
static int columns[NCOLS];
static size_t ncolumns;

static int verbose;
static int partx_flags;
static struct loopdev_cxt lc;
static int loopdev;

static void assoc_loopdev(const char *fname)
{
	int rc;

	if (loopcxt_init(&lc, 0))
		err(EXIT_FAILURE, _("failed to initialize loopcxt"));

	rc = loopcxt_find_unused(&lc);
	if (rc)
		err(EXIT_FAILURE, _("%s: failed to find unused loop device"),
		    fname);

	if (verbose)
		printf(_("Trying to use '%s' for the loop device\n"),
		       loopcxt_get_device(&lc));

	if (loopcxt_set_backing_file(&lc, fname))
		err(EXIT_FAILURE, _("%s: failed to set backing file"), fname);

	rc = loopcxt_setup_device(&lc);

	if (rc == -EBUSY)
		err(EXIT_FAILURE, _("%s: failed to set up loop device"), fname);

	loopdev = 1;
}

static inline int get_column_id(int num)
{
	assert(ARRAY_SIZE(columns) == NCOLS);
	assert((size_t)num < ncolumns);
	assert(columns[num] < (int) NCOLS);
	return columns[num];
}

static inline const struct colinfo *get_column_info(int num)
{
	return &infos[ get_column_id(num) ];
}

static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	assert(name);

	for (i = 0; i < NCOLS; i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

/*
 * Given a partition return the corresponding partition number.
 *
 * Note that this function tries to use sysfs, otherwise it assumes that the
 * last characters are always numeric (sda1, sdc20, etc).
 */
static int get_partno_from_device(char *partition, dev_t devno)
{
	int partno = 0;
	size_t sz;
	char *p, *end = NULL;

	assert(partition);

	if (devno) {
		struct path_cxt *pc;
		int rc;

		pc = ul_new_sysfs_path(devno, NULL, NULL);
		if (!pc)
			goto err;

		rc = ul_path_read_s32(pc, &partno, "partition");
		ul_unref_path(pc);

		if (rc == 0)
			return partno;
	}

	sz = strlen(partition);
	p = partition + sz - 1;

	if (!isdigit((unsigned char) *p))
		goto err;

	while (isdigit((unsigned char) *(p - 1))) p--;

	errno = 0;
	partno = strtol(p, &end, 10);
	if (errno || !end || *end || p == end)
		goto err;

	return partno;
err:
	errx(EXIT_FAILURE, _("%s: failed to get partition number"), partition);
}

static int get_max_partno(const char *disk, dev_t devno)
{
	char path[PATH_MAX], *parent, *dirname = NULL;
	struct stat st;
	DIR *dir;
	struct dirent *d;
	int partno = 0;

	if (!devno && !stat(disk, &st))
		devno = st.st_rdev;
	if (!devno)
		goto dflt;
	parent = strrchr(disk, '/');
	if (!parent)
		goto dflt;
	parent++;

	snprintf(path, sizeof(path), _PATH_SYS_DEVBLOCK "/%d:%d/",
			major(devno), minor(devno));

	dir = opendir(path);
	if (!dir)
		goto dflt;

	dirname = xstrdup(path);

	while ((d = readdir(dir))) {
		int fd;

		if (!strcmp(d->d_name, ".") ||
		    !strcmp(d->d_name, ".."))
			continue;
#ifdef _DIRENT_HAVE_D_TYPE
		if (d->d_type != DT_DIR && d->d_type != DT_UNKNOWN)
			continue;
#endif
		if (strncmp(parent, d->d_name, strlen(parent)) != 0)
			continue;
		snprintf(path, sizeof(path), "%s/partition", d->d_name);

		fd = openat(dirfd(dir), path, O_RDONLY);
		if (fd) {
			int x = 0;
			FILE *f = fdopen(fd, "r");
			if (f) {
				if (fscanf(f, "%d", &x) == 1 && x > partno)
					partno = x;
				fclose(f);
			}
		}
	}

	free(dirname);
	closedir(dir);
	return partno;
dflt:
	return SLICES_MAX;
}

static int recount_range_by_pt(blkid_partlist ls, int *lower, int *upper)
{
	int n = 0, i, nparts = blkid_partlist_numof_partitions(ls);

	for (i = 0; i < nparts; i++) {
		blkid_partition par = blkid_partlist_get_partition(ls, i);
		int partno = blkid_partition_get_partno(par);
		n = max(partno, n);
	}

	if (*lower < 0)
		*lower = n + *lower + 1;
	if (*upper < 0)
		*upper = n + *upper + 1;

	if (*lower > *upper && *upper != 0) {
		warnx(_("specified range <%d:%d> does not make sense"), *lower, *upper);
		return -EINVAL;
	}
	if (verbose)
		printf(_("range recount: max partno=%d, lower=%d, upper=%d\n"), n, *lower, *upper);
	return 0;
}

static void del_parts_warnx(const char *device, int first, int last)
{
	if (first == last)
		warnx(_("%s: error deleting partition %d"), device, first);
	else
		warnx(_("%s: error deleting partitions %d-%d"),
				device, first, last);
}

static int del_parts(int fd, const char *device, dev_t devno,
		     int lower, int upper)
{
	int rc = 0, i, errfirst = 0, errlast = 0;

	assert(fd >= 0);
	assert(device);

	/* recount range by information in /sys */
	if (!lower)
		lower = 1;
	if (!upper || lower < 0 || upper < 0) {
		int n = get_max_partno(device, devno);
		if (!upper)
			upper = n;
		else if (upper < 0)
			upper = n + upper + 1;
		if (lower < 0)
			lower = n + lower + 1;
	}
	if (lower > upper) {
		warnx(_("specified range <%d:%d> "
			"does not make sense"), lower, upper);
		return -1;
	}

	for (i = lower; i <= upper; i++) {
		if (partx_del_partition(fd, i) == 0) {
			if (verbose)
				printf(_("%s: partition #%d removed\n"), device, i);
			continue;
		}

		if (errno == ENXIO) {
			if (verbose)
				printf(_("%s: partition #%d doesn't exist\n"), device, i);
			continue;
		}
		rc = -1;
		if (verbose)
			warn(_("%s: deleting partition #%d failed"), device, i);
		if (!errfirst)
			errlast = errfirst = i;
		else if (errlast + 1 == i)
			errlast++;
		else {
			del_parts_warnx(device, errfirst, errlast);
			errlast = errfirst = i;
		}
	}

	if (errfirst)
		del_parts_warnx(device, errfirst, errlast);
	return rc;
}


static void add_parts_warnx(const char *device, int first, int last)
{
	if (first == last)
		warnx(_("%s: error adding partition %d"), device, first);
	else
		warnx(_("%s: error adding partitions %d-%d"),
				device, first, last);
}

static int add_parts(int fd, const char *device,
		     blkid_partlist ls, int lower, int upper)
{
	int i, nparts, rc, errfirst = 0, errlast = 0;

	assert(fd >= 0);
	assert(device);
	assert(ls);

	rc = recount_range_by_pt(ls, &lower, &upper);
	if (rc)
		return rc;

	nparts = blkid_partlist_numof_partitions(ls);

	for (i = 0; i < nparts; i++) {
		blkid_partition par = blkid_partlist_get_partition(ls, i);
		int n = blkid_partition_get_partno(par);
		uintmax_t start, size;

		if (lower && n < lower)
			continue;
		if (upper && n > upper)
			continue;

		start = blkid_partition_get_start(par);
		size =  blkid_partition_get_size(par);

		if (blkid_partition_is_extended(par))
			/*
			 * Let's follow the Linux kernel and reduce
			 * DOS extended partition to 1 or 2 sectors.
			 */
			size = min(size, (uintmax_t) 2);

		if (partx_add_partition(fd, n, start, size) == 0) {
			if (verbose)
				printf(_("%s: partition #%d added\n"), device, n);
			continue;
		}
		rc = -1;
		if (verbose)
			warn(_("%s: adding partition #%d failed"), device, n);
		if (!errfirst)
			errlast = errfirst = n;
		else if (errlast + 1 == n)
			errlast++;
		else {
			add_parts_warnx(device, errfirst, errlast);
			errlast = errfirst = n;
		}
	}

	if (errfirst)
		add_parts_warnx(device, errfirst, errlast);

	/*
	 * The kernel with enabled partitions scanner for loop devices add *all*
	 * partitions, so we should delete any extra, unwanted ones, when the -n
	 * option is passed.
	 */
	if (loopdev && loopcxt_is_partscan(&lc) && (lower || upper)) {
		for (i = 0; i < nparts; i++) {
			blkid_partition par = blkid_partlist_get_partition(ls, i);
			int n = blkid_partition_get_partno(par);

			if (n < lower || n > upper)
				partx_del_partition(fd, n);
		}
	}

	return rc;
}

static void upd_parts_warnx(const char *device, int first, int last)
{
	if (first == last)
		warnx(_("%s: error updating partition %d"), device, first);
	else
		warnx(_("%s: error updating partitions %d-%d"),
				device, first, last);
}

static int upd_parts(int fd, const char *device, dev_t devno,
		     blkid_partlist ls, int lower, int upper)
{
	int n, nparts, rc = 0, errfirst = 0, errlast = 0, err;
	blkid_partition par;
	uintmax_t start, size;

	assert(fd >= 0);
	assert(device);
	assert(ls);

	/* recount range by information in /sys, if on disk number of
	 * partitions is greater than in /sys the use on-disk limit */
	nparts = blkid_partlist_numof_partitions(ls);
	if (!lower)
		lower = 1;
	if (!upper || lower < 0 || upper < 0) {
		n = get_max_partno(device, devno);
		if (!upper)
			upper = n > nparts ? n : nparts;
		else if (upper < 0)
			upper = n + upper + 1;
		if (lower < 0)
			lower = n + lower + 1;
	}
	if (lower > upper) {
		warnx(_("specified range <%d:%d> "
			"does not make sense"), lower, upper);
		return -1;
	}

	for (n = lower; n <= upper; n++) {
		par = blkid_partlist_get_partition_by_partno(ls, n);
		if (!par) {
			if (verbose)
				warn(_("%s: no partition #%d"), device, n);
			continue;
		}

		start = blkid_partition_get_start(par);
		size =  blkid_partition_get_size(par);
		if (blkid_partition_is_extended(par))
			/*
			 * Let's follow the Linux kernel and reduce
			 * DOS extended partition to 1 or 2 sectors.
			 */
			size = min(size, (uintmax_t) 2);

		err = partx_del_partition(fd, n);
		if (err == -1 && errno == ENXIO)
			err = 0; /* good, it already doesn't exist */
		if (err == -1 && errno == EBUSY)
		{
			/* try to resize */
			err = partx_resize_partition(fd, n, start, size);
			if (verbose)
				printf(_("%s: partition #%d resized\n"), device, n);
			if (err == 0)
				continue;
		}
		if (err == 0 && partx_add_partition(fd, n, start, size) == 0) {
			if (verbose)
				printf(_("%s: partition #%d added\n"), device, n);
			continue;
		}

		if (err == 0)
			continue;
		rc = -1;
		if (verbose)
			warn(_("%s: updating partition #%d failed"), device, n);
		if (!errfirst)
			errlast = errfirst = n;
		else if (errlast + 1 == n)
			errlast++;
		else {
			upd_parts_warnx(device, errfirst, errlast);
			errlast = errfirst = n;
		}
	}

	if (errfirst)
		upd_parts_warnx(device, errfirst, errlast);
	return rc;
}

static int list_parts(blkid_partlist ls, int lower, int upper)
{
	int i, nparts, rc;

	assert(ls);

	rc = recount_range_by_pt(ls, &lower, &upper);
	if (rc)
		return rc;

	nparts = blkid_partlist_numof_partitions(ls);

	for (i = 0; i < nparts; i++) {
		blkid_partition par = blkid_partlist_get_partition(ls, i);
		int n = blkid_partition_get_partno(par);
		uintmax_t start, size;

		if (lower && n < lower)
			continue;
		if (upper && n > upper)
			continue;

		start = blkid_partition_get_start(par);
		size =  blkid_partition_get_size(par);

		printf(P_("#%2d: %9ju-%9ju (%9ju sector, %6ju MB)\n",
			  "#%2d: %9ju-%9ju (%9ju sectors, %6ju MB)\n",
			  size),
		       n, start, start + size -1,
		       size, (size << 9) / 1000000);
	}
	return 0;
}

static int add_scols_line(struct libscols_table *table, blkid_partition par)
{
	struct libscols_line *line;
	int i, rc = 0;

	assert(table);
	assert(par);

	line = scols_table_new_line(table, NULL);
	if (!line) {
		warn(_("failed to allocate output line"));
		return -ENOMEM;
	}

	for (i = 0; (size_t)i < ncolumns; i++) {
		char *str = NULL;			/* allocated string */
		const char *cstr = NULL;		/* foreign string */

		switch (get_column_id(i)) {
		case COL_PARTNO:
			xasprintf(&str, "%d", blkid_partition_get_partno(par));
			break;
		case COL_START:
			xasprintf(&str, "%ju", blkid_partition_get_start(par));
			break;
		case COL_END:
			xasprintf(&str, "%ju",
					blkid_partition_get_start(par) +
					blkid_partition_get_size(par) - 1);
			break;
		case COL_SECTORS:
			xasprintf(&str, "%ju", blkid_partition_get_size(par));
			break;
		case COL_SIZE:
			if (partx_flags & FL_BYTES)
				xasprintf(&str, "%ju", (uintmax_t)
					blkid_partition_get_size(par) << 9);
			else
				str = size_to_human_string(SIZE_SUFFIX_1LETTER,
					blkid_partition_get_size(par) << 9);
			break;
		case COL_NAME:
			cstr = blkid_partition_get_name(par);
			break;
		case COL_UUID:
			cstr = blkid_partition_get_uuid(par);
			break;
		case COL_TYPE:
			if (blkid_partition_get_type_string(par))
				cstr = blkid_partition_get_type_string(par);
			else
				xasprintf(&str, "0x%x",
					blkid_partition_get_type(par));
			break;
		case COL_FLAGS:
			xasprintf(&str, "0x%llx", blkid_partition_get_flags(par));
			break;
		case COL_SCHEME:
		{
			blkid_parttable tab = blkid_partition_get_table(par);
			if (tab)
				cstr = blkid_parttable_get_type(tab);
			break;
		}
		default:
			break;
		}

		if (cstr)
			rc = scols_line_set_data(line, i, cstr);
		else if (str)
			rc = scols_line_refer_data(line, i, str);
		if (rc) {
			warn(_("failed to add output data"));
			break;
		}
	}

	return rc;
}

static int show_parts(blkid_partlist ls, int scols_flags, int lower, int upper)
{
	int i, rc = -1;
	struct libscols_table *table;
	int nparts;

	assert(ls);

	nparts = blkid_partlist_numof_partitions(ls);
	if (!nparts)
		return 0;

	scols_init_debug(0);
	table = scols_new_table();
	if (!table) {
		warn(_("failed to allocate output table"));
		return -1;
	}
	scols_table_enable_raw(table, !!(scols_flags & PARTX_RAW));
	scols_table_enable_export(table, !!(scols_flags & PARTX_EXPORT));
	scols_table_enable_noheadings(table, !!(scols_flags & PARTX_NOHEADINGS));

	for (i = 0; (size_t)i < ncolumns; i++) {
		const struct colinfo *col = get_column_info(i);

		if (!scols_table_new_column(table, col->name, col->whint, col->flags)) {
			warnx(_("failed to allocate output column"));
			goto done;
		}
	}

	rc = recount_range_by_pt(ls, &lower, &upper);
	if (rc)
		goto done;

	for (i = 0; i < nparts; i++) {
		blkid_partition par = blkid_partlist_get_partition(ls, i);
		int n = blkid_partition_get_partno(par);

		if (lower && n < lower)
			continue;
		if (upper && n > upper)
			continue;

		rc = add_scols_line(table, par);
		if (rc)
			break;
	}

	rc = 0;
	scols_print_table(table);
done:
	scols_unref_table(table);
	return rc;
}

static blkid_partlist get_partlist(blkid_probe pr,
			const char *device, char *type)
{
	blkid_partlist ls;
	blkid_parttable tab;

	assert(pr);
	assert(device);

	if (type) {
		char *name[] = { type, NULL };

		if (blkid_probe_filter_partitions_type(pr,
				BLKID_FLTR_ONLYIN, name)) {
			warnx(_("failed to initialize blkid "
				"filter for '%s'"), type);
			return NULL;
		}
	}

	ls = blkid_probe_get_partitions(pr);
	if (!ls) {
		warnx(_("%s: failed to read partition table"), device);
		return NULL;
	}

	tab = blkid_partlist_get_table(ls);
	if (verbose && tab) {
		printf(_("%s: partition table type '%s' detected\n"),
			       device, blkid_parttable_get_type(tab));

		if (!blkid_partlist_numof_partitions(ls))
			printf(_("%s: partition table with no partitions"), device);
	}

	return ls;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	size_t i;

	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [-a|-d|-s|-u] [--nr <n:m> | <partition>] <disk>\n"),
		program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Tell the kernel about the presence and numbering of partitions.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --add            add specified partitions or all of them\n"), out);
	fputs(_(" -d, --delete         delete specified partitions or all of them\n"), out);
	fputs(_(" -u, --update         update specified partitions or all of them\n"), out);
	fputs(_(" -s, --show           list partitions\n\n"), out);
	fputs(_(" -b, --bytes          print SIZE in bytes rather than in human readable format\n"), out);
	fputs(_(" -g, --noheadings     don't print headings for --show\n"), out);
	fputs(_(" -n, --nr <n:m>       specify the range of partitions (e.g. --nr 2:4)\n"), out);
	fputs(_(" -o, --output <list>  define which output columns to use\n"), out);
	fputs(_("     --output-all     output all columns\n"), out);
	fputs(_(" -P, --pairs          use key=\"value\" output format\n"), out);
	fputs(_(" -r, --raw            use raw output format\n"), out);
	fputs(_(" -S, --sector-size <num>  overwrite sector size\n"), out);
	fputs(_(" -t, --type <type>    specify the partition type\n"), out);
	fputs(_("     --list-types     list supported partition types and exit\n"), out);
	fputs(_(" -v, --verbose        verbose mode\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(22));

	fputs(USAGE_COLUMNS, out);
	for (i = 0; i < NCOLS; i++)
		fprintf(out, " %10s  %s\n", infos[i].name, _(infos[i].help));

	fprintf(out, USAGE_MAN_TAIL("partx(8)"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int fd, c, what = ACT_NONE, lower = 0, upper = 0, rc = 0;
	int scols_flags = 0;
	char *type = NULL;
	char *device = NULL; /* pointer to argv[], ie: /dev/sda1 */
	char *wholedisk = NULL; /* allocated, ie: /dev/sda */
	char *outarg = NULL;
	dev_t disk_devno = 0, part_devno = 0;
	unsigned int sector_size = 0;

	enum {
		OPT_LIST_TYPES = CHAR_MAX + 1,
		OPT_OUTPUT_ALL
	};
	static const struct option long_opts[] = {
		{ "bytes",	no_argument,       NULL, 'b' },
		{ "noheadings",	no_argument,       NULL, 'g' },
		{ "raw",	no_argument,       NULL, 'r' },
		{ "list",	no_argument,	   NULL, 'l' },
		{ "show",	no_argument,       NULL, 's' },
		{ "add",	no_argument,       NULL, 'a' },
		{ "delete",	no_argument,	   NULL, 'd' },
		{ "update",     no_argument,       NULL, 'u' },
		{ "type",	required_argument, NULL, 't' },
		{ "list-types", no_argument,       NULL, OPT_LIST_TYPES },
		{ "nr",		required_argument, NULL, 'n' },
		{ "output",	required_argument, NULL, 'o' },
		{ "output-all", no_argument,       NULL, OPT_OUTPUT_ALL },
		{ "pairs",      no_argument,       NULL, 'P' },
		{ "sector-size",required_argument, NULL, 'S' },
		{ "help",	no_argument,       NULL, 'h' },
		{ "version",    no_argument,       NULL, 'V' },
		{ "verbose",	no_argument,       NULL, 'v' },
		{ NULL, 0, NULL, 0 }
	};

	static const ul_excl_t excl[] = {	/* rows and cols in ASCII order */
		{ 'P','a','d','l','r','s','u' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv,
				"abdglrsuvn:t:o:PS:hV", long_opts, NULL)) != -1) {

		err_exclusive_options(c, long_opts, excl, excl_st);

		switch(c) {
		case 'a':
			what = ACT_ADD;
			break;
		case 'b':
			partx_flags |= FL_BYTES;
			break;
		case 'd':
			what = ACT_DELETE;
			break;
		case 'g':
			scols_flags |= PARTX_NOHEADINGS;
			break;
		case 'l':
			what = ACT_LIST;
			break;
		case 'n':
			if (parse_range(optarg, &lower, &upper, 0))
				errx(EXIT_FAILURE, _("failed to parse --nr <M-N> range"));
			break;
		case 'o':
			outarg = optarg;
			break;
		case OPT_OUTPUT_ALL:
			for (ncolumns = 0; ncolumns < ARRAY_SIZE(infos); ncolumns++)
				columns[ncolumns] = ncolumns;
			break;
		case 'P':
			scols_flags |= PARTX_EXPORT;
			what = ACT_SHOW;
			break;
		case 'r':
			scols_flags |= PARTX_RAW;
			what = ACT_SHOW;
			break;
		case 's':
			what = ACT_SHOW;
			break;
		case 'S':
			sector_size = strtou32_or_err(optarg, _("invalid sector size argument"));
			break;
		case 't':
			type = optarg;
			break;
		case 'u':
			what = ACT_UPD;
			break;
		case 'v':
			verbose = 1;
			break;
		case OPT_LIST_TYPES:
		{
			size_t idx = 0;
			const char *name = NULL;

			while (blkid_partitions_get_name(idx++, &name) == 0)
				puts(name);
			return EXIT_SUCCESS;
		}
		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (what == ACT_NONE)
		what = ACT_SHOW;

	/* --show default, could by modified by -o  */
	if (what == ACT_SHOW && !ncolumns) {
		columns[ncolumns++] = COL_PARTNO;
		columns[ncolumns++] = COL_START;
		columns[ncolumns++] = COL_END;
		columns[ncolumns++] = COL_SECTORS;
		columns[ncolumns++] = COL_SIZE;
		columns[ncolumns++] = COL_NAME;
		columns[ncolumns++] = COL_UUID;
	}

	if (what == ACT_SHOW && outarg &&
	    string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
				   &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	/*
	 * Note that 'partx /dev/sda1' == 'partx /dev/sda1 /dev/sda'
	 * so assume that the device and/or disk are always the last
	 * arguments to be passed to partx.
	 */
	if (optind == argc - 2) {
		/* passed 2 arguments:
		 *   /dev/sda1 /dev/sda  : partition + whole-disk
		 *   -- /dev/sda1	 : partition that should be used as a whole-disk
		 */
		device = argv[optind];

		if (strcmp(device, "-") == 0) {
			device = NULL;
			wholedisk = xstrdup(argv[optind + 1]);
		} else {
			device = argv[optind];
			wholedisk = xstrdup(argv[optind + 1]);

			if (device && wholedisk && !startswith(device, wholedisk))
				errx(EXIT_FAILURE, _("partition and disk name do not match"));
		}
	} else if (optind == argc - 1) {
		/* passed only one arg (ie: /dev/sda3 or /dev/sda) */
		struct stat sb;

		device = argv[optind];

		if (stat(device, &sb))
			err(EXIT_FAILURE, _("stat of %s failed"), device);

		part_devno = sb.st_rdev;

		if (blkid_devno_to_wholedisk(part_devno,
					     NULL, 0, &disk_devno) == 0 &&
		    part_devno != disk_devno)
			wholedisk = blkid_devno_to_devname(disk_devno);

		if (!wholedisk) {
			wholedisk = xstrdup(device);
			disk_devno = part_devno;
			device = NULL;
			part_devno = 0;
		}
	} else {
		warnx(_("bad usage"));
		errtryhelp(EXIT_FAILURE);
	}
	if (device && (upper || lower))
		errx(EXIT_FAILURE, _("--nr and <partition> are mutually exclusive"));

	assert(wholedisk);

	if (device) {
		/* use partno from given partition instead of --nr range, e.g:
		 *   partx -d /dev/sda3
		 * is the same like:
		 *   partx -d --nr 3 /dev/sda
		 */
		struct stat sb;

		if (!part_devno && !stat(device, &sb))
			part_devno = sb.st_rdev;

		lower = upper = get_partno_from_device(device, part_devno);
	}

	if (verbose)
		printf(_("partition: %s, disk: %s, lower: %d, upper: %d\n"),
		       device ? device : "none", wholedisk, lower, upper);

	if (what == ACT_ADD || what == ACT_DELETE) {
		struct stat x;

		if (stat(wholedisk, &x))
			errx(EXIT_FAILURE, "%s", wholedisk);

		if  (S_ISREG(x.st_mode)) {
			/* not a blkdev, try to associate it to a loop device */
			if (what == ACT_DELETE)
				errx(EXIT_FAILURE, _("%s: cannot delete partitions"),
				     wholedisk);
			if (!loopmod_supports_partscan())
				errx(EXIT_FAILURE, _("%s: partitioned loop devices unsupported"),
				     wholedisk);
			assoc_loopdev(wholedisk);
			free(wholedisk);
			wholedisk = xstrdup(lc.device);
		} else if (!S_ISBLK(x.st_mode))
			errx(EXIT_FAILURE, _("%s: not a block device"), wholedisk);
	}
	if ((fd = open(wholedisk, O_RDONLY)) == -1)
		err(EXIT_FAILURE, _("cannot open %s"), wholedisk);

	if (what == ACT_DELETE)
		rc = del_parts(fd, wholedisk, disk_devno, lower, upper);
	else {
		blkid_probe pr = blkid_new_probe();
		blkid_partlist ls = NULL;

		if (!pr || blkid_probe_set_device(pr, fd, 0, 0))
			warnx(_("%s: failed to initialize blkid prober"),
					wholedisk);
		else {
			if (sector_size)
				blkid_probe_set_sectorsize(pr, sector_size);

			ls = get_partlist(pr, wholedisk, type);
		}

		if (ls) {
			switch (what) {
			case ACT_SHOW:
				rc = show_parts(ls, scols_flags, lower, upper);
				break;
			case ACT_LIST:
				rc = list_parts(ls, lower, upper);
				break;
			case ACT_ADD:
				rc = add_parts(fd, wholedisk, ls, lower, upper);
				break;
			case ACT_UPD:
				rc = upd_parts(fd, wholedisk, disk_devno, ls, lower, upper);
				break;
			case ACT_NONE:
				break;
			default:
				abort();
			}
		} else
			rc = 1;

		blkid_free_probe(pr);
	}

	free(wholedisk);

	if (loopdev)
		loopcxt_deinit(&lc);

	if (close_fd(fd) != 0)
		err(EXIT_FAILURE, _("write failed"));

	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
