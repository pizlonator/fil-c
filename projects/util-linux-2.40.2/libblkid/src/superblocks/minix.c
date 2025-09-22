/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008-2013 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <string.h>
#include "superblocks.h"
#include "minix.h"

#define minix_swab16(doit, num)	((uint16_t) (doit ? swab16(num) : num))
#define minix_swab32(doit, num)	((uint32_t) (doit ? swab32(num) : num))

static int get_minix_version(const unsigned char *data, int *other_endian)
{
	const struct minix_super_block *sb = (const struct minix_super_block *) data;
	const struct minix3_super_block *sb3 = (const struct minix3_super_block *) data;
	int version = 0;
	char *endian;

	*other_endian = 0;

	switch (sb->s_magic) {
	case MINIX_SUPER_MAGIC:
	case MINIX_SUPER_MAGIC2:
		version = 1;
		break;
	case MINIX2_SUPER_MAGIC:
	case MINIX2_SUPER_MAGIC2:
		version = 2;
		break;
	default:
		if (sb3->s_magic == MINIX3_SUPER_MAGIC)
			version = 3;
		break;
	}

	if (!version) {
		*other_endian = 1;

		switch (swab16(sb->s_magic)) {
		case MINIX_SUPER_MAGIC:
		case MINIX_SUPER_MAGIC2:
			version = 1;
			break;
		case MINIX2_SUPER_MAGIC:
		case MINIX2_SUPER_MAGIC2:
			version = 2;
			break;
		default:
			if (sb3->s_magic == MINIX3_SUPER_MAGIC)
				version = 3;
			break;
		}
	}
	if (!version)
		return -1;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	endian = *other_endian ? "LE" : "BE";
#else
	endian = *other_endian ? "BE" : "LE";
#endif
	DBG(LOWPROBE, ul_debug("minix version %d detected [%s]", version,
			       endian));
	return version;
}

static int probe_minix(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	const unsigned char *ext;
	const unsigned char *data;
	int version = 0, swabme = 0;
	unsigned long zones, ninodes, imaps, zmaps;
	off_t firstz;
	size_t zone_size;
	unsigned block_size;

	data = blkid_probe_get_buffer(pr, 1024,
			max(sizeof(struct minix_super_block),
			    sizeof(struct minix3_super_block)));
	if (!data)
		return errno ? -errno : 1;
	version = get_minix_version(data, &swabme);
	switch (version) {
	case 1:
	case 2: {
		const struct minix_super_block *sb = (const struct minix_super_block *) data;

		uint16_t state = minix_swab16(swabme, sb->s_state);
		if ((state & (MINIX_VALID_FS | MINIX_ERROR_FS)) != state)
			return 1;

		zones = version == 2 ? minix_swab32(swabme, sb->s_zones) :
				       minix_swab16(swabme, sb->s_nzones);
		ninodes = minix_swab16(swabme, sb->s_ninodes);
		imaps   = minix_swab16(swabme, sb->s_imap_blocks);
		zmaps   = minix_swab16(swabme, sb->s_zmap_blocks);
		firstz  = minix_swab16(swabme, sb->s_firstdatazone);
		zone_size = sb->s_log_zone_size;
		block_size = 1024;
		break;
	}
	case 3: {
		const struct minix3_super_block *sb = (const struct minix3_super_block *) data;

		zones = minix_swab32(swabme, sb->s_zones);
		ninodes = minix_swab32(swabme, sb->s_ninodes);
		imaps   = minix_swab16(swabme, sb->s_imap_blocks);
		zmaps   = minix_swab16(swabme, sb->s_zmap_blocks);
		firstz  = minix_swab16(swabme, sb->s_firstdatazone);
		zone_size = sb->s_log_zone_size;
		block_size = minix_swab16(swabme, sb->s_blocksize);

		break;
	}
	default:
		return 1;
	}

	/* sanity checks to be sure that the FS is really minix.
	 * see disk-utils/fsck.minix.c read_superblock
	 */
	if (zone_size != 0 || ninodes == 0 || ninodes == UINT32_MAX)
		return 1;
	if (imaps * MINIX_BLOCK_SIZE * 8 < ninodes + 1)
		return 1;
	if (firstz > (off_t) zones)
		return 1;
	if (zmaps * MINIX_BLOCK_SIZE * 8 < zones - firstz + 1)
		return 1;

	/* unfortunately, some parts of ext3 is sometimes possible to
	 * interpreted as minix superblock. So check for extN magic
	 * string. (For extN magic string and offsets see ext.c.)
	 */
	ext = blkid_probe_get_buffer(pr, 0x400 + 0x38, 2);
	if (!ext)
		return errno ? -errno : 1;

	if (memcmp(ext, "\123\357", 2) == 0)
		return 1;

	blkid_probe_sprintf_version(pr, "%d", version);
	blkid_probe_set_fsblocksize(pr, block_size);
	blkid_probe_set_block_size(pr, block_size);
	blkid_probe_set_fsendianness(pr, !swabme ?
			BLKID_ENDIANNESS_NATIVE : BLKID_ENDIANNESS_OTHER);
	return 0;
}

const struct blkid_idinfo minix_idinfo =
{
	.name		= "minix",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_minix,
	.magics		=
	{
		/* version 1 - LE */
		{ .magic = "\177\023", .len = 2, .kboff = 1, .sboff = 0x10 },
		{ .magic = "\217\023", .len = 2, .kboff = 1, .sboff = 0x10 },

		/* version 1 - BE */
		{ .magic = "\023\177", .len = 2, .kboff = 1, .sboff = 0x10 },
		{ .magic = "\023\217", .len = 2, .kboff = 1, .sboff = 0x10 },

		/* version 2 - LE */
		{ .magic = "\150\044", .len = 2, .kboff = 1, .sboff = 0x10 },
		{ .magic = "\170\044", .len = 2, .kboff = 1, .sboff = 0x10 },

		/* version 2 - BE */
		{ .magic = "\044\150", .len = 2, .kboff = 1, .sboff = 0x10 },
		{ .magic = "\044\170", .len = 2, .kboff = 1, .sboff = 0x10 },

		/* version 3 - LE */
		{ .magic = "\132\115", .len = 2, .kboff = 1, .sboff = 0x18 },

		/* version 3 - BE */
		{ .magic = "\115\132", .len = 2, .kboff = 1, .sboff = 0x18 },

		{ NULL }
	}
};

