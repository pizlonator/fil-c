/*
 * Copyright (C) 2018 Harry Mallon <hjmallon@gmail.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include "superblocks.h"

#define APFS_CONTAINER_SUPERBLOCK_TYPE 1
#define APFS_CONTAINER_SUPERBLOCK_SUBTYPE 0
#define APFS_STANDARD_BLOCK_SIZE 4096

/*
 * This struct is much longer than this, but this seems
 * to contain the useful bits (for now).
 *
 * All values are little-endian.
 */
struct apfs_super_block {
	// Generic part to all APFS objects
	uint64_t checksum;
	uint64_t oid;
	uint64_t xid;
	uint16_t type;
	uint16_t flags;
	uint16_t subtype;
	uint16_t pad;

	// Specific to container header
	uint32_t magic; // 'NXSB'
	uint32_t block_size;
	uint64_t block_count;
	uint64_t features;
	uint64_t read_only_features;
	uint64_t incompatible_features;
	uint8_t uuid[16];
};

static int probe_apfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct apfs_super_block *sb;

	sb = blkid_probe_get_sb(pr, mag, struct apfs_super_block);
	if (!sb)
		return errno ? -errno : BLKID_PROBE_NONE;

	if (le16_to_cpu(sb->type) != APFS_CONTAINER_SUPERBLOCK_TYPE)
		return BLKID_PROBE_NONE;

	if (le16_to_cpu(sb->subtype) != APFS_CONTAINER_SUPERBLOCK_SUBTYPE)
		return BLKID_PROBE_NONE;

	if (le16_to_cpu(sb->pad) != 0)
		return BLKID_PROBE_NONE;

	/*
	 * This check is pretty draconian, but should avoid false
	 * positives. Can be improved as more APFS documentation
	 * is published.
	 */
	if (le32_to_cpu(sb->block_size) != APFS_STANDARD_BLOCK_SIZE)
		return BLKID_PROBE_NONE;

	if (blkid_probe_set_uuid(pr, sb->uuid) < 0)
		return BLKID_PROBE_NONE;

	blkid_probe_set_fsblocksize(pr, le32_to_cpu(sb->block_size));
	blkid_probe_set_block_size(pr, le32_to_cpu(sb->block_size));

	return BLKID_PROBE_OK;
}

const struct blkid_idinfo apfs_idinfo =
{
	.name		= "apfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_apfs,
	.magics		=
	{
		{ .magic = "NXSB", .len = 4, .sboff = 32 },
		{ NULL }
	}
};
