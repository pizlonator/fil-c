/*
 * Copyright (C) 2009 by Bastian Friedrich <bastian.friedrich@collax.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * defines, structs taken from drbd source; file names represent drbd source
 * files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>

#include "superblocks.h"

enum {
	DRBD_VERSION_08,
	DRBD_VERSION_09,
};

/*
 * drbd/drbd_int.h
 */
#define BM_BLOCK_SHIFT	12			 /* 4k per bit */
#define BM_BLOCK_SIZE	 (1<<BM_BLOCK_SHIFT)

/*
 * user/drbdmeta.c
 * We support v08 and v09
 */
#define DRBD_MD_MAGIC_08		"\x83\x74\x02\x6b"
#define DRBD_MD_MAGIC_84_UNCLEAN	"\x83\x74\x02\x6c"
#define DRBD_MD_MAGIC_09		"\x83\x74\x02\x6d"
/* there is no DRBD_MD_MAGIC_09_UNCLEAN */

/*
 * drbd/linux/drbd.h
 */
enum drbd_uuid_index {
	UI_CURRENT,
	UI_BITMAP,
	UI_HISTORY_START,
	UI_HISTORY_END,
	UI_SIZE,		/* nl-packet: number of dirty bits */
	UI_FLAGS,		/* nl-packet: flags */
	UI_EXTENDED_SIZE	/* Everything. */
};


/*
 * Used by libblkid to avoid unnecessary padding at the end of the structs and
 * too large unused structs in memory.
 */
#define DRBD_MD_OFFSET 4096

/*
 * user/shared/drbdmeta.c
 * Minor modifications wrt. types
 */
struct md_on_disk_08 {
	uint64_t la_sect;         /* last agreed size. */
	uint64_t uuid[UI_SIZE];   /* UUIDs */
	uint64_t device_uuid;
	uint64_t reserved_u64_1;
	uint32_t flags;
	uint32_t magic;
	uint32_t md_size_sect;
	int32_t  al_offset;       /* signed sector offset to this block */
	uint32_t al_nr_extents;   /* important for restoring the AL */
	int32_t  bm_offset;       /* signed sector offset to the bitmap, from here */
	uint32_t bm_bytes_per_bit;
	uint32_t reserved_u32[4];

	unsigned char padding_start[0];
	unsigned char padding_end[0] __attribute__((aligned(4096)));
};

/*
 * linux/drbd.h, v9 only
 */
#define DRBD_PEERS_MAX 32
#define HISTORY_UUIDS DRBD_PEERS_MAX

/*
 * drbd-headers/drbd_meta_data.h
 * Minor modifications wrt. types
 */
struct peer_dev_md_on_disk_9 {
	uint64_t bitmap_uuid;
	uint64_t bitmap_dagtag;
	uint32_t flags;
	int32_t bitmap_index;
	uint32_t reserved_u32[2];
} __attribute__((packed));

struct meta_data_on_disk_9 {
	uint64_t effective_size;    /* last agreed size */
	uint64_t current_uuid;
	uint64_t reserved_u64[4];   /* to have the magic at the same position as in v07, and v08 */
	uint64_t device_uuid;
	uint32_t flags;             /* MDF */
	uint32_t magic;
	uint32_t md_size_sect;
	uint32_t al_offset;         /* offset to this block */
	uint32_t al_nr_extents;     /* important for restoring the AL */
	uint32_t bm_offset;         /* offset to the bitmap, from here */
	uint32_t bm_bytes_per_bit;  /* BM_BLOCK_SIZE */
	uint32_t la_peer_max_bio_size;   /* last peer max_bio_size */
	uint32_t bm_max_peers;
	int32_t node_id;

	/* see al_tr_number_to_on_disk_sector() */
	uint32_t al_stripes;
	uint32_t al_stripe_size_4k;

	uint32_t reserved_u32[2];

	struct peer_dev_md_on_disk_9 peers[DRBD_PEERS_MAX];
	uint64_t history_uuids[HISTORY_UUIDS];

	unsigned char padding_start[0];
	unsigned char padding_end[0] __attribute__((aligned(4096)));
} __attribute__((packed));


static int is_zero_padded(const unsigned char *padding_start,
			  const unsigned char *padding_end)
{
	for (; padding_start < padding_end; padding_start++) {
		if (*padding_start != 0)
			return 0;
	}
	return 1;
}

static int probe_drbd_84(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct md_on_disk_08 *md;

	md = blkid_probe_get_sb(pr, mag, struct md_on_disk_08);
	if (!md)
		return errno ? -errno : 1;

	if (be32_to_cpu(read_unaligned_member(md, bm_bytes_per_bit)) != BM_BLOCK_SIZE)
		return 1;

	if (!is_zero_padded(member_ptr(md, padding_start),
			    member_ptr(md, padding_end)))
		return 1;

	/*
	 * DRBD does not have "real" uuids; the following resembles DRBD's
	 * notion of uuids (64 bit, see struct above)
	 */
	blkid_probe_sprintf_uuid(pr,
		member_ptr(md, device_uuid), sizeof(md->device_uuid),
		"%" PRIx64, be64_to_cpu(read_unaligned_member(md, device_uuid)));

	blkid_probe_set_version(pr, "v08");

	return 0;
}

static int probe_drbd_90(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct meta_data_on_disk_9 *md;

	md = blkid_probe_get_sb(pr, mag, struct meta_data_on_disk_9);
	if (!md)
		return errno ? -errno : 1;

	if (be32_to_cpu(read_unaligned_member(md, bm_bytes_per_bit)) != BM_BLOCK_SIZE)
		return 1;

	if (!is_zero_padded(member_ptr(md, padding_start),
			    member_ptr(md, padding_end)))
		return 1;

	/*
	 * DRBD does not have "real" uuids; the following resembles DRBD's
	 * notion of uuids (64 bit, see struct above)
	 */
	blkid_probe_sprintf_uuid(pr,
		member_ptr(md, device_uuid), sizeof(md->device_uuid),
		"%" PRIx64, be64_to_cpu(read_unaligned_member(md, device_uuid)));

	blkid_probe_set_version(pr, "v09");

	return 0;
}

static int probe_drbd(blkid_probe pr, const struct blkid_idmag *mag)
{
	if (mag->hint == DRBD_VERSION_08)
		return probe_drbd_84(pr, mag);

	if (mag->hint == DRBD_VERSION_09)
		return probe_drbd_90(pr, mag);

	return 1;
}

const struct blkid_idinfo drbd_idinfo =
{
	.name		= "drbd",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_drbd,
	/*
	 * Smaller ones are certainly not DRBD9 devices.
	 * Recent utils even refuse to generate larger ones,
	 * keep this as a sufficient lower bound.
	 */
	.minsz		= 0x10000,
	.magics		= {
		{
			.magic = DRBD_MD_MAGIC_08,
			.len   = sizeof(DRBD_MD_MAGIC_08) - 1,
			.hint  = DRBD_VERSION_08,
			.kboff = -(DRBD_MD_OFFSET >> 10),
			.sboff = offsetof(struct md_on_disk_08, magic),
		},
		{
			.magic = DRBD_MD_MAGIC_84_UNCLEAN,
			.len   = sizeof(DRBD_MD_MAGIC_84_UNCLEAN) - 1,
			.hint  = DRBD_VERSION_08,
			.kboff = -(DRBD_MD_OFFSET >> 10),
			.sboff = offsetof(struct md_on_disk_08, magic),
		},
		{
			.magic = DRBD_MD_MAGIC_09,
			.len   = sizeof(DRBD_MD_MAGIC_09) - 1,
			.hint  = DRBD_VERSION_09,
			.kboff = -(DRBD_MD_OFFSET >> 10),
			.sboff = offsetof(struct meta_data_on_disk_9, magic),
		},
		{ NULL }
	}
};

