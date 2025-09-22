/*
 * Copyright (C) 1999, 2001 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

#include "superblocks.h"

struct romfs_super_block {
	unsigned char	ros_magic[8];
	uint32_t	ros_full_size;
	uint32_t	ros_checksum;
	unsigned char	ros_volume[16];
} __attribute__((packed));

static int romfs_verify_csum(blkid_probe pr, const struct blkid_idmag *mag,
		const struct romfs_super_block *ros)
{
	uint32_t csummed_size = min((uint32_t) 512,
			be32_to_cpu(ros->ros_full_size));
	const unsigned char *csummed;
	uint32_t csum;

	if (csummed_size % sizeof(uint32_t) != 0)
		return 0;

	csummed = blkid_probe_get_sb_buffer(pr, mag, csummed_size);
	if (!csummed)
		return 0;

	csum = 0;
	while (csummed_size) {
		csum += be32_to_cpu(*(uint32_t *) csummed);
		csummed_size -= sizeof(uint32_t);
		csummed += sizeof(uint32_t);
	}
	return blkid_probe_verify_csum(pr, csum, 0);
}

static int probe_romfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct romfs_super_block *ros;

	ros = blkid_probe_get_sb(pr, mag, struct romfs_super_block);
	if (!ros)
		return errno ? -errno : 1;

	if (!romfs_verify_csum(pr, mag, ros))
		return 1;

	if (*((char *) ros->ros_volume) != '\0')
		blkid_probe_set_label(pr, ros->ros_volume,
				sizeof(ros->ros_volume));

	blkid_probe_set_fsblocksize(pr, 1024);
	blkid_probe_set_fssize(pr, be32_to_cpu(ros->ros_full_size));
	blkid_probe_set_block_size(pr, 1024);

	return 0;
}

const struct blkid_idinfo romfs_idinfo =
{
	.name		= "romfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_romfs,
	.magics		=
	{
		{ .magic = "-rom1fs-", .len = 8 },
		{ NULL }
	}
};

