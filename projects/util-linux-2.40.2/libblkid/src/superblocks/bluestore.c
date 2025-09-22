/*
 * Copyright (C) 2018 by Kenneth Van Alstyne <kvanals@kvanals.org>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 *
 * Ceph BlueStore is one of the supported storage
 * methods for Object Storage Devices (OSDs).
 * This is used to detect the backing block devices
 * used for these types of OSDs in a Ceph Cluster.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>

#include "bitops.h"
#include "superblocks.h"

#define BLUESTORE_MAGIC_L		22

struct bluestore_phdr {
	uint8_t		magic[BLUESTORE_MAGIC_L];
} __attribute__((packed));

static int probe_bluestore(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct bluestore_phdr *header;

	header = blkid_probe_get_sb(pr, mag, struct bluestore_phdr);
	if (header == NULL)
		return errno ? -errno : 1;

	return 0;
}

const struct blkid_idinfo bluestore_idinfo =
{
	.name		= "ceph_bluestore",
	.usage		= BLKID_USAGE_OTHER,
	.probefunc	= probe_bluestore,
	.magics		=
	{
		{ .magic = "bluestore block device", .len = 22 },
		{ NULL }
	}
};
