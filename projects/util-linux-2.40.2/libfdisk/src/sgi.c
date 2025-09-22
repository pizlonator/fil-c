/*
 *
 * Copyright (C) 2012 Davidlohr Bueso <dave@gnu.org>
 *               2013 Karel Zak <kzak@redhat.com>
 *
 * This is a re-written version for libfdisk, the original was fdisksgilabel.c
 * from util-linux fdisk, by:
 *
 *               Andreas Neuper, Sep 1998,
 *               Arnaldo Carvalho de Melo <acme@conectiva.com.br>, Mar 1999,
 *               Phillip Kesling <pkesling@sgi.com>, Mar 2003.
 */

#include "c.h"
#include "all-io.h"

#include "blkdev.h"

#include "bitops.h"
#include "pt-sgi.h"
#include "pt-mbr.h"
#include "fdiskP.h"

/**
 * SECTION: sgi
 * @title: SGI
 * @short_description: disk label specific functions
 *
 */

/*
 * in-memory fdisk SGI stuff
 */
struct fdisk_sgi_label {
	struct fdisk_label	head;		/* generic fdisk part */
	struct sgi_disklabel	*header;	/* on-disk data (pointer to cxt->firstsector) */

	struct sgi_freeblocks {
		unsigned int first;
		unsigned int last;
	} freelist[SGI_MAXPARTITIONS + 1];
};

static const struct fdisk_parttype sgi_parttypes[] =
{
	{SGI_TYPE_VOLHDR,	N_("SGI volhdr")},
	{SGI_TYPE_TRKREPL,	N_("SGI trkrepl")},
	{SGI_TYPE_SECREPL,	N_("SGI secrepl")},
	{SGI_TYPE_SWAP,		N_("SGI raw")},
	{SGI_TYPE_BSD,		N_("SGI bsd")},
	{SGI_TYPE_SYSV,		N_("SGI sysv")},
	{SGI_TYPE_ENTIRE_DISK,	N_("SGI volume")},
	{SGI_TYPE_EFS,		N_("SGI efs")},
	{SGI_TYPE_LVOL,		N_("SGI lvol")},
	{SGI_TYPE_RLVOL,	N_("SGI rlvol")},
	{SGI_TYPE_XFS,		N_("SGI xfs")},
	{SGI_TYPE_XFSLOG,	N_("SGI xfslog")},
	{SGI_TYPE_XLV,		N_("SGI xlv")},
	{SGI_TYPE_XVM,		N_("SGI xvm")},
	{MBR_LINUX_SWAP_PARTITION, N_("Linux swap")},
	{MBR_LINUX_DATA_PARTITION, N_("Linux native")},
	{MBR_LINUX_LVM_PARTITION, N_("Linux LVM")},
	{MBR_LINUX_RAID_PARTITION, N_("Linux RAID")},
	{0, NULL }
};

static unsigned int sgi_get_start_sector(struct fdisk_context *cxt, int i );
static unsigned int sgi_get_num_sectors(struct fdisk_context *cxt, int i );
static int sgi_get_bootpartition(struct fdisk_context *cxt);
static int sgi_get_swappartition(struct fdisk_context *cxt);

/* Returns a pointer buffer with on-disk data. */
static inline struct sgi_disklabel *self_disklabel(struct fdisk_context *cxt)
{
	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	return ((struct fdisk_sgi_label *) cxt->label)->header;
}

/* Returns in-memory fdisk data. */
static inline struct fdisk_sgi_label *self_label(struct fdisk_context *cxt)
{
	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	return (struct fdisk_sgi_label *) cxt->label;
}

/*
 * Information within second on-disk block
 */
#define	SGI_INFO_MAGIC		0x00072959

struct sgi_info {
	unsigned int   magic;		/* looks like a magic number */
	unsigned int   a2;
	unsigned int   a3;
	unsigned int   a4;
	unsigned int   b1;
	unsigned short b2;
	unsigned short b3;
	unsigned int   c[16];
	unsigned short d[3];
	unsigned char  scsi_string[50];
	unsigned char  serial[137];
	unsigned short check1816;
	unsigned char  installer[225];
};

static struct sgi_info *sgi_new_info(void)
{
	struct sgi_info *info = calloc(1, sizeof(struct sgi_info));

	if (!info)
		return NULL;

	info->magic = cpu_to_be32(SGI_INFO_MAGIC);
	info->b1 = cpu_to_be32(-1);
	info->b2 = cpu_to_be16(-1);
	info->b3 = cpu_to_be16(1);

	/* You may want to replace this string !!!!!!! */
	strncpy((char *) info->scsi_string, "IBM OEM 0662S12         3 30",
		sizeof(info->scsi_string));
	strncpy((char *) info->serial, "0000", sizeof(info->serial));
	info->check1816 = cpu_to_be16(18 * 256 + 16);
	strncpy((char *) info->installer, "Sfx version 5.3, Oct 18, 1994",
		sizeof(info->installer));

	return info;
}

static void sgi_free_info(struct sgi_info *info)
{
	free(info);
}

/**
 * fdisk_sgi_create_info:
 * @cxt: context
 *
 * This function add hint about SGI label (e.g. set "sgilabel" as volume name)
 * to the first SGI volume. This is probably old SGI convention without any
 * effect to the device partitioning.
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_sgi_create_info(struct fdisk_context *cxt)
{
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);

	/* I keep SGI's habit to write the sgilabel to the second block */
	sgilabel->volume[0].block_num = cpu_to_be32(2);
	sgilabel->volume[0].num_bytes = cpu_to_be32(sizeof(struct sgi_info));
	memcpy((char *) sgilabel->volume[0].name, "sgilabel", 8);

	fdisk_info(cxt, _("SGI info created on second sector."));
	return 0;
}


/*
 * only dealing with free blocks here
 */
static void set_freelist(struct fdisk_context *cxt,
		size_t i, unsigned int f, unsigned int l)
{
	struct fdisk_sgi_label *sgi = self_label(cxt);

	if (i < ARRAY_SIZE(sgi->freelist)) {
		sgi->freelist[i].first = f;
		sgi->freelist[i].last = l;
	}
}

static void add_to_freelist(struct fdisk_context *cxt,
		unsigned int f, unsigned int l)
{
	struct fdisk_sgi_label *sgi = self_label(cxt);
	size_t i;

	for (i = 0; i < ARRAY_SIZE(sgi->freelist); i++) {
		if (sgi->freelist[i].last == 0)
			break;
	}
	set_freelist(cxt, i, f, l);
}

static void clear_freelist(struct fdisk_context *cxt)
{
	struct fdisk_sgi_label *sgi = self_label(cxt);

	memset(sgi->freelist, 0, sizeof(sgi->freelist));
}

static unsigned int is_in_freelist(struct fdisk_context *cxt, unsigned int b)
{
	struct fdisk_sgi_label *sgi = self_label(cxt);
	size_t i;

	for (i = 0; i < ARRAY_SIZE(sgi->freelist); i++) {
		if (sgi->freelist[i].first <= b
		    && sgi->freelist[i].last >= b)
			return sgi->freelist[i].last;
	}

	return 0;
}


static int sgi_get_nsect(struct fdisk_context *cxt)
{
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);
	return be16_to_cpu(sgilabel->devparam.nsect);
}

static int sgi_get_ntrks(struct fdisk_context *cxt)
{
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);
	return be16_to_cpu(sgilabel->devparam.ntrks);
}

static size_t count_used_partitions(struct fdisk_context *cxt)
{
	size_t i, ct = 0;

	for (i = 0; i < cxt->label->nparts_max; i++)
		ct += sgi_get_num_sectors(cxt, i) > 0;

	return ct;
}

static int sgi_probe_label(struct fdisk_context *cxt)
{
	struct fdisk_sgi_label *sgi;
	struct sgi_disklabel *sgilabel;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));
	assert(sizeof(struct sgi_disklabel) <= 512);

	/* map first sector to header */
	sgi = (struct fdisk_sgi_label *) cxt->label;
	sgi->header = (struct sgi_disklabel *) cxt->firstsector;
	sgilabel = sgi->header;

	if (be32_to_cpu(sgilabel->magic) != SGI_LABEL_MAGIC) {
		sgi->header = NULL;
		return 0;
	}

	/*
	 * test for correct checksum
	 */
	if (sgi_pt_checksum(sgilabel) != 0)
		fdisk_warnx(cxt, _("Detected an SGI disklabel with wrong checksum."));

	clear_freelist(cxt);
	cxt->label->nparts_max = SGI_MAXPARTITIONS;
	cxt->label->nparts_cur = count_used_partitions(cxt);
	return 1;
}

static int sgi_get_disklabel_item(struct fdisk_context *cxt, struct fdisk_labelitem *item)
{
	struct sgi_disklabel *sgilabel;
	struct sgi_device_parameter *sgiparam;
	int rc = 0;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	sgilabel = self_disklabel(cxt);
	sgiparam = &sgilabel->devparam;

	switch (item->id) {
	case SGI_LABELITEM_PCYLCOUNT:
		item->name = _("Physical cylinders");
		item->type = 'j';
		item->data.num64 = (uint64_t) be16_to_cpu(sgiparam->pcylcount);
		break;
	case SGI_LABELITEM_SPARECYL:
		item->name = _("Extra sects/cyl");
		item->type = 'j';
		item->data.num64 = (uint64_t) sgiparam->sparecyl;
		break;
	case SGI_LABELITEM_ILFACT:
		item->name = _("Interleave");
		item->type = 'j';
		item->data.num64 = (uint64_t) be16_to_cpu(sgiparam->ilfact);
		break;
	case SGI_LABELITEM_BOOTFILE:
		item->name = _("Bootfile");
		item->type = 's';
		item->data.str = *sgilabel->boot_file ? strdup((char *) sgilabel->boot_file) : NULL;
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

static unsigned int sgi_get_start_sector(struct fdisk_context *cxt, int i)
{
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);
	return be32_to_cpu(sgilabel->partitions[i].first_block);
}

static unsigned int sgi_get_num_sectors(struct fdisk_context *cxt, int i)
{
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);
	return be32_to_cpu(sgilabel->partitions[i].num_blocks);
}

static int sgi_get_sysid(struct fdisk_context *cxt, int i)
{
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);
	return be32_to_cpu(sgilabel->partitions[i].type);
}

static int sgi_get_bootpartition(struct fdisk_context *cxt)
{
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);
	return be16_to_cpu(sgilabel->root_part_num);
}

static int sgi_get_swappartition(struct fdisk_context *cxt)
{
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);
	return be16_to_cpu(sgilabel->swap_part_num);
}

static unsigned int sgi_get_lastblock(struct fdisk_context *cxt)
{
	return cxt->geom.heads * cxt->geom.sectors * cxt->geom.cylinders;
}

static struct fdisk_parttype *sgi_get_parttype(struct fdisk_context *cxt, size_t n)
{
	struct fdisk_parttype *t;

	if (n >= cxt->label->nparts_max)
		return NULL;

	t = fdisk_label_get_parttype_from_code(cxt->label, sgi_get_sysid(cxt, n));
	return t ? : fdisk_new_unknown_parttype(sgi_get_sysid(cxt, n), NULL);
}

/* fdisk_get_partition() backend */
static int sgi_get_partition(struct fdisk_context *cxt, size_t n, struct fdisk_partition *pa)
{
	fdisk_sector_t start, len;

	pa->used = sgi_get_num_sectors(cxt, n) > 0;
	if (!pa->used)
		return 0;

	start = sgi_get_start_sector(cxt, n);
	len = sgi_get_num_sectors(cxt, n);

	pa->type = sgi_get_parttype(cxt, n);
	pa->size = len;
	pa->start = start;

	if (pa->type && pa->type->code == SGI_TYPE_ENTIRE_DISK)
		pa->wholedisk = 1;

	pa->attrs = sgi_get_swappartition(cxt) == (int) n ? "swap" :
		    sgi_get_bootpartition(cxt) == (int) n ? "boot" : NULL;
	if (pa->attrs)
		pa->attrs = strdup(pa->attrs);

	return 0;
}


static int sgi_check_bootfile(struct fdisk_context *cxt, const char *name)
{
	size_t sz;
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);

	sz = strlen(name);

	if (sz < 3) {
		/* "/a\n" is minimum */
		fdisk_warnx(cxt, _("Invalid bootfile!  The bootfile must "
				   "be an absolute non-zero pathname, "
				   "e.g. \"/unix\" or \"/unix.save\"."));
		return -EINVAL;

	}

	if (sz > sizeof(sgilabel->boot_file)) {
		fdisk_warnx(cxt, P_("Name of bootfile is too long: %zu byte maximum.",
				    "Name of bootfile is too long: %zu bytes maximum.",
				    sizeof(sgilabel->boot_file)),
			    sizeof(sgilabel->boot_file));
		return -EINVAL;

	}

	if (*name != '/') {
		fdisk_warnx(cxt, _("Bootfile must have a fully qualified pathname."));
		return -EINVAL;
	}

	if (strncmp(name, (char *) sgilabel->boot_file,
				sizeof(sgilabel->boot_file)) != 0) {
		fdisk_warnx(cxt, _("Be aware that the bootfile is not checked "
				   "for existence.  SGI's default is \"/unix\", "
				   "and for backup \"/unix.save\"."));
		return 0;	/* filename is correct and did change */
	}

	return 1;	/* filename did not change */
}

/**
 * fdisk_sgi_set_bootfile:
 * @cxt: context
 *
 * Allows to set SGI boot file. The function uses Ask API for dialog with
 * user.
 *
 * Returns: 0 on success, <0 on error
 */
int fdisk_sgi_set_bootfile(struct fdisk_context *cxt)
{
	int rc = 0;
	size_t sz;
	char *name = NULL;
	struct sgi_disklabel *sgilabel = self_disklabel(cxt);

	fdisk_info(cxt, _("The current boot file is: %s"), sgilabel->boot_file);

	rc = fdisk_ask_string(cxt, _("Enter of the new boot file"), &name);
	if (rc == 0)
		rc = sgi_check_bootfile(cxt, name);
	if (rc) {
		if (rc == 1)
			fdisk_info(cxt, _("Boot file is unchanged."));
		goto done;
	}

	memset(sgilabel->boot_file, 0, sizeof(sgilabel->boot_file));
	sz = strlen(name);

	assert(sz <= sizeof(sgilabel->boot_file));	/* see sgi_check_bootfile() */

	memcpy(sgilabel->boot_file, name, sz);

	fdisk_info(cxt, _("Bootfile has been changed to \"%s\"."), name);
done:
	free(name);
	return rc;
}

static int sgi_write_disklabel(struct fdisk_context *cxt)
{
	struct sgi_disklabel *sgilabel;
	struct sgi_info *info = NULL;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	sgilabel = self_disklabel(cxt);
	sgilabel->csum = 0;
	sgilabel->csum = cpu_to_be32(sgi_pt_checksum(sgilabel));

	assert(sgi_pt_checksum(sgilabel) == 0);

	if (lseek(cxt->dev_fd, 0, SEEK_SET) < 0)
		goto err;
	if (write_all(cxt->dev_fd, sgilabel, DEFAULT_SECTOR_SIZE))
		goto err;
	if (!strncmp((char *) sgilabel->volume[0].name, "sgilabel", 8)) {
		/*
		 * Keep this habit of first writing the "sgilabel".
		 * I never tested whether it works without. (AN 1998-10-02)
		 */
		int infostartblock
			= be32_to_cpu(sgilabel->volume[0].block_num);

		if (lseek(cxt->dev_fd, (off_t) infostartblock *
					DEFAULT_SECTOR_SIZE, SEEK_SET) < 0)
			goto err;
		info = sgi_new_info();
		if (!info)
			goto err;
		if (write_all(cxt->dev_fd, info, sizeof(*info)))
			goto err;
	}

	sgi_free_info(info);
	return 0;
err:
	sgi_free_info(info);
	return -errno;
}

static int compare_start(struct fdisk_context *cxt,
			 const void *x, const void *y)
{
	/*
	 * Sort according to start sectors and prefer the largest partition:
	 * entry zero is the entire-disk entry.
	 */
	const unsigned int i = *(const int *) x;
	const unsigned int j = *(const int *) y;
	unsigned int a = sgi_get_start_sector(cxt, i);
	unsigned int b = sgi_get_start_sector(cxt, j);
	unsigned int c = sgi_get_num_sectors(cxt, i);
	unsigned int d = sgi_get_num_sectors(cxt, j);

	if (a == b)
		return (d > c) ? 1 : (d == c) ? 0 : -1;
	return (a > b) ? 1 : -1;
}

static void generic_swap(void *a0, void *b0, int size)
{
	char *a = a0, *b = b0;

	for (; size > 0; --size, a++, b++) {
		char t = *a;
		*a = *b;
		*b = t;
	}
}


/* heap sort, based on Matt Mackall's linux kernel version */
static void sort(void *base0, size_t num, size_t size, struct fdisk_context *cxt,
		 int (*cmp_func)(struct fdisk_context *, const void *, const void *))
{
	/* pre-scale counters for performance */
	int i = (num/2 - 1) * size;
	size_t n = num * size, c, r;
	char *base = base0;

	/* heapify */
	for ( ; i >= 0; i -= size) {
		for (r = i; r * 2 + size < n; r  = c) {
			c = r * 2 + size;
			if (c < n - size &&
			    cmp_func(cxt, base + c, base + c + size) < 0)
				c += size;
			if (cmp_func(cxt, base + r, base + c) >= 0)
				break;
			generic_swap(base + r, base + c, size);
		}
	}

	/* sort */
	for (i = n - size; i > 0; i -= size) {
		generic_swap(base, base + i, size);
		for (r = 0; r * 2 + size < (size_t) i; r = c) {
			c = r * 2 + size;
			if (c < i - size &&
			    cmp_func(cxt, base + c, base + c + size) < 0)
				c += size;
			if (cmp_func(cxt, base + r, base + c) >= 0)
				break;
			generic_swap(base + r, base + c, size);
		}
	}
}

static int verify_disklabel(struct fdisk_context *cxt, int verbose)
{
	int Index[SGI_MAXPARTITIONS];	/* list of valid partitions */
	int sortcount = 0;		/* number of used partitions, i.e. non-zero lengths */
	int entire = 0, i = 0;
	unsigned int start = 0;
	long long gap = 0;	/* count unused blocks */
	unsigned int lastblock = sgi_get_lastblock(cxt);

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	clear_freelist(cxt);
	memset(Index, 0, sizeof(Index));

	for (i=0; i < SGI_MAXPARTITIONS; i++) {
		if (sgi_get_num_sectors(cxt, i) != 0) {
			Index[sortcount++] = i;
			if (sgi_get_sysid(cxt, i) == SGI_TYPE_ENTIRE_DISK
			    && entire++ == 1 && verbose) {
				fdisk_info(cxt, _("More than one entire "
						"disk entry present."));
			}
		}
	}
	if (sortcount == 0) {
		if (verbose)
			fdisk_info(cxt, _("No partitions defined."));
		if (lastblock)
			add_to_freelist(cxt, 0, lastblock);
		return (lastblock > 0) ? 1 : (lastblock == 0) ? 0 : -1;
	}

	sort(Index, sortcount, sizeof(Index[0]), cxt, compare_start);

	if (sgi_get_sysid(cxt, Index[0]) == SGI_TYPE_ENTIRE_DISK) {
		if (verbose && Index[0] != 10)
			fdisk_info(cxt, _("IRIX likes it when partition 11 "
					  "covers the entire disk."));

		if (verbose && sgi_get_start_sector(cxt, Index[0]) != 0)
			fdisk_info(cxt, _("The entire disk partition should "
					  "start at block 0, not at block %d."),
				   sgi_get_start_sector(cxt, Index[0]));

		if (verbose && sgi_get_num_sectors(cxt, Index[0]) != lastblock)
			DBG(LABEL, ul_debug(
				"entire disk partition=%ds, but disk=%ds",
				sgi_get_num_sectors(cxt, Index[0]),
				lastblock));
		lastblock = sgi_get_num_sectors(cxt, Index[0]);
	} else if (verbose) {
		fdisk_info(cxt, _("Partition 11 should cover the entire disk."));
		DBG(LABEL, ul_debug("sysid=%d\tpartition=%d",
			       sgi_get_sysid(cxt, Index[0]), Index[0]+1));
	}
	for (i=1, start=0; i<sortcount; i++) {
		int cylsize = sgi_get_nsect(cxt) * sgi_get_ntrks(cxt);

		if (verbose && cylsize
		    && (sgi_get_start_sector(cxt, Index[i]) % cylsize) != 0)
			DBG(LABEL, ul_debug("partition %d does not start on "
					"cylinder boundary.", Index[i]+1));

		if (verbose && cylsize
		    && sgi_get_num_sectors(cxt, Index[i]) % cylsize != 0)
			DBG(LABEL, ul_debug("partition %d does not end on "
					"cylinder boundary.", Index[i]+1));

		/* We cannot handle several "entire disk" entries. */
		if (sgi_get_sysid(cxt, Index[i]) == SGI_TYPE_ENTIRE_DISK)
			continue;

		if (start > sgi_get_start_sector(cxt, Index[i])) {
			if (verbose)
				fdisk_info(cxt,
					   P_("Partitions %d and %d overlap by %d sector.",
					      "Partitions %d and %d overlap by %d sectors.",
					      start - sgi_get_start_sector(cxt, Index[i])),
					   Index[i-1]+1, Index[i]+1,
					   start - sgi_get_start_sector(cxt, Index[i]));
			if (gap >  0) gap = -gap;
			if (gap == 0) gap = -1;
		}
		if (start < sgi_get_start_sector(cxt, Index[i])) {
			if (verbose)
				fdisk_info(cxt,
					   P_("Unused gap of %8u sector: sector %8u",
					      "Unused gap of %8u sectors: sectors %8u-%u",
					      sgi_get_start_sector(cxt, Index[i]) - start),
					   sgi_get_start_sector(cxt, Index[i]) - start,
					   start, sgi_get_start_sector(cxt, Index[i])-1);
			gap += sgi_get_start_sector(cxt, Index[i]) - start;
			add_to_freelist(cxt, start,
					sgi_get_start_sector(cxt, Index[i]));
		}
		start = sgi_get_start_sector(cxt, Index[i])
			+ sgi_get_num_sectors(cxt, Index[i]);
		/* Align free space on cylinder boundary. */
		if (cylsize && start % cylsize)
			start += cylsize - (start % cylsize);

		DBG(LABEL, ul_debug("%2d:%12d\t%12d\t%12d", Index[i],
				       sgi_get_start_sector(cxt, Index[i]),
				       sgi_get_num_sectors(cxt, Index[i]),
				       sgi_get_sysid(cxt, Index[i])));
	}
	if (start < lastblock) {
		if (verbose)
			fdisk_info(cxt, P_("Unused gap of %8u sector: sector %8u",
					   "Unused gap of %8u sectors: sectors %8u-%u",
					   lastblock - start),
				   lastblock - start, start, lastblock-1);
		gap += lastblock - start;
		add_to_freelist(cxt, start, lastblock);
	}
	/*
	 * Done with arithmetic. Go for details now.
	 */
	if (verbose) {
		if (sgi_get_bootpartition(cxt) < 0
		    || !sgi_get_num_sectors(cxt, sgi_get_bootpartition(cxt)))
			fdisk_info(cxt, _("The boot partition does not exist."));

		if (sgi_get_swappartition(cxt) < 0
		    || !sgi_get_num_sectors(cxt, sgi_get_swappartition(cxt)))
			fdisk_info(cxt, _("The swap partition does not exist."));

		else if (sgi_get_sysid(cxt, sgi_get_swappartition(cxt)) != SGI_TYPE_SWAP
		    && sgi_get_sysid(cxt, sgi_get_swappartition(cxt)) != MBR_LINUX_SWAP_PARTITION)
			fdisk_info(cxt, _("The swap partition has no swap type."));

		if (sgi_check_bootfile(cxt, "/unix"))
			fdisk_info(cxt, _("You have chosen an unusual bootfile name."));
	}

	return (gap > 0) ? 1 : (gap == 0) ? 0 : -1;
}

static int sgi_verify_disklabel(struct fdisk_context *cxt)
{
	return verify_disklabel(cxt, 1);
}

static int sgi_gaps(struct fdisk_context *cxt)
{
	/*
	 * returned value is:
	 *  = 0 : disk is properly filled to the rim
	 *  < 0 : there is an overlap
	 *  > 0 : there is still some vacant space
	 */
	return verify_disklabel(cxt, 0);
}

/* Returns partition index of first entry marked as entire disk. */
static int sgi_entire(struct fdisk_context *cxt)
{
	size_t i;

	for (i = 0; i < SGI_MAXPARTITIONS; i++)
		if (sgi_get_sysid(cxt, i) == SGI_TYPE_ENTIRE_DISK)
			return i;
	return -1;
}

static int set_partition(struct fdisk_context *cxt, size_t i,
			     unsigned int start, unsigned int length, int sys)
{
	struct sgi_disklabel *sgilabel;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	sgilabel = self_disklabel(cxt);
	sgilabel->partitions[i].type = cpu_to_be32(sys);
	sgilabel->partitions[i].num_blocks = cpu_to_be32(length);
	sgilabel->partitions[i].first_block = cpu_to_be32(start);

	fdisk_label_set_changed(cxt->label, 1);

	if (sgi_gaps(cxt) < 0)	/* rebuild freelist */
		fdisk_warnx(cxt, _("Partition overlap on the disk."));
	if (length) {
		struct fdisk_parttype *t =
				fdisk_label_get_parttype_from_code(cxt->label, sys);
		fdisk_info_new_partition(cxt, i + 1, start, start + length, t);
	}

	return 0;
}

static void sgi_set_entire(struct fdisk_context *cxt)
{
	size_t n;

	for (n = 10; n < cxt->label->nparts_max; n++) {
		if (!sgi_get_num_sectors(cxt, n)) {
			set_partition(cxt, n, 0, sgi_get_lastblock(cxt), SGI_TYPE_ENTIRE_DISK);
			break;
		}
	}
}

static void sgi_set_volhdr(struct fdisk_context *cxt)
{
	size_t n;

	for (n = 8; n < cxt->label->nparts_max; n++) {
		if (!sgi_get_num_sectors(cxt, n)) {
			/* Choose same default volume header size as IRIX fx uses. */
			if (4096 < sgi_get_lastblock(cxt))
				set_partition(cxt, n, 0, 4096, SGI_TYPE_VOLHDR);
			break;
		}
	}
}

static int sgi_delete_partition(struct fdisk_context *cxt, size_t partnum)
{
	int rc;

	assert(cxt);
	assert(cxt->label);

	if (partnum > cxt->label->nparts_max)
		return -EINVAL;

	rc = set_partition(cxt, partnum, 0, 0, 0);

	cxt->label->nparts_cur = count_used_partitions(cxt);

	return rc;
}

static int sgi_add_partition(struct fdisk_context *cxt,
			     struct fdisk_partition *pa,
			     size_t *partno)
{
	struct fdisk_sgi_label *sgi;
	char mesg[256];
	unsigned int first = 0, last = 0;
	struct fdisk_ask *ask;
	int sys = pa && pa->type ? pa->type->code : SGI_TYPE_XFS;
	int rc;
	size_t n;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	rc = fdisk_partition_next_partno(pa, cxt, &n);
	if (rc)
		return rc;
	if (n == 10)
		sys = SGI_TYPE_ENTIRE_DISK;
	else if (n == 8)
		sys = 0;

	sgi = self_label(cxt);

	if (sgi_get_num_sectors(cxt, n)) {
		fdisk_warnx(cxt, _("Partition %zu is already defined.  "
				   "Delete it before re-adding it."), n + 1);
		return -EINVAL;
	}
	if (!cxt->script && sgi_entire(cxt) == -1 &&  sys != SGI_TYPE_ENTIRE_DISK) {
		fdisk_info(cxt, _("Attempting to generate entire disk entry automatically."));
		sgi_set_entire(cxt);
		sgi_set_volhdr(cxt);
	}
	if (sgi_gaps(cxt) == 0 && sys != SGI_TYPE_ENTIRE_DISK) {
		fdisk_warnx(cxt, _("The entire disk is already covered with partitions."));
		return -EINVAL;
	}
	if (sgi_gaps(cxt) < 0) {
		fdisk_warnx(cxt, _("You got a partition overlap on the disk. Fix it first!"));
		return -EINVAL;
	}

	if (sys == SGI_TYPE_ENTIRE_DISK) {
		first = 0;
		last = sgi_get_lastblock(cxt);
	} else {
		first = sgi->freelist[0].first;
		last  = sgi->freelist[0].last;
	}

	/* first sector */
	if (pa && pa->start_follow_default)
		;
	else if (pa && fdisk_partition_has_start(pa)) {
		first = pa->start;
		last = is_in_freelist(cxt, first);

		if (sys != SGI_TYPE_ENTIRE_DISK && !last)
			return -ERANGE;
	} else {
		snprintf(mesg, sizeof(mesg), _("First %s"),
				fdisk_get_unit(cxt, FDISK_SINGULAR));
		ask = fdisk_new_ask();
		if (!ask)
			return -ENOMEM;

		fdisk_ask_set_query(ask, mesg);
		fdisk_ask_set_type(ask, FDISK_ASKTYPE_NUMBER);

		fdisk_ask_number_set_low(ask,     fdisk_scround(cxt, first));	/* minimal */
		fdisk_ask_number_set_default(ask, fdisk_scround(cxt, first));	/* default */
		fdisk_ask_number_set_high(ask,    fdisk_scround(cxt, last) - 1); /* maximal */

		rc = fdisk_do_ask(cxt, ask);
		first = fdisk_ask_number_get_result(ask);
		fdisk_unref_ask(ask);

		if (rc)
			return rc;
		if (fdisk_use_cylinders(cxt))
			first *= fdisk_get_units_per_sector(cxt);
	}

	if (first && sys == SGI_TYPE_ENTIRE_DISK)
		fdisk_info(cxt, _("It is highly recommended that the "
				  "eleventh partition covers the entire "
				  "disk and is of type 'SGI volume'."));
	if (!last)
		last = is_in_freelist(cxt, first);

	/* last sector */
	if (pa && pa->end_follow_default)
		last -= 1ULL;
	else if (pa && fdisk_partition_has_size(pa)) {
		if (first + pa->size - 1ULL > last)
			return -ERANGE;
		last = first + pa->size - 1ULL;
	} else {
		snprintf(mesg, sizeof(mesg),
			 _("Last %s or +%s or +size{K,M,G,T,P}"),
			 fdisk_get_unit(cxt, FDISK_SINGULAR),
			 fdisk_get_unit(cxt, FDISK_PLURAL));

		ask = fdisk_new_ask();
		if (!ask)
			return -ENOMEM;

		fdisk_ask_set_query(ask, mesg);
		fdisk_ask_set_type(ask, FDISK_ASKTYPE_OFFSET);

		fdisk_ask_number_set_low(ask,     fdisk_scround(cxt, first));	/* minimal */
		fdisk_ask_number_set_default(ask, fdisk_scround(cxt, last) - 1);/* default */
		fdisk_ask_number_set_high(ask,    fdisk_scround(cxt, last) - 1);/* maximal */
		fdisk_ask_number_set_base(ask,    fdisk_scround(cxt, first));
		fdisk_ask_number_set_wrap_negative(ask, 1); /* wrap negative around high */

		if (fdisk_use_cylinders(cxt))
			fdisk_ask_number_set_unit(ask,
				     cxt->sector_size *
				     fdisk_get_units_per_sector(cxt));
		else
			fdisk_ask_number_set_unit(ask,cxt->sector_size);

		rc = fdisk_do_ask(cxt, ask);
		last = fdisk_ask_number_get_result(ask) + 1;

		fdisk_unref_ask(ask);
		if (rc)
			return rc;
		if (fdisk_use_cylinders(cxt))
			last *= fdisk_get_units_per_sector(cxt);
	}

	if (sys == SGI_TYPE_ENTIRE_DISK
	    && (first != 0 || last != sgi_get_lastblock(cxt)))
		fdisk_info(cxt, _("It is highly recommended that the "
				  "eleventh partition covers the entire "
				  "disk and is of type 'SGI volume'."));

	set_partition(cxt, n, first, last - first, sys);
	cxt->label->nparts_cur = count_used_partitions(cxt);
	if (partno)
		*partno = n;
	return 0;
}

static int sgi_create_disklabel(struct fdisk_context *cxt)
{
	struct fdisk_sgi_label *sgi;
	struct sgi_disklabel *sgilabel;
	int rc;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	if (cxt->geom.heads && cxt->geom.sectors) {
		fdisk_sector_t llsectors;

		if (blkdev_get_sectors(cxt->dev_fd, (unsigned long long *) &llsectors) == 0) {
			/* the get device size ioctl was successful */
			fdisk_sector_t llcyls;
			int sec_fac = cxt->sector_size / 512;

			llcyls = llsectors / (cxt->geom.heads * cxt->geom.sectors * sec_fac);
			cxt->geom.cylinders = llcyls;
			if (cxt->geom.cylinders != llcyls)	/* truncated? */
				cxt->geom.cylinders = ~0;
		} else {
			/* otherwise print error and use truncated version */
			fdisk_warnx(cxt,
				_("BLKGETSIZE ioctl failed on %s. "
				  "Using geometry cylinder value of %ju. "
				  "This value may be truncated for devices "
				  "> 33.8 GB."), cxt->dev_path,
				(uintmax_t) cxt->geom.cylinders);
		}
	}

	rc = fdisk_init_firstsector_buffer(cxt, 0, 0);
	if (rc)
		return rc;

	sgi = (struct fdisk_sgi_label *) cxt->label;
	sgi->header = (struct sgi_disklabel *) cxt->firstsector;

	sgilabel = sgi->header;

	sgilabel->magic = cpu_to_be32(SGI_LABEL_MAGIC);
	sgilabel->root_part_num = cpu_to_be16(0);
	sgilabel->swap_part_num = cpu_to_be16(1);

	/* sizeof(sgilabel->boot_file) = 16 > 6 */
	memset(sgilabel->boot_file, 0, 16);
	strncpy((char *) sgilabel->boot_file, "/unix",
		sizeof(sgilabel->boot_file));

	sgilabel->devparam.skew			= (0);
	sgilabel->devparam.gap1			= (0);
	sgilabel->devparam.gap2			= (0);
	sgilabel->devparam.sparecyl			= (0);
	sgilabel->devparam.pcylcount	= cpu_to_be16(cxt->geom.cylinders);
	sgilabel->devparam.head_vol0	= cpu_to_be16(0);
	sgilabel->devparam.ntrks	= cpu_to_be16(cxt->geom.heads);
	/* tracks/cylinder (heads) */
	sgilabel->devparam.cmd_tag_queue_depth	= (0);
	sgilabel->devparam.unused0			= (0);
	sgilabel->devparam.unused1	= cpu_to_be16(0);
	sgilabel->devparam.nsect	= cpu_to_be16(cxt->geom.sectors);
	/* sectors/track */
	sgilabel->devparam.bytes	= cpu_to_be16(cxt->sector_size);
	sgilabel->devparam.ilfact	= cpu_to_be16(1);
	sgilabel->devparam.flags	= cpu_to_be32(
			SGI_DEVPARAM_TRACK_FWD
			| SGI_DEVPARAM_IGNORE_ERRORS
			| SGI_DEVPARAM_RESEEK);
	sgilabel->devparam.datarate	= cpu_to_be32(0);
	sgilabel->devparam.retries_on_error	= cpu_to_be32(1);
	sgilabel->devparam.ms_per_word		= cpu_to_be32(0);
	sgilabel->devparam.xylogics_gap1	= cpu_to_be16(0);
	sgilabel->devparam.xylogics_syncdelay	= cpu_to_be16(0);
	sgilabel->devparam.xylogics_readdelay	= cpu_to_be16(0);
	sgilabel->devparam.xylogics_gap2	= cpu_to_be16(0);
	sgilabel->devparam.xylogics_readgate	= cpu_to_be16(0);
	sgilabel->devparam.xylogics_writecont	= cpu_to_be16(0);

	memset(&(sgilabel->volume), 0,
			sizeof(struct sgi_volume) * SGI_MAXVOLUMES);
	memset(&(sgilabel->partitions), 0,
			sizeof(struct sgi_partition) * SGI_MAXPARTITIONS);
	cxt->label->nparts_max = SGI_MAXPARTITIONS;

	/* don't create default layout when a script defined */
	if (!cxt->script) {
		sgi_set_entire(cxt);
		sgi_set_volhdr(cxt);
	}
	cxt->label->nparts_cur = count_used_partitions(cxt);

	fdisk_info(cxt, _("Created a new SGI disklabel."));
	return 0;
}

static int sgi_set_partition(struct fdisk_context *cxt,
		size_t i,
		struct fdisk_partition *pa)
{
	struct sgi_disklabel *sgilabel;

	if (i >= cxt->label->nparts_max)
		return -EINVAL;

	sgilabel = self_disklabel(cxt);

	if (pa->type) {
		struct fdisk_parttype *t = pa->type;

		if (sgi_get_num_sectors(cxt, i) == 0)	/* caught already before, ... */ {
			fdisk_warnx(cxt, _("Sorry, only for non-empty partitions you can change the tag."));
			return -EINVAL;
		}

		if ((i == 10 && t->code != SGI_TYPE_ENTIRE_DISK)
		    || (i == 8 && t->code != 0))
			fdisk_info(cxt, _("Consider leaving partition 9 as volume header (0), "
					  "and partition 11 as entire volume (6), "
					  "as IRIX expects it."));

		if (cxt->script == NULL
		    && ((t->code != SGI_TYPE_ENTIRE_DISK) && (t->code != SGI_TYPE_VOLHDR))
		    && (sgi_get_start_sector(cxt, i) < 1)) {
			int yes = 0;
			fdisk_ask_yesno(cxt,
				_("It is highly recommended that the partition at offset 0 "
				  "is of type \"SGI volhdr\", the IRIX system will rely on it to "
				  "retrieve from its directory standalone tools like sash and fx. "
				  "Only the \"SGI volume\" entire disk section may violate this. "
				  "Are you sure about tagging this partition differently?"), &yes);
			if (!yes)
				return 1;
		}

		sgilabel->partitions[i].type = cpu_to_be32(t->code);
	}

	if (fdisk_partition_has_start(pa))
		sgilabel->partitions[i].first_block = cpu_to_be32(pa->start);
	if (fdisk_partition_has_size(pa))
		sgilabel->partitions[i].num_blocks = cpu_to_be32(pa->size);

	fdisk_label_set_changed(cxt->label, 1);
	return 0;
}


static int sgi_partition_is_used(
		struct fdisk_context *cxt,
		size_t i)
{
	assert(cxt);
	assert(fdisk_is_label(cxt, SGI));

	if (i >= cxt->label->nparts_max)
		return 0;
	return sgi_get_num_sectors(cxt, i) ? 1 : 0;
}

static int sgi_toggle_partition_flag(struct fdisk_context *cxt, size_t i, unsigned long flag)
{
	struct sgi_disklabel *sgilabel;
	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, SGI));

	if (i >= cxt->label->nparts_max)
		return -EINVAL;

	sgilabel = self_disklabel(cxt);

	switch (flag) {
	case SGI_FLAG_BOOT:
		sgilabel->root_part_num =
			be16_to_cpu(sgilabel->root_part_num) == i ?
			0 : cpu_to_be16(i);
		fdisk_label_set_changed(cxt->label, 1);
		break;
	case SGI_FLAG_SWAP:
		sgilabel->swap_part_num =
			be16_to_cpu(sgilabel->swap_part_num) == i ?
			0 : cpu_to_be16(i);
		fdisk_label_set_changed(cxt->label, 1);
		break;
	default:
		return 1;
	}

	return 0;
}

static const struct fdisk_field sgi_fields[] =
{
	{ FDISK_FIELD_DEVICE,	N_("Device"),	 10,	0 },
	{ FDISK_FIELD_START,	N_("Start"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_END,	N_("End"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_SECTORS,	N_("Sectors"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_CYLINDERS,N_("Cylinders"),  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_SIZE,	N_("Size"),	  5,	FDISK_FIELDFL_NUMBER | FDISK_FIELDFL_EYECANDY },
	{ FDISK_FIELD_TYPEID,	N_("Id"),	  2,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_TYPE,	N_("Type"),	0.1,	FDISK_FIELDFL_EYECANDY },
	{ FDISK_FIELD_ATTR,	N_("Attrs"),	  0,	FDISK_FIELDFL_NUMBER }
};

static const struct fdisk_label_operations sgi_operations =
{
	.probe		= sgi_probe_label,
	.write		= sgi_write_disklabel,
	.verify		= sgi_verify_disklabel,
	.get_item	= sgi_get_disklabel_item,
	.create		= sgi_create_disklabel,

	.get_part	= sgi_get_partition,
	.set_part	= sgi_set_partition,
	.add_part	= sgi_add_partition,
	.del_part	= sgi_delete_partition,

	.part_is_used	= sgi_partition_is_used,
	.part_toggle_flag = sgi_toggle_partition_flag
};

/* Allocates an SGI label driver. */
struct fdisk_label *fdisk_new_sgi_label(struct fdisk_context *cxt __attribute__ ((__unused__)))
{
	struct fdisk_label *lb;
	struct fdisk_sgi_label *sgi;

	sgi = calloc(1, sizeof(*sgi));
	if (!sgi)
		return NULL;

	/* initialize generic part of the driver */
	lb = (struct fdisk_label *) sgi;
	lb->name = "sgi";
	lb->id = FDISK_DISKLABEL_SGI;
	lb->op = &sgi_operations;
	lb->parttypes = sgi_parttypes;
	lb->nparttypes = ARRAY_SIZE(sgi_parttypes) - 1;
	lb->fields = sgi_fields;
	lb->nfields = ARRAY_SIZE(sgi_fields);

	lb->flags |= FDISK_LABEL_FL_REQUIRE_GEOMETRY;

	/* return calloc() result to keep static anaylizers happy */
	return (struct fdisk_label *) sgi;
}
