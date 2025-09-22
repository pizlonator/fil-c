/*
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "superblocks.h"

struct ntfs_bios_parameters {
	uint16_t	sector_size;	/* Size of a sector in bytes. */
	uint8_t		sectors_per_cluster;	/* Size of a cluster in sectors. */
	uint16_t	reserved_sectors;	/* zero */
	uint8_t		fats;			/* zero */
	uint16_t	root_entries;		/* zero */
	uint16_t	sectors;		/* zero */
	uint8_t		media_type;		/* 0xf8 = hard disk */
	uint16_t	sectors_per_fat;	/* zero */
	uint16_t	sectors_per_track;	/* irrelevant */
	uint16_t	heads;			/* irrelevant */
	uint32_t	hidden_sectors;		/* zero */
	uint32_t	large_sectors;		/* zero */
} __attribute__ ((__packed__));

struct ntfs_super_block {
	uint8_t		jump[3];
	uint8_t		oem_id[8];	/* magic string */

	struct ntfs_bios_parameters	bpb;

	uint16_t	unused[2];
	uint64_t	number_of_sectors;
	uint64_t	mft_cluster_location;
	uint64_t	mft_mirror_cluster_location;
	int8_t		clusters_per_mft_record;
	uint8_t		reserved1[3];
	int8_t		cluster_per_index_record;
	uint8_t		reserved2[3];
	uint64_t	volume_serial;
	uint32_t	checksum;
} __attribute__((packed));

struct master_file_table_record {
	uint32_t	magic;
	uint16_t	usa_ofs;
	uint16_t	usa_count;
	uint64_t	lsn;
	uint16_t	sequence_number;
	uint16_t	link_count;
	uint16_t	attrs_offset;
	uint16_t	flags;
	uint32_t	bytes_in_use;
	uint32_t	bytes_allocated;
} __attribute__((__packed__));

struct file_attribute {
	uint32_t	type;
	uint32_t	len;
	uint8_t		non_resident;
	uint8_t		name_len;
	uint16_t	name_offset;
	uint16_t	flags;
	uint16_t	instance;
	uint32_t	value_len;
	uint16_t	value_offset;
} __attribute__((__packed__));

#define MFT_RECORD_VOLUME	3
/* Windows 10 Creators edition has extended the cluster size limit to 2MB */
#define NTFS_MAX_CLUSTER_SIZE	(2 * 1024 * 1024)

#define	MFT_RECORD_ATTR_VOLUME_NAME	0x60
#define	MFT_RECORD_ATTR_END		0xffffffff

static int __probe_ntfs(blkid_probe pr, const struct blkid_idmag *mag, int save_info)
{
	const struct ntfs_super_block *ns;
	const struct master_file_table_record *mft;

	uint32_t sectors_per_cluster, mft_record_size;
	uint16_t sector_size;
	uint64_t nr_clusters, off, attr_off;
	const unsigned char *buf_mft;

	ns = blkid_probe_get_sb(pr, mag, struct ntfs_super_block);
	if (!ns)
		return errno ? -errno : 1;

	/*
	 * Check bios parameters block
	 */
	sector_size = le16_to_cpu(ns->bpb.sector_size);

	if (sector_size < 256 || sector_size > 4096 || !is_power_of_2(sector_size))
		return 1;

	switch (ns->bpb.sectors_per_cluster) {
	case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128:
		sectors_per_cluster = ns->bpb.sectors_per_cluster;
		break;
	default:
		if ((ns->bpb.sectors_per_cluster < 240)
		    || (ns->bpb.sectors_per_cluster > 249))
			return 1;
		sectors_per_cluster = 1 << (256 - ns->bpb.sectors_per_cluster);
	}

	if ((uint16_t) le16_to_cpu(ns->bpb.sector_size) *
			sectors_per_cluster > NTFS_MAX_CLUSTER_SIZE)
		return 1;

	/* Unused fields must be zero */
	if (le16_to_cpu(ns->bpb.reserved_sectors)
	    || le16_to_cpu(ns->bpb.root_entries)
	    || le16_to_cpu(ns->bpb.sectors)
	    || le16_to_cpu(ns->bpb.sectors_per_fat)
	    || le32_to_cpu(ns->bpb.large_sectors)
	    || ns->bpb.fats)
		return 1;

	if ((uint8_t) ns->clusters_per_mft_record < 0xe1
	    || (uint8_t) ns->clusters_per_mft_record > 0xf7) {

		switch (ns->clusters_per_mft_record) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64:
			break;
		default:
			return 1;
		}
	}

	if (ns->clusters_per_mft_record > 0) {
		mft_record_size = ns->clusters_per_mft_record *
				  sectors_per_cluster * sector_size;
	} else {
		int8_t mft_record_size_shift = 0 - ns->clusters_per_mft_record;
		if (mft_record_size_shift < 0 || mft_record_size_shift >= 31)
			return 1;
		mft_record_size = 1 << mft_record_size_shift;
	}

	nr_clusters = le64_to_cpu(ns->number_of_sectors) / sectors_per_cluster;

	if ((le64_to_cpu(ns->mft_cluster_location) > nr_clusters) ||
	    (le64_to_cpu(ns->mft_mirror_cluster_location) > nr_clusters))
		return 1;


	off = le64_to_cpu(ns->mft_cluster_location) * sector_size *
		sectors_per_cluster;

	DBG(LOWPROBE, ul_debug("NTFS: sector_size=%"PRIu16", mft_record_size=%"PRIu32", "
			"sectors_per_cluster=%"PRIu32", nr_clusters=%"PRIu64" "
			"cluster_offset=%"PRIu64"",
			sector_size, mft_record_size,
			sectors_per_cluster, nr_clusters,
			off));

	if (mft_record_size < 4)
		return 1;

	buf_mft = blkid_probe_get_buffer(pr, off, mft_record_size);
	if (!buf_mft)
		return errno ? -errno : 1;

	if (memcmp(buf_mft, "FILE", 4) != 0)
		return 1;

	off += MFT_RECORD_VOLUME * mft_record_size;

	buf_mft = blkid_probe_get_buffer(pr, off, mft_record_size);
	if (!buf_mft)
		return errno ? -errno : 1;

	if (memcmp(buf_mft, "FILE", 4) != 0)
		return 1;

	/* return if caller does not care about UUID and LABEL */
	if (!save_info)
		return 0;

	mft = (struct master_file_table_record *) buf_mft;
	attr_off = le16_to_cpu(mft->attrs_offset);

	while (attr_off + sizeof(struct file_attribute) <= mft_record_size &&
	       attr_off <= le32_to_cpu(mft->bytes_allocated)) {

		uint32_t attr_len;
		struct file_attribute *attr;

		attr = (struct file_attribute *) (buf_mft + attr_off);
		attr_len = le32_to_cpu(attr->len);
		if (!attr_len)
			break;

		if (le32_to_cpu(attr->type) == (uint32_t) MFT_RECORD_ATTR_END)
			break;
		if (le32_to_cpu(attr->type) == (uint32_t) MFT_RECORD_ATTR_VOLUME_NAME) {
			unsigned int val_off = le16_to_cpu(attr->value_offset);
			unsigned int val_len = le32_to_cpu(attr->value_len);
			unsigned char *val = ((uint8_t *) attr) + val_off;

			if (attr_off + val_off + val_len <= mft_record_size)
				blkid_probe_set_utf8label(pr, val, val_len,
							  UL_ENCODE_UTF16LE);
			break;
		}

		attr_off += attr_len;
	}


	blkid_probe_set_fsblocksize(pr, sector_size * sectors_per_cluster);
	blkid_probe_set_block_size(pr, sector_size);
	blkid_probe_set_fssize(pr, le64_to_cpu(ns->number_of_sectors) * sector_size);

	blkid_probe_sprintf_uuid(pr,
			(unsigned char *) &ns->volume_serial,
			sizeof(ns->volume_serial),
			"%016" PRIX64, le64_to_cpu(ns->volume_serial));
	return 0;
}

static int probe_ntfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	return __probe_ntfs(pr, mag, 1);
}

int blkid_probe_is_ntfs(blkid_probe pr)
{
	const struct blkid_idmag *mag = NULL;
	int rc;

	rc = blkid_probe_get_idmag(pr, &ntfs_idinfo, NULL, &mag);
	if (rc < 0)
		return rc;	/* error */
	if (rc != BLKID_PROBE_OK || !mag)
		return 0;

	return __probe_ntfs(pr, mag, 0) == 0 ? 1 : 0;
}

const struct blkid_idinfo ntfs_idinfo =
{
	.name		= "ntfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ntfs,
	.magics		=
	{
		{ .magic = "NTFS    ", .len = 8, .sboff = 3 },
		{ NULL }
	}
};

