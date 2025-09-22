/*
 * Copyright (C) 2015 by Philipp Marek <philipp.marek@linbit.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * DRBD is a blocklevel replication solution in the Linux kernel,
 * upstream since 2.6.33. (See http://drbd.linbit.com/)
 * DRBDmanage is a configuration frontend that assists in
 * creating/deleting/modifying DRBD resources across multiple machines
 * (a DRBDmanage "cluster"); this file detects its control volume,
 * which is replicated (via DRBD 9) on some of the nodes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>

#include "bitops.h"
#include "superblocks.h"

struct drbdmanage_hdr {
	unsigned char magic[11];
	unsigned char uuid[32];
	unsigned char lf;
} __attribute__ ((packed));

struct drbdmanage_pers {
	char magic[4];
	uint32_t version_le;
} __attribute__ ((packed));


static const char persistence_magic[4] = { '\x1a', '\xdb', '\x98', '\xa2' };


static int probe_drbdmanage(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct drbdmanage_hdr *hdr;
	unsigned char *cp;
	struct drbdmanage_pers *prs;

	hdr = (struct drbdmanage_hdr*)
		blkid_probe_get_buffer(pr, 0, sizeof(*hdr));
	if (!hdr)
		return errno ? -errno : 1;

	for(cp=hdr->uuid; cp<&hdr->lf; cp++)
		if (!isxdigit(*cp))
			return 1;
	if (hdr->lf != '\n')
		return 1;

	if (blkid_probe_strncpy_uuid(pr,
				hdr->uuid, sizeof(hdr->uuid)))
		return errno ? -errno : 1;

	prs = (struct drbdmanage_pers*)
		blkid_probe_get_buffer(pr, 0x1000, sizeof(*prs));
	if (!prs)
		return errno ? -errno : 1;

	if (memcmp(prs->magic, persistence_magic, sizeof(prs->magic)) == 0 &&
	    blkid_probe_sprintf_version(pr, "%d", be32_to_cpu(prs->version_le)) != 0)
		return errno ? -errno : 1;

	return 0;
}


const struct blkid_idinfo drbdmanage_idinfo =
{
	.name		= "drbdmanage_control_volume",
	.usage		= BLKID_USAGE_OTHER,
	.probefunc	= probe_drbdmanage,
	.minsz		= 64 * 1024,
	.magics		= {
		{ .magic = "$DRBDmgr=q", .len = 10, .sboff = 0 },
		{ NULL }
	},
};

