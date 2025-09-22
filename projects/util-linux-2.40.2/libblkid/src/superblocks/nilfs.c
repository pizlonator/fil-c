/*
 * Copyright (C) 2010 by Jiro SEKIBA <jir@unicus.jp>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License
 */
#include <stddef.h>
#include <string.h>

#include "superblocks.h"
#include "crc32.h"

struct nilfs_super_block {
	uint32_t	s_rev_level;
	uint16_t	s_minor_rev_level;
	uint16_t	s_magic;

	uint16_t	s_bytes;

	uint16_t	s_flags;
	uint32_t	s_crc_seed;
	uint32_t	s_sum;

	uint32_t	s_log_block_size;

	uint64_t	s_nsegments;
	uint64_t	s_dev_size;
	uint64_t	s_first_data_block;
	uint32_t	s_blocks_per_segment;
	uint32_t	s_r_segments_percentage;

	uint64_t	s_last_cno;
	uint64_t	s_last_pseg;
	uint64_t	s_last_seq;
	uint64_t	s_free_blocks_count;

	uint64_t	s_ctime;

	uint64_t	s_mtime;
	uint64_t	s_wtime;
	uint16_t	s_mnt_count;
	uint16_t	s_max_mnt_count;
	uint16_t	s_state;
	uint16_t	s_errors;
	uint64_t	s_lastcheck;

	uint32_t	s_checkinterval;
	uint32_t	s_creator_os;
	uint16_t	s_def_resuid;
	uint16_t	s_def_resgid;
	uint32_t	s_first_ino;

	uint16_t	s_inode_size;
	uint16_t	s_dat_entry_size;
	uint16_t	s_checkpoint_size;
	uint16_t	s_segment_usage_size;

	uint8_t		s_uuid[16];
	char		s_volume_name[80];

	uint32_t	s_c_interval;
	uint32_t	s_c_block_max;
	uint32_t	s_reserved[192];
} __attribute__((__packed__));

#define NILFS_SB_MAGIC		0x3434
#define NILFS_SB_OFFSET		0x400
#define NILFS_SBB_OFFSET(_sz)	((((_sz) / 0x200) - 8) * 0x200)

static int nilfs_valid_sb(blkid_probe pr, struct nilfs_super_block *sb, int is_bak)
{
	static unsigned char sum[4];
	const int sumoff = offsetof(struct nilfs_super_block, s_sum);
	size_t bytes;
	const size_t crc_start = sumoff + 4;
	uint32_t crc;

	if (!sb || le16_to_cpu(sb->s_magic) != NILFS_SB_MAGIC)
		return 0;

	if (is_bak && blkid_probe_is_wholedisk(pr) &&
	    sb->s_dev_size != pr->size)
		return 0;

	bytes = le16_to_cpu(sb->s_bytes);
	/* ensure that no underrun can happen in the length parameter
	 * of the crc32 call or more data are processed than read into
	 * sb */
	if (bytes < crc_start || bytes > sizeof(struct nilfs_super_block))
		return 0;

	crc = ul_crc32(le32_to_cpu(sb->s_crc_seed), (unsigned char *)sb, sumoff);
	crc = ul_crc32(crc, sum, 4);
	crc = ul_crc32(crc, (unsigned char *)sb + crc_start, bytes - crc_start);

	return blkid_probe_verify_csum(pr, crc, le32_to_cpu(sb->s_sum));
}

static int probe_nilfs2(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct nilfs_super_block *sb, *sbp, *sbb;
	int valid[2], swp = 0;
	uint64_t magoff;

	/* primary */
	sbp = (struct nilfs_super_block *) blkid_probe_get_buffer(
			pr, NILFS_SB_OFFSET, sizeof(struct nilfs_super_block));
	if (!sbp)
		return errno ? -errno : 1;

	valid[0] = nilfs_valid_sb(pr, sbp, 0);


	/* backup */
	sbb = (struct nilfs_super_block *) blkid_probe_get_buffer(
			pr, NILFS_SBB_OFFSET(pr->size), sizeof(struct nilfs_super_block));
	if (!sbb) {
		valid[1] = 0;

		/* If the primary block is valid then continue and ignore also
		 * I/O errors for backup block. Note the this is probably CD
		 * where I/O errors and the end of the disk/session are "normal".
		 */
		if (!valid[0])
			return errno ? -errno : 1;
	} else
		valid[1] = nilfs_valid_sb(pr, sbb, 1);


	/*
	 * Compare two super blocks and set 1 in swp if the secondary
	 * super block is valid and newer.  Otherwise, set 0 in swp.
	 */
	if (!valid[0] && !valid[1])
		return 1;

	swp = valid[1] && (!valid[0] ||
			   le64_to_cpu(sbp->s_last_cno) >
			   le64_to_cpu(sbb->s_last_cno));
	sb = swp ? sbb : sbp;

	DBG(LOWPROBE, ul_debug("nilfs2: primary=%d, backup=%d, swap=%d",
				valid[0], valid[1], swp));

	if (*(sb->s_volume_name) != '\0')
		blkid_probe_set_label(pr, (unsigned char *) sb->s_volume_name,
				      sizeof(sb->s_volume_name));

	blkid_probe_set_uuid(pr, sb->s_uuid);
	blkid_probe_sprintf_version(pr, "%u", le32_to_cpu(sb->s_rev_level));

	magoff = swp ? NILFS_SBB_OFFSET(pr->size) : NILFS_SB_OFFSET;
	magoff += offsetof(struct nilfs_super_block, s_magic);

	if (blkid_probe_set_magic(pr, magoff, sizeof(sb->s_magic),
				(unsigned char *) &sb->s_magic))
		return 1;

	if (le32_to_cpu(sb->s_log_block_size) < 32){
		blkid_probe_set_fsblocksize(pr, 1024U << le32_to_cpu(sb->s_log_block_size));
		blkid_probe_set_block_size(pr, 1024U << le32_to_cpu(sb->s_log_block_size));
	}

	return 0;
}

const struct blkid_idinfo nilfs2_idinfo =
{
	.name		= "nilfs2",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_nilfs2,
	/* default min.size is 128MiB, but 1MiB for "mkfs.nilfs2 -b 1024 -B 16" */
	.minsz		= (1024 * 1024),
	.magics		= BLKID_NONE_MAGIC
};
