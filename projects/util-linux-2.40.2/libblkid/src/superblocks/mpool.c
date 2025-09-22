/*
 * Copyright (C) 2016 Micron Technology, Inc.
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "crc32c.h"
#include "superblocks.h"

#define MAX_MPOOL_NAME_LEN 32

struct omf_sb_descriptor {
	uint64_t        osb_magic;
	uint8_t         osb_name[MAX_MPOOL_NAME_LEN];
	unsigned char   osb_poolid[16]; /* UUID of pool this drive belongs to */
	uint16_t        osb_vers;
	uint32_t        osb_gen;
	uint32_t        osb_cksum1; /* crc32c of the preceding fields */
} __attribute__((packed));

static int probe_mpool(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct omf_sb_descriptor *osd;
	uint32_t sb_crc;

	osd = blkid_probe_get_sb(pr, mag, struct omf_sb_descriptor);
	if (!osd)
		return errno ? -errno : 1;

	sb_crc = crc32c(~0L, (const void *)osd,
			offsetof(struct omf_sb_descriptor, osb_cksum1));
	sb_crc ^= ~0L;

	if (!blkid_probe_verify_csum(pr, sb_crc, le32_to_cpu(osd->osb_cksum1)))
		return 1;

	blkid_probe_set_label(pr, osd->osb_name, sizeof(osd->osb_name));
	blkid_probe_set_uuid(pr, osd->osb_poolid);

	return 0;
}

/* "mpoolDev" in ASCII */
#define MPOOL_SB_MAGIC "\x6D\x70\x6f\x6f\x6c\x44\x65\x76"

const struct blkid_idinfo mpool_idinfo =
{
	.name		= "mpool",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_mpool,
	.magics		=
	{
		{ .magic = MPOOL_SB_MAGIC, .len = 8},
		{ NULL }
	}
};
