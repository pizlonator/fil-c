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
#ifdef __linux__
#include <sys/utsname.h>
#endif
#include <time.h>

#include "superblocks.h"
#include "crc32c.h"

struct ext2_super_block {
	uint32_t		s_inodes_count;
	uint32_t		s_blocks_count;
	uint32_t		s_r_blocks_count;
	uint32_t		s_free_blocks_count;
	uint32_t		s_free_inodes_count;
	uint32_t		s_first_data_block;
	uint32_t		s_log_block_size;
	uint32_t		s_dummy3[7];
	unsigned char		s_magic[2];
	uint16_t		s_state;
	uint16_t		s_errors;
	uint16_t		s_minor_rev_level;
	uint32_t		s_lastcheck;
	uint32_t		s_checkinterval;
	uint32_t		s_creator_os;
	uint32_t		s_rev_level;
	uint16_t		s_def_resuid;
	uint16_t		s_def_resgid;
	uint32_t		s_first_ino;
	uint16_t		s_inode_size;
	uint16_t		s_block_group_nr;
	uint32_t		s_feature_compat;
	uint32_t		s_feature_incompat;
	uint32_t		s_feature_ro_compat;
	unsigned char		s_uuid[16];
	char			s_volume_name[16];
	char			s_last_mounted[64];
	uint32_t		s_algorithm_usage_bitmap;
	uint8_t			s_prealloc_blocks;
	uint8_t			s_prealloc_dir_blocks;
	uint16_t		s_reserved_gdt_blocks;
	uint8_t			s_journal_uuid[16];
	uint32_t		s_journal_inum;
	uint32_t		s_journal_dev;
	uint32_t		s_last_orphan;
	uint32_t		s_hash_seed[4];
	uint8_t			s_def_hash_version;
	uint8_t			s_jnl_backup_type;
	uint16_t		s_reserved_word_pad;
	uint32_t		s_default_mount_opts;
	uint32_t		s_first_meta_bg;
	uint32_t		s_mkfs_time;
	uint32_t		s_jnl_blocks[17];
	uint32_t		s_blocks_count_hi;
	uint32_t		s_r_blocks_count_hi;
	uint32_t		s_free_blocks_hi;
	uint16_t		s_min_extra_isize;
	uint16_t		s_want_extra_isize;
	uint32_t		s_flags;
	uint16_t		s_raid_stride;
	uint16_t		s_mmp_interval;
	uint64_t		s_mmp_block;
	uint32_t		s_raid_stripe_width;
	uint32_t		s_reserved[162];
	uint32_t		s_checksum;
} __attribute__((packed));

/* magic string */
#define EXT_SB_MAGIC				"\123\357"
/* supper block offset */
#define EXT_SB_OFF				0x400
/* supper block offset in kB */
#define EXT_SB_KBOFF				(EXT_SB_OFF >> 10)
/* magic string offset within super block */
#define EXT_MAG_OFF				0x38



/* for s_flags */
#define EXT2_FLAGS_TEST_FILESYS		0x0004

/* for s_feature_compat */
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004

/* for s_feature_ro_compat */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE	0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM		0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK	0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE	0x0040
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM	0x0400

/* for s_feature_incompat */
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS		0x0040 /* extents support */
#define EXT4_FEATURE_INCOMPAT_64BIT		0x0080
#define EXT4_FEATURE_INCOMPAT_MMP		0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200

#define EXT2_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED	~EXT2_FEATURE_INCOMPAT_SUPP
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT2_FEATURE_RO_COMPAT_SUPP

#define EXT3_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT3_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT3_FEATURE_INCOMPAT_RECOVER| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT3_FEATURE_INCOMPAT_UNSUPPORTED	~EXT3_FEATURE_INCOMPAT_SUPP
#define EXT3_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT3_FEATURE_RO_COMPAT_SUPP

/*
 * Starting in 2.6.29, ext4 can be used to support filesystems
 * without a journal.
 */
#define EXT4_SUPPORTS_EXT2 KERNEL_VERSION(2, 6, 29)

/*
 * reads superblock and returns:
 *	fc = feature_compat
 *	fi = feature_incompat
 *	frc = feature_ro_compat
 */
static struct ext2_super_block *ext_get_super(
		blkid_probe pr, uint32_t *fc, uint32_t *fi, uint32_t *frc)
{
	struct ext2_super_block *es;

	es = (struct ext2_super_block *)
			blkid_probe_get_buffer(pr, EXT_SB_OFF, sizeof(struct ext2_super_block));
	if (!es)
		return NULL;
	if (le32_to_cpu(es->s_feature_ro_compat) & EXT4_FEATURE_RO_COMPAT_METADATA_CSUM) {
		uint32_t csum = crc32c(~0, es, offsetof(struct ext2_super_block, s_checksum));
		if (!blkid_probe_verify_csum(pr, csum, le32_to_cpu(es->s_checksum)))
			return NULL;
	}
	if (fc)
		*fc = le32_to_cpu(es->s_feature_compat);
	if (fi)
		*fi = le32_to_cpu(es->s_feature_incompat);
	if (frc)
		*frc = le32_to_cpu(es->s_feature_ro_compat);

	return es;
}

static void ext_get_info(blkid_probe pr, int ver, struct ext2_super_block *es)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);
	uint32_t s_feature_incompat = le32_to_cpu(es->s_feature_incompat);

	DBG(PROBE, ul_debug("ext2_sb.compat = %08X:%08X:%08X",
		   le32_to_cpu(es->s_feature_compat),
		   s_feature_incompat,
		   le32_to_cpu(es->s_feature_ro_compat)));

	if (*es->s_volume_name != '\0')
		blkid_probe_set_label(pr, (unsigned char *) es->s_volume_name,
					sizeof(es->s_volume_name));
	blkid_probe_set_uuid(pr, es->s_uuid);

	if (le32_to_cpu(es->s_feature_compat) & EXT3_FEATURE_COMPAT_HAS_JOURNAL)
		blkid_probe_set_uuid_as(pr, es->s_journal_uuid, "EXT_JOURNAL");

	if (ver != 2 && (chn->flags & BLKID_SUBLKS_SECTYPE) &&
	    ((s_feature_incompat & EXT2_FEATURE_INCOMPAT_UNSUPPORTED) == 0))
		blkid_probe_set_value(pr, "SEC_TYPE",
				(unsigned char *) "ext2",
				sizeof("ext2"));

	blkid_probe_sprintf_version(pr, "%u.%u",
		le32_to_cpu(es->s_rev_level),
		le16_to_cpu(es->s_minor_rev_level));

	uint32_t block_size = 0;
	if (le32_to_cpu(es->s_log_block_size) < 32){
		block_size = 1024U << le32_to_cpu(es->s_log_block_size);
		blkid_probe_set_fsblocksize(pr, block_size);
		blkid_probe_set_block_size(pr, block_size);
	}

	uint64_t fslastblock = le32_to_cpu(es->s_blocks_count) |
		((s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) ?
		(uint64_t) le32_to_cpu(es->s_blocks_count_hi) << 32 : 0);
	blkid_probe_set_fslastblock(pr, fslastblock);

	/* The total number of blocks is taken without substraction of overhead
	 * (journal, metadata). The ext4 has non-trivial overhead calculation
	 * viz. ext4_calculate_overhead(). Thefore, the FSSIZE would show number
	 * slightly higher than the real value (for example, calculated via
	 * statfs()).
	 */
	uint64_t fssize = (uint64_t) block_size * le32_to_cpu(es->s_blocks_count);
	blkid_probe_set_fssize(pr, fssize);
}


static int probe_jbd(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct ext2_super_block *es;
	uint32_t fi;

	es = ext_get_super(pr, NULL, &fi, NULL);
	if (!es)
		return errno ? -errno : 1;
	if (!(fi & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV))
		return 1;

	ext_get_info(pr, 2, es);
	blkid_probe_set_uuid_as(pr, es->s_uuid, "LOGUUID");

	return 0;
}

static int probe_ext2(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct ext2_super_block *es;
	uint32_t fc, frc, fi;

	es = ext_get_super(pr, &fc, &fi, &frc);
	if (!es)
		return errno ? -errno : 1;

	/* Distinguish between ext3 and ext2 */
	if (fc & EXT3_FEATURE_COMPAT_HAS_JOURNAL)
		return 1;

	/* Any features which ext2 doesn't understand */
	if ((frc & EXT2_FEATURE_RO_COMPAT_UNSUPPORTED) ||
	    (fi  & EXT2_FEATURE_INCOMPAT_UNSUPPORTED))
		return 1;

	ext_get_info(pr, 2, es);
	return 0;
}

static int probe_ext3(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct ext2_super_block *es;
	uint32_t fc, frc, fi;

	es = ext_get_super(pr, &fc, &fi, &frc);
	if (!es)
		return errno ? -errno : 1;

	/* ext3 requires journal */
	if (!(fc & EXT3_FEATURE_COMPAT_HAS_JOURNAL))
		return 1;

	/* Any features which ext3 doesn't understand */
	if ((frc & EXT3_FEATURE_RO_COMPAT_UNSUPPORTED) ||
	    (fi  & EXT3_FEATURE_INCOMPAT_UNSUPPORTED))
		return 1;

	ext_get_info(pr, 3, es);
	return 0;
}


static int probe_ext4dev(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct ext2_super_block *es;
	uint32_t fc, frc, fi;

	es = ext_get_super(pr, &fc, &fi, &frc);
	if (!es)
		return errno ? -errno : 1;

	/* Distinguish from jbd */
	if (fi & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)
		return 1;

	if (!(le32_to_cpu(es->s_flags) & EXT2_FLAGS_TEST_FILESYS))
		return 1;

	ext_get_info(pr, 4, es);
	return 0;
}

static int probe_ext4(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct ext2_super_block *es;
	uint32_t fc, frc, fi;

	es = ext_get_super(pr, &fc, &fi, &frc);
	if (!es)
		return errno ? -errno : 1;

	/* Distinguish from jbd */
	if (fi & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)
		return 1;

	/* Ext4 has at least one feature which ext3 doesn't understand */
	if (!(frc & EXT3_FEATURE_RO_COMPAT_UNSUPPORTED) &&
	    !(fi  & EXT3_FEATURE_INCOMPAT_UNSUPPORTED))
		return 1;

	/*
	 * If the filesystem is a OK for use by in-development
	 * filesystem code, and ext4dev is supported or ext4 is not
	 * supported, then don't call ourselves ext4, so we can redo
	 * the detection and mark the filesystem as ext4dev.
	 *
	 * If the filesystem is marked as in use by production
	 * filesystem, then it can only be used by ext4 and NOT by
	 * ext4dev.
	 */
	if (le32_to_cpu(es->s_flags) & EXT2_FLAGS_TEST_FILESYS)
		return 1;

	ext_get_info(pr, 4, es);
	return 0;
}

#define BLKID_EXT_MAGICS \
	{ \
		{	 \
			.magic = EXT_SB_MAGIC, \
			.len = sizeof(EXT_SB_MAGIC) - 1, \
			.kboff = EXT_SB_KBOFF, \
			.sboff = EXT_MAG_OFF \
		}, \
		{ NULL } \
	}

const struct blkid_idinfo jbd_idinfo =
{
	.name		= "jbd",
	.usage		= BLKID_USAGE_OTHER,
	.probefunc	= probe_jbd,
	.magics		= BLKID_EXT_MAGICS
};

const struct blkid_idinfo ext2_idinfo =
{
	.name		= "ext2",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ext2,
	.magics		= BLKID_EXT_MAGICS
};

const struct blkid_idinfo ext3_idinfo =
{
	.name		= "ext3",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ext3,
	.magics		= BLKID_EXT_MAGICS
};

const struct blkid_idinfo ext4_idinfo =
{
	.name		= "ext4",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ext4,
	.magics		= BLKID_EXT_MAGICS
};

const struct blkid_idinfo ext4dev_idinfo =
{
	.name		= "ext4dev",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ext4dev,
	.magics		= BLKID_EXT_MAGICS
};

