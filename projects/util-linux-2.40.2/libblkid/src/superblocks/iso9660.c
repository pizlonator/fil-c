/*
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * Inspired also by libvolume_id by
 *     Kay Sievers <kay.sievers@vrfy.org>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "superblocks.h"
#include "cctype.h"
#include "iso9660.h"

struct hs_date {
	unsigned char year[4];
	unsigned char month[2];
	unsigned char day[2];
	unsigned char hour[2];
	unsigned char minute[2];
	unsigned char second[2];
	unsigned char hundredth[2];
} __attribute__ ((packed));

struct iso9660_date {
	struct hs_date common;
	unsigned char offset;
} __attribute__ ((packed));

/* PVD - Primary volume descriptor */
struct iso_volume_descriptor {
	/* High Sierra has 8 bytes before descriptor with Volume Descriptor LBN value, those are skipped by blkid_probe_get_buffer() */
	unsigned char	vd_type;
	unsigned char	vd_id[5];
	unsigned char	vd_version;
	unsigned char	flags;
	unsigned char	system_id[32];
	unsigned char	volume_id[32];
	unsigned char	unused[8];
	unsigned char	space_size[8];
	unsigned char	escape_sequences[32];
	unsigned char  set_size[4];
	unsigned char  vol_seq_num[4];
	unsigned char  logical_block_size[4];
	unsigned char  path_table_size[8];
	union {
		struct {
			unsigned char type_l_path_table[4];
			unsigned char opt_type_l_path_table[4];
			unsigned char type_m_path_table[4];
			unsigned char opt_type_m_path_table[4];
			unsigned char root_dir_record[34];
			unsigned char volume_set_id[128];
			unsigned char publisher_id[128];
			unsigned char data_preparer_id[128];
			unsigned char application_id[128];
			unsigned char copyright_file_id[37];
			unsigned char abstract_file_id[37];
			unsigned char bibliographic_file_id[37];
			struct iso9660_date created;
			struct iso9660_date modified;
			struct iso9660_date expiration;
			struct iso9660_date effective;
			unsigned char std_version;
		} iso; /* ISO9660 */
		struct {
			unsigned char type_l_path_table[4];
			unsigned char opt1_type_l_path_table[4];
			unsigned char opt2_type_l_path_table[4];
			unsigned char opt3_type_l_path_table[4];
			unsigned char type_m_path_table[4];
			unsigned char opt1_type_m_path_table[4];
			unsigned char opt2_type_m_path_table[4];
			unsigned char opt3_type_m_path_table[4];
			unsigned char root_dir_record[34];
			unsigned char volume_set_id[128];
			unsigned char publisher_id[128];
			unsigned char data_preparer_id[128];
			unsigned char application_id[128];
			unsigned char copyright_file_id[32];
			unsigned char abstract_file_id[32];
			struct hs_date created;
			struct hs_date modified;
			struct hs_date expiration;
			struct hs_date effective;
			unsigned char std_version;
		} hs; /* High Sierra */
	};
} __attribute__((packed));

/* Boot Record */
struct boot_record {
	/* High Sierra has 8 bytes before descriptor with Volume Descriptor LBN value, those are skipped by blkid_probe_get_buffer() */
	unsigned char	vd_type;
	unsigned char	vd_id[5];
	unsigned char	vd_version;
	unsigned char	boot_system_id[32];
	unsigned char	boot_id[32];
	unsigned char	unused[1];
} __attribute__((packed));

#define ISO_SUPERBLOCK_OFFSET		0x8000
#define ISO_SECTOR_SIZE			0x800
#define ISO_VD_BOOT_RECORD		0x0
#define ISO_VD_PRIMARY			0x1
#define ISO_VD_SUPPLEMENTARY		0x2
#define ISO_VD_END			0xff
#define ISO_VD_MAX			16
/* maximal string field size used anywhere in ISO; update if necessary */
#define ISO_MAX_FIELDSIZ  sizeof_member(struct iso_volume_descriptor, iso.volume_set_id)

static int probe_iso9660_set_uuid (blkid_probe pr, const struct hs_date *date, unsigned char offset)
{
	unsigned char buffer[16];
	unsigned int i, zeros = 0;

	buffer[0] = date->year[0];
	buffer[1] = date->year[1];
	buffer[2] = date->year[2];
	buffer[3] = date->year[3];
	buffer[4] = date->month[0];
	buffer[5] = date->month[1];
	buffer[6] = date->day[0];
	buffer[7] = date->day[1];
	buffer[8] = date->hour[0];
	buffer[9] = date->hour[1];
	buffer[10] = date->minute[0];
	buffer[11] = date->minute[1];
	buffer[12] = date->second[0];
	buffer[13] = date->second[1];
	buffer[14] = date->hundredth[0];
	buffer[15] = date->hundredth[1];

	/* count the number of zeros ('0') in the date buffer */
	for (i = 0, zeros = 0; i < sizeof(buffer); i++)
		if (buffer[i] == '0')
			zeros++;

	/* due to the iso9660 standard if all date fields are '0' and offset is 0, the date is unset */
	if (zeros == sizeof(buffer) && offset == 0)
		return 0;

	/* generate an UUID using this date and return success */
	blkid_probe_sprintf_uuid (pr, buffer, sizeof(buffer),
		"%c%c%c%c-%c%c-%c%c-%c%c-%c%c-%c%c-%c%c",
		buffer[0], buffer[1], buffer[2], buffer[3],
		buffer[4], buffer[5],
		buffer[6], buffer[7],
		buffer[8], buffer[9],
		buffer[10], buffer[11],
		buffer[12], buffer[13],
		buffer[14], buffer[15]);

	return 1;
}

static int is_str_empty(const unsigned char *str, size_t len)
{
	size_t i;

	if (!str || !*str)
		return 1;

	for (i = 0; i < len; i++)
		if (!isspace(str[i]))
			return 0;
	return 1;
}

static int is_utf16be_str_empty(unsigned char *utf16, size_t len)
{
	size_t i;

	for (i = 0; i < len; i += 2) {
		if (utf16[i] != 0x0 || !isspace(utf16[i + 1]))
			return 0;
	}
	return 1;
}

/* if @utf16 is prefix of @ascii (ignoring non-representable characters and upper-case conversion)
 * then reconstruct prefix from @utf16 and @ascii, append suffix from @ascii, fill it into @out
 * and returns length of bytes written into @out; otherwise returns zero */
static size_t merge_utf16be_ascii(unsigned char *out, size_t out_len, const unsigned char *utf16, const unsigned char *ascii, size_t len)
{
	size_t o, a, u;

	for (o = 0, a = 0, u = 0; u + 1 < len && a < len && o + 1 < out_len; o += 2, a++, u += 2) {
		/* Surrogate pair with code point above U+FFFF */
		if (utf16[u] >= 0xD8 && utf16[u] <= 0xDB && u + 3 < len &&
		    utf16[u + 2] >= 0xDC && utf16[u + 2] <= 0xDF) {
			out[o++] = utf16[u++];
			out[o++] = utf16[u++];
		}
		/* Value '_' is replacement for non-representable character */
		if (ascii[a] == '_') {
			out[o] = utf16[u];
			out[o + 1] = utf16[u + 1];
		} else if (utf16[u] == 0x00 && utf16[u + 1] == '_') {
			out[o] = 0x00;
			out[o + 1] = ascii[a];
		} else if (utf16[u] == 0x00 && c_toupper(ascii[a]) == c_toupper(utf16[u + 1])) {
			out[o] = 0x00;
			out[o + 1] = c_isupper(ascii[a]) ? utf16[u + 1] : ascii[a];
		} else {
			return 0;
		}
	}

	for (; a < len && o + 1 < out_len; o += 2, a++) {
		out[o] = 0x00;
		out[o + 1] = ascii[a];
	}

	return o;
}

/* iso9660 [+ Microsoft Joliet Extension] */
static int probe_iso9660(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct boot_record *boot = NULL;
	struct iso_volume_descriptor *pvd = NULL;
	struct iso_volume_descriptor *joliet = NULL;
	/* space for merge_utf16be_ascii(ISO_ID_BUFSIZ bytes) */
	unsigned char buf[ISO_MAX_FIELDSIZ * 5 / 2];
	const struct hs_date *modified;
	const struct hs_date *created;
	unsigned char modified_offset;
	unsigned char created_offset;
	size_t len;
	int is_hs;
	int is_unicode_empty;
	int is_ascii_hs_empty;
	int is_ascii_iso_empty;
	int i;
	uint64_t off;

	if (blkid_probe_get_hint(pr, mag->hoff, &off) < 0)
		off = 0;

	if (off % ISO_SECTOR_SIZE)
		return 1;

	is_hs = (strcmp(mag->magic, "CDROM") == 0);

	for (i = 0, off += ISO_SUPERBLOCK_OFFSET; i < ISO_VD_MAX && (!boot || !pvd || (!is_hs && !joliet)); i++, off += ISO_SECTOR_SIZE) {
		const unsigned char *desc =
			blkid_probe_get_buffer(pr,
					off + (is_hs ? 8 : 0), /* High Sierra has 8 bytes before descriptor with Volume Descriptor LBN value */
					max(sizeof(struct boot_record),
					    sizeof(struct iso_volume_descriptor)));

		if (desc == NULL || desc[0] == ISO_VD_END)
			break;
		else if (!boot && desc[0] == ISO_VD_BOOT_RECORD)
			boot = (struct boot_record *)desc;
		else if (!pvd && desc[0] == ISO_VD_PRIMARY)
			pvd = (struct iso_volume_descriptor *)desc;
		else if (!is_hs && !joliet && desc[0] == ISO_VD_SUPPLEMENTARY) {
			joliet = (struct iso_volume_descriptor *)desc;
			if (memcmp(joliet->escape_sequences, "%/@", 3) != 0 &&
			    memcmp(joliet->escape_sequences, "%/C", 3) != 0 &&
			    memcmp(joliet->escape_sequences, "%/E", 3) != 0)
				joliet = NULL;
		}
	}

	if (!pvd)
		return errno ? -errno : 1;

	uint16_t logical_block_size = isonum_723(pvd->logical_block_size, false);
	uint32_t space_size = isonum_733(pvd->space_size, false);

	blkid_probe_set_fsblocksize(pr, logical_block_size);
	blkid_probe_set_block_size(pr, logical_block_size);
	blkid_probe_set_fssize(pr, (uint64_t) space_size * logical_block_size);

	if (joliet && (len = merge_utf16be_ascii(buf, sizeof(buf), joliet->system_id, pvd->system_id, sizeof(pvd->system_id))) != 0)
		blkid_probe_set_utf8_id_label(pr, "SYSTEM_ID", buf, len, UL_ENCODE_UTF16BE);
	else if (joliet)
		blkid_probe_set_utf8_id_label(pr, "SYSTEM_ID", joliet->system_id, sizeof(joliet->system_id), UL_ENCODE_UTF16BE);
	else
		blkid_probe_set_id_label(pr, "SYSTEM_ID", pvd->system_id, sizeof(pvd->system_id));

	if (joliet && (len = merge_utf16be_ascii(buf, sizeof(buf), joliet->iso.volume_set_id, pvd->iso.volume_set_id, sizeof(pvd->iso.volume_set_id))) != 0)
		blkid_probe_set_utf8_id_label(pr, "VOLUME_SET_ID", buf, len, UL_ENCODE_UTF16BE);
	else if (joliet)
		blkid_probe_set_utf8_id_label(pr, "VOLUME_SET_ID", joliet->iso.volume_set_id, sizeof(joliet->iso.volume_set_id), UL_ENCODE_UTF16BE);
	else if (is_hs)
		blkid_probe_set_id_label(pr, "VOLUME_SET_ID", pvd->hs.volume_set_id, sizeof(pvd->hs.volume_set_id));
	else
		blkid_probe_set_id_label(pr, "VOLUME_SET_ID", pvd->iso.volume_set_id, sizeof(pvd->iso.volume_set_id));

	is_ascii_hs_empty = (!is_hs || is_str_empty(pvd->hs.publisher_id, sizeof(pvd->hs.publisher_id)) || pvd->hs.publisher_id[0] == '_');
	is_ascii_iso_empty = (is_hs || is_str_empty(pvd->iso.publisher_id, sizeof(pvd->iso.publisher_id)) || pvd->iso.publisher_id[0] == '_');
	is_unicode_empty = (!joliet || is_utf16be_str_empty(joliet->iso.publisher_id, sizeof(joliet->iso.publisher_id)) || (joliet->iso.publisher_id[0] == 0x00 && joliet->iso.publisher_id[1] == '_'));
	if (!is_unicode_empty && !is_ascii_iso_empty && (len = merge_utf16be_ascii(buf, sizeof(buf), joliet->iso.publisher_id, pvd->iso.publisher_id, sizeof(pvd->iso.publisher_id))) != 0)
		blkid_probe_set_utf8_id_label(pr, "PUBLISHER_ID", buf, len, UL_ENCODE_UTF16BE);
	else if (!is_unicode_empty)
		blkid_probe_set_utf8_id_label(pr, "PUBLISHER_ID", joliet->iso.publisher_id, sizeof(joliet->iso.publisher_id), UL_ENCODE_UTF16BE);
	else if (!is_ascii_hs_empty)
		blkid_probe_set_id_label(pr, "PUBLISHER_ID", pvd->hs.publisher_id, sizeof(pvd->hs.publisher_id));
	else if (!is_ascii_iso_empty)
		blkid_probe_set_id_label(pr, "PUBLISHER_ID", pvd->iso.publisher_id, sizeof(pvd->iso.publisher_id));

	is_ascii_hs_empty = (!is_hs || is_str_empty(pvd->hs.data_preparer_id, sizeof(pvd->hs.data_preparer_id)) || pvd->hs.data_preparer_id[0] == '_');
	is_ascii_iso_empty = (is_hs || is_str_empty(pvd->iso.data_preparer_id, sizeof(pvd->iso.data_preparer_id)) || pvd->iso.data_preparer_id[0] == '_');
	is_unicode_empty = (!joliet || is_utf16be_str_empty(joliet->iso.data_preparer_id, sizeof(joliet->iso.data_preparer_id)) || (joliet->iso.data_preparer_id[0] == 0x00 && joliet->iso.data_preparer_id[1] == '_'));
	if (!is_unicode_empty && !is_ascii_iso_empty && (len = merge_utf16be_ascii(buf, sizeof(buf), joliet->iso.data_preparer_id, pvd->iso.data_preparer_id, sizeof(pvd->iso.data_preparer_id))) != 0)
		blkid_probe_set_utf8_id_label(pr, "DATA_PREPARER_ID", buf, len, UL_ENCODE_UTF16BE);
	else if (!is_unicode_empty)
		blkid_probe_set_utf8_id_label(pr, "DATA_PREPARER_ID", joliet->iso.data_preparer_id, sizeof(joliet->iso.data_preparer_id), UL_ENCODE_UTF16BE);
	else if (!is_ascii_hs_empty)
		blkid_probe_set_id_label(pr, "DATA_PREPARER_ID", pvd->hs.data_preparer_id, sizeof(pvd->hs.data_preparer_id));
	else if (!is_ascii_iso_empty)
		blkid_probe_set_id_label(pr, "DATA_PREPARER_ID", pvd->iso.data_preparer_id, sizeof(pvd->iso.data_preparer_id));

	is_ascii_hs_empty = (!is_hs || is_str_empty(pvd->hs.application_id, sizeof(pvd->hs.application_id)) || pvd->hs.application_id[0] == '_');
	is_ascii_iso_empty = (is_hs || is_str_empty(pvd->iso.application_id, sizeof(pvd->iso.application_id)) || pvd->iso.application_id[0] == '_');
	is_unicode_empty = (!joliet || is_utf16be_str_empty(joliet->iso.application_id, sizeof(joliet->iso.application_id)) || (joliet->iso.application_id[0] == 0x00 && joliet->iso.application_id[1] == '_'));
	if (!is_unicode_empty && !is_ascii_iso_empty && (len = merge_utf16be_ascii(buf, sizeof(buf), joliet->iso.application_id, pvd->iso.application_id, sizeof(pvd->iso.application_id))) != 0)
		blkid_probe_set_utf8_id_label(pr, "APPLICATION_ID", buf, len, UL_ENCODE_UTF16BE);
	else if (!is_unicode_empty)
		blkid_probe_set_utf8_id_label(pr, "APPLICATION_ID", joliet->iso.application_id, sizeof(joliet->iso.application_id), UL_ENCODE_UTF16BE);
	else if (!is_ascii_hs_empty)
		blkid_probe_set_id_label(pr, "APPLICATION_ID", pvd->hs.application_id, sizeof(pvd->hs.application_id));
	else if (!is_ascii_iso_empty)
		blkid_probe_set_id_label(pr, "APPLICATION_ID", pvd->iso.application_id, sizeof(pvd->iso.application_id));

	if (is_hs) {
		modified = &pvd->hs.modified;
		created = &pvd->hs.created;
		modified_offset = 0;
		created_offset = 0;
	} else {
		modified = &pvd->iso.modified.common;
		created = &pvd->iso.created.common;
		modified_offset = pvd->iso.modified.offset;
		created_offset = pvd->iso.created.offset;
	}

	/* create an UUID using the modified/created date */
	if (! probe_iso9660_set_uuid(pr, modified, modified_offset))
		probe_iso9660_set_uuid(pr, created, created_offset);

	if (boot)
		blkid_probe_set_id_label(pr, "BOOT_SYSTEM_ID",
					boot->boot_system_id,
					sizeof(boot->boot_system_id));

	if (joliet)
		blkid_probe_set_version(pr, "Joliet Extension");
	else if (is_hs)
		blkid_probe_set_version(pr, "High Sierra");

	/* Label in Joliet is UNICODE (UTF16BE) but can contain only 16 characters. Label in PVD is
	 * subset of ASCII but can contain up to the 32 characters. Non-representable characters are
	 * stored as replacement character '_'. Label in Joliet is in most cases trimmed but UNICODE
	 * version of label in PVD. Based on these facts try to reconstruct original label if label
	 * in Joliet is prefix of the label in PVD (ignoring non-representable characters).
	 */
	if (joliet && (len = merge_utf16be_ascii(buf, sizeof(buf), joliet->volume_id, pvd->volume_id, sizeof(pvd->volume_id))) != 0)
		blkid_probe_set_utf8label(pr, buf, len, UL_ENCODE_UTF16BE);
	else if (joliet)
		blkid_probe_set_utf8label(pr, joliet->volume_id, sizeof(joliet->volume_id), UL_ENCODE_UTF16BE);
	else
		blkid_probe_set_label(pr, pvd->volume_id, sizeof(pvd->volume_id));

	return 0;
}

const struct blkid_idinfo iso9660_idinfo =
{
	.name		= "iso9660",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_iso9660,
	.flags		= BLKID_IDINFO_TOLERANT,
	.magics		=
	{
		/*
		 * Due to different location of vd_id[] in ISO9660 and High Sierra, IS9660 can match
		 * also High Sierra vd_id[]. So always check ISO9660 (CD001) before High Sierra (CDROM).
		 */
		{ .magic = "CD001", .len = 5, .kboff = 32, .sboff = 1, .hoff = "session_offset" },
		{ .magic = "CDROM", .len = 5, .kboff = 32, .sboff = 9, .hoff = "session_offset" },
		{ NULL }
	}
};

