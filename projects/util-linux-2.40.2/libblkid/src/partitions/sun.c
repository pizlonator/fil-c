/*
 * sun (solaris-sparc) partition parsing code
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
#include <stddef.h>

#include "pt-sun.h"
#include "partitions.h"

static int probe_sun_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct sun_disklabel *l;
	struct sun_partition *p;
	blkid_parttable tab = NULL;
	blkid_partlist ls;
	uint16_t nparts;
	uint64_t spc;
	int i, use_vtoc;

	l = (struct sun_disklabel *) blkid_probe_get_sector(pr, 0);
	if (!l) {
		if (errno)
			return -errno;
		goto nothing;
	}

	if (!blkid_probe_verify_csum(pr, sun_pt_checksum(l), 0)) {
		DBG(LOWPROBE, ul_debug(
			"detected corrupted sun disk label -- ignore"));
		goto nothing;
	}

	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return BLKID_PROBE_OK;

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto nothing;

	tab = blkid_partlist_new_parttable(ls, "sun", 0);
	if (!tab)
		goto err;

	/* sectors per cylinder (partition offset is in cylinders...) */
	spc = (uint64_t) be16_to_cpu(l->nhead) * be16_to_cpu(l->nsect);

	DBG(LOWPROBE, ul_debug("Sun VTOC sanity=%u version=%u nparts=%u",
			be32_to_cpu(l->vtoc.sanity),
			be32_to_cpu(l->vtoc.version),
			be16_to_cpu(l->vtoc.nparts)));

	/* Check to see if we can use the VTOC table */
	use_vtoc = ((be32_to_cpu(l->vtoc.sanity) == SUN_VTOC_SANITY) &&
		    (be32_to_cpu(l->vtoc.version) == SUN_VTOC_VERSION) &&
		    (be16_to_cpu(l->vtoc.nparts) <= SUN_MAXPARTITIONS));

	/* Use 8 partition entries if not specified in validated VTOC */
	nparts = use_vtoc ? be16_to_cpu(l->vtoc.nparts) : SUN_MAXPARTITIONS;

	/*
	 * So that old Linux-Sun partitions continue to work,
	 * allow the VTOC to be used under the additional condition ...
	 */
	use_vtoc = use_vtoc || !(l->vtoc.sanity || l->vtoc.version || l->vtoc.nparts);

	for (i = 0, p = l->partitions; i < nparts; i++, p++) {

		uint64_t start, size;
		uint16_t type = 0, flags = 0;
		blkid_partition par;

		start = be32_to_cpu(p->start_cylinder) * spc;
		size = be32_to_cpu(p->num_sectors);
		if (use_vtoc) {
			type = be16_to_cpu(l->vtoc.infos[i].id);
			flags = be16_to_cpu(l->vtoc.infos[i].flags);
		}

		if (type == SUN_TAG_WHOLEDISK || !size) {
			blkid_partlist_increment_partno(ls);
			continue;
		}
		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			goto err;

		if (type)
			blkid_partition_set_type(par, type);
		if (flags)
			blkid_partition_set_flags(par, flags);
	}
	return BLKID_PROBE_OK;

nothing:
	return BLKID_PROBE_NONE;
err:
	return -ENOMEM;
}


const struct blkid_idinfo sun_pt_idinfo =
{
	.name		= "sun",
	.probefunc	= probe_sun_pt,
	.magics		=
	{
		{
		  .magic = "\xDA\xBE",		/* big-endian magic string */
		  .len = 2,
		  .sboff = offsetof(struct sun_disklabel, magic)
		},
		{ NULL }
	}
};

