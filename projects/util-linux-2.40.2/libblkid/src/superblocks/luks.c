/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2018-2024 Milan Broz <gmazyland@gmail.com>
 *
 * Inspired by libvolume_id by
 *     Kay Sievers <kay.sievers@vrfy.org>
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
#include <stdbool.h>

#include "superblocks.h"

#define LUKS_CIPHERNAME_L		32
#define LUKS_CIPHERMODE_L		32
#define LUKS_HASHSPEC_L			32
#define LUKS_DIGESTSIZE			20
#define LUKS_SALTSIZE			32
#define LUKS_MAGIC_L			6
#define UUID_STRING_L			40
#define LUKS2_LABEL_L			48
#define LUKS2_SALT_L			64
#define LUKS2_CHECKSUM_ALG_L		32
#define LUKS2_CHECKSUM_L		64

#define LUKS_MAGIC	"LUKS\xba\xbe"
#define LUKS_MAGIC_2	"SKUL\xba\xbe"

/* Offsets for secondary header (for scan if primary header is corrupted). */
#define LUKS2_HDR2_OFFSETS { 0x04000, 0x008000, 0x010000, 0x020000, \
                             0x40000, 0x080000, 0x100000, 0x200000, 0x400000 }

static const uint64_t secondary_offsets[] = LUKS2_HDR2_OFFSETS;

struct luks_phdr {
	uint8_t		magic[LUKS_MAGIC_L];
	uint16_t	version;
	uint8_t		cipherName[LUKS_CIPHERNAME_L];
	uint8_t		cipherMode[LUKS_CIPHERMODE_L];
	uint8_t		hashSpec[LUKS_HASHSPEC_L];
	uint32_t	payloadOffset;
	uint32_t	keyBytes;
	uint8_t		mkDigest[LUKS_DIGESTSIZE];
	uint8_t		mkDigestSalt[LUKS_SALTSIZE];
	uint32_t	mkDigestIterations;
	uint8_t		uuid[UUID_STRING_L];
} __attribute__((packed));

struct luks2_phdr {
	char		magic[LUKS_MAGIC_L];
	uint16_t	version;
	uint64_t	hdr_size;	/* in bytes, including JSON area */
	uint64_t	seqid;		/* increased on every update */
	char		label[LUKS2_LABEL_L];
	char		checksum_alg[LUKS2_CHECKSUM_ALG_L];
	uint8_t		salt[LUKS2_SALT_L]; /* unique for every header/offset */
	char		uuid[UUID_STRING_L];
	char		subsystem[LUKS2_LABEL_L]; /* owner subsystem label */
	uint64_t	hdr_offset;	/* offset from device start in bytes */
	char		_padding[184];
	uint8_t		csum[LUKS2_CHECKSUM_L];
	/* Padding to 4k, then JSON area */
} __attribute__ ((packed));

static int luks_attributes(blkid_probe pr, struct luks2_phdr *header, uint64_t offset)
{
	int version;
	struct luks_phdr *header_v1;

	if (blkid_probe_set_magic(pr, offset, LUKS_MAGIC_L, (unsigned char *) &header->magic))
		return BLKID_PROBE_NONE;

	version = be16_to_cpu(header->version);
	blkid_probe_sprintf_version(pr, "%u", version);

	if (version == 1) {
		header_v1 = (struct luks_phdr *)header;
		blkid_probe_strncpy_uuid(pr,
			(unsigned char *) header_v1->uuid, UUID_STRING_L);
	} else if (version == 2) {
		blkid_probe_strncpy_uuid(pr,
			(unsigned char *) header->uuid, UUID_STRING_L);
		blkid_probe_set_label(pr,
			(unsigned char *) header->label, LUKS2_LABEL_L);
		blkid_probe_set_id_label(pr, "SUBSYSTEM",
			(unsigned char *) header->subsystem, LUKS2_LABEL_L);
	}

	return BLKID_PROBE_OK;
}

static bool luks_valid(struct luks2_phdr *header, const char *magic, uint64_t offset)
{
	if (memcmp(header->magic, magic, LUKS_MAGIC_L))
		return false;

	/* LUKS2 header is not at expected offset */
	if (be16_to_cpu(header->version) == 2 &&
	    be64_to_cpu(header->hdr_offset) != offset)
		return false;

	return true;
}

static int probe_luks(blkid_probe pr, const struct blkid_idmag *mag __attribute__((__unused__)))
{
	struct luks2_phdr *header;
	size_t i;

	header = (struct luks2_phdr *) blkid_probe_get_buffer(pr, 0, sizeof(struct luks2_phdr));
	if (!header)
		return errno ? -errno : BLKID_PROBE_NONE;

	if (luks_valid(header, LUKS_MAGIC, 0)) {
		/* LUKS primary header was found. */
		return luks_attributes(pr, header, 0);
	}

	/* No primary header, scan for known offsets of LUKS2 secondary header. */
	for (i = 0; i < ARRAY_SIZE(secondary_offsets); i++) {
		header = (struct luks2_phdr *) blkid_probe_get_buffer(pr,
			  secondary_offsets[i], sizeof(struct luks2_phdr));

		if (!header)
			return errno ? -errno : BLKID_PROBE_NONE;

		if (luks_valid(header, LUKS_MAGIC_2, secondary_offsets[i]))
			return luks_attributes(pr, header, secondary_offsets[i]);
	}

	return BLKID_PROBE_NONE;
}

const struct blkid_idinfo luks_idinfo =
{
	.name		= "crypto_LUKS",
	.usage		= BLKID_USAGE_CRYPTO,
	.probefunc	= probe_luks,
	.magics		= BLKID_NONE_MAGIC
};
