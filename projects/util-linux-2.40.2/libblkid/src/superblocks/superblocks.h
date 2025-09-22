/*
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#ifndef _BLKID_SUPERBLOCKS_H
#define _BLKID_SUPERBLOCKS_H

#include "blkidP.h"

enum blkid_endianness {
	BLKID_ENDIANNESS_LITTLE,
	BLKID_ENDIANNESS_BIG,
};
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BLKID_ENDIANNESS_NATIVE BLKID_ENDIANNESS_LITTLE
#define BLKID_ENDIANNESS_OTHER BLKID_ENDIANNESS_BIG
#else
#define BLKID_ENDIANNESS_NATIVE BLKID_ENDIANNESS_BIG
#define BLKID_ENDIANNESS_OTHER BLKID_ENDIANNESS_LITTLE
#endif

extern const struct blkid_idinfo cramfs_idinfo;
extern const struct blkid_idinfo swap_idinfo;
extern const struct blkid_idinfo swsuspend_idinfo;
extern const struct blkid_idinfo adraid_idinfo;
extern const struct blkid_idinfo ddfraid_idinfo;
extern const struct blkid_idinfo iswraid_idinfo;
extern const struct blkid_idinfo jmraid_idinfo;
extern const struct blkid_idinfo lsiraid_idinfo;
extern const struct blkid_idinfo nvraid_idinfo;
extern const struct blkid_idinfo pdcraid_idinfo;
extern const struct blkid_idinfo silraid_idinfo;
extern const struct blkid_idinfo viaraid_idinfo;
extern const struct blkid_idinfo linuxraid_idinfo;
extern const struct blkid_idinfo ext4dev_idinfo;
extern const struct blkid_idinfo ext4_idinfo;
extern const struct blkid_idinfo ext3_idinfo;
extern const struct blkid_idinfo ext2_idinfo;
extern const struct blkid_idinfo jbd_idinfo;
extern const struct blkid_idinfo jfs_idinfo;
extern const struct blkid_idinfo xfs_idinfo;
extern const struct blkid_idinfo xfs_log_idinfo;
extern const struct blkid_idinfo exfs_idinfo;
extern const struct blkid_idinfo gfs_idinfo;
extern const struct blkid_idinfo gfs2_idinfo;
extern const struct blkid_idinfo romfs_idinfo;
extern const struct blkid_idinfo ocfs_idinfo;
extern const struct blkid_idinfo ocfs2_idinfo;
extern const struct blkid_idinfo oracleasm_idinfo;
extern const struct blkid_idinfo reiser_idinfo;
extern const struct blkid_idinfo reiser4_idinfo;
extern const struct blkid_idinfo hfs_idinfo;
extern const struct blkid_idinfo hfsplus_idinfo;
extern const struct blkid_idinfo ntfs_idinfo;
extern const struct blkid_idinfo refs_idinfo;
extern const struct blkid_idinfo iso9660_idinfo;
extern const struct blkid_idinfo udf_idinfo;
extern const struct blkid_idinfo vxfs_idinfo;
extern const struct blkid_idinfo minix_idinfo;
extern const struct blkid_idinfo vfat_idinfo;
extern const struct blkid_idinfo ufs_idinfo;
extern const struct blkid_idinfo hpfs_idinfo;
extern const struct blkid_idinfo lvm2_idinfo;
extern const struct blkid_idinfo lvm1_idinfo;
extern const struct blkid_idinfo snapcow_idinfo;
extern const struct blkid_idinfo verity_hash_idinfo;
extern const struct blkid_idinfo integrity_idinfo;
extern const struct blkid_idinfo luks_idinfo;
extern const struct blkid_idinfo highpoint37x_idinfo;
extern const struct blkid_idinfo highpoint45x_idinfo;
extern const struct blkid_idinfo squashfs_idinfo;
extern const struct blkid_idinfo squashfs3_idinfo;
extern const struct blkid_idinfo netware_idinfo;
extern const struct blkid_idinfo sysv_idinfo;
extern const struct blkid_idinfo xenix_idinfo;
extern const struct blkid_idinfo btrfs_idinfo;
extern const struct blkid_idinfo ubi_idinfo;
extern const struct blkid_idinfo ubifs_idinfo;
extern const struct blkid_idinfo zfs_idinfo;
extern const struct blkid_idinfo bfs_idinfo;
extern const struct blkid_idinfo vmfs_volume_idinfo;
extern const struct blkid_idinfo vmfs_fs_idinfo;
extern const struct blkid_idinfo bluestore_idinfo;
extern const struct blkid_idinfo drbd_idinfo;
extern const struct blkid_idinfo drbdmanage_idinfo;
extern const struct blkid_idinfo drbdproxy_datalog_idinfo;
extern const struct blkid_idinfo befs_idinfo;
extern const struct blkid_idinfo nilfs2_idinfo;
extern const struct blkid_idinfo exfat_idinfo;
extern const struct blkid_idinfo f2fs_idinfo;
extern const struct blkid_idinfo bcache_idinfo;
extern const struct blkid_idinfo bcachefs_idinfo;
extern const struct blkid_idinfo mpool_idinfo;
extern const struct blkid_idinfo vdo_idinfo;
extern const struct blkid_idinfo stratis_idinfo;
extern const struct blkid_idinfo bitlocker_idinfo;
extern const struct blkid_idinfo apfs_idinfo;
extern const struct blkid_idinfo zonefs_idinfo;
extern const struct blkid_idinfo erofs_idinfo;
extern const struct blkid_idinfo cs_fvault2_idinfo;

/*
 * superblock functions
 */
extern int blkid_probe_set_version(blkid_probe pr, const char *version);
extern int blkid_probe_sprintf_version(blkid_probe pr, const char *fmt, ...)
		__attribute__ ((__format__ (__printf__, 2, 3)));

extern int blkid_probe_set_label(blkid_probe pr, const unsigned char *label, size_t len);
extern int blkid_probe_set_utf8label(blkid_probe pr, const unsigned char *label,
                size_t len, int enc);
extern int blkid_probe_sprintf_uuid(blkid_probe pr, const unsigned char *uuid,
                size_t len, const char *fmt, ...)
		__attribute__ ((__format__ (__printf__, 4, 5)));
extern int blkid_probe_strncpy_uuid(blkid_probe pr, const unsigned char *str, size_t len);

extern int blkid_probe_set_uuid(blkid_probe pr, const unsigned char *uuid);
extern int blkid_probe_set_uuid_as(blkid_probe pr, const unsigned char *uuid, const char *name);

extern int blkid_probe_set_id_label(blkid_probe pr, const char *name,
			     const unsigned char *data, size_t len);
extern int blkid_probe_set_utf8_id_label(blkid_probe pr, const char *name,
			     const unsigned char *data, size_t len, int enc);

int blkid_probe_set_block_size(blkid_probe pr, unsigned block_size);
int blkid_probe_set_fssize(blkid_probe pr, uint64_t size);
int blkid_probe_set_fslastblock(blkid_probe pr, uint64_t lastblock);
int blkid_probe_set_fsblocksize(blkid_probe pr, uint32_t block_size);
int blkid_probe_set_fsendianness(blkid_probe pr, enum blkid_endianness endianness);

extern int blkid_probe_is_bitlocker(blkid_probe pr);
extern int blkid_probe_is_ntfs(blkid_probe pr);

/*
 * utility functions
 */
static inline int blkid32_to_cpu(enum blkid_endianness e, uint32_t i)
{
	if (e == BLKID_ENDIANNESS_LITTLE)
		return le32_to_cpu(i);
	else if (e == BLKID_ENDIANNESS_BIG)
		return be32_to_cpu(i);
	abort();
}

#endif /* _BLKID_SUPERBLOCKS_H */
