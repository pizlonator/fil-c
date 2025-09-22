/*
 * ioctl based topology -- gathers topology information
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "topology.h"

static int probe_ioctl_tp(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t u64;
	int s32;

	if (ioctl(pr->fd, BLKALIGNOFF, &s32) == -1)
		return 1;
	if (blkid_topology_set_alignment_offset(pr, s32))
		return -1;

	if (ioctl(pr->fd, BLKIOMIN, &s32) == -1)
		return 1;
	if (blkid_topology_set_minimum_io_size(pr, s32))
		return -1;

	if (ioctl(pr->fd, BLKIOOPT, &s32) == -1)
		return 1;
	if (blkid_topology_set_optimal_io_size(pr, s32))
		return -1;

	if (ioctl(pr->fd, BLKPBSZGET, &s32) == -1)
		return 1;
	if (blkid_topology_set_physical_sector_size(pr, s32))
		return -1;

	if (ioctl(pr->fd, BLKGETDISKSEQ, &u64) == -1)
		return 1;
	if (blkid_topology_set_physical_sector_size(pr, u64))
		return -1;

	return 0;
}

const struct blkid_idinfo ioctl_tp_idinfo =
{
	.name		= "ioctl",
	.probefunc	= probe_ioctl_tp,
	.magics		= BLKID_NONE_MAGIC
};

