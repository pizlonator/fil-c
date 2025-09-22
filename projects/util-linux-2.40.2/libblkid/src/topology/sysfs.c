/*
 * sysfs based topology -- gathers topology information from Linux sysfs
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * For more information see Linux kernel Documentation/ABI/testing/sysfs-block.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "sysfs.h"
#include "topology.h"

/*
 * Sysfs topology values (since 2.6.31, May 2009).
 */
static const struct topology_val {

	/* /sys/dev/block/<maj>:<min>/<ATTR> */
	const char * const attr;

	/* functions to set probing result */
	int (*set_ulong)(blkid_probe, unsigned long);
	int (*set_int)(blkid_probe, int);
	int (*set_u64)(blkid_probe, uint64_t);

} topology_vals[] = {
	{ "alignment_offset", NULL, blkid_topology_set_alignment_offset },
	{ "queue/minimum_io_size", blkid_topology_set_minimum_io_size },
	{ "queue/optimal_io_size", blkid_topology_set_optimal_io_size },
	{ "queue/physical_block_size", blkid_topology_set_physical_sector_size },
	{ "queue/dax", blkid_topology_set_dax },
	{ "diskseq", .set_u64 = blkid_topology_set_diskseq },
};

static int probe_sysfs_tp(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	dev_t dev;
	int rc, set_parent = 1;
	struct path_cxt *pc;
	size_t i, count = 0;

	dev = blkid_probe_get_devno(pr);
	if (!dev)
		return 1;
	pc = ul_new_sysfs_path(dev, NULL, NULL);
	if (!pc)
		return 1;

	rc = 1;		/* nothing (default) */

	for (i = 0; i < ARRAY_SIZE(topology_vals); i++) {
		const struct topology_val *val = &topology_vals[i];
		int ok = ul_path_access(pc, F_OK, val->attr) == 0;

		rc = 1;	/* nothing */

		if (!ok && set_parent) {
			dev_t disk = blkid_probe_get_wholedisk_devno(pr);
			set_parent = 0;

			/*
			 * Read attributes from "disk" if the current device is
			 * a partition. Note that sysfs ul_path_* API is able
			 * to redirect requests to attributes if parent is set.
			 */
			if (disk && disk != dev) {
				struct path_cxt *parent = ul_new_sysfs_path(disk, NULL, NULL);
				if (!parent)
					goto done;

				sysfs_blkdev_set_parent(pc, parent);
				ul_unref_path(parent);

				/* try it again */
				ok = ul_path_access(pc, F_OK, val->attr) == 0;
			}
		}
		if (!ok)
			continue;	/* attribute does not exist */

		if (val->set_ulong) {
			uint64_t data;

			if (ul_path_read_u64(pc, &data, val->attr) != 0)
				continue;
			rc = val->set_ulong(pr, (unsigned long) data);

		} else if (val->set_int) {
			int64_t data;

			if (ul_path_read_s64(pc, &data, val->attr) != 0)
				continue;
			rc = val->set_int(pr, (int) data);
		} else if (val->set_u64) {
			uint64_t data;

			if (ul_path_read_u64(pc, &data, val->attr) != 0)
				continue;
			rc = val->set_u64(pr, data);
		}

		if (rc < 0)
			goto done;	/* error */
		if (rc == 0)
			count++;
	}

done:
	ul_unref_path(pc);		/* unref pc and parent */
	if (count)
		return 0;		/* success */
	return rc;			/* error or nothing */
}

const struct blkid_idinfo sysfs_tp_idinfo =
{
	.name		= "sysfs",
	.probefunc	= probe_sysfs_tp,
	.magics		= BLKID_NONE_MAGIC
};

