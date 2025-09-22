/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2013 Eric Sandeen <sandeen@redhat.com>
 * Copyright (C) 2017 Hewlett Packard Enterprise Development LP
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

struct exfs_super_block {
	uint32_t	sb_magicnum;	/* magic number == EXFS_SB_MAGIC */
	uint32_t	sb_blocksize;	/* logical block size, bytes */
	uint64_t	sb_dblocks;	/* number of data blocks */
	uint64_t	sb_rblocks;	/* number of realtime blocks */
	uint64_t	sb_rextents;	/* number of realtime extents */
	unsigned char	sb_uuid[16];	/* file system unique id */
	uint64_t	sb_logstart;	/* starting block of log if internal */
	uint64_t	sb_rootino;	/* root inode number */
	uint64_t	sb_rbmino;	/* bitmap inode for realtime extents */
	uint64_t	sb_rsumino;	/* summary inode for rt bitmap */
	uint32_t	sb_rextsize;	/* realtime extent size, blocks */
	uint32_t	sb_agblocks;	/* size of an allocation group */
	uint32_t	sb_agcount;	/* number of allocation groups */
	uint32_t	sb_rbmblocks;	/* number of rt bitmap blocks */
	uint32_t	sb_logblocks;	/* number of log blocks */

	uint16_t	sb_versionnum;	/* header version == EXFS_SB_VERSION */
	uint16_t	sb_sectsize;	/* volume sector size, bytes */
	uint16_t	sb_inodesize;	/* inode size, bytes */
	uint16_t	sb_inopblock;	/* inodes per block */
	char		sb_fname[12];	/* file system name */
	uint8_t		sb_blocklog;	/* log2 of sb_blocksize */
	uint8_t		sb_sectlog;	/* log2 of sb_sectsize */
	uint8_t		sb_inodelog;	/* log2 of sb_inodesize */
	uint8_t		sb_inopblog;	/* log2 of sb_inopblock */
	uint8_t		sb_agblklog;	/* log2 of sb_agblocks (rounded up) */
	uint8_t		sb_rextslog;	/* log2 of sb_rextents */
	uint8_t		sb_inprogress;	/* mkfs is in progress, don't mount */
	uint8_t		sb_imax_pct;	/* max % of fs for inode space */
					/* statistics */
	uint64_t	sb_icount;	/* allocated inodes */
	uint64_t	sb_ifree;	/* free inodes */
	uint64_t	sb_fdblocks;	/* free data blocks */
	uint64_t	sb_frextents;	/* free realtime extents */

	/* this is not all... but enough for libblkid */

} __attribute__((packed));

#define EXFS_MIN_BLOCKSIZE_LOG	9	/* i.e. 512 bytes */
#define EXFS_MAX_BLOCKSIZE_LOG	16	/* i.e. 65536 bytes */
#define EXFS_MIN_BLOCKSIZE	(1 << EXFS_MIN_BLOCKSIZE_LOG)
#define EXFS_MAX_BLOCKSIZE	(1 << EXFS_MAX_BLOCKSIZE_LOG)
#define EXFS_MIN_SECTORSIZE_LOG	9	/* i.e. 512 bytes */
#define EXFS_MAX_SECTORSIZE_LOG	15	/* i.e. 32768 bytes */
#define EXFS_MIN_SECTORSIZE	(1 << EXFS_MIN_SECTORSIZE_LOG)
#define EXFS_MAX_SECTORSIZE	(1 << EXFS_MAX_SECTORSIZE_LOG)

#define	EXFS_DINODE_MIN_LOG	8
#define	EXFS_DINODE_MAX_LOG	11
#define	EXFS_DINODE_MIN_SIZE	(1 << EXFS_DINODE_MIN_LOG)
#define	EXFS_DINODE_MAX_SIZE	(1 << EXFS_DINODE_MAX_LOG)

#define	EXFS_MAX_RTEXTSIZE	(1024 * 1024 * 1024)	/* 1GB */
#define	EXFS_DFL_RTEXTSIZE	(64 * 1024)		/* 64kB */
#define	EXFS_MIN_RTEXTSIZE	(4 * 1024)		/* 4kB */

#define EXFS_MIN_AG_BLOCKS	64
#define EXFS_MAX_DBLOCKS(s) ((uint64_t)(s)->sb_agcount * (s)->sb_agblocks)
#define EXFS_MIN_DBLOCKS(s) ((uint64_t)((s)->sb_agcount - 1) *	\
			 (s)->sb_agblocks + EXFS_MIN_AG_BLOCKS)


static void sb_from_disk(const struct exfs_super_block *from,
			 struct exfs_super_block *to)
{

	to->sb_magicnum = be32_to_cpu(from->sb_magicnum);
	to->sb_blocksize = be32_to_cpu(from->sb_blocksize);
	to->sb_dblocks = be64_to_cpu(from->sb_dblocks);
	to->sb_rblocks = be64_to_cpu(from->sb_rblocks);
	to->sb_rextents = be64_to_cpu(from->sb_rextents);
	to->sb_logstart = be64_to_cpu(from->sb_logstart);
	to->sb_rootino = be64_to_cpu(from->sb_rootino);
	to->sb_rbmino = be64_to_cpu(from->sb_rbmino);
	to->sb_rsumino = be64_to_cpu(from->sb_rsumino);
	to->sb_rextsize = be32_to_cpu(from->sb_rextsize);
	to->sb_agblocks = be32_to_cpu(from->sb_agblocks);
	to->sb_agcount = be32_to_cpu(from->sb_agcount);
	to->sb_rbmblocks = be32_to_cpu(from->sb_rbmblocks);
	to->sb_logblocks = be32_to_cpu(from->sb_logblocks);
	to->sb_versionnum = be16_to_cpu(from->sb_versionnum);
	to->sb_sectsize = be16_to_cpu(from->sb_sectsize);
	to->sb_inodesize = be16_to_cpu(from->sb_inodesize);
	to->sb_inopblock = be16_to_cpu(from->sb_inopblock);
	to->sb_blocklog = from->sb_blocklog;
	to->sb_sectlog = from->sb_sectlog;
	to->sb_inodelog = from->sb_inodelog;
	to->sb_inopblog = from->sb_inopblog;
	to->sb_agblklog = from->sb_agblklog;
	to->sb_rextslog = from->sb_rextslog;
	to->sb_inprogress = from->sb_inprogress;
	to->sb_imax_pct = from->sb_imax_pct;
	to->sb_icount = be64_to_cpu(from->sb_icount);
	to->sb_ifree = be64_to_cpu(from->sb_ifree);
	to->sb_fdblocks = be64_to_cpu(from->sb_fdblocks);
	to->sb_frextents = be64_to_cpu(from->sb_frextents);
}

static int exfs_verify_sb(const struct exfs_super_block *ondisk)
{
	struct exfs_super_block sb, *sbp = &sb;

	/* beXX_to_cpu(), but don't convert UUID and fsname! */
	sb_from_disk(ondisk, sbp);

	/* sanity checks, we don't want to rely on magic string only */
	if (sbp->sb_agcount <= 0					||
	    sbp->sb_sectsize < EXFS_MIN_SECTORSIZE			||
	    sbp->sb_sectsize > EXFS_MAX_SECTORSIZE			||
	    sbp->sb_sectlog < EXFS_MIN_SECTORSIZE_LOG			||
	    sbp->sb_sectlog > EXFS_MAX_SECTORSIZE_LOG			||
	    sbp->sb_sectsize != (1 << sbp->sb_sectlog)			||
	    sbp->sb_blocksize < EXFS_MIN_BLOCKSIZE			||
	    sbp->sb_blocksize > EXFS_MAX_BLOCKSIZE			||
	    sbp->sb_blocklog < EXFS_MIN_BLOCKSIZE_LOG			||
	    sbp->sb_blocklog > EXFS_MAX_BLOCKSIZE_LOG			||
	    sbp->sb_blocksize != (1ULL << sbp->sb_blocklog)		||
	    sbp->sb_inodesize < EXFS_DINODE_MIN_SIZE			||
	    sbp->sb_inodesize > EXFS_DINODE_MAX_SIZE			||
	    sbp->sb_inodelog < EXFS_DINODE_MIN_LOG			||
	    sbp->sb_inodelog > EXFS_DINODE_MAX_LOG			||
	    sbp->sb_inodesize != (1 << sbp->sb_inodelog)		||
	    (sbp->sb_blocklog - sbp->sb_inodelog != sbp->sb_inopblog)	||
	    (sbp->sb_rextsize * sbp->sb_blocksize > EXFS_MAX_RTEXTSIZE)	||
	    (sbp->sb_rextsize * sbp->sb_blocksize < EXFS_MIN_RTEXTSIZE)	||
	    (sbp->sb_imax_pct > 100 /* zero sb_imax_pct is valid */)	||
	    sbp->sb_dblocks == 0					||
	    sbp->sb_dblocks > EXFS_MAX_DBLOCKS(sbp)			||
	    sbp->sb_dblocks < EXFS_MIN_DBLOCKS(sbp))
		return 0;
	return 1;
}

static int probe_exfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct exfs_super_block *xs;

	xs = blkid_probe_get_sb(pr, mag, struct exfs_super_block);
	if (!xs)
		return errno ? -errno : 1;

	if (!exfs_verify_sb(xs))
		return 1;

	if (*xs->sb_fname != '\0')
		blkid_probe_set_label(pr, (unsigned char *) xs->sb_fname,
				sizeof(xs->sb_fname));

	blkid_probe_set_uuid(pr, xs->sb_uuid);

	blkid_probe_set_fsblocksize(pr, be32_to_cpu(xs->sb_blocksize));
	blkid_probe_set_block_size(pr, be32_to_cpu(xs->sb_blocksize));

	return 0;
}

const struct blkid_idinfo exfs_idinfo =
{
	.name		= "exfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_exfs,
	.magics		=
	{
		{ .magic = "EXFS", .len = 4 },
		{ NULL }
	}
};
