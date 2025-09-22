/*
 * Copyright (C) 2018 Tony Asleson <tasleson@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/*
 * Specification for on disk format
 * https://stratis-storage.github.io/StratisSoftwareDesign.pdf
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "superblocks.h"
#include "crc32c.h"

/* Macro to avoid error in struct declaration. */
#define STRATIS_UUID_LEN 32
/* Contains 4 hyphens and trailing null byte. */
#define STRATIS_UUID_STR_LEN 37

struct stratis_sb {
	uint32_t crc32;
	uint8_t magic[16];
	uint64_t sectors;
	uint8_t reserved[4];
	uint8_t pool_uuid[STRATIS_UUID_LEN];
	uint8_t dev_uuid[STRATIS_UUID_LEN];
	uint64_t mda_size;
	uint64_t reserved_size;
	uint64_t flags;
	uint64_t initialization_time;
} __attribute__ ((__packed__));

#define BS 512
#define FIRST_COPY_OFFSET BS
#define SECOND_COPY_OFFSET (BS * 9)
#define SB_AREA_SIZE (BS * 16)

const char STRATIS_MAGIC[] = "!Stra0tis\x86\xff\x02^\x41rh";
#define MAGIC_LEN (sizeof(STRATIS_MAGIC) - 1)

#define _MAGIC_OFFSET (offsetof(const struct stratis_sb, magic))
#define MAGIC_OFFSET_COPY_1 (FIRST_COPY_OFFSET + _MAGIC_OFFSET)
#define MAGIC_OFFSET_COPY_2 (SECOND_COPY_OFFSET + _MAGIC_OFFSET)

static int stratis_valid_sb(const uint8_t *p)
{
	const struct stratis_sb *stratis = (const struct stratis_sb *)p;
	uint32_t crc = 0;

	/* generate CRC from byte position 4 for length 508 == 512 byte sector */
	crc = crc32c(~0L, p + sizeof(stratis->crc32),
			BS - sizeof(stratis->crc32));
	crc ^= ~0L;

	return crc == le32_to_cpu(stratis->crc32);
}

/*
 * Format UUID string representation to include hyphens given that Stratis stores
 * UUIDs without hyphens in the superblock to keep the UUID length a power of 2.
 */
static void stratis_format_uuid(const unsigned char *src_uuid,
				unsigned char *dst_uuid)
{
	int i;

	for (i = 0; i < STRATIS_UUID_LEN; i++) {
		*dst_uuid++ = *src_uuid++;
		if (i == 7 || i == 11 || i == 15 || i == 19)
			*dst_uuid ++ = '-';
	}
	*dst_uuid = '\0';
}

static int probe_stratis(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	const struct stratis_sb *stratis = NULL;
	const uint8_t *buf = blkid_probe_get_buffer(pr, 0, SB_AREA_SIZE);
	unsigned char uuid[STRATIS_UUID_STR_LEN];

	if (!buf)
		return errno ? -errno : 1;

	if (stratis_valid_sb(buf + FIRST_COPY_OFFSET)) {
		stratis = (const struct stratis_sb *)(buf + FIRST_COPY_OFFSET);
	} else {
		if (!stratis_valid_sb(buf + SECOND_COPY_OFFSET))
			return 1;

		stratis = (const struct stratis_sb *)
				(buf + SECOND_COPY_OFFSET);
	}

	stratis_format_uuid(stratis->dev_uuid, uuid);
	blkid_probe_strncpy_uuid(pr, uuid, sizeof(uuid));

	stratis_format_uuid(stratis->pool_uuid, uuid);
	blkid_probe_set_value(pr, "POOL_UUID", uuid, sizeof(uuid));

	blkid_probe_sprintf_value(pr, "BLOCKDEV_SECTORS", "%" PRIu64,
				le64_to_cpu(stratis->sectors));
	blkid_probe_sprintf_value(pr, "BLOCKDEV_INITTIME", "%" PRIu64,
				le64_to_cpu(stratis->initialization_time));
	return 0;
}

const struct blkid_idinfo stratis_idinfo = {
	.name		= "stratis",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_stratis,
	.minsz		= SB_AREA_SIZE,
	.magics		= {
		{ .magic = STRATIS_MAGIC, .len = MAGIC_LEN,
			.sboff = MAGIC_OFFSET_COPY_1},
		{ .magic = STRATIS_MAGIC, .len = MAGIC_LEN,
			.sboff = MAGIC_OFFSET_COPY_2},
		{ NULL }
	}
};
