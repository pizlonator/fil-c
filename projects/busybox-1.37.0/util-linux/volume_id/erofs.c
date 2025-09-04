/*
 * volume_id - reads filesystem label and uuid
 *
 * Copyright (C) 2020 Norbert Lange <nolange79@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config FEATURE_VOLUMEID_EROFS
//config:	bool "erofs filesystem"
//config:	default y
//config:	depends on VOLUMEID
//config:	help
//config:	Erofs is a compressed readonly filesystem for Linux.

//kbuild:lib-$(CONFIG_FEATURE_VOLUMEID_EROFS) += erofs.o

#include "volume_id_internal.h"

#define EROFS_SUPER_MAGIC_V1    0xE0F5E1E2
#define EROFS_SUPER_OFFSET      1024
#define EROFS_FEATURE_COMPAT_SB_CHKSUM		0x00000001

/* 128-byte erofs on-disk super block */
struct erofs_super_block {
	uint32_t magic;           /* file system magic number */
	uint32_t checksum;        /* crc32c(super_block) */
	uint32_t feature_compat;
	uint8_t blkszbits;         /* support block_size == PAGE_SIZE only */
	uint8_t reserved;

	uint16_t root_nid;	/* nid of root directory */
	uint64_t inos;            /* total valid ino # (== f_files - f_favail) */

	uint64_t build_time;      /* inode v1 time derivation */
	uint32_t build_time_nsec;	/* inode v1 time derivation in nano scale */
	uint32_t blocks;          /* used for statfs */
	uint32_t meta_blkaddr;	/* start block address of metadata area */
	uint32_t xattr_blkaddr;	/* start block address of shared xattr area */
	uint8_t uuid[16];          /* 128-bit uuid for volume */
	uint8_t volume_name[16];   /* volume name */
	uint32_t feature_incompat;
	uint8_t reserved2[44];
} PACKED;

int FAST_FUNC volume_id_probe_erofs(struct volume_id *id /*,uint64_t off*/)
{
	struct erofs_super_block *sb;

	BUILD_BUG_ON(sizeof(struct erofs_super_block) != 128);

	dbg("erofs: probing at offset 0x%llx", EROFS_SUPER_OFFSET);
	sb = volume_id_get_buffer(id, EROFS_SUPER_OFFSET, sizeof(*sb));
	if (!sb)
		return -1;

	if (sb->magic != cpu_to_le32(EROFS_SUPER_MAGIC_V1))
		return -1;

	IF_FEATURE_BLKID_TYPE(id->type = "erofs");

	volume_id_set_label_string(id, sb->volume_name,
		MIN(sizeof(sb->volume_name), VOLUME_ID_LABEL_SIZE));

	volume_id_set_uuid(id, sb->uuid, UUID_DCE);

	return 0;
}
