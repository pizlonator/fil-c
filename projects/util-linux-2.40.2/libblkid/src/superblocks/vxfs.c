/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>

#include "superblocks.h"

struct vxfs_super_block {
	uint32_t		vs_magic;
	int32_t			vs_version;
	uint32_t		vs_ctime;
	uint32_t		vs_cutime;
	uint32_t		__unused1;
	uint32_t		__unused2;
	uint32_t		vs_old_logstart;
	uint32_t		vs_old_logend;
	uint32_t		vs_bsize;
	uint32_t		vs_size;
	uint32_t		vs_dsize;
};

static int probe_vxfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct vxfs_super_block *vxs;
	enum blkid_endianness e = mag->hint;

	vxs = blkid_probe_get_sb(pr, mag, struct vxfs_super_block);
	if (!vxs)
		return errno ? -errno : 1;

	blkid_probe_sprintf_version(pr, "%d",
				    (unsigned int)blkid32_to_cpu(e, vxs->vs_version));
	blkid_probe_set_fsblocksize(pr, blkid32_to_cpu(e, vxs->vs_bsize));
	blkid_probe_set_block_size(pr, blkid32_to_cpu(e, vxs->vs_bsize));
	blkid_probe_set_fsendianness(pr, e);

	return 0;
}


const struct blkid_idinfo vxfs_idinfo =
{
	.name		= "vxfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_vxfs,
	.magics		=
	{
		{ .magic = "\xf5\xfc\x01\xa5", .len = 4, .kboff = 1,
		  .hint = BLKID_ENDIANNESS_LITTLE },
		{ .magic = "\xa5\x01\xfc\xf5", .len = 4, .kboff = 8,
		  .hint = BLKID_ENDIANNESS_BIG },
		{ NULL }
	}
};

