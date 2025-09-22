/*
 * lsfd-bdev.c - handle associations opening block devices
 *
 * Copyright (C) 2021 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "lsfd.h"

static struct list_head partitions;

struct partition {
	struct list_head partitions;
	dev_t dev;
	char *name;
};

static bool bdev_fill_column(struct proc *proc __attribute__((__unused__)),
			     struct file *file __attribute__((__unused__)),
			     struct libscols_line *ln,
			     int column_id,
			     size_t column_index)
{
	char *str = NULL;
	const char *partition, *devdrv;

	switch(column_id) {
	case COL_TYPE:
		if (scols_line_set_data(ln, column_index, "BLK"))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_BLKDRV:
		devdrv = get_blkdrv(major(file->stat.st_rdev));
		if (devdrv)
			str = xstrdup(devdrv);
		else
			xasprintf(&str, "%u",
				  major(file->stat.st_rdev));
		break;
	case COL_DEVTYPE:
		if (scols_line_set_data(ln, column_index,
					"blk"))
			err(EXIT_FAILURE, _("failed to add output data"));
		return true;
	case COL_SOURCE:
	case COL_PARTITION:
		partition = get_partition(file->stat.st_rdev);
		if (partition) {
			str = xstrdup(partition);
			break;
		}
		devdrv = get_blkdrv(major(file->stat.st_rdev));
		if (devdrv) {
			xasprintf(&str, "%s:%u", devdrv,
				  minor(file->stat.st_rdev));
			break;
		}
		/* FALL THROUGH */
	case COL_MAJMIN:
		xasprintf(&str, "%u:%u",
			  major(file->stat.st_rdev),
			  minor(file->stat.st_rdev));
		break;
	default:
		return false;
	}

	if (!str)
		err(EXIT_FAILURE, _("failed to add output data"));
	if (scols_line_refer_data(ln, column_index, str))
		err(EXIT_FAILURE, _("failed to add output data"));
	return true;
}

static struct partition *new_partition(dev_t dev, const char *name)
{
	struct partition *partition = xcalloc(1, sizeof(*partition));

	INIT_LIST_HEAD(&partition->partitions);

	partition->dev = dev;
	partition->name = xstrdup(name);

	return partition;
}

static void free_partition(struct partition *partition)
{
	free(partition->name);
	free(partition);
}

static void read_partitions(struct list_head *partitions_list, FILE *part_fp)
{
	unsigned int major, minor;
	char line[256];
	char name[sizeof(line)];

	while (fgets(line, sizeof(line), part_fp)) {
		struct partition *partition;

		if (sscanf(line, "%u %u %*u %s", &major, &minor, name) != 3)
			continue;
		partition = new_partition(makedev(major, minor), name);
		list_add_tail(&partition->partitions, partitions_list);
	}
}

static void bdev_class_initialize(void)
{
	FILE *part_fp;

	INIT_LIST_HEAD(&partitions);

	part_fp = fopen("/proc/partitions", "r");
	if (part_fp) {
		read_partitions(&partitions, part_fp);
		fclose(part_fp);
	}
}

static void bdev_class_finalize(void)
{
	list_free(&partitions, struct partition,  partitions, free_partition);
}

const char *get_partition(dev_t dev)
{
	struct list_head *p;
	list_for_each(p, &partitions) {
		struct partition *partition = list_entry(p, struct partition, partitions);
		if (partition->dev == dev)
			return partition->name;
	}
	return NULL;
}

const struct file_class bdev_class = {
	.super = &file_class,
	.size = sizeof(struct file),
	.initialize_class = bdev_class_initialize,
	.finalize_class = bdev_class_finalize,
	.fill_column = bdev_fill_column,
	.free_content = NULL,
};
