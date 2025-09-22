/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "superblocks.h"
#include "crc32.h"

struct cramfs_super
{
	uint8_t		magic[4];
	uint32_t	size;
	uint32_t	flags;
	uint32_t	future;
	uint8_t		signature[16];
	struct cramfs_info
	{
		uint32_t	crc;
		uint32_t	edition;
		uint32_t	blocks;
		uint32_t	files;
	} __attribute__((packed)) info;
	uint8_t		name[16];
} __attribute__((packed));

#define CRAMFS_FLAG_FSID_VERSION_2	0x00000001	/* fsid version #2 */

static uint32_t cfs32_to_cpu(int le, uint32_t value)
{
	if (le)
		return le32_to_cpu(value);
	else
		return be32_to_cpu(value);
}

static int cramfs_verify_csum(blkid_probe pr, const struct blkid_idmag *mag,
		const struct cramfs_super *cs, int le)
{
	uint32_t crc, expected, csummed_size;
	const unsigned char *csummed;

	expected = cfs32_to_cpu(le, cs->info.crc);
	csummed_size = cfs32_to_cpu(le, cs->size);

	if (csummed_size > (1 << 16)
	    || csummed_size < sizeof(struct cramfs_super))
		return 0;

	csummed = blkid_probe_get_sb_buffer(pr, mag, csummed_size);
	if (!csummed)
		return 0;

	crc = ~ul_crc32_exclude_offset(~0LL, csummed, csummed_size,
			offsetof(struct cramfs_super, info.crc),
			sizeof_member(struct cramfs_super, info.crc));

	return blkid_probe_verify_csum(pr, crc, expected);
}

static int probe_cramfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct cramfs_super *cs;

	cs = blkid_probe_get_sb(pr, mag, struct cramfs_super);
	if (!cs)
		return errno ? -errno : 1;

	int le = mag->hint == BLKID_ENDIANNESS_LITTLE;
	int v2 = cfs32_to_cpu(le, cs->flags) & CRAMFS_FLAG_FSID_VERSION_2;

	if (v2 && !cramfs_verify_csum(pr, mag, cs, le))
		return 1;

	blkid_probe_set_label(pr, cs->name, sizeof(cs->name));
	blkid_probe_set_fssize(pr, cfs32_to_cpu(le, cs->size));
	blkid_probe_sprintf_version(pr, "%d", v2 ? 2 : 1);
	blkid_probe_set_fsendianness(pr, mag->hint);
	return 0;
}

const struct blkid_idinfo cramfs_idinfo =
{
	.name		= "cramfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_cramfs,
	.magics		=
	{
		{ .magic = "\x45\x3d\xcd\x28", .len = 4,
		  .hint = BLKID_ENDIANNESS_LITTLE },
		{ .magic = "\x28\xcd\x3d\x45", .len = 4,
		  .hint = BLKID_ENDIANNESS_BIG },
		{ NULL }
	}
};


