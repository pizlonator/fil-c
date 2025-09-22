/*
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License
 */
#include <stddef.h>
#include <string.h>

#include "superblocks.h"
#include "crc32.h"

#define ZONEFS_MAGIC		"SFOZ" /* 0x5a4f4653 'Z' 'O' 'F' 'S' */
#define ZONEFS_MAGIC_SIZE	4
#define ZONEFS_MAGIC_OFST	0
#define ZONEFS_UUID_SIZE	16
#define ZONEFS_LABEL_SIZE	32
#define ZONEFS_SB_OFST		0

#define ZONEFS_BLOCK_SIZE	4096U

/* All in little-endian */
struct zonefs_super {

	/* Magic number */
	int32_t		s_magic;

	/* Checksum */
	int32_t		s_crc;

	/* Volume label */
	char		s_label[ZONEFS_LABEL_SIZE];

	/* 128-bit uuid */
	uint8_t		s_uuid[ZONEFS_UUID_SIZE];

	/* Features */
	int64_t		s_features;

	/* UID/GID to use for files */
	int32_t		s_uid;
	int32_t		s_gid;

	/* File permissions */
	int32_t		s_perm;

	/* Padding to 4096 bytes */
	uint8_t		s_reserved[4020];

} __attribute__ ((packed));

static int zonefs_verify_csum(blkid_probe pr, const struct zonefs_super *sb)
{
	uint32_t expected = le32_to_cpu(sb->s_crc);
	uint32_t crc = ul_crc32_exclude_offset(
			~0LL, (unsigned char *) sb, sizeof(*sb),
		       offsetof(__typeof__(*sb), s_crc), sizeof(sb->s_crc));
	return blkid_probe_verify_csum(pr, crc, expected);
}

static int probe_zonefs(blkid_probe pr,
		const struct blkid_idmag *mag  __attribute__((__unused__)))
{
	const struct zonefs_super *sb;

	sb = (struct zonefs_super *)
		blkid_probe_get_buffer(pr, ZONEFS_SB_OFST,
				       sizeof(struct zonefs_super));
	if (!sb)
		return errno ? -errno : 1;

	if (!zonefs_verify_csum(pr, sb))
		return 1;

	if (sb->s_label[0])
		blkid_probe_set_label(pr, (unsigned char *) sb->s_label,
				      sizeof(sb->s_label));

	blkid_probe_set_uuid(pr, sb->s_uuid);
	blkid_probe_set_fsblocksize(pr, ZONEFS_BLOCK_SIZE);
	blkid_probe_set_block_size(pr, ZONEFS_BLOCK_SIZE);

	return 0;
}

const struct blkid_idinfo zonefs_idinfo =
{
	.name           = "zonefs",
	.usage          = BLKID_USAGE_FILESYSTEM,
	.probefunc      = probe_zonefs,
	.magics         =
        {
		{
			.magic = (char *)ZONEFS_MAGIC,
			.len = ZONEFS_MAGIC_SIZE,
			.kboff = ZONEFS_SB_OFST,
			.sboff = ZONEFS_MAGIC_OFST,
		},
		{ NULL }
	}
};
