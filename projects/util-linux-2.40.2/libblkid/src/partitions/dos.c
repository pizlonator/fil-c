/*
 * MS-DOS partition parsing code
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * Inspired by fdisk, partx, Linux kernel and libparted.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"
#include "superblocks/superblocks.h"
#include "aix.h"

/* see superblocks/vfat.c */
extern int blkid_probe_is_vfat(blkid_probe pr);
/* see superblocks/exfat.c */
extern int blkid_probe_is_exfat(blkid_probe pr);

static const struct dos_subtypes {
	unsigned char type;
	const struct blkid_idinfo *id;
} dos_nested[] = {
	{ MBR_FREEBSD_PARTITION, &bsd_pt_idinfo },
	{ MBR_NETBSD_PARTITION, &bsd_pt_idinfo },
	{ MBR_OPENBSD_PARTITION, &bsd_pt_idinfo },
	{ MBR_UNIXWARE_PARTITION, &unixware_pt_idinfo },
	{ MBR_SOLARIS_X86_PARTITION, &solaris_x86_pt_idinfo },
	{ MBR_MINIX_PARTITION, &minix_pt_idinfo }
};

static inline int is_extended(const struct dos_partition *p)
{
	return (p->sys_ind == MBR_DOS_EXTENDED_PARTITION ||
		p->sys_ind == MBR_W95_EXTENDED_PARTITION ||
		p->sys_ind == MBR_LINUX_EXTENDED_PARTITION);
}

static int parse_dos_extended(blkid_probe pr, blkid_parttable tab,
		uint32_t ex_start, uint32_t ex_size, int ssf)
{
	blkid_partlist ls = blkid_probe_get_partlist(pr);
	uint32_t cur_start = ex_start, cur_size = ex_size;
	const unsigned char *data;
	int ct_nodata = 0;	/* count ext.partitions without data partitions */
	int i;

	DBG(LOWPROBE, ul_debug("parse EBR [start=%d, size=%d]", ex_start/ssf, ex_size/ssf));
	if (ex_start == 0) {
		DBG(LOWPROBE, ul_debug("Bad offset in primary extended partition -- ignore"));
		return 0;
	}

	while (1) {
		const struct dos_partition *p, *p0;
		uint32_t start = 0, size;

		if (++ct_nodata > 100)
			return BLKID_PROBE_OK;
		data = blkid_probe_get_sector(pr, cur_start);
		if (!data) {
			if (errno)
				return -errno;
			goto leave;	/* malformed partition? */
		}

		if (!mbr_is_valid_magic(data))
			goto leave;

		p0 = mbr_get_partition(data, 0);

		/* Usually, the first entry is the real data partition,
		 * the 2nd entry is the next extended partition, or empty,
		 * and the 3rd and 4th entries are unused.
		 * However, DRDOS sometimes has the extended partition as
		 * the first entry (when the data partition is empty),
		 * and OS/2 seems to use all four entries.
		 * -- Linux kernel fs/partitions/dos.c
		 *
		 * See also http://en.wikipedia.org/wiki/Extended_boot_record
		 */

		/* Parse data partition */
		for (p = p0, i = 0; i < 4; i++, p++) {
			uint32_t abs_start;
			blkid_partition par;

			/* the start is relative to the parental ext.partition */
			start = dos_partition_get_start(p) * ssf;
			size = dos_partition_get_size(p) * ssf;
			abs_start = cur_start + start;	/* absolute start */

			if (!size || is_extended(p))
				continue;
			if (i >= 2) {
				/* extra checks to detect real data on
				 * 3rd and 4th entries */
				if (start + size > cur_size)
					continue;
				if (abs_start < ex_start)
					continue;
				if (abs_start + size > ex_start + ex_size)
					continue;
			}

			/* Avoid recursive non-empty links, see ct_nodata counter */
			if (blkid_partlist_get_partition_by_start(ls, abs_start)) {
				DBG(LOWPROBE, ul_debug("#%d: EBR duplicate data partition [abs start=%u] -- ignore",
							i + 1, abs_start));
				continue;
			}

			par = blkid_partlist_add_partition(ls, tab, abs_start, size);
			if (!par)
				return -ENOMEM;

			blkid_partition_set_type(par, p->sys_ind);
			blkid_partition_set_flags(par, p->boot_ind);
			blkid_partition_gen_uuid(par);
			ct_nodata = 0;
		}
		/* The first nested ext.partition should be a link to the next
		 * logical partition. Everything other (recursive ext.partitions)
		 * is junk.
		 */
		for (p = p0, i = 0; i < 4; i++, p++) {
			start = dos_partition_get_start(p) * ssf;
			size = dos_partition_get_size(p) * ssf;

			if (size && is_extended(p)) {
				if (start == 0)
					DBG(LOWPROBE, ul_debug("#%d: EBR link offset is zero -- ignore", i + 1));
				else
					break;
			}
		}
		if (i == 4)
			goto leave;

		cur_start = ex_start + start;
		cur_size = size;
	}
leave:
	return BLKID_PROBE_OK;
}

static inline int is_lvm(blkid_probe pr)
{
	struct blkid_prval *v = __blkid_probe_lookup_value(pr, "TYPE");

	return (v && v->data && strcmp((char *) v->data, "LVM2_member") == 0);
}

static inline int is_empty_mbr(const unsigned char *mbr)
{
	const struct dos_partition *p = mbr_get_partition(mbr, 0);
	int i, nparts = 0;

	for (i = 0; i < 4; i++) {
		if (dos_partition_get_size(p) > 0)
			nparts++;
		p++;
	}

	return nparts == 0;
}

static int probe_dos_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	int i;
	int ssf;
	blkid_parttable tab = NULL;
	blkid_partlist ls;
	const struct dos_partition *p0, *p;
	const unsigned char *data;
	uint32_t start, size, id;
	char idstr[UUID_STR_LEN];


	data = blkid_probe_get_sector(pr, 0);
	if (!data) {
		if (errno)
			return -errno;
		goto nothing;
	}

	/* ignore disks with AIX magic number -- for more details see aix.c */
	if (memcmp(data, BLKID_AIX_MAGIC_STRING, BLKID_AIX_MAGIC_STRLEN) == 0)
		goto nothing;

	p0 = mbr_get_partition(data, 0);

	/*
	 * Reject PT where boot indicator is not 0 or 0x80.
	 */
	for (p = p0, i = 0; i < 4; i++, p++)
		if (p->boot_ind != 0 && p->boot_ind != 0x80) {
			DBG(LOWPROBE, ul_debug("missing boot indicator -- ignore"));
			goto nothing;
		}

	/*
	 * GPT uses valid MBR
	 */
	for (p = p0, i = 0; i < 4; i++, p++) {
		if (p->sys_ind == MBR_GPT_PARTITION) {
			DBG(LOWPROBE, ul_debug("probably GPT -- ignore"));
			goto nothing;
		}
	}

	/*
	 * Now that the 55aa signature is present, this is probably
	 * either the boot sector of a FAT filesystem or a DOS-type
	 * partition table.
	 */
	if (blkid_probe_is_vfat(pr) == 1 || blkid_probe_is_exfat(pr) == 1) {
		DBG(LOWPROBE, ul_debug("probably FAT -- ignore"));
		goto nothing;
	}

	/* Another false positive is NTFS */
	if (blkid_probe_is_ntfs(pr) == 1) {
		DBG(LOWPROBE, ul_debug("probably NTFS -- ignore"));
		goto nothing;
	}

	/*
	 * Ugly exception, if the device contains a valid LVM physical volume
	 * and empty MBR (=no partition defined) then it's LVM and MBR should
	 * be ignored. Crazy people use it to boot from LVM devices.
	 */
	if (is_lvm(pr) && is_empty_mbr(data)) {
		DBG(LOWPROBE, ul_debug("empty MBR on LVM device -- ignore"));
		goto nothing;
	}

	blkid_probe_use_wiper(pr, MBR_PT_OFFSET, 512 - MBR_PT_OFFSET);

	id = mbr_get_id(data);
	if (id)
		snprintf(idstr, sizeof(idstr), "%08x", id);

	/*
	 * Well, all checks pass, it's MS-DOS partition table
	 */
	if (blkid_partitions_need_typeonly(pr)) {
		/* Non-binary interface -- caller does not ask for details
		 * about partitions, just set generic variables only. */
		if (id)
			blkid_partitions_strcpy_ptuuid(pr, idstr);
		return 0;
	}

	ls = blkid_probe_get_partlist(pr);
	if (!ls)
		goto nothing;

	/* sector size factor (the start and size are in the real sectors, but
	 * we need to convert all sizes to 512 logical sectors
	 */
	ssf = blkid_probe_get_sectorsize(pr) / 512;

	/* allocate a new partition table */
	tab = blkid_partlist_new_parttable(ls, "dos", MBR_PT_OFFSET);
	if (!tab)
		return -ENOMEM;

	if (id)
		blkid_parttable_set_id(tab, (unsigned char *) idstr);

	/* Parse primary partitions */
	for (p = p0, i = 0; i < 4; i++, p++) {
		blkid_partition par;

		start = dos_partition_get_start(p) * ssf;
		size = dos_partition_get_size(p) * ssf;

		if (!size) {
			/* Linux kernel ignores empty partitions, but partno for
			 * the empty primary partitions is not reused */
			blkid_partlist_increment_partno(ls);
			continue;
		}
		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			return -ENOMEM;

		blkid_partition_set_type(par, p->sys_ind);
		blkid_partition_set_flags(par, p->boot_ind);
		blkid_partition_gen_uuid(par);
	}

	/* Linux uses partition numbers greater than 4
	 * for all logical partition and all nested partition tables (bsd, ..)
	 */
	blkid_partlist_set_partno(ls, 5);

	/* Parse logical partitions */
	for (p = p0, i = 0; i < 4; i++, p++) {
		start = dos_partition_get_start(p) * ssf;
		size = dos_partition_get_size(p) * ssf;

		if (!size)
			continue;
		if (is_extended(p) &&
		    parse_dos_extended(pr, tab, start, size, ssf) == -1)
			goto nothing;
	}

	/* Parse subtypes (nested partitions) on large disks */
	if (!blkid_probe_is_tiny(pr)) {
		int nparts = blkid_partlist_numof_partitions(ls);

		DBG(LOWPROBE, ul_debug("checking for subtypes"));

		for (i = 0; i < nparts; i++) {
			size_t n;
			int type;
			blkid_partition pa = blkid_partlist_get_partition(ls, i);

			if (pa == NULL
			    || blkid_partition_get_size(pa) == 0
			    || blkid_partition_is_extended(pa)
			    || blkid_partition_is_logical(pa))
				continue;

			type = blkid_partition_get_type(pa);

			for (n = 0; n < ARRAY_SIZE(dos_nested); n++) {
				int rc;

				if (dos_nested[n].type != type)
					continue;

				rc = blkid_partitions_do_subprobe(pr, pa,
							dos_nested[n].id);
				if (rc < 0)
					return rc;
				break;
			}
		}
	}
	return BLKID_PROBE_OK;

nothing:
	return BLKID_PROBE_NONE;
}


const struct blkid_idinfo dos_pt_idinfo =
{
	.name		= "dos",
	.probefunc	= probe_dos_pt,
	.magics		=
	{
		/* DOS master boot sector:
		 *
		 *     0 | Code Area
		 *   440 | Optional Disk signature
		 *   446 | Partition table
		 *   510 | 0x55
		 *   511 | 0xAA
		 */
		{ .magic = "\x55\xAA", .len = 2, .sboff = 510 },
		{ NULL }
	}
};

