/*
 * sgi partition parsing code
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"
#include "pt-sgi.h"

static int probe_sgi_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct sgi_disklabel *l;
	struct sgi_partition *p;
	blkid_parttable tab = NULL;
	blkid_partlist ls;
	int i;

	l = (struct sgi_disklabel *) blkid_probe_get_sector(pr, 0);
	if (!l) {
		if (errno)
			return -errno;
		goto nothing;
	}

	if (!blkid_probe_verify_csum(pr, sgi_pt_checksum(l), 0)) {
		DBG(LOWPROBE, ul_debug(
			"detected corrupted sgi disk label -- ignore"));
		goto nothing;
	}

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return BLKID_PROBE_OK;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto nothing;

	tab = blkid_partlist_new_parttable(ls, "sgi", 0);
	if (!tab)
		goto err;

	for(i = 0, p = &l->partitions[0]; i < SGI_MAXPARTITIONS; i++, p++) {
		uint32_t size = be32_to_cpu(p->num_blocks);
		uint32_t start = be32_to_cpu(p->first_block);
		uint32_t type = be32_to_cpu(p->type);
		blkid_partition par;

		if (!size) {
			blkid_partlist_increment_partno(ls);
			continue;
		}
		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			goto err;

		blkid_partition_set_type(par, type);
	}

	return BLKID_PROBE_OK;

nothing:
	return BLKID_PROBE_NONE;
err:
	return -ENOMEM;
}

const struct blkid_idinfo sgi_pt_idinfo =
{
	.name		= "sgi",
	.probefunc	= probe_sgi_pt,
	.magics		=
	{
		{ .magic = "\x0B\xE5\xA9\x41", .len = 4	},
		{ NULL }
	}
};

