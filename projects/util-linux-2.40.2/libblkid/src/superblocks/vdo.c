/*
 * Copyright (C) 2017 Red Hat, Inc.
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
#include <stdint.h>

#include "superblocks.h"

struct vdo_super_block {
	char magic[8];			/* magic number 'dmvdo001'*/
	char unused[32];		/* 32 bytes of unimportant space */
	unsigned char sb_uuid[16];	/* vdo unique id */

	/* this is not all... but enough for libblkid */
} __attribute__((packed));

static int probe_vdo(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct vdo_super_block *vsb;

	vsb = blkid_probe_get_sb(pr, mag, struct vdo_super_block);
	if (!vsb)
		return errno ? -errno : 1;

	blkid_probe_set_uuid(pr, vsb->sb_uuid);
	return 0;
}

const struct blkid_idinfo vdo_idinfo =
{
	.name		= "vdo",
	.usage		= BLKID_USAGE_OTHER,
	.probefunc	= probe_vdo,
	.magics		=
	{
		{ .magic = "dmvdo001", .len = 8 },
		{ NULL }
	}
};
