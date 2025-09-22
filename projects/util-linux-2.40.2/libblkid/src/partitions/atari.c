/*
 * atari partitions parsing code
 *
 * Copyright (C) 2018 Vaclav Dolezal <vdolezal@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * Based on Linux kernel implementation and atari-fdisk
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"

struct atari_part_def {
	/*
	 * flags:
	 * 0 (LSB): active
	 * 1-6:     (reserved)
	 * 7 (MSB): bootable
	 */
	unsigned char flags;
	char id[3];
	uint32_t start;
	uint32_t size;
} __attribute__((packed));

struct atari_rootsector {
	char unused0[0x156]; /* boot code */
	struct atari_part_def icd_part[8]; /* ICD partition entries */
	char unused1[0xc];
	uint32_t hd_size;
	struct atari_part_def part[4]; /* primary partition entries */
	uint32_t bsl_start; /* bad sector list start */
	uint32_t bsl_len; /* bad sector list length */
	uint16_t checksum;
} __attribute__((packed));


/*
 * Generated using linux kernel ctype.{c,h}
 *
 * Since kernel uses isalnum() to detect whether it is Atari PT, we need same
 * definition of alnum character to be consistent with kernel.
 */
static const unsigned char _linux_isalnum[] = {
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1
};

static int linux_isalnum(unsigned char c) {
	return _linux_isalnum[c];
}

#define isalnum linux_isalnum

#define IS_ACTIVE(partdef) ((partdef).flags & 1)

static int is_valid_dimension(uint32_t start, uint32_t size, uint32_t maxoff)
{
	uint64_t end = start + size;

	return  end >= start
		&& 0 < start && start <= maxoff
		&& 0 < size && size <= maxoff
		&& 0 < end && end <= maxoff;
}

static int is_valid_partition(struct atari_part_def *part, uint32_t maxoff)
{
	uint32_t start = be32_to_cpu(part->start),
		 size = be32_to_cpu(part->size);

	return (part->flags & 1)
		&& isalnum(part->id[0])
		&& isalnum(part->id[1])
		&& isalnum(part->id[2])
		&& is_valid_dimension(start, size, maxoff);
}

static int is_id_common(char *id)
{
	const char *ids[] = {"GEM", "BGM", "LNX", "SWP", "RAW", };
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(ids); i++) {
		if (!memcmp(ids[i], id, 3))
			return 1;
	}
	return 0;
}

static int parse_partition(blkid_partlist ls, blkid_parttable tab,
	struct atari_part_def *part, uint32_t offset)
{
	blkid_partition par;
	uint32_t start;
	uint32_t size;

	start = be32_to_cpu(part->start) + offset;
	size = be32_to_cpu(part->size);

	if (blkid_partlist_get_partition_by_start(ls, start)) {
		/* Don't increment partno for extended parts */
		if (!offset)
			blkid_partlist_increment_partno(ls);
		return 0;
	}

	par = blkid_partlist_add_partition(ls, tab, start, size);
	if (!par)
		return -ENOMEM;

	blkid_partition_set_type_string(par, (unsigned char *) part->id,
					sizeof(part->id));
	return 1;
}

/*
 * \return 1: OK, 0: bad format or -errno
 */
static int parse_extended(blkid_probe pr, blkid_partlist ls,
	blkid_parttable tab, struct atari_part_def *part)
{
	uint32_t x0start, xstart;
	unsigned ct = 0, i = 0;
	int rc;

	x0start = xstart = be32_to_cpu(part->start);
	while (1) {
		struct atari_rootsector *xrs;

		if (++ct > 100)
			break;

		xrs = (struct atari_rootsector *) blkid_probe_get_sector(pr, xstart);
		if (!xrs) {
			if (errno)
				return -errno;
			return 0;
		}

		/*
		 * There must be data partition followed by reference to next
		 * XGM or inactive entry.
		 */
		for (i=0; ; i++) {
			if (i >= ARRAY_SIZE(xrs->part) - 1)
				return 0;
			if (IS_ACTIVE(xrs->part[i]))
				break;
		}

		if (!memcmp(xrs->part[i].id, "XGM", 3))
			return 0;

		rc = parse_partition(ls, tab, &xrs->part[i], xstart);
		if (rc <= 0)
			return rc;

		if (!IS_ACTIVE(xrs->part[i+1]))
			break;

		if (memcmp(xrs->part[i+1].id, "XGM", 3) != 0)
			return 0;

		xstart = x0start + be32_to_cpu(xrs->part[i+1].start);
	}

	return 1;
}

static int probe_atari_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct atari_rootsector *rs;

	blkid_parttable tab = NULL;
	blkid_partlist ls;

	unsigned i;
	int has_xgm = 0;
	int rc = 0;
	uint32_t rssize;	/* size in sectors from root sector */
	uint64_t size;		/* size in sectors from system */

	/* Atari partition is not defined for other sector sizes */
	if (blkid_probe_get_sectorsize(pr) != 512)
		goto nothing;

	size = blkid_probe_get_size(pr) / 512;

	/* Atari is not well defined to support large disks */
	if (size > INT32_MAX)
		goto nothing;

	/* read root sector */
	rs = (struct atari_rootsector *) blkid_probe_get_sector(pr, 0);
	if (!rs) {
		if (errno)
			return -errno;
		goto nothing;
	}

	rssize = be32_to_cpu(rs->hd_size);

	/* check number of sectors stored in the root sector */
	if (rssize < 2 || rssize > size)
		goto nothing;

	/* check list of bad blocks */
	if ((rs->bsl_start || rs->bsl_len)
	    && !is_valid_dimension(be32_to_cpu(rs->bsl_start),
				   be32_to_cpu(rs->bsl_len),
				   rssize))
		goto nothing;

	/*
	 * At least one valid partition required
	 */
	for (i = 0; i < 4; i++) {
		if (is_valid_partition(&rs->part[i], rssize)) {
			if (blkid_probe_set_magic(pr,
					offsetof(struct atari_rootsector, part[i]),
					sizeof(rs->part[i].flags) + sizeof(rs->part[i].id),
					(unsigned char *) &rs->part[i]))
				goto err;
			break;
		}
	}

	if (i == 4)
		goto nothing;

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return BLKID_PROBE_OK;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto nothing;

	tab = blkid_partlist_new_parttable(ls, "atari", 0);
	if (!tab)
		goto err;

	for (i = 0; i < ARRAY_SIZE(rs->part); i++) {
		struct atari_part_def *p = &rs->part[i];

		if (!IS_ACTIVE(*p)) {
			blkid_partlist_increment_partno(ls);
			continue;
		}
		if (!memcmp(p->id, "XGM", 3)) {
			has_xgm = 1;
			rc = parse_extended(pr, ls, tab, p);
		} else {
			rc = parse_partition(ls, tab, p, 0);
		}
		if (rc < 0)
			return rc;
	}

	/* if there are no XGM partitions, we can try ICD format */
	/* if first ICD partition ID is not valid, assume no ICD format */
	if (!has_xgm && is_id_common(rs->icd_part[0].id)) {
		for (i = 0; i < ARRAY_SIZE(rs->icd_part); i++) {
			struct atari_part_def *p = &rs->icd_part[i];

			if (!IS_ACTIVE(*p) || !is_id_common(p->id)) {
				blkid_partlist_increment_partno(ls);
				continue;
			}

			rc = parse_partition(ls, tab, p, 0);
			if (rc < 0)
				return rc;
		}
	}

	return BLKID_PROBE_OK;

nothing:
	return BLKID_PROBE_NONE;
err:
	return -ENOMEM;
}

const struct blkid_idinfo atari_pt_idinfo =
{
	.name		= "atari",
	.probefunc	= probe_atari_pt,
	.magics		= BLKID_NONE_MAGIC
};
