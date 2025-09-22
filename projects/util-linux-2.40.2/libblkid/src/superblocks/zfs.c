/*
 * Copyright (C) 2009-2010 by Andreas Dilger <adilger@sun.com>
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
#include <inttypes.h>
#include <limits.h>

#include "superblocks.h"

#define VDEV_LABEL_UBERBLOCK	(128 * 1024ULL)
#define VDEV_LABEL_NVPAIR	( 16 * 1024ULL)
#define VDEV_LABEL_SIZE		(256 * 1024ULL)
#define UBERBLOCK_SIZE		1024ULL
#define UBERBLOCKS_COUNT   128

/* #include <sys/uberblock_impl.h> */
#define UBERBLOCK_MAGIC         0x00bab10c              /* oo-ba-bloc!  */
struct zfs_uberblock {
	uint64_t	ub_magic;	/* UBERBLOCK_MAGIC		*/
	uint64_t	ub_version;	/* SPA_VERSION			*/
	uint64_t	ub_txg;		/* txg of last sync		*/
	uint64_t	ub_guid_sum;	/* sum of all vdev guids	*/
	uint64_t	ub_timestamp;	/* UTC time of last sync	*/
	char		ub_rootbp;	/* MOS objset_phys_t		*/
} __attribute__((packed));

#define ZFS_WANT	 4

#define DATA_TYPE_UINT64 8
#define DATA_TYPE_STRING 9
#define DATA_TYPE_DIRECTORY 19

struct nvpair {
	uint32_t	nvp_size;
	uint32_t	nvp_unkown;
	uint32_t	nvp_namelen;
	char		nvp_name[0]; /* aligned to 4 bytes */
	/* aligned ptr array for string arrays */
	/* aligned array of data for value */
};

struct nvstring {
	uint32_t	nvs_type;
	uint32_t	nvs_elem;
	uint32_t	nvs_strlen;
	unsigned char	nvs_string[0];
};

struct nvuint64 {
	uint32_t	nvu_type;
	uint32_t	nvu_elem;
	uint64_t	nvu_value;
} __attribute__((packed));

struct nvdirectory {
	uint32_t	nvd_type;
	uint32_t	nvd_unknown[3];
};

struct nvlist {
	uint32_t	nvl_unknown[3];
	struct nvpair	nvl_nvpair;
};

static void zfs_process_value(blkid_probe pr, const char *name, size_t namelen,
			     const void *value, size_t max_value_size, unsigned directory_level)
{
	if (strncmp(name, "name", namelen) == 0 &&
	    sizeof(struct nvstring) <= max_value_size &&
	    !directory_level) {
		const struct nvstring *nvs = value;
		uint32_t nvs_type = be32_to_cpu(nvs->nvs_type);
		uint32_t nvs_strlen = be32_to_cpu(nvs->nvs_strlen);

		if (nvs_type != DATA_TYPE_STRING ||
		    (uint64_t)nvs_strlen + sizeof(*nvs) > max_value_size)
			return;

		DBG(LOWPROBE, ul_debug("nvstring: type %u string %*s",
				       nvs_type, nvs_strlen, nvs->nvs_string));

		blkid_probe_set_label(pr, nvs->nvs_string, nvs_strlen);
	} else if (strncmp(name, "guid", namelen) == 0 &&
		   sizeof(struct nvuint64) <= max_value_size &&
		   !directory_level) {
		const struct nvuint64 *nvu = value;
		uint32_t nvu_type = be32_to_cpu(nvu->nvu_type);
		uint64_t nvu_value;

		memcpy(&nvu_value, &nvu->nvu_value, sizeof(nvu_value));
		nvu_value = be64_to_cpu(nvu_value);

		if (nvu_type != DATA_TYPE_UINT64)
			return;

		DBG(LOWPROBE, ul_debug("nvuint64: type %u value %"PRIu64,
				       nvu_type, nvu_value));

		blkid_probe_sprintf_value(pr, "UUID_SUB",
					  "%"PRIu64, nvu_value);
	} else if (strncmp(name, "pool_guid", namelen) == 0 &&
		   sizeof(struct nvuint64) <= max_value_size &&
		   !directory_level) {
		const struct nvuint64 *nvu = value;
		uint32_t nvu_type = be32_to_cpu(nvu->nvu_type);
		uint64_t nvu_value;

		memcpy(&nvu_value, &nvu->nvu_value, sizeof(nvu_value));
		nvu_value = be64_to_cpu(nvu_value);

		if (nvu_type != DATA_TYPE_UINT64)
			return;

		DBG(LOWPROBE, ul_debug("nvuint64: type %u value %"PRIu64,
				       nvu_type, nvu_value));

		blkid_probe_sprintf_uuid(pr, (unsigned char *) &nvu_value,
					 sizeof(nvu_value),
					 "%"PRIu64, nvu_value);
	} else if (strncmp(name, "ashift", namelen) == 0 &&
		   sizeof(struct nvuint64) <= max_value_size) {
		const struct nvuint64 *nvu = value;
		uint32_t nvu_type = be32_to_cpu(nvu->nvu_type);
		uint64_t nvu_value;

		memcpy(&nvu_value, &nvu->nvu_value, sizeof(nvu_value));
		nvu_value = be64_to_cpu(nvu_value);

		if (nvu_type != DATA_TYPE_UINT64)
			return;

		if (nvu_value < 32){
			blkid_probe_set_fsblocksize(pr, 1U << nvu_value);
			blkid_probe_set_block_size(pr, 1U << nvu_value);
		}
	}
}

static void zfs_extract_guid_name(blkid_probe pr, loff_t offset)
{
	const unsigned char *p;
	const struct nvlist *nvl;
	const struct nvpair *nvp;
	size_t left = 4096;
	unsigned directory_level = 0;

	offset = (offset & ~(VDEV_LABEL_SIZE - 1)) + VDEV_LABEL_NVPAIR;

	/* Note that we currently assume that the desired fields are within
	 * the first 4k (left) of the nvlist.  This is true for all pools
	 * I've seen, and simplifies this code somewhat, because we don't
	 * have to handle an nvpair crossing a buffer boundary. */
	p = blkid_probe_get_buffer(pr, offset, left);
	if (!p)
		return;

	DBG(LOWPROBE, ul_debug("zfs_extract: nvlist offset %jd",
			       (intmax_t)offset));

	nvl = (const struct nvlist *) p;
	nvp = &nvl->nvl_nvpair;
	left -= (const unsigned char *)nvp - p; /* Already used up 12 bytes */

	while (left > sizeof(*nvp)) {
		uint32_t nvp_size = be32_to_cpu(nvp->nvp_size);
		uint32_t nvp_namelen = be32_to_cpu(nvp->nvp_namelen);
		uint64_t namesize = ((uint64_t)nvp_namelen + 3) & ~3;
		size_t max_value_size;
		const void *value;

		if (!nvp->nvp_size) {
			if (!directory_level)
				break;
			directory_level--;
			nvp_size = 8;
			goto cont;
		}

		DBG(LOWPROBE, ul_debug("left %zd nvp_size %u",
				       left, nvp_size));

		/* nvpair fits in buffer and name fits in nvpair? */
		if (nvp_size > left || namesize + sizeof(*nvp) > nvp_size)
			break;

		DBG(LOWPROBE,
		    ul_debug("nvlist: size %u, namelen %u, name %*s",
			     nvp_size, nvp_namelen, nvp_namelen,
			     nvp->nvp_name));

		max_value_size = nvp_size - (namesize + sizeof(*nvp));
		value = nvp->nvp_name + namesize;

		if (sizeof(struct nvdirectory) <= max_value_size) {
			const struct nvdirectory *nvu = value;
			if (be32_to_cpu(nvu->nvd_type) == DATA_TYPE_DIRECTORY) {
				nvp_size = sizeof(*nvp) + namesize + sizeof(*nvu);
				directory_level++;
				goto cont;
			}
		}

		zfs_process_value(pr, nvp->nvp_name, nvp_namelen,
				  value, max_value_size, directory_level);

cont:
		if (nvp_size > left)
			break;
		left -= nvp_size;

		nvp = (struct nvpair *)((char *)nvp + nvp_size);
	}
}

static int find_uberblocks(const void *label, loff_t *ub_offset, int *swap_endian)
{
	uint64_t swab_magic = swab64((uint64_t)UBERBLOCK_MAGIC);
	const struct zfs_uberblock *ub;
	int i, found = 0;
	loff_t offset = VDEV_LABEL_UBERBLOCK;

	for (i = 0; i < UBERBLOCKS_COUNT; i++, offset += UBERBLOCK_SIZE) {
		ub = (const struct zfs_uberblock *)((const char *) label + offset);

		if (ub->ub_magic == UBERBLOCK_MAGIC) {
			*ub_offset = offset;
			*swap_endian = 0;
			found++;
			DBG(LOWPROBE, ul_debug("probe_zfs: found little-endian uberblock at %jd", (intmax_t)offset >> 10));
		}

		if (ub->ub_magic == swab_magic) {
			*ub_offset = offset;
			*swap_endian = 1;
			found++;
			DBG(LOWPROBE, ul_debug("probe_zfs: found big-endian uberblock at %jd", (intmax_t)offset >> 10));
		}
	}

	return found;
}

/* ZFS has 128x1kB host-endian root blocks, stored in 2 areas at the start
 * of the disk, and 2 areas at the end of the disk.  Check only some of them...
 * #4 (@ 132kB) is the first one written on a new filesystem. */
static int probe_zfs(blkid_probe pr,
	const struct blkid_idmag *mag  __attribute__((__unused__)))
{
	int swab_endian = 0;
	struct zfs_uberblock *ub = NULL;
	loff_t offset = 0, ub_offset = 0;
	int label_no, found = 0, found_in_label;
	const void *label;
	loff_t blk_align = (pr->size % (256 * 1024ULL));

	DBG(PROBE, ul_debug("probe_zfs"));
	/* Look for at least 4 uberblocks to ensure a positive match */
	for (label_no = 0; label_no < 4; label_no++) {
		switch(label_no) {
		case 0: // jump to L0
			offset = 0;
			break;
		case 1: // jump to L1
			offset = VDEV_LABEL_SIZE;
			break;
		case 2: // jump to L2
			offset = pr->size - 2 * VDEV_LABEL_SIZE - blk_align;
			break;
		case 3: // jump to L3
			offset = pr->size - VDEV_LABEL_SIZE - blk_align;
			break;
		}

		if ((S_ISREG(pr->mode) || blkid_probe_is_wholedisk(pr)) &&
		    blkid_probe_is_covered_by_pt(pr,  offset, VDEV_LABEL_SIZE))
			/* ignore this area, it's within any partition and
			 * we are working with whole-disk now */
			continue;

		label = blkid_probe_get_buffer(pr, offset, VDEV_LABEL_SIZE);
		if (label == NULL)
			return errno ? -errno : 1;

		found_in_label = find_uberblocks(label, &ub_offset, &swab_endian);

		if (found_in_label > 0) {
			found+= found_in_label;
			ub = (struct zfs_uberblock *)((char *) label + ub_offset);
			ub_offset += offset;

			if (found >= ZFS_WANT)
				break;
		}
	}

	if (found < ZFS_WANT)
		return 1;

	/* If we found the 4th uberblock, then we will have exited from the
	 * scanning loop immediately, and ub will be a valid uberblock. */
	blkid_probe_sprintf_version(pr, "%" PRIu64, swab_endian ?
				    swab64(ub->ub_version) : ub->ub_version);

	zfs_extract_guid_name(pr, offset);

	if (blkid_probe_set_magic(pr, ub_offset,
				sizeof(ub->ub_magic),
				(unsigned char *) &ub->ub_magic))
		return 1;

	blkid_probe_set_fsendianness(pr, !swab_endian ?
			BLKID_ENDIANNESS_NATIVE : BLKID_ENDIANNESS_OTHER);

	return 0;
}

const struct blkid_idinfo zfs_idinfo =
{
	.name		= "zfs_member",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_zfs,
	.minsz		= 64 * 1024 * 1024,
	.magics		= BLKID_NONE_MAGIC
};
