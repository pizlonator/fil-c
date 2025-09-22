/*
 * BSD/OSF partition parsing code
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * Inspired by fdisk, partx, Linux kernel, libparted and openbsd header files.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"
#include "pt-bsd.h"

/* Returns 'blkid_idmag' in 512-sectors */
#define BLKID_MAG_SECTOR(_mag)  (((_mag)->kboff / 2)  + ((_mag)->sboff >> 9))

/* Returns 'blkid_idmag' in bytes */
#define BLKID_MAG_OFFSET(_mag)  ((_mag)->kboff << 10) + ((_mag)->sboff)

/* Returns 'blkid_idmag' offset in bytes within the last sector */
#define BLKID_MAG_LASTOFFSET(_mag) \
		 (BLKID_MAG_OFFSET(_mag) - (BLKID_MAG_SECTOR(_mag) << 9))

static uint16_t bsd_checksum(const struct bsd_disklabel *l)
{
	uint16_t v, csum = 0;
	const char *end = (const char *) (l + 1);

	for (const char *c = (const char *) l; c < end; c += sizeof(uint16_t)) {
		memcpy(&v, c, sizeof(v));
		csum ^= v;
	}
	return csum ^ le16_to_cpu(l->d_checksum);
}

static int probe_bsd_pt(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct bsd_disklabel *l;
	struct bsd_partition *p;
	const char *name = "bsd" ;
	blkid_parttable tab = NULL;
	blkid_partition parent;
	blkid_partlist ls;
	int i, nparts = BSD_MAXPARTITIONS;
	const unsigned char *data;
	int rc = BLKID_PROBE_NONE;
	uint32_t abs_offset = 0;

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return rc;

	data = blkid_probe_get_sector(pr, BLKID_MAG_SECTOR(mag));
	if (!data) {
		if (errno)
			rc = -errno;
		goto nothing;
	}

	l = (struct bsd_disklabel *) (data + BLKID_MAG_LASTOFFSET(mag));

	if (!blkid_probe_verify_csum(pr, bsd_checksum(l), le16_to_cpu(l->d_checksum))) {
		rc = BLKID_PROBE_NONE;
		goto nothing;
	}

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto nothing;

	/* try to determine the real type of BSD system according to
	 * (parental) primary partition */
	parent = blkid_partlist_get_parent(ls);
	if (parent) {
		switch(blkid_partition_get_type(parent)) {
		case MBR_FREEBSD_PARTITION:
			name = "freebsd";
			abs_offset = blkid_partition_get_start(parent);
			break;
		case MBR_NETBSD_PARTITION:
			name = "netbsd";
			break;
		case MBR_OPENBSD_PARTITION:
			name = "openbsd";
			break;
		default:
			DBG(LOWPROBE, ul_debug(
				"WARNING: BSD label detected on unknown (0x%x) "
				"primary partition",
				blkid_partition_get_type(parent)));
			break;
		}
	}

	tab = blkid_partlist_new_parttable(ls, name, BLKID_MAG_OFFSET(mag));
	if (!tab) {
		rc = -ENOMEM;
		goto nothing;
	}

	if (le16_to_cpu(l->d_npartitions) < BSD_MAXPARTITIONS)
		nparts = le16_to_cpu(l->d_npartitions);

	else if (le16_to_cpu(l->d_npartitions) > BSD_MAXPARTITIONS)
		DBG(LOWPROBE, ul_debug(
			"WARNING: ignore %d more BSD partitions",
			le16_to_cpu(l->d_npartitions) - BSD_MAXPARTITIONS));

	for (i = 0, p = l->d_partitions; i < nparts; i++, p++) {
		blkid_partition par;
		uint32_t start, size;

		if (p->p_fstype == BSD_FS_UNUSED)
			continue;

		start = le32_to_cpu(p->p_offset);
		size = le32_to_cpu(p->p_size);

		/* FreeBSD since version 10 uses relative offsets. We can use
		 * 3rd partition (special wholedisk partition) to detect this
		 * situation.
		 */
		if (abs_offset && nparts >= 3
		    && le32_to_cpu(l->d_partitions[2].p_offset) == 0)
			start += abs_offset;

		if (parent && blkid_partition_get_start(parent) == start
			   && blkid_partition_get_size(parent) == size) {
			DBG(LOWPROBE, ul_debug(
				"WARNING: BSD partition (%d) same like parent, "
				"ignore", i));
			continue;
		}
		if (parent && !blkid_is_nested_dimension(parent, start, size)) {
			DBG(LOWPROBE, ul_debug(
				"WARNING: BSD partition (%d) overflow "
				"detected, ignore", i));
			continue;
		}

		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par) {
			rc = -ENOMEM;
			goto nothing;
		}

		blkid_partition_set_type(par, p->p_fstype);
	}

	return BLKID_PROBE_OK;

nothing:
	return rc;
}


/*
 * All BSD variants use the same magic string (little-endian),
 * and the same disklabel.
 *
 * The difference between {Free,Open,...}BSD is in the (parental)
 * primary partition type.
 *
 * See also: http://en.wikipedia.org/wiki/BSD_disklabel
 *
 * The location of BSD disk label is architecture specific and in defined by
 * LABELSECTOR and LABELOFFSET macros in the disklabel.h file. The location
 * also depends on BSD variant, FreeBSD uses only one location, NetBSD and
 * OpenBSD are more creative...
 *
 * The basic overview:
 *
 * arch                    | LABELSECTOR | LABELOFFSET
 * ------------------------+-------------+------------
 * amd64 arm hppa hppa64   |             |
 * i386, macppc, mvmeppc   |      1      |      0
 * sgi, aviion, sh, socppc |             |
 * ------------------------+-------------+------------
 * alpha luna88k mac68k    |      0      |     64
 * sparc(OpenBSD) vax      |             |
 * ------------------------+-------------+------------
 * sparc64 sparc(NetBSD)   |      0      |    128
 * ------------------------+-------------+------------
 *
 * ...and more (see http://fxr.watson.org/fxr/ident?v=NETBSD;i=LABELSECTOR)
 *
 */
const struct blkid_idinfo bsd_pt_idinfo =
{
	.name		= "bsd",
	.probefunc	= probe_bsd_pt,
	.magics		=
	{
		{ .magic = "\x57\x45\x56\x82", .len = 4, .sboff = 512 },
		{ .magic = "\x57\x45\x56\x82", .len = 4, .sboff = 64 },
		{ .magic = "\x57\x45\x56\x82", .len = 4, .sboff = 128 },
		{ NULL }
	}
};

