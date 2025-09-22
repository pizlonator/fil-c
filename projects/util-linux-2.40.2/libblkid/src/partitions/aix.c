/*
 * aix partitions
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"
#include "aix.h"

static int probe_aix_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	blkid_partlist ls;
	blkid_parttable tab;

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return BLKID_PROBE_OK;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		return BLKID_PROBE_NONE;

	tab = blkid_partlist_new_parttable(ls, "aix", 0);
	if (!tab)
		return -ENOMEM;

	return BLKID_PROBE_OK;
}

/*
 * We know nothing about AIX on-disk structures. Everything what we know is the
 * magic number at begin of the disk.
 *
 * Note, Linux kernel is trying to be smart and AIX signature is ignored when
 * there is a valid DOS partitions table. We don't support such behavior. All
 * fdisk-like programs has to properly wipe the fist sector. Everything other
 * is a bug.
 */
const struct blkid_idinfo aix_pt_idinfo =
{
	.name		= "aix",
	.probefunc	= probe_aix_pt,
	.magics		=
	{
		{ .magic = BLKID_AIX_MAGIC_STRING, .len = BLKID_AIX_MAGIC_STRLEN },
		{ NULL }
	}
};

