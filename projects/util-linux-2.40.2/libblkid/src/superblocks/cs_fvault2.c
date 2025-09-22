/*
 * Copyright (C) 2022 Milan Broz <gmazyland@gmail.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include "superblocks.h"
#include "crc32c.h"

/*
 * For header details, see:
 * https://github.com/libyal/libfvde/blob/main/documentation/FileVault%20Drive%20Encryption%20(FVDE).asciidoc
 * https://is.muni.cz/auth/th/p0aok/thesis.pdf
 */

/* Apple Core Storage magic bytes */
#define CS_MAGIC	"CS"

struct crc32_checksum {
	uint32_t value;
	uint32_t seed;
} __attribute__((packed));

/*
 * The superblock structure describes "physical volume"; Core Storage
 * then uses another abstractions above, similar to LVM.
 * After activation through dm-crypt, filesystem (usually HFS+) is on top.
 * The filesystem block size and used data size cannot be directly derived from
 * this superblock structure without parsing other metadata blocks.
 */

struct cs_fvault2_sb {
	struct crc32_checksum checksum;
	uint16_t version;
	uint16_t block_type;
	uint8_t unknown1[52];
	uint64_t ph_vol_size;
	uint8_t unknown2[16];
	uint16_t magic;
	uint32_t checksum_algo;
	uint8_t unknown3[2];
	uint32_t block_size;
	uint32_t metadata_size;
	uint64_t disklbl_blkoff;
	uint64_t other_md_blkoffs[3];
	uint8_t unknown4[32];
	uint32_t key_data_size;
	uint32_t cipher;
	uint8_t key_data[16];
	uint8_t unknown5[112];
	uint8_t ph_vol_uuid[16];
	uint8_t unknown6[192];
} __attribute__((packed));

static int cs_fvault2_verify_csum(blkid_probe pr, const struct cs_fvault2_sb *sb)
{
	uint32_t seed = le32_to_cpu(sb->checksum.seed);
	uint32_t crc = le32_to_cpu(sb->checksum.value);
	unsigned char *buf = (unsigned char *)sb + sizeof(sb->checksum);
	size_t buf_size = sizeof(*sb) - sizeof(sb->checksum);

	return blkid_probe_verify_csum(pr, crc32c(seed, buf, buf_size), crc);
}

static int probe_cs_fvault2(blkid_probe pr, const struct blkid_idmag *mag)
{
	const struct cs_fvault2_sb *sb;

	sb = blkid_probe_get_sb(pr, mag, struct cs_fvault2_sb);
	if (!sb)
		return errno ? -errno : BLKID_PROBE_NONE;

	/* Apple Core storage Physical Volume Header; only type 1 checksum is supported  */
	if (le16_to_cpu(sb->version) != 1 ||
	    le32_to_cpu(sb->checksum_algo) != 1)
		return BLKID_PROBE_NONE;

	if (!cs_fvault2_verify_csum(pr, sb))
		return BLKID_PROBE_NONE;

	/* We support only block type 0x10 as it should be used for FileVault2 */
	if (le16_to_cpu(sb->block_type) != 0x10 ||
	    le32_to_cpu(sb->key_data_size) != 16 ||
	    le32_to_cpu(sb->cipher) != 2 /* AES-XTS */)
		return BLKID_PROBE_NONE;

	blkid_probe_sprintf_version(pr, "%u", le16_to_cpu(sb->version));
	blkid_probe_set_uuid(pr, sb->ph_vol_uuid);

	return BLKID_PROBE_OK;
}

const struct blkid_idinfo cs_fvault2_idinfo =
{
	.name		= "cs_fvault2",
	.usage		= BLKID_USAGE_CRYPTO,
	.probefunc	= probe_cs_fvault2,
	.magics		=
	{
		{ .magic = CS_MAGIC, .len = 2, .sboff = 88 },
		{ NULL }
	}
};
