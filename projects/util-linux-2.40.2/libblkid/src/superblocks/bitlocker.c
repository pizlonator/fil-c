/*
 * Copyright (C) 2018 Karel Zak <kzak@redhat.com>
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

#define BDE_HDR_SIZE	512
#define BDE_HDR_OFFSET	0

struct bde_header_win7 {
/*   0 */ unsigned char	boot_entry_point[3];
/*   3 */ unsigned char	fs_signature[8];
/*  11 */ unsigned char	__dummy1[67 - 11];
/*  67 */ uint32_t      volume_serial;		/* NTFS uses 64bit serial number */
/*  71 */ unsigned char volume_label[11];	/* "NO NAME\x20\x20\x20\x20" only */
/*  82 */ unsigned char __dummy2[160 - 82];
/* 160 */ unsigned char guid[16];		/* BitLocker specific GUID */
/* 176 */ uint64_t      fve_metadata_offset;
} __attribute__((packed));


struct bde_header_togo {
/*   0 */ unsigned char	boot_entry_point[3];
/*   3 */ unsigned char	fs_signature[8];
/*  11 */ unsigned char	__dummy[424 - 11];
/* 424 */ unsigned char guid[16];
/* 440 */ uint64_t      fve_metadata_offset;
} __attribute__((packed));


struct bde_fve_metadata {
/*   0 */ unsigned char  signature[8];
/*   8 */ uint16_t       size;
/*  10 */ uint16_t       version;
};

enum {
	BDE_VERSION_VISTA = 0,
	BDE_VERSION_WIN7,
	BDE_VERSION_TOGO
};

#define BDE_MAGIC_VISTA		"\xeb\x52\x90-FVE-FS-"
#define BDE_MAGIC_WIN7		"\xeb\x58\x90-FVE-FS-"
#define BDE_MAGIC_TOGO		"\xeb\x58\x90MSWIN4.1"

#define BDE_MAGIC_FVE		"-FVE-FS-"

static int get_bitlocker_type(const unsigned char *buf)
{
	size_t i;
	static const char *map[] = {
		[BDE_VERSION_VISTA] = BDE_MAGIC_VISTA,
		[BDE_VERSION_WIN7]  = BDE_MAGIC_WIN7,
		[BDE_VERSION_TOGO]  = BDE_MAGIC_TOGO
	};

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		if (memcmp(buf, map[i], 11) == 0)
			return (int) i;
	}

	return -1;
}

/* Returns: < 0 error, 1 nothing, 0 success
 */
static int get_bitlocker_headers(blkid_probe pr,
				int *type,
				const unsigned char **buf_hdr,
				const unsigned char **buf_fve)
{

	const unsigned char *buf;
	const struct bde_fve_metadata *fve;
	uint64_t off = 0;
	int kind;

	if (buf_hdr)
		*buf_hdr = NULL;
	if (buf_fve)
		*buf_fve = NULL;
	if (type)
		*type = -1;

	buf = blkid_probe_get_buffer(pr, BDE_HDR_OFFSET, BDE_HDR_SIZE);
	if (!buf)
		return errno ? -errno : 1;

	kind = get_bitlocker_type(buf);

	/* Check BitLocker header */
	switch (kind) {
	case BDE_VERSION_WIN7:
		off = le64_to_cpu(((const struct bde_header_win7 *) buf)->fve_metadata_offset);
		break;
	case BDE_VERSION_TOGO:
		off = le64_to_cpu(((const struct bde_header_togo *) buf)->fve_metadata_offset);
		break;
	case BDE_VERSION_VISTA:
		goto done;
	default:
		goto nothing;
	}

	if (!off || off % 64)
		goto nothing;
	if (buf_hdr)
		*buf_hdr = buf;

	/* Check Bitlocker FVE metadata header */
	buf = blkid_probe_get_buffer(pr, off, sizeof(struct bde_fve_metadata));
	if (!buf)
		return errno ? -errno : 1;

	fve = (const struct bde_fve_metadata *) buf;
	if (memcmp(fve->signature, BDE_MAGIC_FVE, sizeof(fve->signature)) != 0)
		goto nothing;
	if (buf_fve)
		*buf_fve = buf;
done:
	if (type)
		*type = kind;
	return 0;
nothing:
	return 1;
}

/*
 * This is used by vFAT and NTFS prober to avoid collisions with bitlocker.
 */
int blkid_probe_is_bitlocker(blkid_probe pr)
{
	return get_bitlocker_headers(pr, NULL, NULL, NULL) == 0;
}

static int probe_bitlocker(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	const unsigned char *buf_fve = NULL;
	const unsigned char *buf_hdr = NULL;
	int rc, kind;

	rc = get_bitlocker_headers(pr, &kind, &buf_hdr, &buf_fve);
	if (rc)
		return rc;

	if (kind == BDE_VERSION_WIN7) {
		const struct bde_header_win7 *hdr = (const struct bde_header_win7 *) buf_hdr;

		/* Unfortunately, it seems volume_serial is always zero */
		blkid_probe_sprintf_uuid(pr,
				(const unsigned char *) &hdr->volume_serial,
				sizeof(hdr->volume_serial),
				"%016d", le32_to_cpu(hdr->volume_serial));
	}

	if (buf_fve) {
		const struct bde_fve_metadata *fve = (const struct bde_fve_metadata *) buf_fve;

		blkid_probe_sprintf_version(pr, "%d", fve->version);
	}
	return 0;
}

/* See header details:
 * https://github.com/libyal/libbde/blob/master/documentation/BitLocker%20Drive%20Encryption%20(BDE)%20format.asciidoc
 */
const struct blkid_idinfo bitlocker_idinfo =
{
	.name		= "BitLocker",
	.usage		= BLKID_USAGE_CRYPTO,
	.probefunc	= probe_bitlocker,
	.magics		=
	{
		{ .magic = BDE_MAGIC_VISTA, .len = 11 },
		{ .magic = BDE_MAGIC_WIN7,  .len = 11 },
		{ .magic = BDE_MAGIC_TOGO,  .len = 11 },
		{ NULL }
	}
};
