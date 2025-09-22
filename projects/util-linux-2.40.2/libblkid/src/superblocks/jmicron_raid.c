/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * Inspired by libvolume_id by
 *     Kay Sievers <kay.sievers@vrfy.org>
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

#define JM_SIGNATURE		"JM"
#define JM_MINOR_VERSION(_x)	(le16_to_cpu((_x)->version) & 0xFF)
#define JM_MAJOR_VERSION(_x)	(le16_to_cpu((_x)->version) >> 8)
#define JM_SPARES		2
#define JM_MEMBERS		8

struct jm_metadata {
	int8_t		signature[2];	/* 0x0 - 0x01 */

	uint16_t	version;	/* 0x03 - 0x04 JMicron version */

	uint16_t	checksum;	/* 0x04 - 0x05 */
	uint8_t		filler[10];

	uint32_t	identity;	/* 0x10 - 0x13 */

	struct {
		uint32_t	base;	/* 0x14 - 0x17 */
		uint32_t	range;	/* 0x18 - 0x1B range */
		uint16_t	range2;	/* 0x1C - 0x1D range2 */
	} segment;

	int8_t		name[16];	/* 0x20 - 0x2F */

	uint8_t		mode;		/* 0x30 RAID level */
	uint8_t		block;		/* 0x31 stride size (2=4K, 3=8K, ...) */
	uint16_t	attribute;	/* 0x32 - 0x33 */
	uint8_t		filler1[4];

	uint32_t	spare[JM_SPARES];	/* 0x38 - 0x3F */
	uint32_t	member[JM_MEMBERS];	/* 0x40 - 0x5F */

	uint8_t		filler2[0x20];
} __attribute__ ((packed));

static int jm_checksum(blkid_probe pr, const struct jm_metadata *jm)
{
	size_t count = sizeof(*jm) / sizeof(uint16_t);
	uint16_t sum = 0;
	unsigned char *ptr = (unsigned char *) jm;

	while (count--) {
		uint16_t val;

		memcpy(&val, ptr, sizeof(uint16_t));
		sum += le16_to_cpu(val);

		ptr += sizeof(uint16_t);
	}

	return blkid_probe_verify_csum(pr, sum == 0 || sum == 1, 1);
}

static int probe_jmraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t off;
	const struct jm_metadata *jm;

	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return 1;

	off = ((pr->size / 0x200) - 1) * 0x200;
	jm = (struct jm_metadata *)
		blkid_probe_get_buffer(pr,
				off,
				sizeof(struct jm_metadata));
	if (!jm)
		return errno ? -errno : 1;

	if (memcmp(jm->signature, JM_SIGNATURE, sizeof(JM_SIGNATURE) - 1) != 0)
		return 1;

	if (!jm_checksum(pr, jm))
		return 1;

	if (jm->mode > 5)
		return 1;

	if (blkid_probe_sprintf_version(pr, "%u.%u",
			JM_MAJOR_VERSION(jm), JM_MINOR_VERSION(jm)) != 0)
		return 1;
	if (blkid_probe_set_magic(pr, off, sizeof(jm->signature),
				(unsigned char *) jm->signature))
		return 1;
	return 0;
}

const struct blkid_idinfo jmraid_idinfo = {
	.name		= "jmicron_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.minsz		= 0x10000,
	.probefunc	= probe_jmraid,
	.magics		= BLKID_NONE_MAGIC
};
