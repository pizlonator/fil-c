/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2005 Kay Sievers <kay.sievers@vrfy.org>
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

struct nv_metadata {
	uint8_t		vendor[8];
	uint32_t	size;
	uint32_t	chksum;
	uint16_t	version;
} __attribute__((packed));

#define NVIDIA_SIGNATURE		"NVIDIA  "
#define NVIDIA_SUPERBLOCK_SIZE		120


static int nvraid_verify_checksum(blkid_probe pr, const struct nv_metadata *nv)
{
	uint32_t csum = le32_to_cpu(nv->chksum);
	for (size_t i = 0; i < le32_to_cpu(nv->size); i++)
		csum += le32_to_cpu(((uint32_t *) nv)[i]);
	return blkid_probe_verify_csum(pr, csum, le32_to_cpu(nv->chksum));
}

static int probe_nvraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t off;
	struct nv_metadata *nv;

	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return 1;

	off = ((pr->size / 0x200) - 2) * 0x200;
	nv = (struct nv_metadata *)
		blkid_probe_get_buffer(pr,
				off,
				NVIDIA_SUPERBLOCK_SIZE);
	if (!nv)
		return errno ? -errno : 1;

	if (memcmp(nv->vendor, NVIDIA_SIGNATURE, sizeof(NVIDIA_SIGNATURE)-1) != 0)
		return 1;
	if (le32_to_cpu(nv->size) * 4 != NVIDIA_SUPERBLOCK_SIZE)
		return 1;
	if (!nvraid_verify_checksum(pr, nv))
		return 1;
	if (blkid_probe_sprintf_version(pr, "%u", le16_to_cpu(nv->version)) != 0)
		return 1;
	if (blkid_probe_set_magic(pr, off, sizeof(nv->vendor),
				(unsigned char *) nv->vendor))
		return 1;
	return 0;
}

const struct blkid_idinfo nvraid_idinfo = {
	.name		= "nvidia_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.minsz		= 0x10000,
	.probefunc	= probe_nvraid,
	.magics		= BLKID_NONE_MAGIC
};


