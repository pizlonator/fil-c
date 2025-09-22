/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
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

/* linux-2.6/include/linux/swap.h */
struct swap_header_v1_2 {
     /*	char		bootbits[1024];	*/ /* Space for disklabel etc. */
	uint32_t	version;
	uint32_t	lastpage;
	uint32_t	nr_badpages;
	unsigned char	uuid[16];
	unsigned char	volume[16];
	uint32_t	padding[117];
	uint32_t	badpages[1];
} __attribute__((packed));

#define PAGESIZE_MIN	0xff6	/* 4086 (arm, i386, ...) */
#define PAGESIZE_MAX	0xfff6	/* 65526 (ia64) */

#define TOI_MAGIC_STRING	"\xed\xc3\x02\xe9\x98\x56\xe5\x0c"
#define TOI_MAGIC_STRLEN	(sizeof(TOI_MAGIC_STRING) - 1)

static void swap_set_info_swap1(blkid_probe pr,
		const struct blkid_idmag *mag,
		const struct swap_header_v1_2 *hdr)
{
	enum blkid_endianness endianness = le32_to_cpu(hdr->version) == 1 ?
		BLKID_ENDIANNESS_LITTLE : BLKID_ENDIANNESS_BIG;
	blkid_probe_set_fsendianness(pr, endianness);

	uint32_t pagesize = mag->sboff + mag->len;
	blkid_probe_set_fsblocksize(pr, pagesize);

	uint32_t lastpage = endianness == BLKID_ENDIANNESS_LITTLE ?
		le32_to_cpu(hdr->lastpage) : be32_to_cpu(hdr->lastpage);
	blkid_probe_set_fssize(pr, (uint64_t) pagesize * lastpage);
}

static int swap_set_info(blkid_probe pr, const struct blkid_idmag *mag,
		const char *version)
{
	struct swap_header_v1_2 *hdr;

	/* Swap header always located at offset of 1024 bytes */
	hdr = (struct swap_header_v1_2 *) blkid_probe_get_buffer(pr, 1024,
				sizeof(struct swap_header_v1_2));
	if (!hdr)
		return errno ? -errno : 1;

	/* SWAPSPACE2 - check for wrong version or zeroed pagecount */
	if (strcmp(version, "1") == 0) {
		if (hdr->version != 1 && swab32(hdr->version) != 1) {
			DBG(LOWPROBE, ul_debug("incorrect swap version"));
			return 1;
		}
		if (hdr->lastpage == 0) {
			DBG(LOWPROBE, ul_debug("not set last swap page"));
			return 1;
		}
		swap_set_info_swap1(pr, mag, hdr);
	}

	/* arbitrary sanity check.. is there any garbage down there? */
	if (hdr->padding[32] == 0 && hdr->padding[33] == 0) {
		if (hdr->volume[0] && blkid_probe_set_label(pr, hdr->volume,
				sizeof(hdr->volume)) < 0)
			return 1;
		if (blkid_probe_set_uuid(pr, hdr->uuid) < 0)
			return 1;
	}

	blkid_probe_set_version(pr, version);
	return 0;
}

static int probe_swap(blkid_probe pr, const struct blkid_idmag *mag)
{
	const unsigned char *buf;

	if (!mag)
		return 1;

	/* TuxOnIce keeps valid swap header at the end of the 1st page */
	buf = blkid_probe_get_buffer(pr, 0, TOI_MAGIC_STRLEN);
	if (!buf)
		return errno ? -errno : 1;

	if (memcmp(buf, TOI_MAGIC_STRING, TOI_MAGIC_STRLEN) == 0)
		return 1;	/* Ignore swap signature, it's TuxOnIce */

	if (!memcmp(mag->magic, "SWAP-SPACE", mag->len)) {
		/* swap v0 doesn't support LABEL or UUID */
		blkid_probe_set_version(pr, "0");
		return 0;

	}

	if (!memcmp(mag->magic, "SWAPSPACE2", mag->len))
		return swap_set_info(pr, mag, "1");

	return 1;
}

static int probe_swsuspend(blkid_probe pr, const struct blkid_idmag *mag)
{
	if (!mag)
		return 1;
	if (!memcmp(mag->magic, "S1SUSPEND", mag->len))
		return swap_set_info(pr, mag, "s1suspend");
	if (!memcmp(mag->magic, "S2SUSPEND", mag->len))
		return swap_set_info(pr, mag, "s2suspend");
	if (!memcmp(mag->magic, "ULSUSPEND", mag->len))
		return swap_set_info(pr, mag, "ulsuspend");
	if (!memcmp(mag->magic, TOI_MAGIC_STRING, TOI_MAGIC_STRLEN))
		return swap_set_info(pr, mag, "tuxonice");
	if (!memcmp(mag->magic, "LINHIB0001", mag->len))
		return swap_set_info(pr, mag, "linhib0001");

	return 1;	/* no signature detected */
}

const struct blkid_idinfo swap_idinfo =
{
	.name		= "swap",
	.usage		= BLKID_USAGE_OTHER,
	.probefunc	= probe_swap,
	.minsz		= 10 * 4096,	/* 10 pages */
	.magics		=
	{
		{ .magic = "SWAP-SPACE", .len = 10, .sboff = 0xff6 },
		{ .magic = "SWAPSPACE2", .len = 10, .sboff = 0xff6 },
		{ .magic = "SWAP-SPACE", .len = 10, .sboff = 0x1ff6 },
		{ .magic = "SWAPSPACE2", .len = 10, .sboff = 0x1ff6 },
		{ .magic = "SWAP-SPACE", .len = 10, .sboff = 0x3ff6 },
		{ .magic = "SWAPSPACE2", .len = 10, .sboff = 0x3ff6 },
		{ .magic = "SWAP-SPACE", .len = 10, .sboff = 0x7ff6 },
		{ .magic = "SWAPSPACE2", .len = 10, .sboff = 0x7ff6 },
		{ .magic = "SWAP-SPACE", .len = 10, .sboff = 0xfff6 },
		{ .magic = "SWAPSPACE2", .len = 10, .sboff = 0xfff6 },
		{ NULL }
	}
};


const struct blkid_idinfo swsuspend_idinfo =
{
	.name		= "swsuspend",
	.usage		= BLKID_USAGE_OTHER,
	.probefunc	= probe_swsuspend,
	.minsz		= 10 * 4096,	/* 10 pages */
	.magics		=
	{
		{ .magic = TOI_MAGIC_STRING, .len = TOI_MAGIC_STRLEN },
		{ .magic = "S1SUSPEND", .len = 9, .sboff = 0xff6 },
		{ .magic = "S2SUSPEND", .len = 9, .sboff = 0xff6 },
		{ .magic = "ULSUSPEND", .len = 9, .sboff = 0xff6 },
		{ .magic = "LINHIB0001", .len = 10, .sboff = 0xff6 },

		{ .magic = "S1SUSPEND", .len = 9, .sboff = 0x1ff6 },
		{ .magic = "S2SUSPEND", .len = 9, .sboff = 0x1ff6 },
		{ .magic = "ULSUSPEND", .len = 9, .sboff = 0x1ff6 },
		{ .magic = "LINHIB0001", .len = 10, .sboff = 0x1ff6 },

		{ .magic = "S1SUSPEND", .len = 9, .sboff = 0x3ff6 },
		{ .magic = "S2SUSPEND", .len = 9, .sboff = 0x3ff6 },
		{ .magic = "ULSUSPEND", .len = 9, .sboff = 0x3ff6 },
		{ .magic = "LINHIB0001", .len = 10, .sboff = 0x3ff6 },

		{ .magic = "S1SUSPEND", .len = 9, .sboff = 0x7ff6 },
		{ .magic = "S2SUSPEND", .len = 9, .sboff = 0x7ff6 },
		{ .magic = "ULSUSPEND", .len = 9, .sboff = 0x7ff6 },
		{ .magic = "LINHIB0001", .len = 10, .sboff = 0x7ff6 },

		{ .magic = "S1SUSPEND", .len = 9, .sboff = 0xfff6 },
		{ .magic = "S2SUSPEND", .len = 9, .sboff = 0xfff6 },
		{ .magic = "ULSUSPEND", .len = 9, .sboff = 0xfff6 },
		{ .magic = "LINHIB0001", .len = 10, .sboff = 0xfff6 },
		{ NULL }
	}
};
