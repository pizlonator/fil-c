/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2012 Milan Broz <gmazyland@gmail.com>
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

#define LVM1_ID_LEN 128
#define LVM2_ID_LEN 32

struct lvm2_pv_label_header {
	/* label_header */
	uint8_t		id[8];		/* LABELONE */
	uint64_t	sector_xl;	/* Sector number of this label */
	uint32_t	crc_xl;		/* From next field to end of sector */
	uint32_t	offset_xl;	/* Offset from start of struct to contents */
	uint8_t		type[8];	/* LVM2 001 */
	/* pv_header */
	uint8_t		pv_uuid[LVM2_ID_LEN];
} __attribute__ ((packed));

struct lvm1_pv_label_header {
	uint8_t id[2];			/* HM */
	uint16_t version;		/* version 1 or 2 */
	uint32_t _notused[10];		/* lvm1 internals */
	uint8_t pv_uuid[LVM1_ID_LEN];
} __attribute__ ((packed));

#define LVM2_LABEL_SIZE 512
static unsigned int lvm2_calc_crc(const void *buf, unsigned int size)
{
	static const unsigned int crctab[] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	unsigned int i, crc = 0xf597a6cf;
	const uint8_t *data = (const uint8_t *) buf;

	for (i = 0; i < size; i++) {
		crc ^= *data++;
		crc = (crc >> 4) ^ crctab[crc & 0xf];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
	}
	return crc;
}

/* Length of real UUID is always LVM2_ID_LEN */
static void format_lvm_uuid(char *dst_uuid, char *src_uuid)
{
	unsigned int i, b;

	for (i = 0, b = 1; i < LVM2_ID_LEN; i++, b <<= 1) {
		if (b & 0x4444440)
			*dst_uuid++ = '-';
		*dst_uuid++ = *src_uuid++;
	}
	*dst_uuid = '\0';
}

static int probe_lvm2(blkid_probe pr, const struct blkid_idmag *mag)
{
	int sector = mag->kboff << 1;
	struct lvm2_pv_label_header *label;
	char uuid[LVM2_ID_LEN + 7];
	const unsigned char *buf;

	buf = blkid_probe_get_buffer(pr,
			mag->kboff << 10,
			512 + LVM2_LABEL_SIZE);
	if (!buf)
		return errno ? -errno : 1;

	/* buf is at 0k or 1k offset; find label inside */
	if (memcmp(buf, "LABELONE", 8) == 0) {
		label = (struct lvm2_pv_label_header *) buf;
	} else if (memcmp(buf + 512, "LABELONE", 8) == 0) {
		label = (struct lvm2_pv_label_header *)(buf + 512);
		sector++;
	} else {
		return 1;
	}

	if (le64_to_cpu(label->sector_xl) != (unsigned) sector)
		return 1;

	if (!blkid_probe_verify_csum(
		pr, lvm2_calc_crc(
			&label->offset_xl, LVM2_LABEL_SIZE -
			((char *) &label->offset_xl - (char *) label)),
			le32_to_cpu(label->crc_xl)))
		return 1;

	format_lvm_uuid(uuid, (char *) label->pv_uuid);
	blkid_probe_sprintf_uuid(pr, label->pv_uuid, sizeof(label->pv_uuid),
			"%s", uuid);

	/* the mag->magic is the same string as label->type,
	 * but zero terminated */
	blkid_probe_set_version(pr, mag->magic);

	/* LVM (pvcreate) wipes begin of the device -- let's remember this
	 * to resolve conflicts between LVM and partition tables, ...
	 */
	blkid_probe_set_wiper(pr, 0, 8 * 1024);

	return 0;
}

static int probe_lvm1(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct lvm1_pv_label_header *label;
	char uuid[LVM2_ID_LEN + 7];
	unsigned int version;

	label = blkid_probe_get_sb(pr, mag, struct lvm1_pv_label_header);
	if (!label)
		return errno ? -errno : 1;

	version = le16_to_cpu(label->version);
	if (version != 1 && version != 2)
		return 1;

	format_lvm_uuid(uuid, (char *) label->pv_uuid);
	blkid_probe_sprintf_uuid(pr, label->pv_uuid, sizeof(label->pv_uuid),
			"%s", uuid);

	return 0;
}

struct verity_sb {
	uint8_t  signature[8];	/* "verity\0\0" */
	uint32_t version;	/* superblock version */
	uint32_t hash_type;	/* 0 - Chrome OS, 1 - normal */
	uint8_t  uuid[16];	/* UUID of hash device */
	uint8_t  algorithm[32];/* hash algorithm name */
	uint32_t data_block_size; /* data block in bytes */
	uint32_t hash_block_size; /* hash block in bytes */
	uint64_t data_blocks;	/* number of data blocks */
	uint16_t salt_size;	/* salt size */
	uint8_t  _pad1[6];
	uint8_t  salt[256];	/* salt */
	uint8_t  _pad2[168];
} __attribute__((packed));

static int probe_verity(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct verity_sb *sb;
	unsigned int version;

	sb = blkid_probe_get_sb(pr, mag, struct verity_sb);
	if (sb == NULL)
		return errno ? -errno : 1;

	version = le32_to_cpu(sb->version);
	if (version != 1)
		return 1;

	blkid_probe_set_uuid(pr, sb->uuid);
	blkid_probe_sprintf_version(pr, "%u", version);
	return 0;
}

struct integrity_sb {
	uint8_t  magic[8];
	uint8_t  version;
	int8_t   log2_interleave_sectors;
	uint16_t integrity_tag_size;
	uint32_t journal_sections;
	uint64_t provided_data_sectors;
	uint32_t flags;
	uint8_t  log2_sectors_per_block;
} __attribute__ ((packed));

static int probe_integrity(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct integrity_sb *sb;

	sb = blkid_probe_get_sb(pr, mag, struct integrity_sb);
	if (sb == NULL)
		return errno ? -errno : 1;

	if (!sb->version)
		return 1;

	blkid_probe_sprintf_version(pr, "%u", sb->version);
	return 0;
}

/* NOTE: the original libblkid uses "lvm2pv" as a name */
const struct blkid_idinfo lvm2_idinfo =
{
	.name		= "LVM2_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_lvm2,
	.magics		=
	{
		{ .magic = "LVM2 001", .len = 8, .sboff = 0x218 },
		{ .magic = "LVM2 001", .len = 8, .sboff = 0x018 },
		{ .magic = "LVM2 001", .len = 8, .kboff = 1, .sboff = 0x018 },
		{ .magic = "LVM2 001", .len = 8, .kboff = 1, .sboff = 0x218 },
		{ NULL }
	}
};

const struct blkid_idinfo lvm1_idinfo =
{
	.name		= "LVM1_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_lvm1,
	.magics		=
	{
		{ .magic = "HM", .len = 2 },
		{ NULL }
	}
};

const struct blkid_idinfo snapcow_idinfo =
{
	.name		= "DM_snapshot_cow",
	.usage		= BLKID_USAGE_OTHER,
	.magics		=
	{
		{ .magic = "SnAp", .len = 4 },
		{ NULL }
	}
};

const struct blkid_idinfo verity_hash_idinfo =
{
	.name		= "DM_verity_hash",
	.usage		= BLKID_USAGE_CRYPTO,
	.probefunc	= probe_verity,
	.magics		=
	{
		{ .magic = "verity\0\0", .len = 8 },
		{ NULL }
	}
};

const struct blkid_idinfo integrity_idinfo =
{
	.name		= "DM_integrity",
	.usage		= BLKID_USAGE_CRYPTO,
	.probefunc	= probe_integrity,
	.magics		=
	{
		{ .magic = "integrt\0", .len = 8 },
		{ NULL }
	}
};
