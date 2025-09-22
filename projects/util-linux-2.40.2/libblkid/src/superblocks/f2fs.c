/*
 * Copyright (C) 2013 Alejandro Martinez Ruiz <alex@nowcomputing.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License
 */

#include <stddef.h>
#include <string.h>

#include "superblocks.h"
#include "crc32.h"

#define F2FS_MAGIC		"\x10\x20\xF5\xF2"
#define F2FS_MAGIC_OFF		0
#define F2FS_UUID_SIZE		16
#define F2FS_LABEL_SIZE		512
#define F2FS_SB1_OFF		0x400
#define F2FS_SB1_KBOFF		(F2FS_SB1_OFF >> 10)
#define F2FS_SB2_OFF		0x1400
#define F2FS_SB2_KBOFF		(F2FS_SB2_OFF >> 10)

struct f2fs_super_block {					/* According to version 1.1 */
/* 0x00 */	uint32_t	magic;				/* Magic Number */
/* 0x04 */	uint16_t	major_ver;			/* Major Version */
/* 0x06 */	uint16_t	minor_ver;			/* Minor Version */
/* 0x08 */	uint32_t	log_sectorsize;			/* log2 sector size in bytes */
/* 0x0C */	uint32_t	log_sectors_per_block;		/* log2 # of sectors per block */
/* 0x10 */	uint32_t	log_blocksize;			/* log2 block size in bytes */
/* 0x14 */	uint32_t	log_blocks_per_seg;		/* log2 # of blocks per segment */
/* 0x18 */	uint32_t	segs_per_sec;			/* # of segments per section */
/* 0x1C */	uint32_t	secs_per_zone;			/* # of sections per zone */
/* 0x20 */	uint32_t	checksum_offset;		/* checksum offset inside super block */
/* 0x24 */	uint64_t	block_count;			/* total # of user blocks */
/* 0x2C */	uint32_t	section_count;			/* total # of sections */
/* 0x30 */	uint32_t	segment_count;			/* total # of segments */
/* 0x34 */	uint32_t	segment_count_ckpt;		/* # of segments for checkpoint */
/* 0x38 */	uint32_t	segment_count_sit;		/* # of segments for SIT */
/* 0x3C */	uint32_t	segment_count_nat;		/* # of segments for NAT */
/* 0x40 */	uint32_t	segment_count_ssa;		/* # of segments for SSA */
/* 0x44 */	uint32_t	segment_count_main;		/* # of segments for main area */
/* 0x48 */	uint32_t	segment0_blkaddr;		/* start block address of segment 0 */
/* 0x4C */	uint32_t	cp_blkaddr;			/* start block address of checkpoint */
/* 0x50 */	uint32_t	sit_blkaddr;			/* start block address of SIT */
/* 0x54 */	uint32_t	nat_blkaddr;			/* start block address of NAT */
/* 0x58 */	uint32_t	ssa_blkaddr;			/* start block address of SSA */
/* 0x5C */	uint32_t	main_blkaddr;			/* start block address of main area */
/* 0x60 */	uint32_t	root_ino;			/* root inode number */
/* 0x64 */	uint32_t	node_ino;			/* node inode number */
/* 0x68 */	uint32_t	meta_ino;			/* meta inode number */
/* 0x6C */	uint8_t		uuid[F2FS_UUID_SIZE];		/* 128-bit uuid for volume */
/* 0x7C */	uint16_t	volume_name[F2FS_LABEL_SIZE];	/* volume name */
#if 0
/* 0x47C */	uint32_t	extension_count;		/* # of extensions below */
/* 0x480 */	uint8_t		extension_list[64][8];		/* extension array */
#endif
} __attribute__((packed));

static int f2fs_validate_checksum(blkid_probe pr, size_t sb_off,
		const struct f2fs_super_block *sb)
{
	uint32_t csum_off = le32_to_cpu(sb->checksum_offset);
	if (!csum_off)
		return 1;
	if (csum_off % sizeof(uint32_t) != 0)
		return 0;
	if (csum_off + sizeof(uint32_t) > 4096)
		return 0;

	const unsigned char *csum_data = blkid_probe_get_buffer(pr,
			sb_off + csum_off, sizeof(uint32_t));
	if (!csum_data)
		return 0;

	uint32_t expected = le32_to_cpu(*(uint32_t *) csum_data);

	const unsigned char *csummed = blkid_probe_get_buffer(pr, sb_off, csum_off);
	if (!csummed)
		return 0;

	uint32_t csum = ul_crc32(0xF2F52010, csummed, csum_off);

	return blkid_probe_verify_csum(pr, csum, expected);
}

static int probe_f2fs(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct f2fs_super_block *sb;
	uint16_t vermaj, vermin;

	sb = blkid_probe_get_sb(pr, mag, struct f2fs_super_block);
	if (!sb)
		return errno ? -errno : 1;

	vermaj = le16_to_cpu(sb->major_ver);
	vermin = le16_to_cpu(sb->minor_ver);

	/* For version 1.0 we cannot know the correct sb structure */
	if (vermaj == 1 && vermin == 0)
		return 0;

	if (!f2fs_validate_checksum(pr, mag->kboff << 10, sb))
		return 1;

	if (*((unsigned char *) sb->volume_name))
		blkid_probe_set_utf8label(pr, (unsigned char *) sb->volume_name,
						sizeof(sb->volume_name),
						UL_ENCODE_UTF16LE);

	blkid_probe_set_uuid(pr, sb->uuid);
	blkid_probe_sprintf_version(pr, "%u.%u", vermaj, vermin);
	if (le32_to_cpu(sb->log_blocksize) < 32){
		uint32_t blocksize = 1U << le32_to_cpu(sb->log_blocksize);
		blkid_probe_set_fsblocksize(pr, blocksize);
		blkid_probe_set_block_size(pr, blocksize);
		blkid_probe_set_fssize(pr, le64_to_cpu(sb->block_count) * blocksize);
	}
	return 0;
}

const struct blkid_idinfo f2fs_idinfo =
{
	.name           = "f2fs",
	.usage          = BLKID_USAGE_FILESYSTEM,
	.probefunc      = probe_f2fs,
	.magics         =
        {
		{
			.magic = F2FS_MAGIC,
			.len = 4,
			.kboff = F2FS_SB1_KBOFF,
			.sboff = F2FS_MAGIC_OFF
		},
		{ NULL }
	}
};
