/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 */
#include <libfdisk.h>
#include <libsmartcols.h>
#include <assert.h>

#include "c.h"
#include "xalloc.h"
#include "nls.h"
#include "blkdev.h"
#include "mbsalign.h"
#include "pathnames.h"
#include "canonicalize.h"
#include "strutils.h"
#include "sysfs.h"
#include "colors.h"
#include "ttyutils.h"

#include "fdisk-list.h"

/* see init_fields() */
static const char *fields_string;
static int *fields_ids;
static size_t fields_nids;
static const struct fdisk_label *fields_label;

static int is_ide_cdrom_or_tape(char *device)
{
	int fd, ret;

	if ((fd = open(device, O_RDONLY|O_NONBLOCK)) < 0)
		return 0;
	ret = blkdev_is_cdrom(fd);

	close(fd);
	return ret;
}

void list_disk_identifier(struct fdisk_context *cxt)
{
	struct fdisk_label *lb = fdisk_get_label(cxt, NULL);
	char *id = NULL;

	if (fdisk_has_label(cxt))
		fdisk_info(cxt, _("Disklabel type: %s"),
				fdisk_label_get_name(lb));

	if (!fdisk_is_details(cxt) && fdisk_get_disklabel_id(cxt, &id) == 0 && id) {
		fdisk_info(cxt, _("Disk identifier: %s"), id);
		free(id);
	}
}

void list_disk_geometry(struct fdisk_context *cxt)
{
	struct fdisk_label *lb = fdisk_get_label(cxt, NULL);
	uint64_t bytes = fdisk_get_nsectors(cxt) * fdisk_get_sector_size(cxt);
	char *strsz = size_to_human_string(SIZE_DECIMAL_2DIGITS
					   | SIZE_SUFFIX_SPACE
					   | SIZE_SUFFIX_3LETTER, bytes);

	color_scheme_enable("header", UL_COLOR_BOLD);
	fdisk_info(cxt,	_("Disk %s: %s, %ju bytes, %ju sectors"),
			fdisk_get_devname(cxt), strsz,
			bytes, (uintmax_t) fdisk_get_nsectors(cxt));
	color_disable();
	free(strsz);

	if (fdisk_get_devmodel(cxt))
		fdisk_info(cxt, _("Disk model: %s"), fdisk_get_devmodel(cxt));

	if (lb && (fdisk_label_require_geometry(lb) || fdisk_use_cylinders(cxt)))
		fdisk_info(cxt, _("Geometry: %d heads, %ju sectors/track, %ju cylinders"),
			       fdisk_get_geom_heads(cxt),
			       (uintmax_t) fdisk_get_geom_sectors(cxt),
			       (uintmax_t) fdisk_get_geom_cylinders(cxt));

	fdisk_info(cxt, _("Units: %s of %d * %ld = %ld bytes"),
	       fdisk_get_unit(cxt, FDISK_PLURAL),
	       fdisk_get_units_per_sector(cxt),
	       fdisk_get_sector_size(cxt),
	       fdisk_get_units_per_sector(cxt) * fdisk_get_sector_size(cxt));

	fdisk_info(cxt, _("Sector size (logical/physical): %lu bytes / %lu bytes"),
				fdisk_get_sector_size(cxt),
				fdisk_get_physector_size(cxt));
	fdisk_info(cxt, _("I/O size (minimum/optimal): %lu bytes / %lu bytes"),
				fdisk_get_minimal_iosize(cxt),
				fdisk_get_optimal_iosize(cxt));
	if (fdisk_get_alignment_offset(cxt))
		fdisk_info(cxt, _("Alignment offset: %lu bytes"),
				fdisk_get_alignment_offset(cxt));

	list_disk_identifier(cxt);
}

void list_disklabel(struct fdisk_context *cxt)
{
	struct fdisk_table *tb = NULL;
	struct fdisk_partition *pa = NULL;
	struct fdisk_iter *itr = NULL;
	struct fdisk_label *lb;
	struct libscols_table *out = NULL;
	const char *bold = NULL;
	int *ids = NULL;		/* IDs of fdisk_fields */
	size_t	nids = 0, i;
	int post = 0;

	/* print label specific stuff by libfdisk FDISK_ASK_INFO API */
	fdisk_list_disklabel(cxt);

	/* get partitions and generate output */
	if (fdisk_get_partitions(cxt, &tb) || fdisk_table_get_nents(tb) <= 0)
		goto done;

	ids = init_fields(cxt, NULL, &nids);
	if (!ids)
		goto done;

	itr = fdisk_new_iter(FDISK_ITER_FORWARD);
	if (!itr) {
		fdisk_warn(cxt, _("failed to allocate iterator"));
		goto done;
	}

	out = scols_new_table();
	if (!out) {
		fdisk_warn(cxt, _("failed to allocate output table"));
		goto done;
	}

	if (colors_wanted()) {
		scols_table_enable_colors(out, 1);
		bold = color_scheme_get_sequence("header", UL_COLOR_BOLD);
	}

	lb = fdisk_get_label(cxt, NULL);
	assert(lb);

	/* define output table columns */
	for (i = 0; i < nids; i++) {
		int fl = 0;
		struct libscols_column *co;
		const struct fdisk_field *field =
				fdisk_label_get_field(lb, ids[i]);
		if (!field)
			continue;
		if (fdisk_field_is_number(field))
			fl |= SCOLS_FL_RIGHT;
		if (fdisk_field_get_id(field) == FDISK_FIELD_TYPE)
			fl |= SCOLS_FL_TRUNC;

		co = scols_table_new_column(out,
				_(fdisk_field_get_name(field)),
				fdisk_field_get_width(field), fl);
		if (!co)
			goto done;

		/* set column header color */
		if (bold)
			scols_cell_set_color(scols_column_get_header(co), bold);
	}

	/* fill-in output table */
	while (fdisk_table_next_partition(tb, itr, &pa) == 0) {
		struct libscols_line *ln = scols_table_new_line(out, NULL);

		if (!ln) {
			fdisk_warn(cxt, _("failed to allocate output line"));
			goto done;
		}

		for (i = 0; i < nids; i++) {
			char *data = NULL;

			if (fdisk_partition_to_string(pa, cxt, ids[i], &data))
				continue;
			if (scols_line_refer_data(ln, i, data)) {
				fdisk_warn(cxt, _("failed to add output data"));
				goto done;
			}
		}
	}

	/* print */
	if (!scols_table_is_empty(out)) {
		fdisk_info(cxt, "%s", "");	/* just line break */
		scols_print_table(out);
	}

	/* print warnings */
	fdisk_reset_iter(itr, FDISK_ITER_FORWARD);
	while (itr && fdisk_table_next_partition(tb, itr, &pa) == 0) {
		if (!fdisk_partition_has_start(pa))
			continue;
		if (!fdisk_lba_is_phy_aligned(cxt, fdisk_partition_get_start(pa))) {
			if (!post)
				fdisk_info(cxt, "%s", ""); /* line break */
			fdisk_warnx(cxt, _("Partition %zu does not start on physical sector boundary."),
					  fdisk_partition_get_partno(pa) + 1);
			post++;
		}
		if (fdisk_partition_has_wipe(cxt, pa)) {
			if (!post)
				fdisk_info(cxt, "%s", ""); /* line break */

			fdisk_info(cxt, _("Filesystem/RAID signature on partition %zu will be wiped."),
					fdisk_partition_get_partno(pa) + 1);
			post++;
		}
	}

	if (fdisk_table_wrong_order(tb)) {
		if (!post)
			fdisk_info(cxt, "%s", ""); /* line break */
		fdisk_info(cxt, _("Partition table entries are not in disk order."));
	}
done:
	scols_unref_table(out);
	fdisk_unref_table(tb);
	fdisk_free_iter(itr);
}

void list_freespace(struct fdisk_context *cxt)
{
	struct fdisk_table *tb = NULL;
	struct fdisk_partition *pa = NULL;
	struct fdisk_iter *itr = NULL;
	struct libscols_table *out = NULL;
	const char *bold = NULL;
	size_t i;
	uintmax_t sumsize = 0, bytes = 0;
	char *strsz;

	static const char *colnames[] = { N_("Start"), N_("End"), N_("Sectors"), N_("Size") };
	static const int colids[] = { FDISK_FIELD_START, FDISK_FIELD_END, FDISK_FIELD_SECTORS, FDISK_FIELD_SIZE };

	if (fdisk_get_freespaces(cxt, &tb))
		goto done;

	itr = fdisk_new_iter(FDISK_ITER_FORWARD);
	if (!itr) {
		fdisk_warn(cxt, _("failed to allocate iterator"));
		goto done;
	}

	out = scols_new_table();
	if (!out) {
		fdisk_warn(cxt, _("failed to allocate output table"));
		goto done;
	}

	if (colors_wanted()) {
		scols_table_enable_colors(out, 1);
		bold = color_scheme_get_sequence("header", UL_COLOR_BOLD);
	}

	for (i = 0; i < ARRAY_SIZE(colnames); i++) {
		struct libscols_column *co = scols_table_new_column(out, _(colnames[i]), 5, SCOLS_FL_RIGHT);

		if (!co)
			goto done;
		if (bold)
			scols_cell_set_color(scols_column_get_header(co), bold);
	}

	/* fill-in output table */
	while (fdisk_table_next_partition(tb, itr, &pa) == 0) {
		struct libscols_line *ln = scols_table_new_line(out, NULL);
		char *data;

		if (!ln) {
			fdisk_warn(cxt, _("failed to allocate output line"));
			goto done;
		}
		for (i = 0; i < ARRAY_SIZE(colids); i++) {
			if (fdisk_partition_to_string(pa, cxt, colids[i], &data))
				continue;
			if (scols_line_refer_data(ln, i, data)) {
				fdisk_warn(cxt, _("failed to add output data"));
				goto done;
			}
		}

		if (fdisk_partition_has_size(pa))
			sumsize += fdisk_partition_get_size(pa);
	}

	bytes = sumsize * fdisk_get_sector_size(cxt);
	strsz = size_to_human_string(SIZE_DECIMAL_2DIGITS
				     | SIZE_SUFFIX_SPACE
				     | SIZE_SUFFIX_3LETTER, bytes);

	color_scheme_enable("header", UL_COLOR_BOLD);
	fdisk_info(cxt,	_("Unpartitioned space %s: %s, %ju bytes, %ju sectors"),
			fdisk_get_devname(cxt), strsz,
			bytes, sumsize);
	color_disable();
	free(strsz);

	fdisk_info(cxt, _("Units: %s of %d * %ld = %ld bytes"),
	       fdisk_get_unit(cxt, FDISK_PLURAL),
	       fdisk_get_units_per_sector(cxt),
	       fdisk_get_sector_size(cxt),
	       fdisk_get_units_per_sector(cxt) * fdisk_get_sector_size(cxt));

	fdisk_info(cxt, _("Sector size (logical/physical): %lu bytes / %lu bytes"),
				fdisk_get_sector_size(cxt),
				fdisk_get_physector_size(cxt));

	/* print */
	if (!scols_table_is_empty(out)) {
		fdisk_info(cxt, "%s", "");	/* line break */
		scols_print_table(out);
	}
done:
	scols_unref_table(out);
	fdisk_unref_table(tb);
	fdisk_free_iter(itr);
}

char *next_proc_partition(FILE **f)
{
	char line[128 + 1];

	if (!*f) {
		*f = fopen(_PATH_PROC_PARTITIONS, "r");
		if (!*f) {
			warn(_("cannot open %s"), _PATH_PROC_PARTITIONS);
			return NULL;
		}
	}

	while (fgets(line, sizeof(line), *f)) {
		char buf[PATH_MAX], *cn;
		dev_t devno;

		if (sscanf(line, " %*d %*d %*d %128[^\n ]", buf) != 1)
			continue;

		devno = sysfs_devname_to_devno(buf);
		if (devno <= 0)
			continue;

		if (sysfs_devno_is_dm_private(devno, NULL) ||
		    sysfs_devno_is_wholedisk(devno) <= 0)
			continue;

		if (!sysfs_devno_to_devpath(devno, buf, sizeof(buf)))
			continue;

		cn = canonicalize_path(buf);
		if (!cn)
			continue;

		if (!is_ide_cdrom_or_tape(cn))
			return cn;
	}
	fclose(*f);
	*f = NULL;

	return NULL;
}

int print_device_pt(struct fdisk_context *cxt, char *device, int warnme,
		    int verify, int separator)
{
	if (fdisk_assign_device(cxt, device, 1) != 0) {	/* read-only */
		if (warnme || errno == EACCES)
			warn(_("cannot open %s"), device);
		return -1;
	}

	if (separator)
		fputs("\n\n", stdout);

	list_disk_geometry(cxt);

	if (fdisk_has_label(cxt)) {
		list_disklabel(cxt);
		if (verify)
			fdisk_verify_disklabel(cxt);
	}
	fdisk_deassign_device(cxt, 1);
	return 0;
}

int print_device_freespace(struct fdisk_context *cxt, char *device, int warnme,
			   int separator)
{
	if (fdisk_assign_device(cxt, device, 1) != 0) {	/* read-only */
		if (warnme || errno == EACCES)
			warn(_("cannot open %s"), device);
		return -1;
	}

	if (separator)
		fputs("\n\n", stdout);

	list_freespace(cxt);
	fdisk_deassign_device(cxt, 1);
	return 0;
}

void print_all_devices_pt(struct fdisk_context *cxt, int verify)
{
	FILE *f = NULL;
	int sep = 0;
	char *dev;

	while ((dev = next_proc_partition(&f))) {
		print_device_pt(cxt, dev, 0, verify, sep);
		free(dev);
		sep = 1;
	}
}

void print_all_devices_freespace(struct fdisk_context *cxt)
{
	FILE *f = NULL;
	int sep = 0;
	char *dev;

	while ((dev = next_proc_partition(&f))) {
		print_device_freespace(cxt, dev, 0, sep);
		free(dev);
		sep = 1;
	}
}

/* usable for example in usage() */
void list_available_columns(FILE *out)
{
	size_t i;
	int termwidth;
	struct fdisk_label *lb = NULL;
	struct fdisk_context *cxt = fdisk_new_context();

	if (!cxt)
		return;

	termwidth = get_terminal_width(80);

	fputs(USAGE_COLUMNS, out);

	while (fdisk_next_label(cxt, &lb) == 0) {
		size_t width = 6;	/* label name and separators */

		fprintf(out, " %s:", fdisk_label_get_name(lb));
		for (i = 1; i < FDISK_NFIELDS; i++) {
			const struct fdisk_field *fl = fdisk_label_get_field(lb, i);
			const char *name = fl ? fdisk_field_get_name(fl) : NULL;
			size_t len;

			if (!name)
				continue;
			len = strlen(name) + 1;
			if (width + len > (size_t) termwidth) {
				fputs("\n     ", out);
				width = 6;
			}
			fprintf(out, " %s", name);
			width += len;
		}
		fputc('\n', out);
	}

	fdisk_unref_context(cxt);
}

static int fieldname_to_id(const char *name, size_t namesz)
{
	const struct fdisk_field *fl;
	char *buf;

	assert(name);
	assert(namesz);
	assert(fields_label);

	buf = strndup(name, namesz);
	if (!buf)
		return -1;

	fl = fdisk_label_get_field_by_name(fields_label, buf);
	if (!fl) {
		warnx(_("%s unknown column: %s"),
				fdisk_label_get_name(fields_label), buf);
		free(buf);
		return -1;
	}
	free(buf);
	return fdisk_field_get_id(fl);
}

/*
 * Initialize array with output columns (fields_ids[]) according to
 * comma delimited list of columns (@str). If the list string is not
 * defined then use library defaults. This function is "-o <list>"
 * backend.
 *
 * If the columns are already initialized then returns already existing columns.
 */
int *init_fields(struct fdisk_context *cxt, const char *str, size_t *n)
{
	int *dflt_ids = NULL;
	struct fdisk_label *lb;

	if (!fields_string)
		fields_string = str;
	if (!cxt)
	       goto done;

	lb = fdisk_get_label(cxt, NULL);

	if (!lb || fields_label != lb) {	/* label changed: reset */
		free(fields_ids);
		fields_ids = NULL;
		fields_label = lb;
		fields_nids = 0;
	}

	if (!fields_label)	/*  no label */
		goto done;
	if (fields_nids)
		goto done;	/* already initialized */

	/* library default */
	if (fdisk_label_get_fields_ids(NULL, cxt, &dflt_ids, &fields_nids))
		goto done;

	fields_ids = xcalloc(FDISK_NFIELDS * 2, sizeof(int));

	/* copy defaults to the list with wanted fields */
	memcpy(fields_ids, dflt_ids, fields_nids * sizeof(int));
	free(dflt_ids);

	/* extend or replace fields_nids[] according to fields_string */
	if (fields_string &&
	    string_add_to_idarray(fields_string, fields_ids, FDISK_NFIELDS * 2,
			          &fields_nids, fieldname_to_id) < 0)
		exit(EXIT_FAILURE);
done:
	fields_label = NULL;
	if (n)
		*n = fields_nids;
	return fields_ids;
}
