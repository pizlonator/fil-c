/*
 * Copyright (C) 2007 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2012 Davidlohr Bueso <dave@gnu.org>
 *
 * GUID Partition Table (GPT) support. Based on UEFI Specs 2.3.1
 * Chapter 5: GUID Partition Table (GPT) Disk Layout (Jun 27th, 2012).
 * Some ideas and inspiration from GNU parted and gptfdisk.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <uuid.h>

#include "fdiskP.h"

#include "crc32.h"
#include "blkdev.h"
#include "bitops.h"
#include "strutils.h"
#include "all-io.h"
#include "pt-mbr.h"
#include "encode.h"

/**
 * SECTION: gpt
 * @title: UEFI GPT
 * @short_description: specific functionality
 */

#define GPT_HEADER_SIGNATURE 0x5452415020494645LL /* EFI PART */
#define GPT_HEADER_REVISION_V1_02 0x00010200
#define GPT_HEADER_REVISION_V1_00 0x00010000
#define GPT_HEADER_REVISION_V0_99 0x00009900
#define GPT_HEADER_MINSZ          92 /* bytes */

#define GPT_PMBR_LBA        0
#define GPT_MBR_PROTECTIVE  1
#define GPT_MBR_HYBRID      2

#define GPT_PRIMARY_PARTITION_TABLE_LBA 0x00000001ULL

#define EFI_PMBR_OSTYPE     0xEE
#define MSDOS_MBR_SIGNATURE 0xAA55
#define GPT_PART_NAME_LEN   (72 / sizeof(uint16_t))
#define GPT_NPARTITIONS     ((size_t) FDISK_GPT_NPARTITIONS_DEFAULT)

/* Globally unique identifier */
struct gpt_guid {
	uint32_t   time_low;
	uint16_t   time_mid;
	uint16_t   time_hi_and_version;
	uint8_t    clock_seq_hi;
	uint8_t    clock_seq_low;
	uint8_t    node[6];
};


/* only checking that the GUID is 0 is enough to verify an empty partition. */
#define GPT_UNUSED_ENTRY_GUID						\
	((struct gpt_guid) { 0x00000000, 0x0000, 0x0000, 0x00, 0x00,	\
			     { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }})

/* Linux native partition type */
#define GPT_DEFAULT_ENTRY_TYPE "0FC63DAF-8483-4772-8E79-3D69D8477DE4"

/*
 * Attribute bits
 */
enum {
	/* UEFI specific */
	GPT_ATTRBIT_REQ      = 0,
	GPT_ATTRBIT_NOBLOCK  = 1,
	GPT_ATTRBIT_LEGACY   = 2,

	/* GUID specific (range 48..64)*/
	GPT_ATTRBIT_GUID_FIRST	= 48,
	GPT_ATTRBIT_GUID_COUNT	= 16
};

#define GPT_ATTRSTR_REQ		"RequiredPartition"
#define GPT_ATTRSTR_REQ_TYPO	"RequiredPartiton"
#define GPT_ATTRSTR_NOBLOCK	"NoBlockIOProtocol"
#define GPT_ATTRSTR_LEGACY	"LegacyBIOSBootable"

/* The GPT Partition entry array contains an array of GPT entries. */
struct gpt_entry {
	struct gpt_guid     type; /* purpose and type of the partition */
	struct gpt_guid     partition_guid;
	uint64_t            lba_start;
	uint64_t            lba_end;
	uint64_t            attrs;
	uint16_t            name[GPT_PART_NAME_LEN];
}  __attribute__ ((packed));

/* GPT header */
struct gpt_header {
	uint64_t            signature; /* header identification */
	uint32_t            revision; /* header version */
	uint32_t            size; /* in bytes */
	uint32_t            crc32; /* header CRC checksum */
	uint32_t            reserved1; /* must be 0 */
	uint64_t            my_lba; /* LBA of block that contains this struct (LBA 1) */
	uint64_t            alternative_lba; /* backup GPT header */
	uint64_t            first_usable_lba; /* first usable logical block for partitions */
	uint64_t            last_usable_lba; /* last usable logical block for partitions */
	struct gpt_guid     disk_guid; /* unique disk identifier */
	uint64_t            partition_entry_lba; /* LBA of start of partition entries array */
	uint32_t            npartition_entries; /* total partition entries - normally 128 */
	uint32_t            sizeof_partition_entry; /* bytes for each GUID pt */
	uint32_t            partition_entry_array_crc32; /* partition CRC checksum */
	uint8_t             reserved2[512 - 92]; /* must all be 0 */
} __attribute__ ((packed));

struct gpt_record {
	uint8_t             boot_indicator; /* unused by EFI, set to 0x80 for bootable */
	uint8_t             start_head; /* unused by EFI, pt start in CHS */
	uint8_t             start_sector; /* unused by EFI, pt start in CHS */
	uint8_t             start_track;
	uint8_t             os_type; /* EFI and legacy non-EFI OS types */
	uint8_t             end_head; /* unused by EFI, pt end in CHS */
	uint8_t             end_sector; /* unused by EFI, pt end in CHS */
	uint8_t             end_track; /* unused by EFI, pt end in CHS */
	uint32_t            starting_lba; /* used by EFI - start addr of the on disk pt */
	uint32_t            size_in_lba; /* used by EFI - size of pt in LBA */
} __attribute__ ((packed));

/* Protected MBR and legacy MBR share same structure */
struct gpt_legacy_mbr {
	uint8_t             boot_code[440];
	uint32_t            unique_mbr_signature;
	uint16_t            unknown;
	struct gpt_record   partition_record[4];
	uint16_t            signature;
} __attribute__ ((packed));

/*
 * Here be dragons!
 * See: http://en.wikipedia.org/wiki/GUID_Partition_Table#Partition_type_GUIDs
 */
#define DEF_GUID(_u, _n) \
	{ \
		.typestr = (_u), \
		.name = (_n),    \
	}

static const struct fdisk_parttype gpt_parttypes[] =
{
	#include "pt-gpt-partnames.h"
};

static const struct fdisk_shortcut gpt_parttype_cuts[] =
{
	{ .shortcut = "L", .alias = "linux",    .data = "0FC63DAF-8483-4772-8E79-3D69D8477DE4" }, /* Linux */
	{ .shortcut = "S", .alias = "swap",     .data = "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F" }, /* Swap */
	{ .shortcut = "H", .alias = "home",     .data = "933AC7E1-2EB4-4F13-B844-0E14E2AEF915" }, /* Home */
	{ .shortcut = "U", .alias = "uefi",     .data = "C12A7328-F81F-11D2-BA4B-00A0C93EC93B" }, /* UEFI system */
	{ .shortcut = "R", .alias = "raid",     .data = "A19D880F-05FC-4D3B-A006-743F0F84911E" }, /* Linux RAID */
	{ .shortcut = "V", .alias = "lvm",      .data = "E6D6D379-F507-44C2-A23C-238F2A3DF928" }, /* LVM */
	{ .shortcut = "X", .alias = "xbootldr", .data = "BC13C2FF-59E6-4262-A352-B275FD6F7172" }, /* Linux extended boot */
};

#define alignment_required(_x)  ((_x)->grain != (_x)->sector_size)

/* gpt_entry macros */
#define gpt_partition_start(_e)		le64_to_cpu((_e)->lba_start)
#define gpt_partition_end(_e)		le64_to_cpu((_e)->lba_end)

/*
 * in-memory fdisk GPT stuff
 */
struct fdisk_gpt_label {
	struct fdisk_label	head;		/* generic part */

	/* gpt specific part */
	struct gpt_header	*pheader;	/* primary header */
	struct gpt_header	*bheader;	/* backup header */

	unsigned char *ents;			/* entries (partitions) */

	unsigned int no_relocate :1,		/* do not fix backup location */
		     minimize :1;
};

static void gpt_deinit(struct fdisk_label *lb);

static inline struct fdisk_gpt_label *self_label(struct fdisk_context *cxt)
{
	return (struct fdisk_gpt_label *) cxt->label;
}

/*
 * Returns the partition length, or 0 if end is before beginning.
 */
static uint64_t gpt_partition_size(const struct gpt_entry *e)
{
	uint64_t start = gpt_partition_start(e);
	uint64_t end = gpt_partition_end(e);

	return start > end ? 0 : end - start + 1ULL;
}

/* prints UUID in the real byte order! */
static void gpt_debug_uuid(const char *mesg, struct gpt_guid *guid)
{
	const unsigned char *uuid = (unsigned char *) guid;

	fprintf(stderr, "%s: "
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
		mesg,
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5],
		uuid[6], uuid[7],
		uuid[8], uuid[9],
		uuid[10], uuid[11], uuid[12], uuid[13], uuid[14],uuid[15]);
}

/*
 * UUID is traditionally 16 byte big-endian array, except Intel EFI
 * specification where the UUID is a structure of little-endian fields.
 */
static void swap_efi_guid(struct gpt_guid *uid)
{
	uid->time_low = swab32(uid->time_low);
	uid->time_mid = swab16(uid->time_mid);
	uid->time_hi_and_version = swab16(uid->time_hi_and_version);
}

static int string_to_guid(const char *in, struct gpt_guid *guid)
{
	if (uuid_parse(in, (unsigned char *) guid)) {   /* BE */
		DBG(GPT, ul_debug("failed to parse GUID: %s", in));
		return -EINVAL;
	}
	swap_efi_guid(guid);				/* LE */
	return 0;
}

static char *guid_to_string(const struct gpt_guid *guid, char *out)
{
	struct gpt_guid u = *guid;	/* LE */

	swap_efi_guid(&u);		/* BE */
	uuid_unparse_upper((unsigned char *) &u, out);

	return out;
}

static struct fdisk_parttype *gpt_partition_parttype(
		struct fdisk_context *cxt,
		const struct gpt_entry *e)
{
	struct fdisk_parttype *t;
	char str[UUID_STR_LEN];
	struct gpt_guid guid = e->type;

	guid_to_string(&guid, str);
	t = fdisk_label_get_parttype_from_string(cxt->label, str);
	return t ? : fdisk_new_unknown_parttype(0, str);
}

static void gpt_entry_set_type(struct gpt_entry *e, struct gpt_guid *uuid)
{
	e->type = *uuid;
	DBG(GPT, gpt_debug_uuid("new type", uuid));
}

static int gpt_entry_set_name(struct gpt_entry *e, char *str)
{
	uint16_t name[GPT_PART_NAME_LEN] = { 0 };
	size_t i, mblen = 0;
	uint8_t *in = (uint8_t *) str;

	for (i = 0; *in && i < GPT_PART_NAME_LEN; in++) {
		if (!mblen) {
			if (!(*in & 0x80)) {
				name[i++] = *in;
			} else if ((*in & 0xE0) == 0xC0) {
				mblen = 1;
				name[i] = (uint16_t)(*in & 0x1F) << (mblen *6);
			} else if ((*in & 0xF0) == 0xE0) {
				mblen = 2;
				name[i] = (uint16_t)(*in & 0x0F) << (mblen *6);
			} else {
				/* broken UTF-8 or code point greater than U+FFFF */
				return -EILSEQ;
			}
		} else {
			/* incomplete UTF-8 sequence */
			if ((*in & 0xC0) != 0x80)
				return -EILSEQ;

			name[i] |= (uint16_t)(*in & 0x3F) << (--mblen *6);
			if (!mblen) {
				/* check for code points reserved for surrogate pairs*/
				if ((name[i] & 0xF800) == 0xD800)
					return -EILSEQ;
				i++;
			}
		}
	}

	for (i = 0; i < GPT_PART_NAME_LEN; i++)
		e->name[i] = cpu_to_le16(name[i]);

	return (int)((char *) in - str);
}

static int gpt_entry_set_uuid(struct gpt_entry *e, char *str)
{
	struct gpt_guid uuid;
	int rc;

	rc = string_to_guid(str, &uuid);
	if (rc)
		return rc;

	e->partition_guid = uuid;
	return 0;
}

static inline int gpt_entry_is_used(const struct gpt_entry *e)
{
	return memcmp(&e->type, &GPT_UNUSED_ENTRY_GUID,
			sizeof(struct gpt_guid)) != 0;
}


static const char *gpt_get_header_revstr(struct gpt_header *header)
{
	if (!header)
		goto unknown;

	switch (le32_to_cpu(header->revision)) {
	case GPT_HEADER_REVISION_V1_02:
		return "1.2";
	case GPT_HEADER_REVISION_V1_00:
		return "1.0";
	case GPT_HEADER_REVISION_V0_99:
		return "0.99";
	default:
		goto unknown;
	}

unknown:
	return "unknown";
}

static inline unsigned char *gpt_get_entry_ptr(struct fdisk_gpt_label *gpt, size_t i)
{
	return gpt->ents + le32_to_cpu(gpt->pheader->sizeof_partition_entry) * i;
}

static inline struct gpt_entry *gpt_get_entry(struct fdisk_gpt_label *gpt, size_t i)
{
	return (struct gpt_entry *) gpt_get_entry_ptr(gpt, i);
}

static inline struct gpt_entry *gpt_zeroize_entry(struct fdisk_gpt_label *gpt, size_t i)
{
	return (struct gpt_entry *) memset(gpt_get_entry_ptr(gpt, i),
			0, le32_to_cpu(gpt->pheader->sizeof_partition_entry));
}

/* Use to access array of entries, for() loops, etc. But don't use when
 * you directly do something with GPT header, then use uint32_t.
 */
static inline size_t gpt_get_nentries(struct fdisk_gpt_label *gpt)
{
	return (size_t) le32_to_cpu(gpt->pheader->npartition_entries);
}

/* calculate size of entries array in bytes for specified number of entries */
static inline int gpt_calculate_sizeof_entries(
				struct gpt_header *hdr,
				uint32_t nents,	size_t *sz)
{
	uint32_t esz = hdr ? le32_to_cpu(hdr->sizeof_partition_entry) :
			     sizeof(struct gpt_entry);

	if (nents == 0 || esz == 0 || SIZE_MAX/esz < nents) {
		DBG(GPT, ul_debug("entries array size check failed"));
		return -ERANGE;
	}

	*sz = (size_t) nents * esz;
	return 0;
}

/* calculate size of entries array in sectors for specified number of entries */
static inline int gpt_calculate_sectorsof_entries(
				struct gpt_header *hdr,
				uint32_t nents, uint64_t *sz,
				struct fdisk_context *cxt)
{
	size_t esz = 0;
	int rc = gpt_calculate_sizeof_entries(hdr, nents, &esz);	/* in bytes */

	if (rc == 0)
		*sz = (esz + cxt->sector_size - 1) / cxt->sector_size;
	return rc;
}

/* calculate alternative (backup) entries array offset from primary header */
static inline int gpt_calculate_alternative_entries_lba(
				struct gpt_header *hdr,
				uint32_t nents,
				uint64_t *sz,
				struct fdisk_context *cxt)
{
	uint64_t esects = 0;
	int rc = gpt_calculate_sectorsof_entries(hdr, nents, &esects, cxt);

	if (rc)
		return rc;
	if (cxt->total_sectors < 1ULL + esects)
		return -ENOSPC;

	*sz = cxt->total_sectors - 1ULL - esects;
	return 0;
}

static inline int gpt_calculate_last_lba(
				struct gpt_header *hdr,
				uint32_t nents,
				uint64_t *sz,
				struct fdisk_context *cxt)
{
	uint64_t esects = 0;
	int rc = gpt_calculate_sectorsof_entries(hdr, nents, &esects, cxt);

	if (rc)
		return rc;
	if (cxt->total_sectors < 2ULL + esects)
		return -ENOSPC;

	*sz = cxt->total_sectors - 2ULL - esects;
	return 0;
}

static inline int gpt_calculate_first_lba(
				struct gpt_header *hdr,
				uint32_t nents,
				uint64_t *sz,
				struct fdisk_context *cxt)
{
	uint64_t esects = 0;
	int rc = gpt_calculate_sectorsof_entries(hdr, nents, &esects, cxt);

	if (rc == 0)
		*sz = esects + 2ULL;
	return rc;
}

/* the current size of entries array in bytes */
static inline int gpt_sizeof_entries(struct gpt_header *hdr, size_t *sz)
{
	return gpt_calculate_sizeof_entries(hdr, le32_to_cpu(hdr->npartition_entries), sz);
}

static char *gpt_get_header_id(struct gpt_header *header)
{
	char str[UUID_STR_LEN];
	struct gpt_guid guid = header->disk_guid;

	guid_to_string(&guid, str);

	return strdup(str);
}

/*
 * Builds a clean new valid protective MBR - will wipe out any existing data.
 * Returns 0 on success, otherwise < 0 on error.
 */
static int gpt_mknew_pmbr(struct fdisk_context *cxt)
{
	struct gpt_legacy_mbr *pmbr = NULL;
	int rc;

	if (!cxt || !cxt->firstsector)
		return -ENOSYS;

	if (fdisk_has_protected_bootbits(cxt))
		rc = fdisk_init_firstsector_buffer(cxt, 0, MBR_PT_BOOTBITS_SIZE);
	else
		rc = fdisk_init_firstsector_buffer(cxt, 0, 0);
	if (rc)
		return rc;

	pmbr = (struct gpt_legacy_mbr *) cxt->firstsector;
	memset(pmbr->partition_record, 0, sizeof(pmbr->partition_record));

	pmbr->signature = cpu_to_le16(MSDOS_MBR_SIGNATURE);
	pmbr->partition_record[0].os_type      = EFI_PMBR_OSTYPE;
	pmbr->partition_record[0].start_sector = 2;
	pmbr->partition_record[0].end_head     = 0xFF;
	pmbr->partition_record[0].end_sector   = 0xFF;
	pmbr->partition_record[0].end_track    = 0xFF;
	pmbr->partition_record[0].starting_lba = cpu_to_le32(1);
	pmbr->partition_record[0].size_in_lba  =
		cpu_to_le32((uint32_t) min( cxt->total_sectors - 1ULL, 0xFFFFFFFFULL) );

	return 0;
}


/* Move backup header to the end of the device */
static int gpt_fix_alternative_lba(struct fdisk_context *cxt, struct fdisk_gpt_label *gpt)
{
	struct gpt_header *p, *b;
	uint64_t x = 0, orig;
	size_t nents;
	int rc;

	if (!cxt)
		return -EINVAL;

	p = gpt->pheader;	/* primary */
	b = gpt->bheader;	/* backup */

	nents = le32_to_cpu(p->npartition_entries);
	orig = le64_to_cpu(p->alternative_lba);

	/* reference from primary to backup */
	p->alternative_lba = cpu_to_le64(cxt->total_sectors - 1ULL);

	/* reference from backup to primary */
	b->alternative_lba = p->my_lba;
	b->my_lba = p->alternative_lba;

	/* fix backup partitions array address */
	rc = gpt_calculate_alternative_entries_lba(p, nents, &x, cxt);
	if (rc)
		goto failed;

	b->partition_entry_lba = cpu_to_le64(x);

	/* update last usable LBA */
	rc = gpt_calculate_last_lba(p, nents, &x, cxt);
	if (rc)
		goto failed;

	p->last_usable_lba  = cpu_to_le64(x);
	b->last_usable_lba  = cpu_to_le64(x);

	DBG(GPT, ul_debug("Alternative-LBA updated from %"PRIu64" to %"PRIu64,
				orig, le64_to_cpu(p->alternative_lba)));
	return 0;
failed:
	DBG(GPT, ul_debug("failed to fix alternative-LBA [rc=%d]", rc));
	return rc;
}

static uint64_t gpt_calculate_minimal_size(struct fdisk_context *cxt, struct fdisk_gpt_label *gpt)
{
	size_t i;
	uint64_t x = 0, total = 0;
	struct gpt_header *hdr;

	assert(cxt);
	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	hdr = gpt->pheader;

	/* LBA behind the last partition */
	for (i = 0; i < gpt_get_nentries(gpt); i++) {
		struct gpt_entry *e = gpt_get_entry(gpt, i);

		if (gpt_entry_is_used(e)) {
			uint64_t end = gpt_partition_end(e);
			if (end > x)
				x = end;
		}
	}
	total = x + 1;

	/* the current last LBA usable for partitions */
	gpt_calculate_last_lba(hdr, le32_to_cpu(hdr->npartition_entries), &x, cxt);

	/* size of all stuff at the end of the device */
	total += cxt->total_sectors - x;

	DBG(GPT, ul_debug("minimal device is %"PRIu64, total));
	return total;
}

static int gpt_possible_minimize(struct fdisk_context *cxt, struct fdisk_gpt_label *gpt)
{
	struct gpt_header *hdr = gpt->pheader;
	uint64_t total = gpt_calculate_minimal_size(cxt, gpt);

	return le64_to_cpu(hdr->alternative_lba) > (total - 1ULL);
}

/* move backup header behind the last partition */
static int gpt_minimize_alternative_lba(struct fdisk_context *cxt, struct fdisk_gpt_label *gpt)
{
	uint64_t total = gpt_calculate_minimal_size(cxt, gpt);
	uint64_t orig = cxt->total_sectors;
	int rc;

	/* Let's temporary change size of the device to recalculate backup header */
	cxt->total_sectors = total;
	rc = gpt_fix_alternative_lba(cxt, gpt);
	if (rc)
		return rc;

	cxt->total_sectors = orig;
	fdisk_label_set_changed(cxt->label, 1);
	return 0;
}

/* some universal differences between the headers */
static void gpt_mknew_header_common(struct fdisk_context *cxt,
				    struct gpt_header *header, uint64_t lba)
{
	if (!cxt || !header)
		return;

	header->my_lba = cpu_to_le64(lba);

	if (lba == GPT_PRIMARY_PARTITION_TABLE_LBA) {
		/* primary */
		header->alternative_lba = cpu_to_le64(cxt->total_sectors - 1ULL);
		header->partition_entry_lba = cpu_to_le64(2ULL);

	} else {
		/* backup */
		uint64_t x = 0;
		gpt_calculate_alternative_entries_lba(header,
				le32_to_cpu(header->npartition_entries), &x, cxt);

		header->alternative_lba = cpu_to_le64(GPT_PRIMARY_PARTITION_TABLE_LBA);
		header->partition_entry_lba = cpu_to_le64(x);
	}
}

/*
 * Builds a new GPT header (at sector lba) from a backup header2.
 * If building a primary header, then backup is the secondary, and vice versa.
 *
 * Always pass a new (zeroized) header to build upon as we don't
 * explicitly zero-set some values such as CRCs and reserved.
 *
 * Returns 0 on success, otherwise < 0 on error.
 */
static int gpt_mknew_header_from_bkp(struct fdisk_context *cxt,
				     struct gpt_header *header,
				     uint64_t lba,
				     struct gpt_header *header2)
{
	if (!cxt || !header || !header2)
		return -ENOSYS;

	header->signature              = header2->signature;
	header->revision               = header2->revision;
	header->size                   = header2->size;
	header->npartition_entries     = header2->npartition_entries;
	header->sizeof_partition_entry = header2->sizeof_partition_entry;
	header->first_usable_lba       = header2->first_usable_lba;
	header->last_usable_lba        = header2->last_usable_lba;

	memcpy(&header->disk_guid,
	       &header2->disk_guid, sizeof(header2->disk_guid));
	gpt_mknew_header_common(cxt, header, lba);

	return 0;
}

static struct gpt_header *gpt_copy_header(struct fdisk_context *cxt,
			   struct gpt_header *src)
{
	struct gpt_header *res;

	if (!cxt || !src)
		return NULL;

	assert(cxt->sector_size >= sizeof(struct gpt_header));

	res = calloc(1, cxt->sector_size);
	if (!res) {
		fdisk_warn(cxt, _("failed to allocate GPT header"));
		return NULL;
	}

	res->my_lba                 = src->alternative_lba;
	res->alternative_lba        = src->my_lba;

	res->signature              = src->signature;
	res->revision               = src->revision;
	res->size                   = src->size;
	res->npartition_entries     = src->npartition_entries;
	res->sizeof_partition_entry = src->sizeof_partition_entry;
	res->first_usable_lba       = src->first_usable_lba;
	res->last_usable_lba        = src->last_usable_lba;

	memcpy(&res->disk_guid, &src->disk_guid, sizeof(src->disk_guid));


	if (res->my_lba == GPT_PRIMARY_PARTITION_TABLE_LBA)
		res->partition_entry_lba = cpu_to_le64(2ULL);
	else {
		uint64_t esz = (uint64_t) le32_to_cpu(src->npartition_entries) * sizeof(struct gpt_entry);
		uint64_t esects = (esz + cxt->sector_size - 1) / cxt->sector_size;

		res->partition_entry_lba = cpu_to_le64(cxt->total_sectors - 1ULL - esects);
	}

	return res;
}

static int get_script_u64(struct fdisk_context *cxt, uint64_t *num, const char *name)
{
	const char *str;
	int pwr = 0, rc = 0;

	assert(cxt);

	*num = 0;

	if (!cxt->script)
		return 1;

	str = fdisk_script_get_header(cxt->script, name);
	if (!str)
		return 1;

	rc = parse_size(str, (uintmax_t *) num, &pwr);
	if (rc < 0)
		return rc;
	if (pwr)
		*num /= cxt->sector_size;
	return 0;
}

static int count_first_last_lba(struct fdisk_context *cxt,
				 uint64_t *first, uint64_t *last,
				 uint32_t *maxents)
{
	int rc = 0;
	uint64_t flba = 0, llba = 0;
	uint64_t nents = GPT_NPARTITIONS;

	assert(cxt);
	assert(first);
	assert(last);

	*first = *last = 0;

	/* Get the table length from the script, if given */
	if (cxt->script) {
		rc = get_script_u64(cxt, &nents, "table-length");
		if (rc == 1)
			nents = GPT_NPARTITIONS;  /* undefined by script */
		else if (rc < 0)
			return rc;
	}

	/* The table length was not changed by the script, compute it. */
	if (flba == 0) {
		/* If the device is not large enough reduce this number of
		 * partitions and try to recalculate it again, until we get
		 * something useful or return error.
		 */
		for (; nents > 0; nents--) {
			rc = gpt_calculate_last_lba(NULL, nents, &llba, cxt);
			if (rc == 0)
				rc = gpt_calculate_first_lba(NULL, nents, &flba, cxt);
			if (llba < flba)
				rc = -ENOSPC;
			else if (rc == 0)
				break;
		}
	}

	if (rc)
		return rc;
	if (maxents)
		*maxents = nents;

	/* script default */
	if (cxt->script) {
		rc = get_script_u64(cxt, first, "first-lba");
		if (rc < 0)
			return rc;

		DBG(GPT, ul_debug("FirstLBA: script=%"PRIu64", uefi=%"PRIu64", topology=%ju.",
		                    *first, flba,  (uintmax_t)cxt->first_lba));

		if (rc == 0 && (*first < flba || *first > llba)) {
			fdisk_warnx(cxt, _("First LBA specified by script is out of range."));
			return -ERANGE;
		}

		rc = get_script_u64(cxt, last, "last-lba");
		if (rc < 0)
			return rc;

		DBG(GPT, ul_debug("LastLBA: script=%"PRIu64", uefi=%"PRIu64", topology=%ju.",
		                    *last, llba,  (uintmax_t)cxt->last_lba));

		if (rc == 0 && (*last > llba || *last < flba)) {
			fdisk_warnx(cxt, _("Last LBA specified by script is out of range."));
			return -ERANGE;
		}
	}

	if (!*last)
		*last = llba;

	/* default by topology */
	if (!*first)
		*first = flba < cxt->first_lba &&
			 cxt->first_lba < *last ? cxt->first_lba : flba;
	return 0;
}

/*
 * Builds a clean new GPT header (currently under revision 1.0).
 *
 * Always pass a new (zeroized) header to build upon as we don't
 * explicitly zero-set some values such as CRCs and reserved.
 *
 * Returns 0 on success, otherwise < 0 on error.
 */
static int gpt_mknew_header(struct fdisk_context *cxt,
			    struct gpt_header *header, uint64_t lba)
{
	uint64_t first, last;
	uint32_t nents = 0;
	int has_id = 0, rc;

	if (!cxt || !header)
		return -ENOSYS;

	header->signature = cpu_to_le64(GPT_HEADER_SIGNATURE);
	header->revision  = cpu_to_le32(GPT_HEADER_REVISION_V1_00);

	/* According to EFI standard it's valid to count all the first
	 * sector into header size, but some tools may have a problem
	 * to accept it, so use the header without the zeroed area.
	 * This does not have any impact to CRC, etc.   --kzak Jan-2015
	 */
	header->size = cpu_to_le32(sizeof(struct gpt_header)
				- sizeof(header->reserved2));

	/* Set {First,Last}LBA and number of the partitions
	 * (default is GPT_NPARTITIONS) */
	rc = count_first_last_lba(cxt, &first, &last, &nents);
	if (rc)
		return rc;

	header->npartition_entries     = cpu_to_le32(nents);
	header->sizeof_partition_entry = cpu_to_le32(sizeof(struct gpt_entry));

	header->first_usable_lba = cpu_to_le64(first);
	header->last_usable_lba  = cpu_to_le64(last);

	gpt_mknew_header_common(cxt, header, lba);

	if (cxt->script) {
		const char *id = fdisk_script_get_header(cxt->script, "label-id");
		struct gpt_guid guid = header->disk_guid;
		if (id && string_to_guid(id, &guid) == 0)
			has_id = 1;
		header->disk_guid = guid;
	}

	if (!has_id) {
		struct gpt_guid guid;

		uuid_generate_random((unsigned char *) &guid);
		swap_efi_guid(&guid);
		header->disk_guid = guid;
	}
	return 0;
}

/*
 * Checks if there is a valid protective MBR partition table.
 * Returns 0 if it is invalid or failure. Otherwise, return
 * GPT_MBR_PROTECTIVE or GPT_MBR_HYBRID, depending on the detection.
 */
static int valid_pmbr(struct fdisk_context *cxt)
{
	int i, part = 0, ret = 0; /* invalid by default */
	struct gpt_legacy_mbr *pmbr = NULL;

	if (!cxt->firstsector)
		goto done;

	pmbr = (struct gpt_legacy_mbr *) cxt->firstsector;

	if (le16_to_cpu(pmbr->signature) != MSDOS_MBR_SIGNATURE)
		goto done;

	/* seems like a valid MBR was found, check DOS primary partitions */
	for (i = 0; i < 4; i++) {
		if (pmbr->partition_record[i].os_type == EFI_PMBR_OSTYPE) {
			/*
			 * Ok, we at least know that there's a protective MBR,
			 * now check if there are other partition types for
			 * hybrid MBR.
			 */
			part = i;
			ret = GPT_MBR_PROTECTIVE;
			break;
		}
	}

	if (ret != GPT_MBR_PROTECTIVE)
		goto done;


	for (i = 0 ; i < 4; i++) {
		if ((pmbr->partition_record[i].os_type != EFI_PMBR_OSTYPE) &&
		    (pmbr->partition_record[i].os_type != 0x00)) {
			ret = GPT_MBR_HYBRID;
			goto done;
		}
	}

	/* LBA of the GPT partition header */
	if (pmbr->partition_record[part].starting_lba !=
	    cpu_to_le32(GPT_PRIMARY_PARTITION_TABLE_LBA))
		goto done;

	/*
	 * Protective MBRs take up the lesser of the whole disk
	 * or 2 TiB (32bit LBA), ignoring the rest of the disk.
	 * Some partitioning programs, nonetheless, choose to set
	 * the size to the maximum 32-bit limitation, disregarding
	 * the disk size.
	 *
	 * Hybrid MBRs do not necessarily comply with this.
	 *
	 * Consider a bad value here to be a warning to support dd-ing
	 * an image from a smaller disk to a bigger disk.
	 */
	if (ret == GPT_MBR_PROTECTIVE) {
		uint64_t sz_lba = (uint64_t) le32_to_cpu(pmbr->partition_record[part].size_in_lba);
		if (sz_lba != cxt->total_sectors - 1ULL && sz_lba != 0xFFFFFFFFULL) {

			fdisk_warnx(cxt, _("GPT PMBR size mismatch (%"PRIu64" != %"PRIu64") "
					   "will be corrected by write."),
					sz_lba, cxt->total_sectors - (uint64_t) 1);

			/* Note that gpt_write_pmbr() overwrites PMBR, but we want to keep it valid already
			 * in memory too to disable warnings when valid_pmbr() called next time */
			pmbr->partition_record[part].size_in_lba  =
				cpu_to_le32((uint32_t) min( cxt->total_sectors - 1ULL, 0xFFFFFFFFULL) );
			fdisk_label_set_changed(cxt->label, 1);
		}
	}
done:
	DBG(GPT, ul_debug("PMBR type: %s",
			ret == GPT_MBR_PROTECTIVE ? "protective" :
			ret == GPT_MBR_HYBRID     ? "hybrid"     : "???" ));
	return ret;
}

static uint64_t last_lba(struct fdisk_context *cxt)
{
	struct stat s;
	uint64_t sectors = 0;

	memset(&s, 0, sizeof(s));
	if (fstat(cxt->dev_fd, &s) == -1) {
		fdisk_warn(cxt, _("gpt: stat() failed"));
		return 0;
	}

	if (S_ISBLK(s.st_mode))
		sectors = cxt->total_sectors - 1ULL;
	else if (S_ISREG(s.st_mode))
		sectors = ((uint64_t) s.st_size /
			   (uint64_t) cxt->sector_size) - 1ULL;
	else
		fdisk_warnx(cxt, _("gpt: cannot handle files with mode %o"), s.st_mode);

	DBG(GPT, ul_debug("last LBA: %"PRIu64"", sectors));
	return sectors;
}

static ssize_t read_lba(struct fdisk_context *cxt, uint64_t lba,
			void *buffer, const size_t bytes)
{
	off_t offset = lba * cxt->sector_size;

	if (lseek(cxt->dev_fd, offset, SEEK_SET) == (off_t) -1)
		return -1;
	return (size_t)read(cxt->dev_fd, buffer, bytes) != bytes;
}


/* Returns the GPT entry array */
static unsigned char *gpt_read_entries(struct fdisk_context *cxt,
					 struct gpt_header *header)
{
	size_t sz = 0;
	ssize_t ssz;

	unsigned char *ret = NULL;
	off_t offset;

	assert(cxt);
	assert(header);

	if (gpt_sizeof_entries(header, &sz))
		return NULL;

	ret = calloc(1, sz);
	if (!ret)
		return NULL;

	offset = (off_t) le64_to_cpu(header->partition_entry_lba) *
		       cxt->sector_size;

	if (offset != lseek(cxt->dev_fd, offset, SEEK_SET))
		goto fail;

	ssz = read(cxt->dev_fd, ret, sz);
	if (ssz < 0 || (size_t) ssz != sz)
		goto fail;

	return ret;

fail:
	free(ret);
	return NULL;
}

static inline uint32_t count_crc32(const unsigned char *buf, size_t len,
				   size_t ex_off, size_t ex_len)
{
	return (ul_crc32_exclude_offset(~0L, buf, len, ex_off, ex_len) ^ ~0L);
}

static inline uint32_t gpt_header_count_crc32(struct gpt_header *header)
{
        return count_crc32((unsigned char *) header,		/* buffer */
			le32_to_cpu(header->size),		/* size of buffer */
			offsetof(struct gpt_header, crc32),	/* exclude */
			sizeof(header->crc32));			/* size of excluded area */
}

static inline uint32_t gpt_entryarr_count_crc32(struct gpt_header *header, unsigned char *ents)
{
	size_t arysz = 0;

	if (gpt_sizeof_entries(header, &arysz))
		return 0;

	return count_crc32(ents, arysz, 0, 0);
}


/*
 * Recompute header and partition array 32bit CRC checksums.
 * This function does not fail - if there's corruption, then it
 * will be reported when checksumming it again (ie: probing or verify).
 */
static void gpt_recompute_crc(struct gpt_header *header, unsigned char *ents)
{
	if (!header)
		return;

	header->partition_entry_array_crc32 =
			cpu_to_le32( gpt_entryarr_count_crc32(header, ents) );

	header->crc32 = cpu_to_le32( gpt_header_count_crc32(header) );
}

/*
 * Compute the 32bit CRC checksum of the partition table header.
 * Returns 1 if it is valid, otherwise 0.
 */
static int gpt_check_header_crc(struct gpt_header *header, unsigned char *ents)
{
	uint32_t orgcrc = le32_to_cpu(header->crc32),
		 crc = gpt_header_count_crc32(header);

	if (crc == orgcrc)
		return 1;

	/*
	 * If we have checksum mismatch it may be due to stale data, like a
	 * partition being added or deleted. Recompute the CRC again and make
	 * sure this is not the case.
	 */
	if (ents) {
		gpt_recompute_crc(header, ents);
		return gpt_header_count_crc32(header) == orgcrc;
	}

	return 0;
}

/*
 * It initializes the partition entry array.
 * Returns 1 if the checksum is valid, otherwise 0.
 */
static int gpt_check_entryarr_crc(struct gpt_header *header, unsigned char *ents)
{
	if (!header || !ents)
		return 0;

	return gpt_entryarr_count_crc32(header, ents) ==
			le32_to_cpu(header->partition_entry_array_crc32);
}

static int gpt_check_lba_sanity(struct fdisk_context *cxt, struct gpt_header *header)
{
	int ret = 0;
	uint64_t lu, fu, lastlba = last_lba(cxt);

	fu = le64_to_cpu(header->first_usable_lba);
	lu = le64_to_cpu(header->last_usable_lba);

	/* check if first and last usable LBA make sense */
	if (lu < fu) {
		DBG(GPT, ul_debug("error: header last LBA is before first LBA"));
		goto done;
	}

	/* check if first and last usable LBAs with the disk's last LBA */
	if (fu > lastlba || lu > lastlba) {
		DBG(GPT, ul_debug("error: header LBAs are after the disk's last LBA (%ju..%ju)",
					(uintmax_t) fu, (uintmax_t) lu));
		goto done;
	}

	/* the header has to be outside usable range */
	if (fu < GPT_PRIMARY_PARTITION_TABLE_LBA &&
	    GPT_PRIMARY_PARTITION_TABLE_LBA < lu) {
		DBG(GPT, ul_debug("error: header outside of usable range"));
		goto done;
	}

	ret = 1; /* sane */
done:
	return ret;
}

/* Check if there is a valid header signature */
static int gpt_check_signature(struct gpt_header *header)
{
	return header->signature == cpu_to_le64(GPT_HEADER_SIGNATURE);
}

/*
 * Return the specified GPT Header, or NULL upon failure/invalid.
 * Note that all tests must pass to ensure a valid header,
 * we do not rely on only testing the signature for a valid probe.
 */
static struct gpt_header *gpt_read_header(struct fdisk_context *cxt,
					  uint64_t lba,
					  unsigned char **_ents)
{
	struct gpt_header *header = NULL;
	unsigned char *ents = NULL;
	uint32_t hsz;

	if (!cxt)
		return NULL;

	/* always allocate all sector, the area after GPT header
	 * has to be fill by zeros */
	assert(cxt->sector_size >= sizeof(struct gpt_header));

	header = calloc(1, cxt->sector_size);
	if (!header)
		return NULL;

	/* read and verify header */
	if (read_lba(cxt, lba, header, cxt->sector_size) != 0)
		goto invalid;

	if (!gpt_check_signature(header))
		goto invalid;

	/* make sure header size is between 92 and sector size bytes */
	hsz = le32_to_cpu(header->size);
	if (hsz < GPT_HEADER_MINSZ || hsz > cxt->sector_size)
		goto invalid;

	if (!gpt_check_header_crc(header, NULL))
		goto invalid;

	/* read and verify entries */
	ents = gpt_read_entries(cxt, header);
	if (!ents)
		goto invalid;

	if (!gpt_check_entryarr_crc(header, ents))
		goto invalid;

	if (!gpt_check_lba_sanity(cxt, header))
		goto invalid;

	/* valid header must be at MyLBA */
	if (le64_to_cpu(header->my_lba) != lba)
		goto invalid;

	if (_ents)
		*_ents = ents;
	else
		free(ents);

	DBG(GPT, ul_debug("found valid header on LBA %"PRIu64"", lba));
	return header;
invalid:
	free(header);
	free(ents);

	DBG(GPT, ul_debug("read header on LBA %"PRIu64" failed", lba));
	return NULL;
}


static int gpt_locate_disklabel(struct fdisk_context *cxt, int n,
		const char **name, uint64_t *offset, size_t *size)
{
	struct fdisk_gpt_label *gpt;

	assert(cxt);

	*name = NULL;
	*offset = 0;
	*size = 0;

	switch (n) {
	case 0:
		*name = "PMBR";
		*offset = 0;
		*size = 512;
		break;
	case 1:
		*name = _("GPT Header");
		*offset = (uint64_t) GPT_PRIMARY_PARTITION_TABLE_LBA * cxt->sector_size;
		*size = sizeof(struct gpt_header);
		break;
	case 2:
		*name = _("GPT Entries");
		gpt = self_label(cxt);
		*offset = (uint64_t) le64_to_cpu(gpt->pheader->partition_entry_lba) *
				     cxt->sector_size;
		return gpt_sizeof_entries(gpt->pheader, size);
	case 3:
		*name = _("GPT Backup Entries");
		gpt = self_label(cxt);
		*offset = (uint64_t) le64_to_cpu(gpt->bheader->partition_entry_lba) *
				     cxt->sector_size;
		return gpt_sizeof_entries(gpt->bheader, size);
	case 4:
		*name = _("GPT Backup Header");
		gpt = self_label(cxt);
		*offset = (uint64_t) le64_to_cpu(gpt->pheader->alternative_lba) * cxt->sector_size;
		*size = sizeof(struct gpt_header);
		break;
	default:
		return 1;			/* no more chunks */
	}

	return 0;
}

static int gpt_get_disklabel_item(struct fdisk_context *cxt, struct fdisk_labelitem *item)
{
	struct gpt_header *h;
	int rc = 0;
	uint64_t x = 0;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	h = self_label(cxt)->pheader;

	switch (item->id) {
	case GPT_LABELITEM_ID:
		item->name = _("Disk identifier");
		item->type = 's';
		item->data.str = gpt_get_header_id(h);
		if (!item->data.str)
			rc = -ENOMEM;
		break;
	case GPT_LABELITEM_FIRSTLBA:
		item->name = _("First usable LBA");
		item->type = 'j';
		item->data.num64 = le64_to_cpu(h->first_usable_lba);
		break;
	case GPT_LABELITEM_LASTLBA:
		item->name = _("Last usable LBA");
		item->type = 'j';
		item->data.num64 = le64_to_cpu(h->last_usable_lba);
		break;
	case GPT_LABELITEM_ALTLBA:
		/* TRANSLATORS: The LBA (Logical Block Address) of the backup GPT header. */
		item->name = _("Alternative LBA");
		item->type = 'j';
		item->data.num64 = le64_to_cpu(h->alternative_lba);
		break;
	case GPT_LABELITEM_ENTRIESLBA:
		/* TRANSLATORS: The start of the array of partition entries. */
		item->name = _("Partition entries starting LBA");
		item->type = 'j';
		item->data.num64 = le64_to_cpu(h->partition_entry_lba);
		break;
	case GPT_LABELITEM_ENTRIESLASTLBA:
		/* TRANSLATORS: The end of the array of partition entries. */
		item->name = _("Partition entries ending LBA");
		item->type = 'j';
		gpt_calculate_sectorsof_entries(h,
				le32_to_cpu(h->npartition_entries), &x, cxt);
		item->data.num64 = le64_to_cpu(h->partition_entry_lba) + x - 1;
		break;
	case GPT_LABELITEM_ENTRIESALLOC:
		item->name = _("Allocated partition entries");
		item->type = 'j';
		item->data.num64 = le32_to_cpu(h->npartition_entries);
		break;
	default:
		if (item->id < __FDISK_NLABELITEMS)
			rc = 1;	/* unsupported generic item */
		else
			rc = 2;	/* out of range */
		break;
	}

	return rc;
}

/*
 * Returns the number of partitions that are in use.
 */
static size_t partitions_in_use(struct fdisk_gpt_label *gpt)
{
	size_t i, used = 0;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	for (i = 0; i < gpt_get_nentries(gpt); i++) {
		struct gpt_entry *e = gpt_get_entry(gpt, i);

		if (gpt_entry_is_used(e))
			used++;
	}
	return used;
}


/*
 * Check if a partition is too big for the disk (sectors).
 * Returns the faulting partition number, otherwise 0.
 */
static uint32_t check_too_big_partitions(struct fdisk_gpt_label *gpt, uint64_t sectors)
{
	size_t i;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	for (i = 0; i < gpt_get_nentries(gpt); i++) {
		struct gpt_entry *e = gpt_get_entry(gpt, i);

		if (!gpt_entry_is_used(e))
			continue;
		if (gpt_partition_end(e) >= sectors)
			return i + 1;
	}

	return 0;
}

/*
 * Check if a partition ends before it begins
 * Returns the faulting partition number, otherwise 0.
 */
static uint32_t check_start_after_end_partitions(struct fdisk_gpt_label *gpt)
{
	size_t i;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	for (i = 0; i < gpt_get_nentries(gpt); i++) {
		struct gpt_entry *e = gpt_get_entry(gpt, i);

		if (!gpt_entry_is_used(e))
			continue;
		if (gpt_partition_start(e) > gpt_partition_end(e))
			return i + 1;
	}

	return 0;
}

/*
 * Check if partition e1 overlaps with partition e2.
 */
static inline int partition_overlap(struct gpt_entry *e1, struct gpt_entry *e2)
{
	uint64_t start1 = gpt_partition_start(e1);
	uint64_t end1   = gpt_partition_end(e1);
	uint64_t start2 = gpt_partition_start(e2);
	uint64_t end2   = gpt_partition_end(e2);

	return (start1 && start2 && (start1 <= end2) != (end1 < start2));
}

/*
 * Find any partitions that overlap.
 */
static uint32_t check_overlap_partitions(struct fdisk_gpt_label *gpt)
{
	size_t i, j;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	for (i = 0; i < gpt_get_nentries(gpt); i++)
		for (j = 0; j < i; j++) {
			struct gpt_entry *ei = gpt_get_entry(gpt, i);
			struct gpt_entry *ej = gpt_get_entry(gpt, j);

			if (!gpt_entry_is_used(ei) || !gpt_entry_is_used(ej))
				continue;
			if (partition_overlap(ei, ej)) {
				DBG(GPT, ul_debug("partitions overlap detected [%zu vs. %zu]", i, j));
				return i + 1;
			}
		}

	return 0;
}

/*
 * Find the first available block after the starting point; returns 0 if
 * there are no available blocks left, or error. From gdisk.
 */
static uint64_t find_first_available(struct fdisk_gpt_label *gpt, uint64_t start)
{
	int first_moved = 0;
	uint64_t first;
	uint64_t fu, lu;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	fu = le64_to_cpu(gpt->pheader->first_usable_lba);
	lu = le64_to_cpu(gpt->pheader->last_usable_lba);

	/*
	 * Begin from the specified starting point or from the first usable
	 * LBA, whichever is greater...
	 */
	first = start < fu ? fu : start;

	/*
	 * Now search through all partitions; if first is within an
	 * existing partition, move it to the next sector after that
	 * partition and repeat. If first was moved, set firstMoved
	 * flag; repeat until firstMoved is not set, so as to catch
	 * cases where partitions are out of sequential order....
	 */
	do {
		size_t i;

		first_moved = 0;
		for (i = 0; i < gpt_get_nentries(gpt); i++) {
			struct gpt_entry *e = gpt_get_entry(gpt, i);

			if (!gpt_entry_is_used(e))
				continue;
			if (first < gpt_partition_start(e))
				continue;
			if (first <= gpt_partition_end(e)) {
				first = gpt_partition_end(e) + 1;
				first_moved = 1;
			}
		}
	} while (first_moved == 1);

	if (first > lu)
		first = 0;

	return first;
}


/* Returns last available sector in the free space pointed to by start. From gdisk. */
static uint64_t find_last_free(struct fdisk_gpt_label *gpt, uint64_t start)
{
	size_t i;
	uint64_t nearest_start;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	nearest_start = le64_to_cpu(gpt->pheader->last_usable_lba);

	for (i = 0; i < gpt_get_nentries(gpt); i++) {
		struct gpt_entry *e = gpt_get_entry(gpt, i);
		uint64_t ps = gpt_partition_start(e);

		if (nearest_start > ps && ps > start)
			nearest_start = ps - 1ULL;
	}

	return nearest_start;
}

/* Returns the last free sector on the disk. From gdisk. */
static uint64_t find_last_free_sector(struct fdisk_gpt_label *gpt)
{
	int last_moved;
	uint64_t last = 0;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	/* start by assuming the last usable LBA is available */
	last = le64_to_cpu(gpt->pheader->last_usable_lba);
	do {
		size_t i;

		last_moved = 0;
		for (i = 0; i < gpt_get_nentries(gpt); i++) {
			struct gpt_entry *e = gpt_get_entry(gpt, i);

			if (last >= gpt_partition_start(e) &&
			    last <= gpt_partition_end(e)) {
				last = gpt_partition_start(e) - 1ULL;
				last_moved = 1;
			}
		}
	} while (last_moved == 1);

	return last;
}

/*
 * Finds the first available sector in the largest block of unallocated
 * space on the disk. Returns 0 if there are no available blocks left.
 * From gdisk.
 */
static uint64_t find_first_in_largest(struct fdisk_gpt_label *gpt)
{
	uint64_t start = 0, first_sect, last_sect;
	uint64_t segment_size, selected_size = 0, selected_segment = 0;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	do {
		first_sect = find_first_available(gpt, start);
		if (first_sect != 0) {
			last_sect = find_last_free(gpt, first_sect);
			segment_size = last_sect - first_sect + 1ULL;

			if (segment_size > selected_size) {
				selected_size = segment_size;
				selected_segment = first_sect;
			}
			start = last_sect + 1ULL;
		}
	} while (first_sect != 0);

	return selected_segment;
}

/*
 * Find the total number of free sectors, the number of segments in which
 * they reside, and the size of the largest of those segments. From gdisk.
 */
static uint64_t get_free_sectors(struct fdisk_context *cxt,
				 struct fdisk_gpt_label *gpt,
				 uint32_t *nsegments,
				 uint64_t *largest_segment)
{
	uint32_t num = 0;
	uint64_t first_sect, last_sect;
	uint64_t largest_seg = 0, segment_sz;
	uint64_t totfound = 0, start = 0; /* starting point for each search */

	if (!cxt->total_sectors)
		goto done;

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	do {
		first_sect = find_first_available(gpt, start);
		if (first_sect) {
			last_sect = find_last_free(gpt, first_sect);
			segment_sz = last_sect - first_sect + 1;

			if (segment_sz > largest_seg)
				largest_seg = segment_sz;
			totfound += segment_sz;
			num++;
			start = last_sect + 1ULL;
		}
	} while (first_sect);

done:
	if (nsegments)
		*nsegments = num;
	if (largest_segment)
		*largest_segment = largest_seg;

	return totfound;
}

static int gpt_probe_label(struct fdisk_context *cxt)
{
	int mbr_type;
	struct fdisk_gpt_label *gpt;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);

	/* TODO: it would be nice to support scenario when GPT headers are OK,
	 *       but PMBR is corrupt */
	mbr_type = valid_pmbr(cxt);
	if (!mbr_type)
		goto failed;

	/* primary header */
	gpt->pheader = gpt_read_header(cxt, GPT_PRIMARY_PARTITION_TABLE_LBA,
				       &gpt->ents);

	if (gpt->pheader)
		/* primary OK, try backup from alternative LBA */
		gpt->bheader = gpt_read_header(cxt,
					le64_to_cpu(gpt->pheader->alternative_lba),
					NULL);
	else
		/* primary corrupted -- try last LBA */
		gpt->bheader = gpt_read_header(cxt, last_lba(cxt), &gpt->ents);

	if (!gpt->pheader && !gpt->bheader)
		goto failed;

	/* primary OK, backup corrupted -- recovery */
	if (gpt->pheader && !gpt->bheader) {
		fdisk_warnx(cxt, _("The backup GPT table is corrupt, but the "
				  "primary appears OK, so that will be used."));
		gpt->bheader = gpt_copy_header(cxt, gpt->pheader);
		if (!gpt->bheader)
			goto failed;
		gpt_recompute_crc(gpt->bheader, gpt->ents);
		fdisk_label_set_changed(cxt->label, 1);

	/* primary corrupted, backup OK -- recovery */
	} else if (!gpt->pheader && gpt->bheader) {
		fdisk_warnx(cxt, _("The primary GPT table is corrupt, but the "
				  "backup appears OK, so that will be used."));
		gpt->pheader = gpt_copy_header(cxt, gpt->bheader);
		if (!gpt->pheader)
			goto failed;
		gpt_recompute_crc(gpt->pheader, gpt->ents);
		fdisk_label_set_changed(cxt->label, 1);
	}

	/* The headers make be correct, but Backup do not have to be on the end
	 * of the device (due to device resize, etc.). Let's fix this issue. */
	if (gpt->minimize == 0 &&
	    (le64_to_cpu(gpt->pheader->alternative_lba) > cxt->total_sectors ||
	     le64_to_cpu(gpt->pheader->alternative_lba) < cxt->total_sectors - 1ULL)) {

		if (gpt->no_relocate || fdisk_is_readonly(cxt))
			fdisk_warnx(cxt, _("The backup GPT table is not on the end of the device."));

		else {
			fdisk_warnx(cxt, _("The backup GPT table is not on the end of the device. "
					   "This problem will be corrected by write."));

			if (gpt_fix_alternative_lba(cxt, gpt) != 0)
				fdisk_warnx(cxt, _("Failed to recalculate backup GPT table location"));
			gpt_recompute_crc(gpt->bheader, gpt->ents);
			gpt_recompute_crc(gpt->pheader, gpt->ents);
			fdisk_label_set_changed(cxt->label, 1);
		}
	}

	if (gpt->minimize && gpt_possible_minimize(cxt, gpt))
		fdisk_label_set_changed(cxt->label, 1);

	cxt->label->nparts_max = gpt_get_nentries(gpt);
	cxt->label->nparts_cur = partitions_in_use(gpt);
	return 1;
failed:
	DBG(GPT, ul_debug("probe failed"));
	gpt_deinit(cxt->label);
	return 0;
}

static char *encode_to_utf8(unsigned char *src, size_t count)
{
	unsigned char *dest;
	size_t len = (count * 3 / 2) + 1;

	dest = calloc(1, len);
	if (!dest)
		return NULL;

	ul_encode_to_utf8(UL_ENCODE_UTF16LE, dest, len, src, count);
	return (char *) dest;
}

static int gpt_entry_attrs_to_string(struct gpt_entry *e, char **res)
{
	unsigned int n, count = 0;
	size_t l;
	char *bits, *p;
	uint64_t attrs;

	assert(e);
	assert(res);

	*res = NULL;
	attrs = e->attrs;
	if (!attrs)
		return 0;	/* no attributes at all */

	bits = (char *) &attrs;

	/* Note that sizeof() is correct here, we need separators between
	 * the strings so also count \0 is correct */
	*res = calloc(1, sizeof(GPT_ATTRSTR_NOBLOCK) +
			 sizeof(GPT_ATTRSTR_REQ) +
			 sizeof(GPT_ATTRSTR_LEGACY) +
			 sizeof("GUID:") + (GPT_ATTRBIT_GUID_COUNT * 3));
	if (!*res)
		return -errno;

	p = *res;
	if (isset(bits, GPT_ATTRBIT_REQ)) {
		memcpy(p, GPT_ATTRSTR_REQ, (l = sizeof(GPT_ATTRSTR_REQ)));
		p += l - 1;
	}
	if (isset(bits, GPT_ATTRBIT_NOBLOCK)) {
		if (p != *res)
			*p++ = ' ';
		memcpy(p, GPT_ATTRSTR_NOBLOCK, (l = sizeof(GPT_ATTRSTR_NOBLOCK)));
		p += l - 1;
	}
	if (isset(bits, GPT_ATTRBIT_LEGACY)) {
		if (p != *res)
			*p++ = ' ';
		memcpy(p, GPT_ATTRSTR_LEGACY, (l = sizeof(GPT_ATTRSTR_LEGACY)));
		p += l - 1;
	}

	for (n = GPT_ATTRBIT_GUID_FIRST;
	     n < GPT_ATTRBIT_GUID_FIRST + GPT_ATTRBIT_GUID_COUNT; n++) {

		if (!isset(bits, n))
			continue;
		if (!count) {
			if (p != *res)
				*p++ = ' ';
			p += sprintf(p, "GUID:%u", n);
		} else
			p += sprintf(p, ",%u", n);
		count++;
	}

	return 0;
}

static int gpt_entry_attrs_from_string(
			struct fdisk_context *cxt,
			struct gpt_entry *e,
			const char *str)
{
	const char *p = str;
	uint64_t attrs = 0;
	char *bits;

	assert(e);
	assert(p);

	DBG(GPT, ul_debug("parsing string attributes '%s'", p));

	bits = (char *) &attrs;

	while (p && *p) {
		int bit = -1;

		while (isblank(*p)) p++;
		if (!*p)
			break;

		DBG(GPT, ul_debug(" item '%s'", p));

		if (strncmp(p, GPT_ATTRSTR_REQ,
					sizeof(GPT_ATTRSTR_REQ) - 1) == 0) {
			bit = GPT_ATTRBIT_REQ;
			p += sizeof(GPT_ATTRSTR_REQ) - 1;
		} else if (strncmp(p, GPT_ATTRSTR_REQ_TYPO,
					sizeof(GPT_ATTRSTR_REQ_TYPO) - 1) == 0) {
			bit = GPT_ATTRBIT_REQ;
			p += sizeof(GPT_ATTRSTR_REQ_TYPO) - 1;
		} else if (strncmp(p, GPT_ATTRSTR_LEGACY,
					sizeof(GPT_ATTRSTR_LEGACY) - 1) == 0) {
			bit = GPT_ATTRBIT_LEGACY;
			p += sizeof(GPT_ATTRSTR_LEGACY) - 1;
		} else if (strncmp(p, GPT_ATTRSTR_NOBLOCK,
					sizeof(GPT_ATTRSTR_NOBLOCK) - 1) == 0) {
			bit = GPT_ATTRBIT_NOBLOCK;
			p += sizeof(GPT_ATTRSTR_NOBLOCK) - 1;

		/* GUID:<bit> as well as <bit> */
		} else if (isdigit((unsigned char) *p)
			   || (strncmp(p, "GUID:", 5) == 0
			       && isdigit((unsigned char) *(p + 5)))) {
			char *end = NULL;

			if (*p == 'G')
				p += 5;

			errno = 0;
			bit = strtol(p, &end, 0);
			if (errno || !end || end == str
			    || bit < GPT_ATTRBIT_GUID_FIRST
			    || bit >= GPT_ATTRBIT_GUID_FIRST + GPT_ATTRBIT_GUID_COUNT)
				bit = -1;
			else
				p = end;
		}

		if (bit < 0) {
			fdisk_warnx(cxt, _("unsupported GPT attribute bit '%s'"), p);
			return -EINVAL;
		}

		if (*p && *p != ',' && !isblank(*p)) {
			fdisk_warnx(cxt, _("failed to parse GPT attribute string '%s'"), str);
			return -EINVAL;
		}

		setbit(bits, bit);

		while (isblank(*p)) p++;
		if (*p == ',')
			p++;
	}

	e->attrs = attrs;
	return 0;
}

static int gpt_get_partition(struct fdisk_context *cxt, size_t n,
			     struct fdisk_partition *pa)
{
	struct fdisk_gpt_label *gpt;
	struct gpt_entry *e;
	char u_str[UUID_STR_LEN];
	int rc = 0;
	struct gpt_guid guid;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);

	if (n >= gpt_get_nentries(gpt))
		return -EINVAL;

	gpt = self_label(cxt);
	e = gpt_get_entry(gpt, n);

	pa->used = gpt_entry_is_used(e) || gpt_partition_start(e);
	if (!pa->used)
		return 0;

	pa->start = gpt_partition_start(e);
	pa->size = gpt_partition_size(e);
	pa->type = gpt_partition_parttype(cxt, e);

	guid = e->partition_guid;
	if (guid_to_string(&guid, u_str)) {
		pa->uuid = strdup(u_str);
		if (!pa->uuid) {
			rc = -errno;
			goto done;
		}
	} else
		pa->uuid = NULL;

	rc = gpt_entry_attrs_to_string(e, &pa->attrs);
	if (rc)
		goto done;

	pa->name = encode_to_utf8((unsigned char *)e->name, sizeof(e->name));
	return 0;
done:
	fdisk_reset_partition(pa);
	return rc;
}


static int gpt_set_partition(struct fdisk_context *cxt, size_t n,
			     struct fdisk_partition *pa)
{
	struct fdisk_gpt_label *gpt;
	struct gpt_entry *e;
	int rc = 0;
	uint64_t start, end;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);

	if (n >= gpt_get_nentries(gpt))
		return -EINVAL;

	FDISK_INIT_UNDEF(start);
	FDISK_INIT_UNDEF(end);

	gpt = self_label(cxt);
	e = gpt_get_entry(gpt, n);

	if (pa->uuid) {
		char new_u[UUID_STR_LEN], old_u[UUID_STR_LEN];
		struct gpt_guid guid;

		guid = e->partition_guid;
		guid_to_string(&guid, old_u);
		rc = gpt_entry_set_uuid(e, pa->uuid);
		if (rc)
			return rc;
		guid = e->partition_guid;
		guid_to_string(&guid, new_u);
		fdisk_info(cxt, _("Partition UUID changed from %s to %s."),
			old_u, new_u);
	}

	if (pa->name) {
		int len;
		char *old = encode_to_utf8((unsigned char *)e->name, sizeof(e->name));
		len = gpt_entry_set_name(e, pa->name);
		if (len < 0)
			fdisk_warn(cxt, _("Failed to translate partition name, name not changed."));
		else
			fdisk_info(cxt, _("Partition name changed from '%s' to '%.*s'."),
				old, len, pa->name);
		free(old);
	}

	if (pa->type && pa->type->typestr) {
		struct gpt_guid typeid;

		rc = string_to_guid(pa->type->typestr, &typeid);
		if (rc)
			return rc;
		gpt_entry_set_type(e, &typeid);
	}
	if (pa->attrs) {
		rc = gpt_entry_attrs_from_string(cxt, e, pa->attrs);
		if (rc)
			return rc;
	}

	if (fdisk_partition_has_start(pa))
		start = pa->start;
	if (fdisk_partition_has_size(pa) || fdisk_partition_has_start(pa)) {
		uint64_t xstart = fdisk_partition_has_start(pa) ? pa->start : gpt_partition_start(e);
		uint64_t xsize  = fdisk_partition_has_size(pa)  ? pa->size  : gpt_partition_size(e);
		end = xstart + xsize - 1ULL;
	}

	if (!FDISK_IS_UNDEF(start)) {
		if (start < le64_to_cpu(gpt->pheader->first_usable_lba)) {
			fdisk_warnx(cxt, _("The start of the partition understeps FirstUsableLBA."));
			return -EINVAL;
		}
		e->lba_start = cpu_to_le64(start);
	}
	if (!FDISK_IS_UNDEF(end)) {
		if (end > le64_to_cpu(gpt->pheader->last_usable_lba)) {
			fdisk_warnx(cxt, _("The end of the partition oversteps LastUsableLBA."));
			return -EINVAL;
		}
		e->lba_end = cpu_to_le64(end);
	}
	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);

	fdisk_label_set_changed(cxt->label, 1);
	return rc;
}

static int gpt_read(struct fdisk_context *cxt, off_t offset, void *buf, size_t count)
{
	if (offset != lseek(cxt->dev_fd, offset, SEEK_SET))
		return -errno;

	if (read_all(cxt->dev_fd, buf, count))
		return -errno;

	DBG(GPT, ul_debug("  read OK [offset=%zu, size=%zu]",
				(size_t) offset, count));
	return 0;
}

static int gpt_write(struct fdisk_context *cxt, off_t offset, void *buf, size_t count)
{
	if (offset != lseek(cxt->dev_fd, offset, SEEK_SET))
		return -errno;

	if (write_all(cxt->dev_fd, buf, count))
		return -errno;

	if (fsync(cxt->dev_fd) != 0)
		return -errno;

	DBG(GPT, ul_debug("  write OK [offset=%zu, size=%zu]",
				(size_t) offset, count));
	return 0;
}

/*
 * Write partitions.
 * Returns 0 on success, or corresponding error otherwise.
 */
static int gpt_write_partitions(struct fdisk_context *cxt,
				struct gpt_header *header, unsigned char *ents)
{
	size_t esz = 0;
	int rc;

	rc = gpt_sizeof_entries(header, &esz);
	if (rc)
		return rc;

	return gpt_write(cxt,
			(off_t) le64_to_cpu(header->partition_entry_lba) * cxt->sector_size,
			ents, esz);
}

/*
 * Write a GPT header to a specified LBA.
 *
 * We read all sector, so we have to write all sector back
 * to the device -- never ever rely on sizeof(struct gpt_header)!
 *
 * Returns 0 on success, or corresponding error otherwise.
 */
static int gpt_write_header(struct fdisk_context *cxt,
			    struct gpt_header *header, uint64_t lba)
{
	return gpt_write(cxt, lba * cxt->sector_size, header, cxt->sector_size);
}

/*
 * Write the protective MBR.
 * Returns 0 on success, or corresponding error otherwise.
 */
static int gpt_write_pmbr(struct fdisk_context *cxt)
{
	struct gpt_legacy_mbr *pmbr;
	struct gpt_legacy_mbr *current;
	int rc;

	assert(cxt);
	assert(cxt->firstsector);

	DBG(GPT, ul_debug("(over)writing PMBR"));
	pmbr = (struct gpt_legacy_mbr *) cxt->firstsector;

	/* zero out the legacy partitions */
	memset(pmbr->partition_record, 0, sizeof(pmbr->partition_record));

	pmbr->signature = cpu_to_le16(MSDOS_MBR_SIGNATURE);
	pmbr->partition_record[0].os_type      = EFI_PMBR_OSTYPE;
	pmbr->partition_record[0].start_sector = 2;
	pmbr->partition_record[0].end_head     = 0xFF;
	pmbr->partition_record[0].end_sector   = 0xFF;
	pmbr->partition_record[0].end_track    = 0xFF;
	pmbr->partition_record[0].starting_lba = cpu_to_le32(1);

	/*
	 * Set size_in_lba to the size of the disk minus one. If the size of the disk
	 * is too large to be represented by a 32bit LBA (2Tb), set it to 0xFFFFFFFF.
	 */
	if (cxt->total_sectors - 1ULL > 0xFFFFFFFFULL)
		pmbr->partition_record[0].size_in_lba = cpu_to_le32(0xFFFFFFFF);
	else
		pmbr->partition_record[0].size_in_lba =
			cpu_to_le32((uint32_t) (cxt->total_sectors - 1ULL));

	/* Read the current PMBR and compare it with the new, don't write if
	 * the same. */
	current = malloc(sizeof(*current));
	if (!current)
		goto do_write;

	rc = gpt_read(cxt, GPT_PMBR_LBA * cxt->sector_size,
		      current, sizeof(*current));
	if (!rc)
		rc = memcmp(pmbr, current, sizeof(*current));

	free(current);

	if (!rc) {
		DBG(GPT, ul_debug("Same MBR on disk => don't write it"));
		return 0;
	}

 do_write:
	/* pMBR covers the first sector (LBA) of the disk */
	return gpt_write(cxt, GPT_PMBR_LBA * cxt->sector_size,
			 pmbr, cxt->sector_size);
}

/*
 * Writes in-memory GPT and pMBR data to disk.
 * Returns 0 if successful write, otherwise, a corresponding error.
 * Any indication of error will abort the operation.
 */
static int gpt_write_disklabel(struct fdisk_context *cxt)
{
	struct fdisk_gpt_label *gpt;
	int mbr_type;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	DBG(GPT, ul_debug("writing..."));

	gpt = self_label(cxt);
	mbr_type = valid_pmbr(cxt);

	/* check that disk is big enough to handle the backup header */
	if (le64_to_cpu(gpt->pheader->alternative_lba) > cxt->total_sectors)
		goto err0;

	/* check that the backup header is properly placed */
	if (le64_to_cpu(gpt->pheader->alternative_lba) < cxt->total_sectors - 1ULL)
		goto err0;

	if (check_overlap_partitions(gpt))
		goto err0;

	if (gpt->minimize)
		gpt_minimize_alternative_lba(cxt, gpt);

	/* recompute CRCs for both headers */
	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);

	/*
	 * UEFI requires writing in this specific order:
	 *   1) backup partition tables
	 *   2) backup GPT header
	 *   3) primary partition tables
	 *   4) primary GPT header
	 *   5) protective MBR
	 *
	 * If any write fails, we abort the rest.
	 */
	if (gpt_write_partitions(cxt, gpt->bheader, gpt->ents) != 0)
		goto err1;
	if (gpt_write_header(cxt, gpt->bheader,
			     le64_to_cpu(gpt->pheader->alternative_lba)) != 0)
		goto err1;
	if (gpt_write_partitions(cxt, gpt->pheader, gpt->ents) != 0)
		goto err1;
	if (gpt_write_header(cxt, gpt->pheader, GPT_PRIMARY_PARTITION_TABLE_LBA) != 0)
		goto err1;

	if (mbr_type == GPT_MBR_HYBRID)
		fdisk_warnx(cxt, _("The device contains hybrid MBR -- writing GPT only."));
	else if (gpt_write_pmbr(cxt) != 0)
		goto err1;

	DBG(GPT, ul_debug("...write success"));
	return 0;
err0:
	DBG(GPT, ul_debug("...write failed: incorrect input"));
	errno = EINVAL;
	return -EINVAL;
err1:
	DBG(GPT, ul_debug("...write failed: %m"));
	return -errno;
}

/*
 * Verify data integrity and report any found problems for:
 *   - primary and backup header validations
 *   - partition validations
 */
static int gpt_verify_disklabel(struct fdisk_context *cxt)
{
	int nerror = 0;
	unsigned int ptnum;
	struct fdisk_gpt_label *gpt;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);
	if (!gpt)
		return -EINVAL;

	if (!gpt->bheader) {
		nerror++;
		fdisk_warnx(cxt, _("Disk does not contain a valid backup header."));
	}

	if (!gpt_check_header_crc(gpt->pheader, gpt->ents)) {
		nerror++;
		fdisk_warnx(cxt, _("Invalid primary header CRC checksum."));
	}
	if (gpt->bheader && !gpt_check_header_crc(gpt->bheader, gpt->ents)) {
		nerror++;
		fdisk_warnx(cxt, _("Invalid backup header CRC checksum."));
	}

	if (!gpt_check_entryarr_crc(gpt->pheader, gpt->ents)) {
		nerror++;
		fdisk_warnx(cxt, _("Invalid partition entry checksum."));
	}

	if (!gpt_check_lba_sanity(cxt, gpt->pheader)) {
		nerror++;
		fdisk_warnx(cxt, _("Invalid primary header LBA sanity checks."));
	}
	if (gpt->bheader && !gpt_check_lba_sanity(cxt, gpt->bheader)) {
		nerror++;
		fdisk_warnx(cxt, _("Invalid backup header LBA sanity checks."));
	}

	if (le64_to_cpu(gpt->pheader->my_lba) != GPT_PRIMARY_PARTITION_TABLE_LBA) {
		nerror++;
		fdisk_warnx(cxt, _("MyLBA mismatch with real position at primary header."));
	}
	if (gpt->bheader && le64_to_cpu(gpt->bheader->my_lba) != last_lba(cxt)) {
		nerror++;
		fdisk_warnx(cxt, _("MyLBA mismatch with real position at backup header."));

	}
	if (le64_to_cpu(gpt->pheader->alternative_lba) >= cxt->total_sectors) {
		nerror++;
		fdisk_warnx(cxt, _("Disk is too small to hold all data."));
	}

	/*
	 * if the GPT is the primary table, check the alternateLBA
	 * to see if it is a valid GPT
	 */
	if (gpt->bheader && (le64_to_cpu(gpt->pheader->my_lba) !=
			     le64_to_cpu(gpt->bheader->alternative_lba))) {
		nerror++;
		fdisk_warnx(cxt, _("Primary and backup header mismatch."));
	}

	ptnum = check_overlap_partitions(gpt);
	if (ptnum) {
		nerror++;
		fdisk_warnx(cxt, _("Partition %u overlaps with partition %u."),
				ptnum, ptnum+1);
	}

	ptnum = check_too_big_partitions(gpt, cxt->total_sectors);
	if (ptnum) {
		nerror++;
		fdisk_warnx(cxt, _("Partition %u is too big for the disk."),
				ptnum);
	}

	ptnum = check_start_after_end_partitions(gpt);
	if (ptnum) {
		nerror++;
		fdisk_warnx(cxt, _("Partition %u ends before it starts."),
				ptnum);
	}

	if (!nerror) { /* yay :-) */
		uint32_t nsegments = 0;
		uint64_t free_sectors = 0, largest_segment = 0;
		char *strsz = NULL;

		fdisk_info(cxt, _("No errors detected."));
		fdisk_info(cxt, _("Header version: %s"), gpt_get_header_revstr(gpt->pheader));
		fdisk_info(cxt, _("Using %zu out of %zu partitions."),
		       partitions_in_use(gpt),
		       gpt_get_nentries(gpt));

		free_sectors = get_free_sectors(cxt, gpt, &nsegments, &largest_segment);
		if (largest_segment)
			strsz = size_to_human_string(SIZE_SUFFIX_SPACE | SIZE_SUFFIX_3LETTER,
					largest_segment * cxt->sector_size);

		fdisk_info(cxt,
			   P_("A total of %ju free sectors is available in %u segment.",
			      "A total of %ju free sectors is available in %u segments "
			      "(the largest is %s).", nsegments),
			   free_sectors, nsegments, strsz ? : "0 B");
		free(strsz);

	} else
		fdisk_warnx(cxt,
			P_("%d error detected.", "%d errors detected.", nerror),
			nerror);

	return nerror;
}

/* Delete a single GPT partition, specified by partnum. */
static int gpt_delete_partition(struct fdisk_context *cxt,
				size_t partnum)
{
	struct fdisk_gpt_label *gpt;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);

	if (partnum >= cxt->label->nparts_max)
		return -EINVAL;

	if (!gpt_entry_is_used(gpt_get_entry(gpt, partnum)))
		return -EINVAL;

	/* hasta la vista, baby! */
	gpt_zeroize_entry(gpt, partnum);

	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);
	cxt->label->nparts_cur--;
	fdisk_label_set_changed(cxt->label, 1);

	return 0;
}


/* Performs logical checks to add a new partition entry */
static int gpt_add_partition(
		struct fdisk_context *cxt,
		struct fdisk_partition *pa,
		size_t *partno)
{
	uint64_t user_f, user_l;	/* user input ranges for first and last sectors */
	uint64_t disk_f, disk_l;	/* first and last available sector ranges on device*/
	uint64_t dflt_f, dflt_l, max_l;	/* largest segment (default) */
	struct gpt_guid typeid;
	struct fdisk_gpt_label *gpt;
	struct gpt_header *pheader;
	struct gpt_entry *e;
	struct fdisk_ask *ask = NULL;
	size_t partnum;
	int rc;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);

	assert(gpt);
	assert(gpt->pheader);
	assert(gpt->ents);

	pheader = gpt->pheader;

	rc = fdisk_partition_next_partno(pa, cxt, &partnum);
	if (rc) {
		DBG(GPT, ul_debug("failed to get next partno"));
		return rc;
	}

	assert(partnum < gpt_get_nentries(gpt));

	if (gpt_entry_is_used(gpt_get_entry(gpt, partnum))) {
		fdisk_warnx(cxt, _("Partition %zu is already defined.  "
			           "Delete it before re-adding it."), partnum +1);
		return -ERANGE;
	}
	if (gpt_get_nentries(gpt) == partitions_in_use(gpt)) {
		fdisk_warnx(cxt, _("All partitions are already in use."));
		return -ENOSPC;
	}
	if (!get_free_sectors(cxt, gpt, NULL, NULL)) {
		fdisk_warnx(cxt, _("No free sectors available."));
		return -ENOSPC;
	}

	rc = string_to_guid(pa && pa->type && pa->type->typestr ?
				pa->type->typestr:
				GPT_DEFAULT_ENTRY_TYPE, &typeid);
	if (rc)
		return rc;

	disk_f = find_first_available(gpt, le64_to_cpu(pheader->first_usable_lba));
	e = gpt_get_entry(gpt, 0);

	/* if first sector no explicitly defined then ignore small gaps before
	 * the first partition */
	if ((!pa || !fdisk_partition_has_start(pa))
	    && gpt_entry_is_used(e)
	    && disk_f < gpt_partition_start(e)) {

		do {
			uint64_t x;
			DBG(GPT, ul_debug("testing first sector %"PRIu64"", disk_f));
			disk_f = find_first_available(gpt, disk_f);
			if (!disk_f)
				break;
			x = find_last_free(gpt, disk_f);
			if (x - disk_f >= cxt->grain / cxt->sector_size)
				break;
			DBG(GPT, ul_debug("first sector %"PRIu64" addresses to small space, continue...", disk_f));
			disk_f = x + 1ULL;
		} while(1);

		if (disk_f == 0)
			disk_f = find_first_available(gpt, le64_to_cpu(pheader->first_usable_lba));
	}

	e = NULL;
	disk_l = find_last_free_sector(gpt);

	/* the default is the largest free space */
	dflt_f = find_first_in_largest(gpt);
	dflt_l = find_last_free(gpt, dflt_f);

	/* don't offer too small free space by default, this is possible to
	 * bypass by sfdisk script */
	if ((!pa || !fdisk_partition_has_start(pa))
	    && dflt_l - dflt_f + 1 < cxt->grain / cxt->sector_size) {
		fdisk_warnx(cxt, _("No enough free sectors available."));
		return -ENOSPC;
	}

	/* align the default in range <dflt_f,dflt_l>*/
	dflt_f = fdisk_align_lba_in_range(cxt, dflt_f, dflt_f, dflt_l);

	/* first sector */
	if (pa && pa->start_follow_default) {
		user_f = dflt_f;

	} else if (pa && fdisk_partition_has_start(pa)) {
		DBG(GPT, ul_debug("first sector defined: %ju",  (uintmax_t)pa->start));
		if (pa->start != find_first_available(gpt, pa->start)) {
			fdisk_warnx(cxt, _("Sector %ju already used."),  (uintmax_t)pa->start);
			return -ERANGE;
		}
		user_f = pa->start;
	} else {
		/*  ask by dialog */
		for (;;) {
			if (!ask)
				ask = fdisk_new_ask();
			else
				fdisk_reset_ask(ask);
			if (!ask)
				return -ENOMEM;

			/* First sector */
			fdisk_ask_set_query(ask, _("First sector"));
			fdisk_ask_set_type(ask, FDISK_ASKTYPE_NUMBER);
			fdisk_ask_number_set_low(ask,     disk_f);	/* minimal */
			fdisk_ask_number_set_default(ask, dflt_f);	/* default */
			fdisk_ask_number_set_high(ask,    disk_l);	/* maximal */

			rc = fdisk_do_ask(cxt, ask);
			if (rc)
				goto done;

			user_f = fdisk_ask_number_get_result(ask);
			if (user_f != find_first_available(gpt, user_f)) {
				fdisk_warnx(cxt, _("Sector %ju already used."), user_f);
				continue;
			}
			break;
		}
	}


	/* Last sector */
	dflt_l = max_l = find_last_free(gpt, user_f);

	/* Make sure the last partition has aligned size by default because
	 * range specified by LastUsableLBA may be unaligned on disks where
	 * logical sector != physical (512/4K) because backup header size is
	 * calculated from logical sectors. */
	if (max_l == le64_to_cpu(gpt->pheader->last_usable_lba))
		dflt_l = fdisk_align_lba_in_range(cxt, max_l, user_f, max_l) - 1;

	if (pa && pa->end_follow_default) {
		user_l = dflt_l;

	} else if (pa && fdisk_partition_has_size(pa)) {
		user_l = user_f + pa->size - 1;
		DBG(GPT, ul_debug("size defined: %ju, end: %"PRIu64
				  "(last possible: %"PRIu64", optimal: %"PRIu64")",
				(uintmax_t)pa->size, user_l, max_l, dflt_l));

		if (user_l != dflt_l
		    && !pa->size_explicit
		    && alignment_required(cxt)
		    && user_l - user_f > (cxt->grain / fdisk_get_sector_size(cxt))) {

			user_l = fdisk_align_lba_in_range(cxt, user_l, user_f, dflt_l);
			if (user_l > user_f)
				user_l -= 1ULL;
		}
	} else {
		for (;;) {
			if (!ask)
				ask = fdisk_new_ask();
			else
				fdisk_reset_ask(ask);
			if (!ask)
				return -ENOMEM;

			fdisk_ask_set_query(ask, _("Last sector, +/-sectors or +/-size{K,M,G,T,P}"));
			fdisk_ask_set_type(ask, FDISK_ASKTYPE_OFFSET);
			fdisk_ask_number_set_low(ask,     user_f);	/* minimal */
			fdisk_ask_number_set_default(ask, dflt_l);	/* default */
			fdisk_ask_number_set_high(ask,    max_l);	/* maximal */
			fdisk_ask_number_set_base(ask,    user_f);	/* base for relative input */
			fdisk_ask_number_set_unit(ask,    cxt->sector_size);
			fdisk_ask_number_set_wrap_negative(ask, 1);	/* wrap negative around high */

			rc = fdisk_do_ask(cxt, ask);
			if (rc)
				goto done;

			user_l = fdisk_ask_number_get_result(ask);
			if (fdisk_ask_number_is_relative(ask)) {
				user_l = fdisk_align_lba_in_range(cxt, user_l, user_f, dflt_l);
				if (user_l > user_f)
					user_l -= 1ULL;
			}

			if (user_l >= user_f && user_l <= disk_l)
				break;

			fdisk_warnx(cxt, _("Value out of range."));
		}
	}


	if (user_f > user_l || partnum >= cxt->label->nparts_max) {
		fdisk_warnx(cxt, _("Could not create partition %zu"), partnum + 1);
		rc = -EINVAL;
		goto done;
	}

	/* Be paranoid and check against on-disk setting rather than against libfdisk cxt */
	if (user_l > le64_to_cpu(pheader->last_usable_lba)) {
		fdisk_warnx(cxt, _("The last usable GPT sector is %ju, but %ju is requested."),
				le64_to_cpu(pheader->last_usable_lba), user_l);
		rc = -EINVAL;
		goto done;
	}

	if (user_f < le64_to_cpu(pheader->first_usable_lba)) {
		fdisk_warnx(cxt, _("The first usable GPT sector is %ju, but %ju is requested."),
				le64_to_cpu(pheader->first_usable_lba), user_f);
		rc = -EINVAL;
		goto done;
	}

	assert(!FDISK_IS_UNDEF(user_l));
	assert(!FDISK_IS_UNDEF(user_f));
	assert(partnum < gpt_get_nentries(gpt));

	e = gpt_get_entry(gpt, partnum);
	e->lba_end = cpu_to_le64(user_l);
	e->lba_start = cpu_to_le64(user_f);

	gpt_entry_set_type(e, &typeid);

	if (pa && pa->uuid) {
		/* Sometimes it's necessary to create a copy of the PT and
		 * reuse already defined UUID
		 */
		rc = gpt_entry_set_uuid(e, pa->uuid);
		if (rc)
			goto done;
	} else {
		/* Any time a new partition entry is created a new GUID must be
		 * generated for that partition, and every partition is guaranteed
		 * to have a unique GUID.
		 */
		struct gpt_guid guid;

		uuid_generate_random((unsigned char *) &guid);
		swap_efi_guid(&guid);
		e->partition_guid = guid;
	}

	if (pa && pa->name && *pa->name)
		gpt_entry_set_name(e, pa->name);
	if (pa && pa->attrs)
		gpt_entry_attrs_from_string(cxt, e, pa->attrs);

	DBG(GPT, ul_debug("new partition: partno=%zu, start=%"PRIu64", end=%"PRIu64", size=%"PRIu64"",
				partnum,
				gpt_partition_start(e),
				gpt_partition_end(e),
				gpt_partition_size(e)));

	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);

	/* report result */
	{
		struct fdisk_parttype *t;

		cxt->label->nparts_cur++;
		fdisk_label_set_changed(cxt->label, 1);

		t = gpt_partition_parttype(cxt, e);
		fdisk_info_new_partition(cxt, partnum + 1, user_f, user_l, t);
		fdisk_unref_parttype(t);
	}

	rc = 0;
	if (partno)
		*partno = partnum;
done:
	fdisk_unref_ask(ask);
	return rc;
}

/*
 * Create a new GPT disklabel - destroys any previous data.
 */
static int gpt_create_disklabel(struct fdisk_context *cxt)
{
	int rc = 0;
	size_t esz = 0;
	char str[UUID_STR_LEN];
	struct fdisk_gpt_label *gpt;
	struct gpt_guid guid;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);

	/* label private stuff has to be empty, see gpt_deinit() */
	assert(gpt->pheader == NULL);
	assert(gpt->bheader == NULL);

	/*
	 * When no header, entries or pmbr is set, we're probably
	 * dealing with a new, empty disk - so always allocate memory
	 * to deal with the data structures whatever the case is.
	 */
	rc = gpt_mknew_pmbr(cxt);
	if (rc < 0)
		goto done;

	assert(cxt->sector_size >= sizeof(struct gpt_header));

	/* primary */
	gpt->pheader = calloc(1, cxt->sector_size);
	if (!gpt->pheader) {
		rc = -ENOMEM;
		goto done;
	}
	rc = gpt_mknew_header(cxt, gpt->pheader, GPT_PRIMARY_PARTITION_TABLE_LBA);
	if (rc < 0)
		goto done;

	/* backup ("copy" primary) */
	gpt->bheader = calloc(1, cxt->sector_size);
	if (!gpt->bheader) {
		rc = -ENOMEM;
		goto done;
	}
	rc = gpt_mknew_header_from_bkp(cxt, gpt->bheader,
			last_lba(cxt), gpt->pheader);
	if (rc < 0)
		goto done;

	rc = gpt_sizeof_entries(gpt->pheader, &esz);
	if (rc)
		goto done;
	gpt->ents = calloc(1, esz);
	if (!gpt->ents) {
		rc = -ENOMEM;
		goto done;
	}
	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);

	cxt->label->nparts_max = gpt_get_nentries(gpt);
	cxt->label->nparts_cur = 0;

	guid = gpt->pheader->disk_guid;
	guid_to_string(&guid, str);
	fdisk_label_set_changed(cxt->label, 1);
	fdisk_info(cxt, _("Created a new GPT disklabel (GUID: %s)."), str);

	if (gpt_get_nentries(gpt) < GPT_NPARTITIONS)
		fdisk_info(cxt, _("The maximal number of partitions is %zu (default is %zu)."),
				gpt_get_nentries(gpt), GPT_NPARTITIONS);
done:
	return rc;
}

static int gpt_set_disklabel_id(struct fdisk_context *cxt, const char *str)
{
	struct fdisk_gpt_label *gpt;
	struct gpt_guid uuid;
	char *old, *new;
	int rc;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);
	if (!str) {
		char *buf = NULL;

		if (fdisk_ask_string(cxt,
				_("Enter new disk UUID (in 8-4-4-4-12 format)"), &buf))
			return -EINVAL;
		rc = string_to_guid(buf, &uuid);
		free(buf);
	} else
		rc = string_to_guid(str, &uuid);

	if (rc) {
		fdisk_warnx(cxt, _("Failed to parse your UUID."));
		return rc;
	}

	old = gpt_get_header_id(gpt->pheader);

	gpt->pheader->disk_guid = uuid;
	gpt->bheader->disk_guid = uuid;

	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);

	new = gpt_get_header_id(gpt->pheader);

	fdisk_info(cxt, _("Disk identifier changed from %s to %s."), old, new);

	free(old);
	free(new);
	fdisk_label_set_changed(cxt->label, 1);
	return 0;
}

static int gpt_check_table_overlap(struct fdisk_context *cxt,
				   uint64_t first_usable,
				   uint64_t last_usable)
{
	struct fdisk_gpt_label *gpt = self_label(cxt);
	size_t i;
	int rc = 0;

	/* First check if there's enough room for the table. last_lba may have wrapped */
	if (first_usable > cxt->total_sectors || /* far too little space */
	    last_usable > cxt->total_sectors || /* wrapped */
	    first_usable > last_usable) { /* too little space */
		fdisk_warnx(cxt, _("Not enough space for new partition table!"));
		return -ENOSPC;
	}

	/* check that all partitions fit in the remaining space */
	for (i = 0; i < gpt_get_nentries(gpt); i++) {
		struct gpt_entry *e = gpt_get_entry(gpt, i);

		if (!gpt_entry_is_used(e))
		        continue;
		if (gpt_partition_start(e) < first_usable) {
			fdisk_warnx(cxt, _("Partition #%zu out of range (minimal start is %"PRIu64" sectors)"),
		                    i + 1, first_usable);
			rc = -EINVAL;
		}
		if (gpt_partition_end(e) > last_usable) {
			fdisk_warnx(cxt, _("Partition #%zu out of range (maximal end is %"PRIu64" sectors)"),
		                    i + 1, last_usable - (uint64_t) 1);
			rc = -EINVAL;
		}
	}
	return rc;
}

/**
 * fdisk_gpt_set_npartitions:
 * @cxt: context
 * @nents: number of wanted entries
 *
 * Elarge GPT entries array if possible. The function check if an existing
 * partition does not overlap the entries array area. If yes, then it report
 * warning and returns -EINVAL.
 *
 * Returns: 0 on success, < 0 on error.
 * Since: 2.29
 */
int fdisk_gpt_set_npartitions(struct fdisk_context *cxt, uint32_t nents)
{
	struct fdisk_gpt_label *gpt;
	size_t new_size = 0;
	uint32_t old_nents;
	uint64_t first_usable = 0ULL, last_usable = 0ULL;
	int rc;

	assert(cxt);
	assert(cxt->label);

	if (!fdisk_is_label(cxt, GPT))
		return -EINVAL;

	gpt = self_label(cxt);

	old_nents = le32_to_cpu(gpt->pheader->npartition_entries);
	if (old_nents == nents)
		return 0;	/* do nothing, say nothing */

	/* calculate the size (bytes) of the entries array */
	rc = gpt_calculate_sizeof_entries(gpt->pheader, nents, &new_size);
	if (rc) {
		uint32_t entry_size = le32_to_cpu(gpt->pheader->sizeof_partition_entry);

		if (entry_size == 0)
			fdisk_warnx(cxt, _("The partition entry size is zero."));
		else
			fdisk_warnx(cxt, _("The number of the partition has to be smaller than %zu."),
				(size_t) UINT32_MAX / entry_size);
		return rc;
	}

	rc = gpt_calculate_first_lba(gpt->pheader, nents, &first_usable, cxt);
	if (rc == 0)
		rc = gpt_calculate_last_lba(gpt->pheader, nents, &last_usable, cxt);
	if (rc)
		return rc;

	/* if expanding the table, first check that everything fits,
	 * then allocate more memory and zero. */
	if (nents > old_nents) {
		unsigned char *ents;
		size_t old_size = 0;

		rc = gpt_calculate_sizeof_entries(gpt->pheader, old_nents, &old_size);
		if (rc == 0)
			rc = gpt_check_table_overlap(cxt, first_usable, last_usable);
		if (rc)
			return rc;
		ents = realloc(gpt->ents, new_size);
		if (!ents) {
			fdisk_warnx(cxt, _("Cannot allocate memory!"));
			return -ENOMEM;
		}
		memset(ents + old_size, 0, new_size - old_size);
		gpt->ents = ents;
	}

	/* everything's ok, apply the new size */
	gpt->pheader->npartition_entries = cpu_to_le32(nents);
	gpt->bheader->npartition_entries = cpu_to_le32(nents);

	/* usable LBA addresses will have changed */
	fdisk_set_first_lba(cxt, first_usable);
	fdisk_set_last_lba(cxt, last_usable);
	gpt->pheader->first_usable_lba = cpu_to_le64(first_usable);
	gpt->bheader->first_usable_lba = cpu_to_le64(first_usable);
	gpt->pheader->last_usable_lba = cpu_to_le64(last_usable);
	gpt->bheader->last_usable_lba = cpu_to_le64(last_usable);

	/* The backup header must be recalculated */
	gpt_mknew_header_common(cxt, gpt->bheader, le64_to_cpu(gpt->pheader->alternative_lba));

	/* CRCs will have changed */
	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);

	/* update library info */
	cxt->label->nparts_max = gpt_get_nentries(gpt);

	fdisk_info(cxt, _("Partition table length changed from %"PRIu32" to %"PRIu32"."),
			old_nents, nents);

	fdisk_label_set_changed(cxt->label, 1);
	return 0;
}

static int gpt_part_is_used(struct fdisk_context *cxt, size_t i)
{
	struct fdisk_gpt_label *gpt;
	struct gpt_entry *e;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);

	if (i >= gpt_get_nentries(gpt))
		return 0;

	e = gpt_get_entry(gpt, i);

	return gpt_entry_is_used(e) || gpt_partition_start(e);
}

/**
 * fdisk_gpt_is_hybrid:
 * @cxt: context
 *
 * The regular GPT contains PMBR (dummy protective MBR) where the protective
 * MBR does not address any partitions.
 *
 * Hybrid GPT contains regular MBR where this partition table addresses the
 * same partitions as GPT. It's recommended to not use hybrid GPT due to MBR
 * limits.
 *
 * The libfdisk does not provide functionality to sync GPT and MBR, you have to
 * directly access and modify (P)MBR (see fdisk_new_nested_context()).
 *
 * Returns: 1 if partition table detected as hybrid otherwise return 0
 */
int fdisk_gpt_is_hybrid(struct fdisk_context *cxt)
{
	assert(cxt);
	return valid_pmbr(cxt) == GPT_MBR_HYBRID;
}

/**
 * fdisk_gpt_get_partition_attrs:
 * @cxt: context
 * @partnum: partition number
 * @attrs: GPT partition attributes
 *
 * Sets @attrs for the given partition
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_gpt_get_partition_attrs(
		struct fdisk_context *cxt,
		size_t partnum,
		uint64_t *attrs)
{
	struct fdisk_gpt_label *gpt;

	assert(cxt);
	assert(cxt->label);

	if (!fdisk_is_label(cxt, GPT))
		return -EINVAL;

	gpt = self_label(cxt);

	if (partnum >= gpt_get_nentries(gpt))
		return -EINVAL;

	*attrs = le64_to_cpu(gpt_get_entry(gpt, partnum)->attrs);
	return 0;
}

/**
 * fdisk_gpt_set_partition_attrs:
 * @cxt: context
 * @partnum: partition number
 * @attrs: GPT partition attributes
 *
 * Sets the GPT partition attributes field to @attrs.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_gpt_set_partition_attrs(
		struct fdisk_context *cxt,
		size_t partnum,
		uint64_t attrs)
{
	struct fdisk_gpt_label *gpt;

	assert(cxt);
	assert(cxt->label);

	if (!fdisk_is_label(cxt, GPT))
		return -EINVAL;

	DBG(GPT, ul_debug("entry attributes change requested partno=%zu", partnum));
	gpt = self_label(cxt);

	if (partnum >= gpt_get_nentries(gpt))
		return -EINVAL;

	gpt_get_entry(gpt, partnum)->attrs = cpu_to_le64(attrs);
	fdisk_info(cxt, _("The attributes on partition %zu changed to 0x%016" PRIx64 "."),
			partnum + 1, attrs);

	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);
	fdisk_label_set_changed(cxt->label, 1);
	return 0;
}

static int gpt_toggle_partition_flag(
		struct fdisk_context *cxt,
		size_t i,
		unsigned long flag)
{
	struct fdisk_gpt_label *gpt;
	struct gpt_entry *e;
	uint64_t attrs;
	uintmax_t tmp;
	char *bits;
	const char *name = NULL;
	int bit = -1, rc;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	DBG(GPT, ul_debug("entry attribute change requested partno=%zu", i));
	gpt = self_label(cxt);

	if (i >= gpt_get_nentries(gpt))
		return -EINVAL;

	e = gpt_get_entry(gpt, i);
	attrs = e->attrs;
	bits = (char *) &attrs;

	switch (flag) {
	case GPT_FLAG_REQUIRED:
		bit = GPT_ATTRBIT_REQ;
		name = GPT_ATTRSTR_REQ;
		break;
	case GPT_FLAG_NOBLOCK:
		bit = GPT_ATTRBIT_NOBLOCK;
		name = GPT_ATTRSTR_NOBLOCK;
		break;
	case GPT_FLAG_LEGACYBOOT:
		bit = GPT_ATTRBIT_LEGACY;
		name = GPT_ATTRSTR_LEGACY;
		break;
	case GPT_FLAG_GUIDSPECIFIC:
		rc = fdisk_ask_number(cxt, 48, 48, 63, _("Enter GUID specific bit"), &tmp);
		if (rc)
			return rc;
		bit = tmp;
		break;
	default:
		/* already specified PT_FLAG_GUIDSPECIFIC bit */
		if (flag >= 48 && flag <= 63) {
			bit = flag;
			flag = GPT_FLAG_GUIDSPECIFIC;
		}
		break;
	}

	if (bit < 0) {
		fdisk_warnx(cxt, _("failed to toggle unsupported bit %lu"), flag);
		return -EINVAL;
	}

	if (!isset(bits, bit))
		setbit(bits, bit);
	else
		clrbit(bits, bit);

	e->attrs = attrs;

	if (flag == GPT_FLAG_GUIDSPECIFIC)
		fdisk_info(cxt, isset(bits, bit) ?
			_("The GUID specific bit %d on partition %zu is enabled now.") :
			_("The GUID specific bit %d on partition %zu is disabled now."),
			bit, i + 1);
	else
		fdisk_info(cxt, isset(bits, bit) ?
			_("The %s flag on partition %zu is enabled now.") :
			_("The %s flag on partition %zu is disabled now."),
			name, i + 1);

	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);
	fdisk_label_set_changed(cxt->label, 1);
	return 0;
}

static int gpt_entry_cmp_start(const void *a, const void *b)
{
	const struct gpt_entry  *ae = (const struct gpt_entry *) a,
				*be = (const struct gpt_entry *) b;
	int au = gpt_entry_is_used(ae),
	    bu = gpt_entry_is_used(be);

	if (!au && !bu)
		return 0;
	if (!au)
		return 1;
	if (!bu)
		return -1;

	return cmp_numbers(gpt_partition_start(ae), gpt_partition_start(be));
}

/* sort partition by start sector */
static int gpt_reorder(struct fdisk_context *cxt)
{
	struct fdisk_gpt_label *gpt;
	size_t i, nparts, mess;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);
	nparts = gpt_get_nentries(gpt);

	for (i = 0, mess = 0; mess == 0 && i + 1 < nparts; i++)
		mess = gpt_entry_cmp_start(
				(const void *) gpt_get_entry(gpt, i),
				(const void *) gpt_get_entry(gpt, i + 1)) > 0;

	if (!mess)
		return 1;

	qsort(gpt->ents, nparts, sizeof(struct gpt_entry),
			gpt_entry_cmp_start);

	gpt_recompute_crc(gpt->pheader, gpt->ents);
	gpt_recompute_crc(gpt->bheader, gpt->ents);
	fdisk_label_set_changed(cxt->label, 1);

	return 0;
}

static int gpt_reset_alignment(struct fdisk_context *cxt)
{
	struct fdisk_gpt_label *gpt;
	struct gpt_header *h;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, GPT));

	gpt = self_label(cxt);
	h = gpt ? gpt->pheader : NULL;

	if (h) {
		/* always follow existing table */
		cxt->first_lba = le64_to_cpu(h->first_usable_lba);
		cxt->last_lba  = le64_to_cpu(h->last_usable_lba);
	} else {
		/* estimate ranges for GPT */
		uint64_t first, last;

		count_first_last_lba(cxt, &first, &last, NULL);
		if (cxt->first_lba < first)
			cxt->first_lba = first;
		if (cxt->last_lba > last)
			cxt->last_lba = last;
	}

	return 0;
}
/*
 * Deinitialize fdisk-specific variables
 */
static void gpt_deinit(struct fdisk_label *lb)
{
	struct fdisk_gpt_label *gpt = (struct fdisk_gpt_label *) lb;

	if (!gpt)
		return;

	free(gpt->ents);
	free(gpt->pheader);
	free(gpt->bheader);

	gpt->ents = NULL;
	gpt->pheader = NULL;
	gpt->bheader = NULL;
}

static const struct fdisk_label_operations gpt_operations =
{
	.probe		= gpt_probe_label,
	.write		= gpt_write_disklabel,
	.verify		= gpt_verify_disklabel,
	.create		= gpt_create_disklabel,
	.locate		= gpt_locate_disklabel,
	.get_item	= gpt_get_disklabel_item,
	.set_id		= gpt_set_disklabel_id,

	.get_part	= gpt_get_partition,
	.set_part	= gpt_set_partition,
	.add_part	= gpt_add_partition,
	.del_part	= gpt_delete_partition,
	.reorder	= gpt_reorder,

	.part_is_used	= gpt_part_is_used,
	.part_toggle_flag = gpt_toggle_partition_flag,

	.deinit		= gpt_deinit,

	.reset_alignment = gpt_reset_alignment
};

static const struct fdisk_field gpt_fields[] =
{
	/* basic */
	{ FDISK_FIELD_DEVICE,	N_("Device"),	 10,	0 },
	{ FDISK_FIELD_START,	N_("Start"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_END,	N_("End"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_SECTORS,	N_("Sectors"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_SIZE,	N_("Size"),	  5,	FDISK_FIELDFL_NUMBER | FDISK_FIELDFL_EYECANDY },
	{ FDISK_FIELD_TYPE,	N_("Type"),	0.1,	FDISK_FIELDFL_EYECANDY },
	/* expert */
	{ FDISK_FIELD_TYPEID,	N_("Type-UUID"), 36,	FDISK_FIELDFL_DETAIL },
	{ FDISK_FIELD_UUID,	N_("UUID"),	 36,	FDISK_FIELDFL_DETAIL },
	{ FDISK_FIELD_NAME,	N_("Name"),	0.2,	FDISK_FIELDFL_DETAIL },
	{ FDISK_FIELD_ATTR,	N_("Attrs"),	  0,	FDISK_FIELDFL_DETAIL }
};

/*
 * allocates GPT in-memory stuff
 */
struct fdisk_label *fdisk_new_gpt_label(struct fdisk_context *cxt __attribute__ ((__unused__)))
{
	struct fdisk_label *lb;
	struct fdisk_gpt_label *gpt;

	gpt = calloc(1, sizeof(*gpt));
	if (!gpt)
		return NULL;

	/* initialize generic part of the driver */
	lb = (struct fdisk_label *) gpt;
	lb->name = "gpt";
	lb->id = FDISK_DISKLABEL_GPT;
	lb->op = &gpt_operations;

	lb->parttypes = gpt_parttypes;
	lb->nparttypes = ARRAY_SIZE(gpt_parttypes);
	lb->parttype_cuts = gpt_parttype_cuts;
	lb->nparttype_cuts = ARRAY_SIZE(gpt_parttype_cuts);

	lb->fields = gpt_fields;
	lb->nfields = ARRAY_SIZE(gpt_fields);

	/* return calloc() result to keep static anaylizers happy */
	return (struct fdisk_label *) gpt;
}

/**
 * fdisk_gpt_disable_relocation
 * @lb: label
 * @disable: 0 or 1
 *
 * Disable automatic backup header relocation to the end of the device. The
 * header position is recalculated during libfdisk probing stage by
 * fdisk_assign_device() and later written by fdisk_write_disklabel(), so you
 * need to call it before fdisk_assign_device().
 *
 * Since: 2.36
 */
void fdisk_gpt_disable_relocation(struct fdisk_label *lb, int disable)
{
	struct fdisk_gpt_label *gpt = (struct fdisk_gpt_label *) lb;

	assert(gpt);
	gpt->no_relocate = disable ? 1 : 0;
}

/**
 * fdisk_gpt_enable_minimize
 * @lb: label
 * @enable: 0 or 1
 *
 * Force libfdisk to write backup header to behind last partition. The
 * header position is recalculated on fdisk_write_disklabel().
 *
 * Since: 2.36
 */
void fdisk_gpt_enable_minimize(struct fdisk_label *lb, int enable)
{
	struct fdisk_gpt_label *gpt = (struct fdisk_gpt_label *) lb;

	assert(gpt);
	gpt->minimize = enable ? 1 : 0;
}

#ifdef TEST_PROGRAM
static int test_getattr(struct fdisk_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	if (argc != 3)
		return -1;

	const char *disk = argv[1];
	size_t part = strtoul(argv[2], NULL, 0) - 1;
	struct fdisk_context *cxt;
	uint64_t atters = 0;

	cxt = fdisk_new_context();
	fdisk_assign_device(cxt, disk, 1);

	if (!fdisk_is_label(cxt, GPT))
		return EXIT_FAILURE;

	if (fdisk_gpt_get_partition_attrs(cxt, part, &atters))
		return EXIT_FAILURE;

	printf("%s: 0x%016" PRIx64 "\n", argv[2], atters);

	fdisk_unref_context(cxt);
	return 0;
}

static int test_setattr(struct fdisk_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	if (argc != 4)
		return -1;

	const char *disk = argv[1];
	size_t part = strtoul(argv[2], NULL, 0) - 1;
	uint64_t atters = strtoull(argv[3], NULL, 0);
	struct fdisk_context *cxt;

	cxt = fdisk_new_context();
	fdisk_assign_device(cxt, disk, 0);

	if (!fdisk_is_label(cxt, GPT))
		return EXIT_FAILURE;

	if (fdisk_gpt_set_partition_attrs(cxt, part, atters))
		return EXIT_FAILURE;

	if (fdisk_write_disklabel(cxt))
		return EXIT_FAILURE;

	fdisk_unref_context(cxt);
	return 0;
}

int main(int argc, char *argv[])
{
	struct fdisk_test tss[] = {
		{ "--getattr",  test_getattr,  "<disk> <partition>             print attributes" },
		{ "--setattr",  test_setattr,  "<disk> <partition> <value>     set attributes" },
		{ NULL }
	};

	return fdisk_run_test(tss, argc, argv);
}

#endif
