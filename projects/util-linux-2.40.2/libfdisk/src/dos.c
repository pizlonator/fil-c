/*
 *
 * Copyright (C) 2007-2013 Karel Zak <kzak@redhat.com>
 *                    2012 Davidlohr Bueso <dave@gnu.org>
 *                    2021 Pali Rohár <pali.rohar@gmail.com>
 *
 * This is re-written version for libfdisk, the original was fdiskdoslabel.c
 * from util-linux fdisk.
 */
#include "c.h"
#include "randutils.h"
#include "pt-mbr.h"
#include "strutils.h"

#include "fdiskP.h"

#include <ctype.h>

#define MAXIMUM_PARTS	60
#define ACTIVE_FLAG     0x80

/**
 * SECTION: dos
 * @title: DOS
 * @short_description: disk label specific functions
 *
 */


#define IS_EXTENDED(i) \
	((i) == MBR_DOS_EXTENDED_PARTITION \
	 || (i) == MBR_W95_EXTENDED_PARTITION \
	 || (i) == MBR_LINUX_EXTENDED_PARTITION)

/*
 * per partition table entry data
 *
 * The four primary partitions have the same sectorbuffer
 * and have NULL ex_entry.
 *
 * Each logical partition table entry has two pointers, one for the
 * partition and one link to the next one.
 */
struct pte {
	struct dos_partition *pt_entry;	/* on-disk MBR entry */
	struct dos_partition *ex_entry;	/* on-disk EBR entry */
	fdisk_sector_t offset;	        /* disk sector number */
	unsigned char *sectorbuffer;	/* disk sector contents */

	unsigned int changed : 1,
		     private_sectorbuffer : 1;
};

/*
 * in-memory fdisk GPT stuff
 */
struct fdisk_dos_label {
	struct fdisk_label	head;		/* generic part */

	struct pte	ptes[MAXIMUM_PARTS];	/* partition */
	fdisk_sector_t	ext_offset;		/* start of the ext.partition */
	size_t		ext_index;		/* ext.partition index (if ext_offset is set) */
	unsigned int	compatible : 1,		/* is DOS compatible? */
			non_pt_changed : 1;	/* MBR, but no PT changed */
};

/*
 * Partition types
 */
static const struct fdisk_parttype dos_parttypes[] = {
	#include "pt-mbr-partnames.h"
};

static const struct fdisk_shortcut dos_parttype_cuts[] =
{
	{ .shortcut = "L", .alias = "linux",    .data = "83" },
	{ .shortcut = "S", .alias = "swap",     .data = "82" },
	{ .shortcut = "E", .alias = "extended", .data = "05", .deprecated = 1 }, /* collision with 0x0e type */
	{ .shortcut = "Ex",.alias = "extended", .data = "05" }, /* MBR extended */
	{ .shortcut = "U", .alias = "uefi",     .data = "EF" }, /* UEFI system */
	{ .shortcut = "R", .alias = "raid",     .data = "FD" }, /* Linux RAID */
	{ .shortcut = "V", .alias = "lvm",      .data = "8E" }, /* LVM */
	{ .shortcut = "X", .alias = "linuxex",  .data = "85" }  /* Linux extended */
};


#define sector(s)	((s) & 0x3f)
#define cylinder(s, c)	((c) | (((s) & 0xc0) << 2))

#define alignment_required(_x)	((_x)->grain != (_x)->sector_size)

#define is_dos_compatible(_x) \
		   (fdisk_is_label(_x, DOS) && \
                    fdisk_dos_is_compatible(fdisk_get_label(_x, NULL)))

#define cround(c, n)	fdisk_cround(c, n)


static inline struct fdisk_dos_label *self_label(struct fdisk_context *cxt)
{
	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	return (struct fdisk_dos_label *) cxt->label;
}

static inline struct pte *self_pte(struct fdisk_context *cxt, size_t i)
{
	struct fdisk_dos_label *l = self_label(cxt);

	if (i >= ARRAY_SIZE(l->ptes))
		return NULL;

	return &l->ptes[i];
}

static inline struct dos_partition *self_partition(
				struct fdisk_context *cxt,
				size_t i)
{
	struct pte *pe = self_pte(cxt, i);
	return pe ? pe->pt_entry : NULL;
}

struct dos_partition *fdisk_dos_get_partition(
				struct fdisk_context *cxt,
				size_t i)
{
	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	return self_partition(cxt, i);
}

static struct fdisk_parttype *dos_partition_parttype(
		struct fdisk_context *cxt,
		struct dos_partition *p)
{
	struct fdisk_parttype *t
		= fdisk_label_get_parttype_from_code(cxt->label, p->sys_ind);
	return t ? : fdisk_new_unknown_parttype(p->sys_ind, NULL);
}

/*
 * Linux kernel cares about partition size only. Things like
 * partition type or so are completely irrelevant -- kzak Nov-2013
 */
static int is_used_partition(struct dos_partition *p)
{
	return p && dos_partition_get_size(p) != 0;
}

static void partition_set_changed(
				struct fdisk_context *cxt,
				size_t i,
				int changed)
{
	struct pte *pe = self_pte(cxt, i);

	if (!pe)
		return;

	DBG(LABEL, ul_debug("DOS: setting %zu partition to %s", i,
				changed ? "changed" : "unchanged"));

	pe->changed = changed ? 1 : 0;
	if (changed)
		fdisk_label_set_changed(cxt->label, 1);
}

static fdisk_sector_t get_abs_partition_start(struct pte *pe)
{
	assert(pe);
	assert(pe->pt_entry);

	return pe->offset + dos_partition_get_start(pe->pt_entry);
}

static fdisk_sector_t get_abs_partition_end(struct pte *pe)
{
	fdisk_sector_t size;

	assert(pe);
	assert(pe->pt_entry);

	size = dos_partition_get_size(pe->pt_entry);
	return get_abs_partition_start(pe) + size - (size ? 1 : 0);
}

static int is_cleared_partition(struct dos_partition *p)
{
	return !(!p || p->boot_ind || p->bh || p->bs || p->bc ||
		 p->sys_ind || p->eh || p->es || p->ec ||
		 dos_partition_get_start(p) || dos_partition_get_size(p));
}

static int get_partition_unused_primary(struct fdisk_context *cxt,
					struct fdisk_partition *pa,
					size_t *partno)
{
	size_t org, n;
	int rc;

	assert(cxt);
	assert(cxt->label);
	assert(partno);

	org = cxt->label->nparts_max;

	cxt->label->nparts_max = 4;
	rc = fdisk_partition_next_partno(pa, cxt, &n);
	cxt->label->nparts_max = org;

	if (rc == 1) {
		fdisk_info(cxt, _("All primary partitions have been defined already."));
		rc = -1;
	} else if (rc == -ERANGE) {
		fdisk_warnx(cxt, _("Primary partition not available."));
	} else if (rc == 0)
		*partno = n;

	return rc;
}

static int seek_sector(struct fdisk_context *cxt, fdisk_sector_t secno)
{
	off_t offset = (off_t) secno * cxt->sector_size;

	return lseek(cxt->dev_fd, offset, SEEK_SET) == (off_t) -1 ? -errno : 0;
}

static int read_sector(struct fdisk_context *cxt, fdisk_sector_t secno,
			unsigned char *buf)
{
	int rc = seek_sector(cxt, secno);
	ssize_t r;

	if (rc < 0)
		return rc;

	r = read(cxt->dev_fd, buf, cxt->sector_size);
	if (r == (ssize_t) cxt->sector_size)
		return 0;
	if (r < 0)
		return -errno;
	return -1;
}

/* Allocate a buffer and read a partition table sector */
static int read_pte(struct fdisk_context *cxt, size_t pno, fdisk_sector_t offset)
{
	int rc;
	unsigned char *buf;
	struct pte *pe = self_pte(cxt, pno);

	if (!pe)
		return -EINVAL;

	buf = calloc(1, cxt->sector_size);
	if (!buf)
		return -ENOMEM;

	DBG(LABEL, ul_debug("DOS: reading EBR %zu: offset=%ju, buffer=%p",
				pno, (uintmax_t) offset, buf));

	pe->offset = offset;
	pe->sectorbuffer = buf;
	pe->private_sectorbuffer = 1;

	rc = read_sector(cxt, offset, pe->sectorbuffer);
	if (rc) {
		fdisk_warn(cxt, _("Failed to read extended partition table "
				"(offset=%ju)"), (uintmax_t) offset);
		return rc;
	}

	pe->changed = 0;
	pe->pt_entry = pe->ex_entry = NULL;
	return 0;
}


static void clear_partition(struct dos_partition *p)
{
	if (!p)
		return;
	p->boot_ind = 0;
	p->bh = 0;
	p->bs = 0;
	p->bc = 0;
	p->sys_ind = 0;
	p->eh = 0;
	p->es = 0;
	p->ec = 0;
	dos_partition_set_start(p,0);
	dos_partition_set_size(p,0);
}

static void dos_init(struct fdisk_context *cxt)
{
	struct fdisk_dos_label *l = self_label(cxt);
	size_t i;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	DBG(LABEL, ul_debug("DOS: initialize, first sector buffer %p", cxt->firstsector));

	cxt->label->nparts_max = 4;	/* default, unlimited number of logical */

	l->ext_index = 0;
	l->ext_offset = 0;
	l->non_pt_changed = 0;

	memset(l->ptes, 0, sizeof(l->ptes));

	for (i = 0; i < 4; i++) {
		struct pte *pe = self_pte(cxt, i);

		assert(pe);
		pe->pt_entry = mbr_get_partition(cxt->firstsector, i);
		pe->ex_entry = NULL;
		pe->offset = 0;
		pe->sectorbuffer = cxt->firstsector;
		pe->private_sectorbuffer = 0;
		pe->changed = 0;

		DBG(LABEL, ul_debug("DOS: initialize: #%zu start=%u size=%u sysid=%02x",
					i,
					dos_partition_get_start(pe->pt_entry),
					dos_partition_get_size(pe->pt_entry),
					pe->pt_entry->sys_ind));
	}

	if (fdisk_is_listonly(cxt))
		return;
	/*
	 * Various warnings...
	 */
	if (fdisk_missing_geometry(cxt))
		fdisk_warnx(cxt, _("You can set geometry from the extra functions menu."));

	if (is_dos_compatible(cxt)) {
		fdisk_warnx(cxt, _("DOS-compatible mode is deprecated."));

		if (cxt->sector_size != cxt->phy_sector_size)
			fdisk_info(cxt, _(
		"The device presents a logical sector size that is smaller than "
		"the physical sector size. Aligning to a physical sector (or optimal "
		"I/O) size boundary is recommended, or performance may be impacted."));
	}

	if (fdisk_use_cylinders(cxt))
		fdisk_warnx(cxt, _("Cylinders as display units are deprecated."));

	if (cxt->total_sectors > UINT_MAX) {
		uint64_t bytes = cxt->total_sectors * cxt->sector_size;
		char *szstr = size_to_human_string(SIZE_SUFFIX_SPACE
					   | SIZE_SUFFIX_3LETTER, bytes);
		fdisk_warnx(cxt,
		_("The size of this disk is %s (%ju bytes). DOS "
		  "partition table format cannot be used on drives for "
		  "volumes larger than %lu bytes for %lu-byte "
		  "sectors. Use GUID partition table format (GPT)."),
			szstr, bytes,
			UINT_MAX * cxt->sector_size,
			cxt->sector_size);
		free(szstr);
	}
}

/* callback called by libfdisk */
static void dos_deinit(struct fdisk_label *lb)
{
	size_t i;
	struct fdisk_dos_label *l = (struct fdisk_dos_label *) lb;

	for (i = 0; i < ARRAY_SIZE(l->ptes); i++) {
		struct pte *pe = &l->ptes[i];

		if (pe->private_sectorbuffer && pe->sectorbuffer) {
			DBG(LABEL, ul_debug("DOS: freeing pte %zu sector buffer %p",
						i, pe->sectorbuffer));
			free(pe->sectorbuffer);
		}
		pe->sectorbuffer = NULL;
		pe->private_sectorbuffer = 0;
	}

	memset(l->ptes, 0, sizeof(l->ptes));
}

static void reset_pte(struct pte *pe)
{
	assert(pe);

	if (pe->private_sectorbuffer) {
		DBG(LABEL, ul_debug("   --> freeing pte sector buffer %p",
					pe->sectorbuffer));
		free(pe->sectorbuffer);
	}
	memset(pe, 0, sizeof(struct pte));
}

static int delete_partition(struct fdisk_context *cxt, size_t partnum)
{
	struct fdisk_dos_label *l;
	struct pte *pe;
	struct dos_partition *p;
	struct dos_partition *q;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	pe = self_pte(cxt, partnum);
	if (!pe)
		return -EINVAL;

	DBG(LABEL, ul_debug("DOS: delete partition %zu (max=%zu)", partnum,
				cxt->label->nparts_max));

	l = self_label(cxt);
	p = pe->pt_entry;
	q = pe->ex_entry;

	/* Note that for the fifth partition (partnum == 4) we don't actually
	   decrement partitions. */
	if (partnum < 4) {
		DBG(LABEL, ul_debug("--> delete primary"));
		if (IS_EXTENDED(p->sys_ind) && partnum == l->ext_index) {
			size_t i;
			DBG(LABEL, ul_debug(" --> delete extended"));
			for (i = 4; i < cxt->label->nparts_max; i++) {
				DBG(LABEL, ul_debug("  --> delete logical #%zu", i));
				reset_pte(&l->ptes[i]);

			}
			cxt->label->nparts_max = 4;
			l->ptes[l->ext_index].ex_entry = NULL;
			l->ext_offset = 0;
			l->ext_index = 0;
		}
		partition_set_changed(cxt, partnum, 1);
		clear_partition(p);
	} else if (!q->sys_ind && partnum > 4) {
		DBG(LABEL, ul_debug("--> delete logical [last in the chain]"));
		reset_pte(&l->ptes[partnum]);
		--cxt->label->nparts_max;
		--partnum;
		/* clear link to deleted partition */
		clear_partition(l->ptes[partnum].ex_entry);
		partition_set_changed(cxt, partnum, 1);
	} else {
		DBG(LABEL, ul_debug("--> delete logical [move down]"));
		if (partnum > 4) {
			DBG(LABEL, ul_debug(" --> delete %zu logical link", partnum));
			p = l->ptes[partnum - 1].ex_entry;
			*p = *q;
			dos_partition_set_start(p, dos_partition_get_start(q));
			dos_partition_set_size(p, dos_partition_get_size(q));
			dos_partition_sync_chs(p, pe->offset, cxt->geom.sectors, cxt->geom.heads);
			partition_set_changed(cxt, partnum - 1, 1);

		} else if (cxt->label->nparts_max > 5) {
			DBG(LABEL, ul_debug(" --> delete first logical link"));
			pe = &l->ptes[5];	/* second logical */

			if (pe->pt_entry)	/* prevent SEGFAULT */
				dos_partition_set_start(pe->pt_entry,
					       get_abs_partition_start(pe) -
					       l->ext_offset);
			pe->offset = l->ext_offset;
			dos_partition_sync_chs(p, pe->offset, cxt->geom.sectors, cxt->geom.heads);
			partition_set_changed(cxt, 5, 1);
		}

		if (cxt->label->nparts_max > 5) {
			DBG(LABEL, ul_debug(" --> move ptes"));
			cxt->label->nparts_max--;
			reset_pte(&l->ptes[partnum]);
			while (partnum < cxt->label->nparts_max) {
				DBG(LABEL, ul_debug("  --> moving pte %zu <-- %zu", partnum, partnum + 1));
				l->ptes[partnum] = l->ptes[partnum + 1];
				partnum++;
			}
			memset(&l->ptes[partnum], 0, sizeof(struct pte));
		} else {
			DBG(LABEL, ul_debug(" --> the only logical: clear only"));
			clear_partition(l->ptes[partnum].pt_entry);
			cxt->label->nparts_max--;

			if (partnum == 4) {
				DBG(LABEL, ul_debug("  --> clear last logical"));
				reset_pte(&l->ptes[partnum]);
				partition_set_changed(cxt, l->ext_index, 1);
			}
		}
	}

	fdisk_label_set_changed(cxt->label, 1);
	return 0;
}

static int dos_delete_partition(struct fdisk_context *cxt, size_t partnum)
{
	struct pte *pe;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	pe = self_pte(cxt, partnum);
	if (!pe || !is_used_partition(pe->pt_entry))
		return -EINVAL;

	return delete_partition(cxt, partnum);
}

static void read_extended(struct fdisk_context *cxt, size_t ext)
{
	size_t i;
	struct pte *pex, *pe;
	struct dos_partition *p, *q;
	struct fdisk_dos_label *l = self_label(cxt);

	l->ext_index = ext;
	pex = self_pte(cxt, ext);
	if (!pex) {
		DBG(LABEL, ul_debug("DOS: uninitialized pointer to %zu pex", ext));
		return;
	}
	pex->ex_entry = pex->pt_entry;

	p = pex->pt_entry;
	if (!dos_partition_get_start(p)) {
		fdisk_warnx(cxt, _("Bad offset in primary extended partition."));
		return;
	}

	DBG(LABEL, ul_debug("DOS: Reading extended %zu", ext));

	while (IS_EXTENDED (p->sys_ind)) {
		if (cxt->label->nparts_max >= MAXIMUM_PARTS) {
			/* This is not a Linux restriction, but
			   this program uses arrays of size MAXIMUM_PARTS.
			   Do not try to `improve' this test. */
			struct pte *pre = self_pte(cxt,
						cxt->label->nparts_max - 1);
			fdisk_warnx(cxt,
			_("Omitting partitions after #%zu. They will be deleted "
			  "if you save this partition table."),
				cxt->label->nparts_max);

			if (pre) {
				clear_partition(pre->ex_entry);
				partition_set_changed(cxt,
						cxt->label->nparts_max - 1, 1);
			}
			return;
		}

		pe = self_pte(cxt, cxt->label->nparts_max);
		if (!pe)
			return;

		if (read_pte(cxt, cxt->label->nparts_max, l->ext_offset +
						dos_partition_get_start(p)))
			return;

		if (!l->ext_offset)
			l->ext_offset = dos_partition_get_start(p);

		assert(pe->sectorbuffer);
		q = p = mbr_get_partition(pe->sectorbuffer, 0);

		for (i = 0; i < 4; i++, p++) {
			if (!dos_partition_get_size(p))
				continue;

			if (IS_EXTENDED (p->sys_ind)) {
				if (pe->ex_entry)
					fdisk_warnx(cxt, _(
					"Extra link pointer in partition "
					"table %zu."),
						cxt->label->nparts_max + 1);
				else
					pe->ex_entry = p;
			} else if (p->sys_ind) {
				if (pe->pt_entry)
					fdisk_warnx(cxt, _(
					"Ignoring extra data in partition "
					"table %zu."),
						cxt->label->nparts_max + 1);
				else
					pe->pt_entry = p;
			}
		}

		/* very strange code here... */
		if (!pe->pt_entry) {
			if (q != pe->ex_entry)
				pe->pt_entry = q;
			else
				pe->pt_entry = q + 1;
		}
		if (!pe->ex_entry) {
			if (q != pe->pt_entry)
				pe->ex_entry = q;
			else
				pe->ex_entry = q + 1;
		}

		p = pe->ex_entry;
		cxt->label->nparts_cur = ++cxt->label->nparts_max;

		DBG(LABEL, ul_debug("DOS: EBR[offset=%ju]: link: type=%x,  start=%u, size=%u; "
				                         " data: type=%x, start=%u, size=%u",
				    (uintmax_t) pe->offset,
				    pe->ex_entry->sys_ind,
				    dos_partition_get_start(pe->ex_entry),
				    dos_partition_get_size(pe->ex_entry),
				    pe->pt_entry->sys_ind,
				    dos_partition_get_start(pe->pt_entry),
				    dos_partition_get_size(pe->pt_entry)));

	}

	/* remove last empty EBR */
	pe = self_pte(cxt, cxt->label->nparts_max - 1);
	if (pe &&
	    is_cleared_partition(pe->ex_entry) &&
	    is_cleared_partition(pe->pt_entry)) {
		DBG(LABEL, ul_debug("DOS: EBR[offset=%ju]: empty, remove", (uintmax_t) pe->offset));
		reset_pte(pe);
		cxt->label->nparts_max--;
		cxt->label->nparts_cur--;
	}

	/* remove empty links */
 remove:
	q = self_partition(cxt, 4);
	for (i = 4; i < cxt->label->nparts_max; i++) {
		p = self_partition(cxt, i);

		if (p && !dos_partition_get_size(p) &&
		    (cxt->label->nparts_max > 5 || (q && q->sys_ind))) {
			fdisk_info(cxt, _("omitting empty partition (%zu)"), i+1);
			delete_partition(cxt, i);
			goto remove; 	/* numbering changed */
		}
	}

	DBG(LABEL, ul_debug("DOS: nparts_max: %zu", cxt->label->nparts_max));
}

static int dos_create_disklabel(struct fdisk_context *cxt)
{
	unsigned int id = 0;
	int rc, has_id = 0;
	struct fdisk_dos_label *l;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	DBG(LABEL, ul_debug("DOS: creating new disklabel"));

	if (cxt->script) {
		char *end = NULL;
		const char *s = fdisk_script_get_header(cxt->script, "label-id");

		if (s) {
			errno = 0;
			id = strtoul(s, &end, 16);
			if (!errno && end && s < end) {
				has_id = 1;
				DBG(LABEL, ul_debug("DOS: re-use ID from script (0x%08x)", id));
			} else
				DBG(LABEL, ul_debug("DOS: failed to parse label=id '%s'", s));
		}
	}

	/* random disk signature */
	if (!has_id) {
		DBG(LABEL, ul_debug("DOS: generate new ID"));
		ul_random_get_bytes(&id, sizeof(id));
	}

	if (fdisk_has_protected_bootbits(cxt))
		rc = fdisk_init_firstsector_buffer(cxt, 0, MBR_PT_BOOTBITS_SIZE);
	else
		rc = fdisk_init_firstsector_buffer(cxt, 0, 0);
	if (rc)
		return rc;
	dos_init(cxt);

	l = self_label(cxt);

	/* Generate an MBR ID for this disk */
	mbr_set_id(cxt->firstsector, id);
	l->non_pt_changed = 1;
	fdisk_label_set_changed(cxt->label, 1);

	/* Put MBR signature */
	mbr_set_magic(cxt->firstsector);

	fdisk_info(cxt, _("Created a new DOS (MBR) disklabel with disk "
			 "identifier 0x%08x."), id);
	return 0;
}

static int dos_set_disklabel_id(struct fdisk_context *cxt, const char *str)
{
	char *buf = NULL;
	unsigned int id, old;
	struct fdisk_dos_label *l;
	int rc = 0;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	DBG(LABEL, ul_debug("DOS: setting Id"));

	l = self_label(cxt);
	old = mbr_get_id(cxt->firstsector);

	if (!str) {
		rc = fdisk_ask_string(cxt,
			_("Enter the new disk identifier"), &buf);
		str = buf;
	}
	if (!rc) {
		char *end = NULL;

		errno = 0;
		id = strtoul(str, &end, 0);
		if (errno || str == end || (end && *end)) {
			fdisk_warnx(cxt, _("Incorrect value."));
			rc = -EINVAL;
		}
	}

	free(buf);
	if (rc)
		return -EINVAL;

	mbr_set_id(cxt->firstsector, id);
	l->non_pt_changed = 1;
	fdisk_label_set_changed(cxt->label, 1);

	fdisk_info(cxt, _("Disk identifier changed from 0x%08x to 0x%08x."),
			old, id);
	return 0;
}

static unsigned int chs_div_minus(unsigned int a1, unsigned int a2, unsigned int b1, unsigned int b2)
{
	if (a1 > a2 && b1 > b2) {
		a1 = a1 - a2;
		b1 = b1 - b2;
	} else if (a2 > a1 && b2 > b1) {
		a1 = a2 - a1;
		b1 = b2 - b1;
	} else {
		return 0;
	}
	if (a1 % b1)
		return 0;
	return a1 / b1;
}

static inline int chs_overflowed(unsigned int c, unsigned int h, unsigned int s)
{
	/* 1023/254/63 or 1023/255/63 indicates overflowed/invalid C/H/S values */
	return (c == 1023 && (h == 254 || h == 255) && s == 63);
}

static inline int lba_overflowed(fdisk_sector_t start, fdisk_sector_t sects)
{
	/* Check if the last LBA sector can be represented by unsigned 32bit int */
	return (start + (sects-1) > UINT32_MAX);
}

static void get_partition_table_geometry(struct fdisk_context *cxt,
			unsigned int *ph, unsigned int *ps)
{
	unsigned char *bufp = cxt->firstsector;
	struct { unsigned int c, h, o, v; } t[8] = { 0 };
	unsigned int n1, n2, n3, n4, n5, n6;
	const struct dos_partition *p;
	unsigned int c, h, s, l;
	unsigned int hh, ss;
	unsigned int sects;
	int i, j, dif;

#define chs_set_t(c, h, s, l, t, i) do { \
	t[i].c = c; \
	t[i].h = h; \
	t[i].o = l - (s-1); \
	t[i].v = (!chs_overflowed(c, h, s) && s && s-1 <= l); \
} while (0)

	/*
	 * Conversion from C/H/S to LBA is defined by formula:
	 *   LBA = (c * N_heads + h) * N_sectors + (s - 1)
	 * Let o to be:
	 *   o = LBA - (s - 1)
	 * Then formula can be expressed as:
	 *   o = (c * N_heads + h) * N_sectors
	 * In general from two tuples (LBA1, c1, h1, s1), (LBA2, c2, h2, s2)
	 * we can derive formulas for N_heads and N_sectors:
	 *   N_heads = (o1 * h2 - o2 * h1) / (o2 * c1 - o1 * c2)
	 *   N_sectors = (o2 * c1 - o1 * c2) / (c1 * h2 - c2 * h1)
	 * MBR table contains for very partition start and end tuple.
	 * So we have up to 8 tuples which leads to up to 28 equations
	 * for calculating N_heads and N_sectors. Try to calculate
	 * N_heads and N_sectors from the first possible partition and
	 * if it fails then try also mixed tuples (beginning from first
	 * partition and end from second). Calculation may fail if both
	 * first and last sectors are on cylinder or head boundary
	 * (dividend or divisor is zero). It is possible that different
	 * partitions would have different C/H/S geometry. In this case
	 * we want geometry from the first partition as in most cases
	 * this partition is or was used by BIOS for booting.
	 */

	hh = ss = 0;
	for (i = 0; i < 4; i++) {
		p = mbr_get_partition(bufp, i);
		if (!p->sys_ind)
			continue;

		c = cylinder(p->bs, p->bc);
		h = p->bh;
		s = sector(p->bs);
		l = dos_partition_get_start(p);
		chs_set_t(c, h, s, l, t, 2*i);

		sects = dos_partition_get_size(p);
		if (!sects || lba_overflowed(l, sects))
			continue;

		c = cylinder(p->es, p->ec);
		h = p->eh;
		s = sector(p->es);
		l += sects-1;
		chs_set_t(c, h, s, l, t, 2*i+1);
	}

	for (dif = 1; dif < 8; dif++) {
		for (i = 0; i + dif < 8; i++) {
			j = i + dif;
			if (!t[i].v || !t[j].v)
				continue;
			n1 = t[i].o * t[j].h;
			n2 = t[j].o * t[i].h;
			n3 = t[j].o * t[i].c;
			n4 = t[i].o * t[j].c;
			n5 = t[i].c * t[j].h;
			n6 = t[j].c * t[i].h;
			if (!hh && n1 != n2 && n3 != n4) {
				h = chs_div_minus(n1, n2, n3, n4);
				if (h > 0 && h <= 256)
					hh = h;
			}
			if (!ss && n3 != n4 && n5 != n6) {
				s = chs_div_minus(n3, n4, n5, n6);
				if (s > 0 && s <= 63)
					ss = s;
			}
			if (hh && ss)
				break;
		}
		if (hh && ss)
			break;
	}

	if (hh && ss) {
		*ph = hh;
		*ps = ss;
	}

	DBG(LABEL, ul_debug("DOS PT geometry: heads=%u, sectors=%u", *ph, *ps));
}

static int dos_reset_alignment(struct fdisk_context *cxt)
{
	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	/* overwrite necessary stuff by DOS deprecated stuff */
	if (is_dos_compatible(cxt)) {
		DBG(LABEL, ul_debug("DOS: resetting alignment for DOS-compatible PT"));
		if (cxt->geom.sectors)
			cxt->first_lba = cxt->geom.sectors;	/* usually 63 */

		cxt->grain = cxt->sector_size;			/* usually 512 */
	}

	return 0;
}

/* TODO: move to include/pt-dos.h and share with libblkid */
#define AIX_MAGIC_STRING	"\xC9\xC2\xD4\xC1"
#define AIX_MAGIC_STRLEN	(sizeof(AIX_MAGIC_STRING) - 1)

static int dos_probe_label(struct fdisk_context *cxt)
{
	size_t i;
	unsigned int h = 0, s = 0;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	/* ignore disks with AIX magic number */
	if (memcmp(cxt->firstsector, AIX_MAGIC_STRING, AIX_MAGIC_STRLEN) == 0)
		return 0;

	if (!mbr_is_valid_magic(cxt->firstsector))
		return 0;

	dos_init(cxt);

	get_partition_table_geometry(cxt, &h, &s);
	if (h && s) {
		cxt->geom.heads = h;
	        cxt->geom.sectors = s;
		cxt->geom.cylinders = cxt->total_sectors /
					(cxt->geom.heads * cxt->geom.sectors);

		if (fdisk_has_user_device_geometry(cxt))
			fdisk_apply_user_device_properties(cxt);
	}

	for (i = 0; i < 4; i++) {
		struct pte *pe = self_pte(cxt, i);

		assert(pe);
		if (is_used_partition(pe->pt_entry))
			cxt->label->nparts_cur++;

		if (IS_EXTENDED (pe->pt_entry->sys_ind)) {
			if (cxt->label->nparts_max != 4)
				fdisk_warnx(cxt, _(
				"Ignoring extra extended partition %zu"),
					i + 1);
			else
				read_extended(cxt, i);
		}
	}

	for (i = 3; i < cxt->label->nparts_max; i++) {
		struct pte *pe = self_pte(cxt, i);
		struct fdisk_dos_label *l = self_label(cxt);

		assert(pe);
		if (!mbr_is_valid_magic(pe->sectorbuffer)) {
			fdisk_info(cxt, _(
			"Invalid flag 0x%02x%02x of EBR (for partition %zu) will "
			"be corrected by w(rite)."),
				pe->sectorbuffer[510],
				pe->sectorbuffer[511],
				i + 1);
			partition_set_changed(cxt, i, 1);

			/* mark also extended as changed to update the first EBR
			 * in situation that there is no logical partitions at all */
			partition_set_changed(cxt, l->ext_index, 1);
		}
	}

	return 1;
}

static void set_partition(struct fdisk_context *cxt,
			  int i, int doext, fdisk_sector_t start,
			  fdisk_sector_t stop, int sysid, int boot)
{
	struct pte *pe = self_pte(cxt, i);
	struct dos_partition *p;
	fdisk_sector_t offset;

	assert(!FDISK_IS_UNDEF(start));
	assert(!FDISK_IS_UNDEF(stop));
	assert(pe);

	if (doext) {
		struct fdisk_dos_label *l = self_label(cxt);
		p = pe->ex_entry;
		offset = l->ext_offset;
	} else {
		p = pe->pt_entry;
		offset = pe->offset;
	}

	DBG(LABEL, ul_debug("DOS: setting partition %d%s, offset=%zu, start=%zu, size=%zu, sysid=%02x",
				i, doext ? " [extended]" : "",
				(size_t) offset,
				(size_t) (start -  offset),
				(size_t) (stop - start + 1),
				sysid));

	p->boot_ind = boot ? ACTIVE_FLAG : 0;
	p->sys_ind = sysid;
	dos_partition_set_start(p, start - offset);
	dos_partition_set_size(p, stop - start + 1);
	dos_partition_sync_chs(p, offset, cxt->geom.sectors, cxt->geom.heads);
	partition_set_changed(cxt, i, 1);
}


static int get_start_from_user(	struct fdisk_context *cxt,
				fdisk_sector_t *start,
				fdisk_sector_t low,
				fdisk_sector_t dflt,
				fdisk_sector_t limit,
				struct fdisk_partition *pa)
{
	assert(start);

	/* try to use template from 'pa' */
	if (pa && pa->start_follow_default)
		*start = dflt;

	else if (pa && fdisk_partition_has_start(pa)) {
		DBG(LABEL, ul_debug("DOS: start: wanted=%ju, low=%ju, limit=%ju",
				(uintmax_t) pa->start, (uintmax_t) low, (uintmax_t) limit));
		*start = pa->start;
		if (*start < low || *start > limit) {
			fdisk_warnx(cxt, _("Start sector %ju out of range."),
					(uintmax_t) *start);
			return -ERANGE;
		}
	} else {
		/* ask user by dialog */
		struct fdisk_ask *ask = fdisk_new_ask();
		int rc;

		if (!ask)
			return -ENOMEM;
		fdisk_ask_set_query(ask,
			fdisk_use_cylinders(cxt) ?
				_("First cylinder") : _("First sector"));
		fdisk_ask_set_type(ask, FDISK_ASKTYPE_NUMBER);
		fdisk_ask_number_set_low(ask, fdisk_cround(cxt, low));
		fdisk_ask_number_set_default(ask, fdisk_cround(cxt, dflt));
		fdisk_ask_number_set_high(ask, fdisk_cround(cxt, limit));

		rc = fdisk_do_ask(cxt, ask);
		*start = fdisk_ask_number_get_result(ask);
		fdisk_unref_ask(ask);
		if (rc)
			return rc;
		if (fdisk_use_cylinders(cxt)) {
		        *start = (*start - 1)
				* fdisk_get_units_per_sector(cxt);
			if (*start < low)
				*start = low;
		}
	}

	DBG(LABEL, ul_debug("DOS: start is %ju", (uintmax_t) *start));
	return 0;
}

/* Returns last available sector in the free space pointed to by start. */
static int find_last_free(
			struct fdisk_context *cxt,
			int logical,
			fdisk_sector_t begin,
			fdisk_sector_t stop,
			fdisk_sector_t *result)
{
	fdisk_sector_t last = stop;

	size_t i = logical ? 4 : 0;

	for ( ; i < cxt->label->nparts_max; i++) {
		struct pte *pe = self_pte(cxt, i);

		assert(pe);
		fdisk_sector_t p_start = get_abs_partition_start(pe);
		fdisk_sector_t p_end = get_abs_partition_end(pe);

		if (is_cleared_partition(pe->pt_entry))
			continue;
		/* count EBR and begin of the logical partition as used area */
		if (pe->offset)
			p_start -= cxt->first_lba;

		if ((p_start >= begin && p_start <= last) ||
		    (p_end >= begin && p_end <= last)) {
			last = p_start - 1;
		}
		if (last < begin) {
			DBG(LABEL, ul_debug("no free space <%ju,%ju>",
					(uintmax_t) begin, (uintmax_t) stop));
			return -ENOSPC;
		}
	}

	if (last == begin)
		last = stop;

	DBG(LABEL, ul_debug("DOS: last free sector  <%ju,%ju>: %ju",
			(uintmax_t) begin, (uintmax_t) stop, (uintmax_t) last));

	*result = last;
	return 0;
}

static int find_last_free_sector_in_range(
			struct fdisk_context *cxt,
			int logical,
			fdisk_sector_t begin,
			fdisk_sector_t end,
			fdisk_sector_t *result)
{
	int last_moved;
	fdisk_sector_t last = end;

	do {
		size_t i = logical ? 4 : 0;

		last_moved = 0;
		for ( ; i < cxt->label->nparts_max; i++) {
			struct pte *pe = self_pte(cxt, i);

			assert(pe);
			fdisk_sector_t p_start = get_abs_partition_start(pe);
			fdisk_sector_t p_end = get_abs_partition_end(pe);

			if (is_cleared_partition(pe->pt_entry))
				continue;

			/* count EBR and begin of the logical partition as used area */
			if (pe->offset)
				p_start -= cxt->first_lba;

			if (last >= p_start && last <= p_end) {
				last = p_start - 1;
				last_moved = 1;

				if (last < begin) {
					DBG(LABEL, ul_debug("DOS: last free out of range <%ju,%ju>: %ju",
						(uintmax_t) begin, (uintmax_t) end, (uintmax_t) last));

					return -ENOSPC;
				}
			}
		}
	} while (last_moved == 1);

	DBG(LABEL, ul_debug("DOS: last unused sector in range <%ju,%ju>: %ju",
			(uintmax_t) begin, (uintmax_t) end, (uintmax_t) last));

	*result = last;
	return 0;
}

static int find_first_free_sector_in_range(
			struct fdisk_context *cxt,
			int logical,
			fdisk_sector_t begin,
			fdisk_sector_t end,
			fdisk_sector_t *result)
{
	int first_moved = 0;
	fdisk_sector_t first = begin;

	do {
		size_t i = logical ? 4 : 0;

		first_moved = 0;
		for (; i < cxt->label->nparts_max; i++) {
			struct pte *pe = self_pte(cxt, i);

			assert(pe);
			fdisk_sector_t p_start = get_abs_partition_start(pe);
			fdisk_sector_t p_end = get_abs_partition_end(pe);

			if (is_cleared_partition(pe->pt_entry))
				continue;
			/* count EBR and begin of the logical partition as used area */
			if (pe->offset)
				p_start -= cxt->first_lba;
			if (first < p_start)
				continue;
			if (first <= p_end) {
				first = p_end + 1 + (logical ? cxt->first_lba : 0);
				first_moved = 1;

				if (first > end) {
					DBG(LABEL, ul_debug("DOS: first free out of range <%ju,%ju>: %ju",
						(uintmax_t) begin, (uintmax_t) end, (uintmax_t) first));
					return -ENOSPC;
				}
			}
		}
	} while (first_moved == 1);

	DBG(LABEL, ul_debug("DOS: first unused sector in range <%ju,%ju>: %ju",
			(uintmax_t) begin, (uintmax_t) end, (uintmax_t) first));
	*result = first;
	return 0;
}

static int get_disk_ranges(struct fdisk_context *cxt, int logical,
			   fdisk_sector_t *first, fdisk_sector_t *last)
{
	if (logical) {
		/* logical partitions */
		struct fdisk_dos_label *l = self_label(cxt);
		struct pte *ext_pe = l->ext_offset ? self_pte(cxt, l->ext_index) : NULL;

		if (!ext_pe)
			return -EINVAL;

		*first = l->ext_offset + cxt->first_lba;
		*last = get_abs_partition_end(ext_pe);

	} else {
		/* primary partitions */
		if (fdisk_use_cylinders(cxt) || !cxt->total_sectors)
			*last = cxt->geom.heads * cxt->geom.sectors * cxt->geom.cylinders - 1;
		else
			*last = cxt->total_sectors - 1;

		if (*last > UINT_MAX)
			*last = UINT_MAX;
		*first = cxt->first_lba;
	}

	return 0;
}

/* first free sector on disk */
static int find_first_free_sector(struct fdisk_context *cxt,
				int logical,
				fdisk_sector_t start,
				fdisk_sector_t *result)
{
	fdisk_sector_t first, last;
	int rc;

	rc = get_disk_ranges(cxt, logical, &first, &last);
	if (rc)
		return rc;

	return find_first_free_sector_in_range(cxt, logical, start, last, result);
}

static int add_partition(struct fdisk_context *cxt, size_t n,
			 struct fdisk_partition *pa)
{
	int sys, read = 0, rc, isrel = 0, is_logical;
	struct fdisk_dos_label *l = self_label(cxt);
	struct dos_partition *p = self_partition(cxt, n);
	struct fdisk_ask *ask = NULL;

	fdisk_sector_t start, stop, limit, temp;

	DBG(LABEL, ul_debug("DOS: adding partition %zu", n));

	sys = pa && pa->type ? pa->type->code : MBR_LINUX_DATA_PARTITION;
	is_logical = n >= 4;

	if (p && is_used_partition(p)) {
		fdisk_warnx(cxt, _("Partition %zu is already defined.  "
			           "Delete it before re-adding it."),
				n + 1);
		return -EINVAL;
	}

	rc = get_disk_ranges(cxt, is_logical, &start, &stop);
	if (rc)
		return rc;

	if (!is_logical && cxt->parent && fdisk_is_label(cxt->parent, GPT))
		start = 1;		/* Bad boy modifies hybrid MBR */

	rc = find_last_free_sector_in_range(cxt, is_logical, start, stop, &limit);
	if (rc == -ENOSPC)
		fdisk_warnx(cxt, _("No free sectors available."));
	if (rc)
		return rc;

	if ((is_logical || !cxt->parent || !fdisk_is_label(cxt->parent, GPT))
	    && cxt->script && pa && fdisk_partition_has_start(pa)
	    && pa->start >= (is_logical ? l->ext_offset : 1)
	    && pa->start < start) {
		fdisk_set_first_lba(cxt, 1);

		rc = get_disk_ranges(cxt, is_logical, &start, &stop);
		if (rc) /* won't happen, but checking to be proper */
			return rc;
	}

	/*
	 * Ask for first sector
	 */
	do {
		fdisk_sector_t dflt, aligned;

		temp = start;

		DBG(LABEL, ul_debug("DOS: >>> search for first free from %ju", start));
		rc = find_first_free_sector(cxt, is_logical, start, &dflt);
		if (rc == -ENOSPC)
			fdisk_warnx(cxt, _("No free sectors available."));
		if (rc)
			return rc;
		start = dflt;

		if (n >= 4 && pa && fdisk_partition_has_start(pa) && cxt->script
		    && cxt->first_lba > 1
		    && temp == start - cxt->first_lba) {
			fdisk_set_first_lba(cxt, 1);
			start = pa->start;
		}

		/* the default sector should be aligned and unused */
		do {
			aligned = fdisk_align_lba_in_range(cxt, dflt, dflt, limit);
			find_first_free_sector(cxt, is_logical, aligned, &dflt);
		} while (dflt != aligned && dflt > aligned && dflt < limit);

		if (dflt >= limit)
			dflt = start;
		if (start > limit)
			break;
		if (start >= temp + fdisk_get_units_per_sector(cxt)
		    && read) {
			if (!pa || !pa->start_follow_default)
				fdisk_info(cxt, _("Sector %ju is already allocated."),
						(uintmax_t) temp);
			temp = start;
			read = 0;
			if (pa && fdisk_partition_has_start(pa))
				break;
		}

		if (!read && start == temp) {
			rc = get_start_from_user(cxt, &start, temp, dflt, limit, pa);
			if (rc)
				return rc;
			read = 1;
		}
		if (pa && fdisk_partition_has_size(pa)) {
			fdisk_sector_t last;

			rc = find_last_free(cxt, is_logical, start, limit, &last);
			if (rc == 0 && last - start + 1 < fdisk_partition_get_size(pa)) {
				DBG(LABEL, ul_debug("DOS: area <%ju,%ju> too small [wanted=%ju aval=%ju]",
							(uintmax_t) start, (uintmax_t) last,
							fdisk_partition_get_size(pa),
							last - start));

				if (fdisk_partition_has_start(pa)
				    && fdisk_partition_get_start(pa) <= last)
					rc = -ENOSPC;
				else {
					start = last + 1;
					continue;
				}
			}
			if (rc == -ENOSPC) {
				fdisk_warnx(cxt, _("No free sectors available."));
				return rc;
			}
		}

	} while (start != temp || !read);

	if (n == 4) {
		/* The first EBR is stored at begin of the extended partition */
		struct pte *pe = self_pte(cxt, n);

		assert(pe);
		pe->offset = l->ext_offset;
	} else if (n > 4) {
		/* The second (and another) EBR */
		struct pte *pe = self_pte(cxt, n);

		assert(pe);
		assert(start >= cxt->first_lba);

		pe->offset = start - cxt->first_lba;
		DBG(LABEL, ul_debug("DOS: setting EBR offset to %ju [start=%ju]", pe->offset, start));

		if (pe->offset == l->ext_offset) { /* must be corrected */
			pe->offset++;
			if (cxt->first_lba == 1)
				start++;
		}
	}

	rc = find_last_free(cxt, is_logical, start, limit, &stop);
	if (rc == -ENOSPC)
		fdisk_warnx(cxt, _("No free sectors available."));
	if (rc)
		return rc;
	limit = stop;

	/*
	 * Ask for last sector
	 */
	if (fdisk_cround(cxt, start) == fdisk_cround(cxt, limit))
		stop = limit;
	else if (pa && pa->end_follow_default)
		stop = limit;
	else if (pa && fdisk_partition_has_size(pa)) {
		stop = start + pa->size;
		isrel = pa->size_explicit ? 0 : 1;
		if ((!isrel || !alignment_required(cxt)) && stop > start)
			stop -= 1;
	} else {
		/* ask user by dialog */
		for (;;) {
			if (!ask)
				ask = fdisk_new_ask();
			else
				fdisk_reset_ask(ask);
			if (!ask)
				return -ENOMEM;
			fdisk_ask_set_type(ask, FDISK_ASKTYPE_OFFSET);

			if (fdisk_use_cylinders(cxt)) {
				fdisk_ask_set_query(ask, _("Last cylinder, +/-cylinders or +/-size{K,M,G,T,P}"));
				fdisk_ask_number_set_unit(ask,
					     cxt->sector_size *
					     fdisk_get_units_per_sector(cxt));
			} else {
				fdisk_ask_set_query(ask, _("Last sector, +/-sectors or +/-size{K,M,G,T,P}"));
				fdisk_ask_number_set_unit(ask,cxt->sector_size);
			}

			fdisk_ask_number_set_low(ask, fdisk_cround(cxt, start));
			fdisk_ask_number_set_default(ask, fdisk_cround(cxt, limit));
			fdisk_ask_number_set_high(ask, fdisk_cround(cxt, limit));
			fdisk_ask_number_set_base(ask, fdisk_cround(cxt, start));	/* base for relative input */
			fdisk_ask_number_set_wrap_negative(ask, 1); /* wrap negative around high */

			rc = fdisk_do_ask(cxt, ask);
			if (rc)
				goto done;

			stop = fdisk_ask_number_get_result(ask);
			isrel = fdisk_ask_number_is_relative(ask);
			if (fdisk_use_cylinders(cxt)) {
				stop = stop * fdisk_get_units_per_sector(cxt) - 1;
				if (stop >limit)
					stop = limit;
			}

			if (stop >= start && stop <= limit)
				break;
			fdisk_warnx(cxt, _("Value out of range."));
		}
	}

	DBG(LABEL, ul_debug("DOS: raw stop: %ju [limit %ju]", (uintmax_t) stop, (uintmax_t) limit));

	if (stop > limit)
		stop = limit;

	if (isrel && stop - start < (cxt->grain / fdisk_get_sector_size(cxt))) {
		/* Don't try to be smart on very small partitions and don't align so small sizes */
		isrel = 0;
		DBG(LABEL, ul_debug("DOS: don't align end of tiny partition [start=%ju, stop=%ju, grain=%lu]",
			    (uintmax_t)start,  (uintmax_t)stop, cxt->grain));
	}

	if (stop < limit && isrel && alignment_required(cxt)) {
		/* the last sector has not been exactly requested (but
		 * defined by +size{K,M,G} convention), so be smart and
		 * align the end of the partition. The next partition
		 * will start at phy.block boundary.
		 */
		stop = fdisk_align_lba_in_range(cxt, stop, start, limit);
		if (stop > start)
			stop -= 1;	/* end one sector before aligned offset */
		if (stop > limit)
			stop = limit;
		DBG(LABEL, ul_debug("DOS: aligned stop: %ju", (uintmax_t) stop));
	}

	set_partition(cxt, n, 0, start, stop, sys, fdisk_partition_is_bootable(pa));
	if (n > 4) {
		struct pte *pe = self_pte(cxt, n);
		assert(pe);
		set_partition(cxt, n - 1, 1, pe->offset, stop,
					MBR_DOS_EXTENDED_PARTITION, 0);
	}

	/* report */
	{
		struct fdisk_parttype *t =
			fdisk_label_get_parttype_from_code(cxt->label, sys);
		fdisk_info_new_partition(cxt, n + 1, start, stop, t);
		fdisk_unref_parttype(t);
	}


	if (IS_EXTENDED(sys)) {
		struct pte *pen = self_pte(cxt, n);

		assert(pen);
		l->ext_index = n;
		l->ext_offset = start;
		pen->ex_entry = p;
	}

	fdisk_label_set_changed(cxt->label, 1);
	rc = 0;
done:
	fdisk_unref_ask(ask);
	return rc;
}

static int add_logical(struct fdisk_context *cxt,
		       struct fdisk_partition *pa,
		       size_t *partno)
{
	struct pte *pe;
	int rc;

	assert(cxt);
	assert(partno);
	assert(cxt->label);
	assert(self_label(cxt)->ext_offset);

	DBG(LABEL, ul_debug("DOS: nparts max: %zu", cxt->label->nparts_max));
	pe = self_pte(cxt, cxt->label->nparts_max);
	assert(pe);

	if (!pe->sectorbuffer) {
		pe->sectorbuffer = calloc(1, cxt->sector_size);
		if (!pe->sectorbuffer)
			return -ENOMEM;
		DBG(LABEL, ul_debug("DOS: logical: %zu: new EBR sector buffer %p",
					cxt->label->nparts_max, pe->sectorbuffer));
		pe->private_sectorbuffer = 1;
	}
	pe->pt_entry = mbr_get_partition(pe->sectorbuffer, 0);
	pe->ex_entry = pe->pt_entry + 1;
	pe->offset = 0;
	partition_set_changed(cxt, cxt->label->nparts_max, 1);

	cxt->label->nparts_max++;

	/* this message makes sense only when we use extended/primary/logical
	 * dialog. The dialog is disable for scripts, see dos_add_partition() */
	if (!cxt->script)
		fdisk_info(cxt, _("Adding logical partition %zu"),
				cxt->label->nparts_max);
	*partno = cxt->label->nparts_max - 1;
	rc = add_partition(cxt, *partno, pa);

	if (rc) {
		/* reset on error */
		cxt->label->nparts_max--;
		pe->pt_entry = NULL;
		pe->ex_entry = NULL;
		pe->offset = 0;
		pe->changed = 0;
	}

	return rc;
}

static int check(struct fdisk_context *cxt, size_t n,
	   unsigned int h, unsigned int s, unsigned int c,
	   unsigned int lba_sector)
{
	unsigned int chs_sector, real_s, real_c;
	int nerrors = 0;

	if (!is_dos_compatible(cxt))
		return 0;

	real_s = sector(s) - 1;
	real_c = cylinder(s, c);
	chs_sector = (real_c * cxt->geom.heads + h) * cxt->geom.sectors + real_s;

	if (!chs_sector) {
		fdisk_warnx(cxt, _("Partition %zu: contains sector 0"), n);
		nerrors++;
	}
	if (h >= cxt->geom.heads) {
		fdisk_warnx(cxt, _("Partition %zu: head %d greater than "
				   "maximum %d"), n, h + 1, cxt->geom.heads);
		nerrors++;
	}
	if (real_s >= cxt->geom.sectors) {
		fdisk_warnx(cxt, _("Partition %zu: sector %d greater than "
				   "maximum %ju"), n, real_s + 1,
				(uintmax_t) cxt->geom.sectors);
		nerrors++;
	}
	if (real_c >= cxt->geom.cylinders) {
		fdisk_warnx(cxt, _("Partition %zu: cylinder %d greater than "
				   "maximum %ju"),
				n, real_c + 1,
				(uintmax_t) cxt->geom.cylinders);
		nerrors++;
	}
	if (lba_sector / (cxt->geom.heads * cxt->geom.sectors) < 1024 && lba_sector != chs_sector) {
		fdisk_warnx(cxt, _("Partition %zu: LBA sector %u "
				   "disagrees with C/H/S calculated sector %u"),
				n, lba_sector, chs_sector);
		nerrors++;
	}

	return nerrors;
}

/* check_consistency() and long2chs() added Sat Mar 6 12:28:16 1993,
 * faith@cs.unc.edu, based on code fragments from pfdisk by Gordon W. Ross,
 * Jan.  1990 (version 1.2.1 by Gordon W. Ross Aug. 1990; Modified by S.
 * Lubkin Oct.  1991). */

static void
long2chs(struct fdisk_context *cxt, unsigned long ls,
	 unsigned int *c, unsigned int *h, unsigned int *s) {
	int spc = cxt->geom.heads * cxt->geom.sectors;

	*c = ls / spc;
	ls = ls % spc;
	*h = ls / cxt->geom.sectors;
	*s = ls % cxt->geom.sectors + 1;	/* sectors count from 1 */
}

static int check_consistency(struct fdisk_context *cxt, struct dos_partition *p,
			      size_t partition)
{
	unsigned int pbc, pbh, pbs;	/* physical beginning c, h, s */
	unsigned int pec, peh, pes;	/* physical ending c, h, s */
	unsigned int lbc, lbh, lbs;	/* logical beginning c, h, s */
	unsigned int lec, leh, les;	/* logical ending c, h, s */
	int nerrors = 0;

	if (!is_dos_compatible(cxt))
		return 0;

	if (!cxt->geom.heads || !cxt->geom.sectors || (partition >= 4))
		return 0;		/* do not check extended partitions */

	/* physical beginning c, h, s */
	pbc = cylinder(p->bs, p->bc);
	pbh = p->bh;
	pbs = sector(p->bs);

	/* physical ending c, h, s */
	pec = cylinder(p->es, p->ec);
	peh = p->eh;
	pes = sector(p->es);

	/* compute logical beginning (c, h, s) */
	long2chs(cxt, dos_partition_get_start(p), &lbc, &lbh, &lbs);

	/* compute logical ending (c, h, s) */
	long2chs(cxt, dos_partition_get_start(p) + dos_partition_get_size(p) - 1, &lec, &leh, &les);

	/* Same physical / logical beginning? */
	if (lbc < 1024
	    && (pbc != lbc || pbh != lbh || pbs != lbs)) {
		fdisk_warnx(cxt, _("Partition %zu: different physical/logical "
			"beginnings (non-Linux?): "
			"phys=(%d, %d, %d), logical=(%d, %d, %d)"),
			partition + 1,
			pbc, pbh, pbs,
			lbc, lbh, lbs);
		nerrors++;
	}

	/* Same physical / logical ending? */
	if (lec < 1024
	    && (pec != lec || peh != leh || pes != les)) {
		fdisk_warnx(cxt, _("Partition %zu: different physical/logical "
			"endings: phys=(%d, %d, %d), logical=(%d, %d, %d)"),
			partition + 1,
			pec, peh, pes,
			lec, leh, les);
		nerrors++;
	}

	/* Ending on cylinder boundary? */
	if (peh != (cxt->geom.heads - 1) || pes != cxt->geom.sectors) {
		fdisk_warnx(cxt, _("Partition %zu: does not end on "
				   "cylinder boundary."),
			partition + 1);
		nerrors++;
	}

	return nerrors;
}

static void fill_bounds(struct fdisk_context *cxt,
			fdisk_sector_t *first, fdisk_sector_t *last)
{
	size_t i;
	struct pte *pe = self_pte(cxt, 0);
	struct dos_partition *p;

	assert(pe);
	for (i = 0; i < cxt->label->nparts_max; pe++,i++) {
		p = pe->pt_entry;
		if (is_cleared_partition(p) || IS_EXTENDED (p->sys_ind)) {
			first[i] = SIZE_MAX;
			last[i] = 0;
		} else {
			first[i] = get_abs_partition_start(pe);
			last[i]  = get_abs_partition_end(pe);
		}
	}
}

static int dos_verify_disklabel(struct fdisk_context *cxt)
{
	size_t i, j;
	fdisk_sector_t total = 1, n_sectors = cxt->total_sectors;
	fdisk_sector_t *first, *last;
	struct dos_partition *p;
	struct fdisk_dos_label *l = self_label(cxt);
	int nerrors = 0;

	assert(fdisk_is_label(cxt, DOS));

	first = calloc(cxt->label->nparts_max, sizeof(*first));
	last = calloc(cxt->label->nparts_max, sizeof(*first));

	if (!first || !last) {
		free(first);
		free(last);
		return -ENOMEM;
	}

	fill_bounds(cxt, first, last);
	for (i = 0; i < cxt->label->nparts_max; i++) {
		struct pte *pe = self_pte(cxt, i);

		p = self_partition(cxt, i);
		if (p && is_used_partition(p) && !IS_EXTENDED(p->sys_ind)) {
			nerrors += check_consistency(cxt, p, i);
			assert(pe);
			if (get_abs_partition_start(pe) < first[i]) {
				fdisk_warnx(cxt, _(
					"Partition %zu: bad start-of-data."),
					 i + 1);
				nerrors++;
			}

			nerrors += check(cxt, i + 1, p->bh, p->bs, p->bc, first[i]);
			nerrors += check(cxt, i + 1, p->eh, p->es, p->ec, last[i]);
			total += last[i] + 1 - first[i];

			if (i == 0)
				total += get_abs_partition_start(pe) - 1;

			for (j = 0; j < i; j++) {
				if ((first[i] >= first[j] && first[i] <= last[j])
				    || ((last[i] <= last[j] && last[i] >= first[j]))) {

					fdisk_warnx(cxt, _("Partition %zu: "
						"overlaps partition %zu."),
						j + 1, i + 1);
					nerrors++;

					total += first[i] >= first[j] ?
						first[i] : first[j];
					total -= last[i] <= last[j] ?
						last[i] : last[j];
				}
			}
		}
	}

	if (l->ext_offset) {
		fdisk_sector_t e_last;
		struct pte *ext_pe = self_pte(cxt, l->ext_index);

		assert(ext_pe);
		e_last = get_abs_partition_end(ext_pe);

		for (i = 4; i < cxt->label->nparts_max; i++) {
			total++;
			p = self_partition(cxt, i);
			assert(p);

			if (!p->sys_ind) {
				if (i != 4 || i + 1 < cxt->label->nparts_max) {
					fdisk_warnx(cxt,
						_("Partition %zu: empty."),
						i + 1);
					nerrors++;
				}
			} else if (first[i] < l->ext_offset
				   || last[i] > e_last) {

				fdisk_warnx(cxt, _("Logical partition %zu: "
					"not entirely in partition %zu."),
					i + 1, l->ext_index + 1);
				nerrors++;
			}
		}
	}

	if (!nerrors) {
		fdisk_info(cxt, _("No errors detected."));
		if (total > n_sectors)
			fdisk_info(cxt, _("Total allocated sectors %ju greater "
				"than the maximum %ju."), (uintmax_t) total, (uintmax_t) n_sectors);
		else if (total < n_sectors)
			fdisk_info(cxt, _("Remaining %ju unallocated %ld-byte "
				"sectors."), (uintmax_t) n_sectors - total, cxt->sector_size);
	} else
		fdisk_warnx(cxt,
			P_("%d error detected.", "%d errors detected.", nerrors),
			nerrors);

	free(first);
	free(last);
	return nerrors;
}

/*
 * Ask the user for new partition type information (logical, extended).
 * This function calls the actual partition adding logic - add_partition.
 *
 * API callback.
 */
static int dos_add_partition(struct fdisk_context *cxt,
			     struct fdisk_partition *pa,
			     size_t *partno)
{
	size_t i;
	uint8_t free_primary = 0, free_sectors = 0;
	fdisk_sector_t first = 0, grain;
	int rc = 0;
	struct fdisk_dos_label *l;
	struct pte *ext_pe;
	size_t res = 0;		/* partno */

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	DBG(LABEL, ul_debug("DOS: new partition wanted"));

	l = self_label(cxt);

	if (cxt->label->nparts_max >= MAXIMUM_PARTS) {
		fdisk_warnx(cxt, _("The maximum number of partitions has "
				  "been created."));
		return -EINVAL;
	}

	ext_pe = l->ext_offset ? self_pte(cxt, l->ext_index) : NULL;

	/*
	 * partition template (@pa) based partitioning
	 */

	/* A) template specifies start within extended partition; add logical */
	if (pa && fdisk_partition_has_start(pa) && ext_pe
	    && pa->start >= l->ext_offset
	    && pa->start <= get_abs_partition_end(ext_pe)) {
		DBG(LABEL, ul_debug("DOS: pa template %p: add logical (by offset)", pa));

		if (fdisk_partition_has_partno(pa) && fdisk_partition_get_partno(pa) < 4) {
			DBG(LABEL, ul_debug("DOS: pa template specifies partno<4 for logical partition"));
			return -EINVAL;
		}
		rc = add_logical(cxt, pa, &res);
		goto done;

	/* B) template specifies start out of extended partition; add primary */
	} else if (pa && fdisk_partition_has_start(pa) && ext_pe) {
		DBG(LABEL, ul_debug("DOS: pa template %p: add primary (by offset)", pa));

		if (fdisk_partition_has_partno(pa) && fdisk_partition_get_partno(pa) >= 4) {
			DBG(LABEL, ul_debug("DOS: pa template specifies partno>=4 for primary partition"));
			return -EINVAL;
		}
		if (ext_pe && pa->type && IS_EXTENDED(pa->type->code)) {
			fdisk_warnx(cxt, _("Extended partition already exists."));
			return -EINVAL;
		}
		rc = get_partition_unused_primary(cxt, pa, &res);
		if (rc == 0)
			rc = add_partition(cxt, res, pa);
		goto done;

	/* C) template specifies start (or default), partno < 4; add primary */
	} else if (pa && (fdisk_partition_start_is_default(pa) || fdisk_partition_has_start(pa))
		   && fdisk_partition_has_partno(pa)
		   && pa->partno < 4) {
		DBG(LABEL, ul_debug("DOS: pa template %p: add primary (by partno)", pa));

		if (ext_pe && pa->type && IS_EXTENDED(pa->type->code)) {
			fdisk_warnx(cxt, _("Extended partition already exists."));
			return -EINVAL;
		}
		rc = get_partition_unused_primary(cxt, pa, &res);
		if (rc == 0)
			rc = add_partition(cxt, res, pa);
		goto done;

	/* D) template specifies start (or default), partno >= 4; add logical */
	} else if (pa && (fdisk_partition_start_is_default(pa) || fdisk_partition_has_start(pa))
		   && fdisk_partition_has_partno(pa)
		   && pa->partno >= 4) {
		DBG(LABEL, ul_debug("DOS: pa template %p: add logical (by partno)", pa));

		if (!ext_pe) {
			fdisk_warnx(cxt, _("Extended partition does not exists. Failed to add logical partition."));
			return -EINVAL;
		}

		if (fdisk_partition_has_start(pa)
			   && pa->start < l->ext_offset
			   && pa->start > get_abs_partition_end(ext_pe)) {
			DBG(LABEL, ul_debug("DOS: pa template specifies partno>=4, but start out of extended"));
			return -EINVAL;
		}

		rc = add_logical(cxt, pa, &res);
		goto done;
	}

	DBG(LABEL, ul_debug("DOS: dialog driven partitioning"));
	/* Note @pa may be still used for things like partition type, etc */

	/* check if there is space for primary partition */
	grain = cxt->grain > cxt->sector_size ? cxt->grain / cxt->sector_size : 1;
	first = cxt->first_lba;

	if (cxt->parent && fdisk_is_label(cxt->parent, GPT)) {
		/* modifying a hybrid MBR, which throws out the rules */
		grain = 1;
		first = 1;
	}

	/* set @first after the last used partition, set @free_sectors if there
	 * is gap in front if the first partition or between used parrtitions. */
	for (i = 0; i < 4; i++) {
		struct dos_partition *p = self_partition(cxt, i);

		if (p && is_used_partition(p)) {
			fdisk_sector_t start = dos_partition_get_start(p);
			if (first + grain <= start)
				free_sectors = 1;
			first = start + dos_partition_get_size(p);
		} else
			free_primary++;
	}

	/* set @free_sectors if there is a space after the first usable sector */
	if (first + grain - 1 <= cxt->total_sectors - 1)
		free_sectors = 1;

	DBG(LABEL, ul_debug("DOS: primary: first free: %ju, last on disk: %ju, "
			    "free_sectors=%d, free_primary=%d",
				(uintmax_t) first,
				(uintmax_t) cxt->total_sectors - 1,
				free_sectors, free_primary));

	if (!free_primary || !free_sectors) {
		DBG(LABEL, ul_debug("DOS: primary impossible"));
		if (l->ext_offset) {
			if (!pa || fdisk_partition_has_start(pa)) {
				/* See above case A); here we have start, but
				 * out of extended partition */
				const char *msg;
				if (!free_primary)
					msg = _("All primary partitions are in use.");
				else
					msg =  _("All space for primary partitions is in use.");

				if (pa && fdisk_partition_has_start(pa)) {
					fdisk_warnx(cxt, "%s", msg);
					return -EINVAL;
				}
				fdisk_info(cxt, "%s", msg);
			}
			DBG(LABEL, ul_debug("DOS: trying logical"));
			rc = add_logical(cxt, pa, &res);
		} else {
			if (free_primary)
				fdisk_info(cxt, _("All space for primary partitions is in use."));
			else
			/* TRANSLATORS: Try to keep this within 80 characters. */
				fdisk_info(cxt, _("To create more partitions, first replace "
					  "a primary with an extended partition."));
			return -EINVAL;
		}
	} else {
		char hint[BUFSIZ];
		struct fdisk_ask *ask;
		int c = 0;

		/* the default layout for scripts is to create primary partitions */
		if (cxt->script || !fdisk_has_dialogs(cxt)) {
			rc = get_partition_unused_primary(cxt, pa, &res);
			if (rc == 0)
				rc = add_partition(cxt, res, pa);
			goto done;
		}

		ask = fdisk_new_ask();
		if (!ask)
			return -ENOMEM;
		fdisk_ask_set_type(ask, FDISK_ASKTYPE_MENU);
		fdisk_ask_set_query(ask, _("Partition type"));
		fdisk_ask_menu_set_default(ask, free_primary == 1
						&& !l->ext_offset ? 'e' : 'p');
		snprintf(hint, sizeof(hint),
				_("%u primary, %d extended, %u free"),
				4 - (l->ext_offset ? 1 : 0) - free_primary,
				l->ext_offset ? 1 : 0,
				free_primary);

		fdisk_ask_menu_add_item(ask, 'p', _("primary"), hint);
		if (!l->ext_offset)
			fdisk_ask_menu_add_item(ask, 'e', _("extended"), _("container for logical partitions"));
		else
			fdisk_ask_menu_add_item(ask, 'l', _("logical"), _("numbered from 5"));

		rc = fdisk_do_ask(cxt, ask);
		if (!rc)
			fdisk_ask_menu_get_result(ask, &c);
		fdisk_unref_ask(ask);
		if (rc)
			return rc;

		if (c == 'p') {
			rc = get_partition_unused_primary(cxt, pa, &res);
			if (rc == 0)
				rc = add_partition(cxt, res, pa);
			goto done;
		} else if (c == 'l' && l->ext_offset) {
			rc = add_logical(cxt, pa, &res);
			goto done;
		} else if (c == 'e' && !l->ext_offset) {
			rc = get_partition_unused_primary(cxt, pa, &res);
			if (rc == 0) {
				struct fdisk_partition *xpa = NULL;
				struct fdisk_parttype *t;

				t = fdisk_label_get_parttype_from_code(cxt->label,
						MBR_DOS_EXTENDED_PARTITION);
				if (!pa) {
					pa = xpa = fdisk_new_partition();
					if (!xpa)
						return -ENOMEM;
				}
				fdisk_partition_set_type(pa, t);
				rc = add_partition(cxt, res, pa);
				if (xpa) {
					fdisk_unref_partition(xpa);
					pa = NULL;
				}
			}
			goto done;
		} else
			fdisk_warnx(cxt, _("Invalid partition type `%c'."), c);
	}
done:
	if (rc == 0) {
		cxt->label->nparts_cur++;
		if (partno)
			*partno = res;
	}
	return rc;
}

static int write_sector(struct fdisk_context *cxt, fdisk_sector_t secno,
			       unsigned char *buf)
{
	int rc;

	rc = seek_sector(cxt, secno);
	if (rc != 0) {
		fdisk_warn(cxt, _("Cannot write sector %jd: seek failed"),
				(uintmax_t) secno);
		return rc;
	}

	DBG(LABEL, ul_debug("DOS: writing to sector %ju", (uintmax_t) secno));

	if (write(cxt->dev_fd, buf, cxt->sector_size) != (ssize_t) cxt->sector_size)
		return -errno;
	return 0;
}

static int dos_write_disklabel(struct fdisk_context *cxt)
{
	struct fdisk_dos_label *l = self_label(cxt);
	size_t i;
	int rc = 0, mbr_changed = 0;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	DBG(LABEL, ul_debug("DOS: write PT requested [label-changed: %d, non-pt-changed: %d]",
				cxt->label->changed, l->non_pt_changed));

	mbr_changed = l->non_pt_changed;

	/* MBR (primary partitions) */
	if (!mbr_changed) {
		for (i = 0; i < 4; i++) {
			struct pte *pe = self_pte(cxt, i);

			assert(pe);
			if (pe->changed)
				mbr_changed = 1;
		}
	}
	if (mbr_changed) {
		DBG(LABEL, ul_debug("DOS: MBR changed, writing"));
		mbr_set_magic(cxt->firstsector);
		rc = write_sector(cxt, 0, cxt->firstsector);
		if (rc)
			goto done;
	}

	if (cxt->label->nparts_max <= 4 && l->ext_offset) {
		/* we have empty extended partition, check if the partition has
		 * been modified and then cleanup possible remaining EBR  */
		struct pte *pe = self_pte(cxt, l->ext_index);
		unsigned char empty[512] = { 0 };
		fdisk_sector_t off = pe ? get_abs_partition_start(pe) : 0;

		if (off && pe->changed) {
			mbr_set_magic(empty);
			write_sector(cxt, off, empty);
		}
	}

	/* EBR (logical partitions) */
	for (i = 4; i < cxt->label->nparts_max; i++) {
		struct pte *pe = self_pte(cxt, i);

		assert(pe);
		if (!pe->changed || !pe->offset || !pe->sectorbuffer)
			continue;

		mbr_set_magic(pe->sectorbuffer);
		rc = write_sector(cxt, pe->offset, pe->sectorbuffer);
		if (rc)
			goto done;
	}

done:
	return rc;
}

static int dos_locate_disklabel(struct fdisk_context *cxt, int n,
		const char **name, uint64_t *offset, size_t *size)
{
	assert(cxt);

	*name = NULL;
	*offset = 0;
	*size = 0;

	switch (n) {
	case 0:
		*name = "MBR";
		*offset = 0;
		*size = 512;
		break;
	default:
		/* extended partitions */
		if ((size_t)n - 1 + 4 < cxt->label->nparts_max) {
			struct pte *pe = self_pte(cxt, n - 1 + 4);

			assert(pe);
			assert(pe->private_sectorbuffer);

			*name = "EBR";
			*offset = (uint64_t) pe->offset * cxt->sector_size;
			*size = 512;
		} else
			return 1;
		break;
	}

	return 0;
}

/*
 * Check whether partition entries are ordered by their starting positions.
 * Return 0 if OK. Return i if partition i should have been earlier.
 * Two separate checks: primary and logical partitions.
 */
static int wrong_p_order(struct fdisk_context *cxt, size_t *prev)
{
	size_t last_p_start_pos = 0, p_start_pos;
	size_t i, last_i = 0;

	for (i = 0 ; i < cxt->label->nparts_max; i++) {

		struct pte *pe = self_pte(cxt, i);
		struct dos_partition *p;

		assert(pe);
		p = pe->pt_entry;

		if (i == 4) {
			last_i = 4;
			last_p_start_pos = 0;
		}
		if (is_used_partition(p)) {
			p_start_pos = get_abs_partition_start(pe);

			if (last_p_start_pos > p_start_pos) {
				if (prev)
					*prev = last_i;
				return i;
			}

			last_p_start_pos = p_start_pos;
			last_i = i;
		}
	}
	return 0;
}

static int dos_get_disklabel_item(struct fdisk_context *cxt, struct fdisk_labelitem *item)
{
	int rc = 0;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	switch (item->id) {
	case FDISK_LABELITEM_ID:
	{
		unsigned int num = mbr_get_id(cxt->firstsector);
		item->name = _("Disk identifier");
		item->type = 's';
		if (asprintf(&item->data.str, "0x%08x", num) < 0)
			rc = -ENOMEM;
		break;
	}
	default:
		if (item->id < __FDISK_NLABELITEMS)
			rc = 1;	/* unsupported generic item */
		else
			rc = 2;	/* out of range */
		break;
	}

	return rc;

}

static int dos_get_partition(struct fdisk_context *cxt, size_t n,
			     struct fdisk_partition *pa)
{
	struct dos_partition *p;
	struct pte *pe;
	struct fdisk_dos_label *lb;

	assert(cxt);
	assert(pa);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	lb = self_label(cxt);

	pe = self_pte(cxt, n);
	assert(pe);

	p = pe->pt_entry;
	pa->used = !is_cleared_partition(p);
	if (!pa->used)
		return 0;

	pa->type = dos_partition_parttype(cxt, p);
	pa->boot = p->boot_ind == ACTIVE_FLAG ? 1 : 0;
	pa->start = get_abs_partition_start(pe);
	pa->size = dos_partition_get_size(p);
	pa->container = lb->ext_offset && n == lb->ext_index;

	if (n >= 4)
		pa->parent_partno = lb->ext_index;

	if (p->boot_ind && asprintf(&pa->attrs, "%02x", p->boot_ind) < 0)
		return -ENOMEM;

	/* start C/H/S */
	if (asprintf(&pa->start_chs, "%d/%d/%d",
				cylinder(p->bs, p->bc),
				p->bh,
				sector(p->bs)) < 0)
		return -ENOMEM;

	/* end C/H/S */
	if (asprintf(&pa->end_chs, "%d/%d/%d",
				cylinder(p->es, p->ec),
				p->eh,
				sector(p->es)) < 0)
		return -ENOMEM;

	return 0;
}

static int has_logical(struct fdisk_context *cxt)
{
	size_t i;
	struct fdisk_dos_label *l = self_label(cxt);

	for (i = 4; i < cxt->label->nparts_max; i++) {
		if (l->ptes[i].pt_entry)
			return 1;
	}
	return 0;
}

static int dos_set_partition(struct fdisk_context *cxt, size_t n,
			     struct fdisk_partition *pa)
{
	struct fdisk_dos_label *l;
	struct dos_partition *p;
	struct pte *pe;
	int orgtype;
	fdisk_sector_t start, size;

	assert(cxt);
	assert(pa);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	if (n >= cxt->label->nparts_max)
		return -EINVAL;

	l = self_label(cxt);
	p = self_partition(cxt, n);
	assert(p);

	pe = self_pte(cxt, n);
	if (!pe)
		return -EINVAL;

	orgtype = p->sys_ind;

	if (pa->type) {
		if (IS_EXTENDED(pa->type->code) && l->ext_offset && l->ext_index != n) {
			fdisk_warnx(cxt, _("Extended partition already exists."));
			return -EINVAL;
		}

		if (!pa->type->code)
			fdisk_warnx(cxt, _("Type 0 means free space to many systems. "
				   "Having partitions of type 0 is probably unwise."));

		if (IS_EXTENDED(p->sys_ind) && !IS_EXTENDED(pa->type->code) && has_logical(cxt)) {
			fdisk_warnx(cxt, _(
				"Cannot change type of the extended partition which is "
				"already used by logical partitions. Delete logical "
				"partitions first."));
			return -EINVAL;
		}
	}

	FDISK_INIT_UNDEF(start);
	FDISK_INIT_UNDEF(size);

	if (fdisk_partition_has_start(pa))
		start = pa->start;
	if (fdisk_partition_has_size(pa))
		size = pa->size;

	if (!FDISK_IS_UNDEF(start) || !FDISK_IS_UNDEF(size)) {
		DBG(LABEL, ul_debug("DOS: resize partition"));

		if (FDISK_IS_UNDEF(start))
			start = get_abs_partition_start(pe);
		if (FDISK_IS_UNDEF(size))
			size = dos_partition_get_size(p);

		set_partition(cxt, n, 0, start, start + size - 1,
				pa->type  ? pa->type->code : p->sys_ind,
				FDISK_IS_UNDEF(pa->boot) ?
					p->boot_ind == ACTIVE_FLAG :
					fdisk_partition_is_bootable(pa));
	} else {
		DBG(LABEL, ul_debug("DOS: keep size, modify properties"));
		if (pa->type)
			p->sys_ind = pa->type->code;
		if (!FDISK_IS_UNDEF(pa->boot))
			p->boot_ind = fdisk_partition_is_bootable(pa) ? ACTIVE_FLAG : 0;
	}

	if (pa->type) {
		if (IS_EXTENDED(pa->type->code) && !IS_EXTENDED(orgtype)) {
			/* new extended partition - create a reference  */
			l->ext_index = n;
			l->ext_offset = dos_partition_get_start(p);
			pe->ex_entry = p;
		 } else if (IS_EXTENDED(orgtype)) {
			/* remove extended partition */
			cxt->label->nparts_max = 4;
			l->ptes[l->ext_index].ex_entry = NULL;
			l->ext_offset = 0;
			l->ext_index = 0;
		 }
	}

	partition_set_changed(cxt, n, 1);
	return 0;
}

static void print_chain_of_logicals(struct fdisk_context *cxt)
{
	size_t i;
	struct fdisk_dos_label *l = self_label(cxt);

	fputc('\n', stdout);

	for (i = 4; i < cxt->label->nparts_max; i++) {
		struct pte *pe = self_pte(cxt, i);

		assert(pe);
		fprintf(stderr, "#%02zu EBR [%10ju], "
			"data[start=%10ju (%10ju), size=%10ju], "
			"link[start=%10ju (%10ju), size=%10ju]\n",
			i, (uintmax_t) pe->offset,
			/* data */
			(uintmax_t) dos_partition_get_start(pe->pt_entry),
			(uintmax_t) get_abs_partition_start(pe),
			(uintmax_t) dos_partition_get_size(pe->pt_entry),
			/* link */
			(uintmax_t) dos_partition_get_start(pe->ex_entry),
			(uintmax_t) l->ext_offset + dos_partition_get_start(pe->ex_entry),
			(uintmax_t) dos_partition_get_size(pe->ex_entry));
	}
}

static int cmp_ebr_offsets(const void *a, const void *b)
{
	const struct pte *ae = (const struct pte *) a,
			 *be = (const struct pte *) b;

	if (ae->offset == 0 && be->offset == 0)
		return 0;
	if (ae->offset == 0)
		return 1;
	if (be->offset == 0)
		return -1;

	return cmp_numbers(ae->offset, be->offset);
}

/*
 * Fix the chain of logicals.
 *
 * The function does not modify data partitions within EBR tables
 * (pte->pt_entry). It sorts the chain by EBR offsets and then update links
 * (pte->ex_entry) between EBR tables.
 *
 */
static void fix_chain_of_logicals(struct fdisk_context *cxt)
{
	struct fdisk_dos_label *l = self_label(cxt);
	struct pte *last;
	size_t i;

	DBG(LABEL, print_chain_of_logicals(cxt));

	/* Sort chain by EBR offsets */
	qsort(&l->ptes[4], cxt->label->nparts_max - 4, sizeof(struct pte),
			cmp_ebr_offsets);

again:
	/* Sort data partitions by start */
	for (i = 4; i < cxt->label->nparts_max - 1; i++) {
		struct pte *cur = self_pte(cxt, i),
			   *nxt = self_pte(cxt, i + 1);

		assert(cur);
		assert(nxt);

		if (get_abs_partition_start(cur) >
		    get_abs_partition_start(nxt)) {

			struct dos_partition tmp = *cur->pt_entry;
			fdisk_sector_t cur_start = get_abs_partition_start(cur),
				 nxt_start = get_abs_partition_start(nxt);

			/* swap data partitions */
			*cur->pt_entry = *nxt->pt_entry;
			*nxt->pt_entry = tmp;

			/* Recount starts according to EBR offsets, the absolute
			 * address still has to be the same! */
			dos_partition_set_start(cur->pt_entry, nxt_start - cur->offset);
			dos_partition_sync_chs(cur->pt_entry, cur->offset, cxt->geom.sectors, cxt->geom.heads);
			dos_partition_set_start(nxt->pt_entry, cur_start - nxt->offset);
			dos_partition_sync_chs(nxt->pt_entry, nxt->offset, cxt->geom.sectors, cxt->geom.heads);

			partition_set_changed(cxt, i, 1);
			partition_set_changed(cxt, i + 1, 1);
			goto again;
		}
	}

	/* Update EBR links */
	for (i = 4; i < cxt->label->nparts_max - 1; i++) {
		struct pte *cur = self_pte(cxt, i),
			   *nxt = self_pte(cxt, i + 1);

		assert(cur);
		assert(nxt);

		fdisk_sector_t noff = nxt->offset - l->ext_offset,
		 ooff = dos_partition_get_start(cur->ex_entry);

		if (noff == ooff)
			continue;

		DBG(LABEL, ul_debug("DOS: fix EBR [%10ju] link %ju -> %ju",
			(uintmax_t) cur->offset,
			(uintmax_t) ooff, (uintmax_t) noff));

		set_partition(cxt, i, 1, nxt->offset,
				get_abs_partition_end(nxt),
				MBR_DOS_EXTENDED_PARTITION, 0);
	}

	/* always terminate the chain ! */
	last = self_pte(cxt, cxt->label->nparts_max - 1);
	if (last) {
		clear_partition(last->ex_entry);
		partition_set_changed(cxt, cxt->label->nparts_max - 1, 1);
	}

	DBG(LABEL, print_chain_of_logicals(cxt));
}

static int dos_reorder(struct fdisk_context *cxt)
{
	struct pte *pei, *pek;
	size_t i,k;

	if (!wrong_p_order(cxt, NULL))
		return 1;

	while ((i = wrong_p_order(cxt, &k)) != 0 && i < 4) {
		/* partition i should have come earlier, move it */
		/* We have to move data in the MBR */
		struct dos_partition *pi, *pk, *pe, pbuf;
		pei = self_pte(cxt, i);
		pek = self_pte(cxt, k);

		assert(pei);
		assert(pek);

		pe = pei->ex_entry;
		pei->ex_entry = pek->ex_entry;
		pek->ex_entry = pe;

		pi = pei->pt_entry;
		pk = pek->pt_entry;

		memmove(&pbuf, pi, sizeof(struct dos_partition));
		memmove(pi, pk, sizeof(struct dos_partition));
		memmove(pk, &pbuf, sizeof(struct dos_partition));

		partition_set_changed(cxt, i, 1);
		partition_set_changed(cxt, k, 1);
	}

	if (i)
		fix_chain_of_logicals(cxt);

	return 0;
}

/**
 * fdisk_dos_fix_chs:
 * @cxt: fdisk context
 *
 * Fix beginning and ending C/H/S values for every partition
 * according to LBA relative offset, relative beginning and
 * size and fdisk idea of disk geometry (sectors per track
 * and number of heads).
 *
 * Returns: number of fixed (changed) partitions.
 */
int fdisk_dos_fix_chs(struct fdisk_context *cxt)
{
	unsigned int obc, obh, obs;	/* old beginning c, h, s */
	unsigned int oec, oeh, oes;	/* old ending c, h, s */
	unsigned int nbc, nbh, nbs;	/* new beginning c, h, s */
	unsigned int nec, neh, nes;	/* new ending c, h, s */
	fdisk_sector_t l, sects;	/* lba beginning and size */
	struct dos_partition *p;
	struct pte *pe;
	int changed = 0;
	size_t i;

	assert(fdisk_is_label(cxt, DOS));

	for (i = 0; i < cxt->label->nparts_max; i++) {
		p = self_partition(cxt, i);
		if (!p || !is_used_partition(p))
			continue;

		pe = self_pte(cxt, i);

		/* old beginning c, h, s */
		obc = cylinder(p->bs, p->bc);
		obh = p->bh;
		obs = sector(p->bs);

		/* old ending c, h, s */
		oec = cylinder(p->es, p->ec);
		oeh = p->eh;
		oes = sector(p->es);

		/* new beginning c, h, s */
		l = get_abs_partition_start(pe);
		long2chs(cxt, l, &nbc, &nbh, &nbs);
		if (l > UINT32_MAX || nbc >= 1024) {
			nbc = 1023;
			nbh = cxt->geom.heads-1;
			nbs = cxt->geom.sectors;
		}

		/* new ending c, h, s */
		sects = dos_partition_get_size(p);
		long2chs(cxt, l + sects - 1, &nec, &neh, &nes);
		if (lba_overflowed(l, sects) || nec >= 1024) {
			nec = 1023;
			neh = cxt->geom.heads-1;
			nes = cxt->geom.sectors;
		}

		if (obc != nbc || obh != nbh || obs != nbs ||
		    oec != nec || oeh != neh || oes != nes) {
			DBG(LABEL, ul_debug("DOS: changing %zu partition CHS "
				"from (%d, %d, %d)-(%d, %d, %d) "
				"to (%d, %d, %d)-(%d, %d, %d)",
				i, obc, obh, obs, oec, oeh, oes,
				nbc, nbh, nbs, nec, neh, nes));
			p->bc = nbc & 0xff;
			p->bh = nbh;
			p->bs = nbs | ((nbc >> 2) & 0xc0);
			p->ec = nec & 0xff;
			p->eh = neh;
			p->es = nes | ((nec >> 2) & 0xc0);
			partition_set_changed(cxt, i, 1);
			changed++;
		}
	}

	return changed;
}

/* TODO: use fdisk_set_partition() API */
int fdisk_dos_move_begin(struct fdisk_context *cxt, size_t i)
{
	struct pte *pe;
	struct dos_partition *p;
	unsigned int new, free_start, curr_start, last;
	uintmax_t res = 0;
	size_t x;
	int rc;

	assert(cxt);
	assert(fdisk_is_label(cxt, DOS));

	pe = self_pte(cxt, i);
	if (!pe)
		return -EINVAL;

	p = pe->pt_entry;

	if (!is_used_partition(p) || IS_EXTENDED (p->sys_ind)) {
		fdisk_warnx(cxt, _("Partition %zu: no data area."), i + 1);
		return 0;
	}

	/* The safe start is at the second sector, but some use-cases require
	 * to have MBR within the first partition , so default to the first
	 * sector of the disk or at the second sector of the extended partition
	 */
	free_start = pe->offset ? pe->offset + 1 : 0;

	curr_start = get_abs_partition_start(pe);

	/* look for a free space before the current start of the partition */
	for (x = 0; x < cxt->label->nparts_max; x++) {
		unsigned int end;
		struct pte *prev_pe = self_pte(cxt, x);
		struct dos_partition *prev_p;

		assert(prev_pe);

		prev_p = prev_pe->pt_entry;
		if (!prev_p)
			continue;
		end = get_abs_partition_start(prev_pe)
		      + dos_partition_get_size(prev_p);

		if (is_used_partition(prev_p) &&
		    end > free_start && end <= curr_start)
			free_start = end;
	}

	last = get_abs_partition_end(pe);

	rc = fdisk_ask_number(cxt, free_start, curr_start, last,
			_("New beginning of data"), &res);
	if (rc)
		return rc;

	new = res - pe->offset;

	if (new != dos_partition_get_size(p)) {
		unsigned int sects = dos_partition_get_size(p)
				+ dos_partition_get_start(p) - new;

		dos_partition_set_size(p, sects);
		dos_partition_set_start(p, new);
		dos_partition_sync_chs(p, pe->offset, cxt->geom.sectors, cxt->geom.heads);

		partition_set_changed(cxt, i, 1);

		if (new == 0)
			fdisk_info(cxt, _("The new beginning of the partition overlaps the disk "
					  "label area. Be very careful when using the partition. "
					  "You can lose all your partitions on the disk."));
	}

	return rc;
}

static int dos_partition_is_used(
		struct fdisk_context *cxt,
		size_t i)
{
	struct dos_partition *p;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	if (i >= cxt->label->nparts_max)
		return 0;

	p = self_partition(cxt, i);

	return p && !is_cleared_partition(p);
}

static int dos_toggle_partition_flag(
		struct fdisk_context *cxt,
		size_t i,
		unsigned long flag)
{
	struct dos_partition *p;

	assert(cxt);
	assert(cxt->label);
	assert(fdisk_is_label(cxt, DOS));

	if (i >= cxt->label->nparts_max)
		return -EINVAL;

	p = self_partition(cxt, i);
	assert(p);

	switch (flag) {
	case DOS_FLAG_ACTIVE:
		if (IS_EXTENDED(p->sys_ind) && !p->boot_ind)
			fdisk_warnx(cxt, _("Partition %zu: is an extended "
					"partition."), i + 1);

		p->boot_ind = (p->boot_ind ? 0 : ACTIVE_FLAG);
		partition_set_changed(cxt, i, 1);
		fdisk_info(cxt,	p->boot_ind ?
			_("The bootable flag on partition %zu is enabled now.") :
			_("The bootable flag on partition %zu is disabled now."),
			i + 1);
		break;
	default:
		return 1;
	}

	return 0;
}

static const struct fdisk_field dos_fields[] =
{
	/* basic */
	{ FDISK_FIELD_DEVICE,	N_("Device"),	 10,	0 },
	{ FDISK_FIELD_BOOT,	N_("Boot"),	  1,	0 },
	{ FDISK_FIELD_START,	N_("Start"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_END,	N_("End"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_SECTORS,	N_("Sectors"),	  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_CYLINDERS,N_("Cylinders"),  5,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_SIZE,	N_("Size"),	  5,	FDISK_FIELDFL_NUMBER | FDISK_FIELDFL_EYECANDY },
	{ FDISK_FIELD_TYPEID,	N_("Id"),	  2,	FDISK_FIELDFL_NUMBER },
	{ FDISK_FIELD_TYPE,	N_("Type"),	0.1,	0 },

	/* expert mode */
	{ FDISK_FIELD_SADDR,	N_("Start-C/H/S"), 1,   FDISK_FIELDFL_NUMBER | FDISK_FIELDFL_DETAIL },
	{ FDISK_FIELD_EADDR,	N_("End-C/H/S"),   1,   FDISK_FIELDFL_NUMBER | FDISK_FIELDFL_DETAIL },
	{ FDISK_FIELD_ATTR,	N_("Attrs"),	   2,   FDISK_FIELDFL_NUMBER | FDISK_FIELDFL_DETAIL }

};

static const struct fdisk_label_operations dos_operations =
{
	.probe		= dos_probe_label,
	.write		= dos_write_disklabel,
	.verify		= dos_verify_disklabel,
	.create		= dos_create_disklabel,
	.locate		= dos_locate_disklabel,
	.get_item	= dos_get_disklabel_item,
	.set_id		= dos_set_disklabel_id,

	.get_part	= dos_get_partition,
	.set_part	= dos_set_partition,
	.add_part	= dos_add_partition,
	.del_part	= dos_delete_partition,
	.reorder	= dos_reorder,

	.part_toggle_flag = dos_toggle_partition_flag,
	.part_is_used	= dos_partition_is_used,

	.reset_alignment = dos_reset_alignment,

	.deinit		= dos_deinit,
};

/*
 * allocates DOS in-memory stuff
 */
struct fdisk_label *fdisk_new_dos_label(struct fdisk_context *cxt __attribute__ ((__unused__)))
{
	struct fdisk_label *lb;
	struct fdisk_dos_label *dos;

	dos = calloc(1, sizeof(*dos));
	if (!dos)
		return NULL;

	/* initialize generic part of the driver */
	lb = (struct fdisk_label *) dos;
	lb->name = "dos";
	lb->id = FDISK_DISKLABEL_DOS;
	lb->op = &dos_operations;

	lb->parttypes = dos_parttypes;
	lb->nparttypes = ARRAY_SIZE(dos_parttypes) - 1;
	lb->parttype_cuts = dos_parttype_cuts;
	lb->nparttype_cuts = ARRAY_SIZE(dos_parttype_cuts);

	lb->fields = dos_fields;
	lb->nfields = ARRAY_SIZE(dos_fields);

	lb->geom_min.sectors = 1;
	lb->geom_min.heads = 1;
	lb->geom_min.cylinders = 1;

	lb->geom_max.sectors = 63;
	lb->geom_max.heads = 255;
	lb->geom_max.cylinders = 1048576;

	/* return calloc() result to keep static anaylizers happy */
	return (struct fdisk_label *) dos;
}

/**
 * fdisk_dos_enable_compatible:
 * @lb: DOS label (see fdisk_get_label())
 * @enable: 0 or 1
 *
 * Enables deprecated DOS compatible mode, in this mode library checks for
 * cylinders boundary, cases about CHS addressing and another obscure things.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_dos_enable_compatible(struct fdisk_label *lb, int enable)
{
	struct fdisk_dos_label *dos = (struct fdisk_dos_label *) lb;

	if (!lb)
		return -EINVAL;

	dos->compatible = enable;
	if (enable)
		lb->flags |= FDISK_LABEL_FL_REQUIRE_GEOMETRY;
	return 0;
}

/**
 * fdisk_dos_is_compatible:
 * @lb: DOS label
 *
 * Returns: 0 if DOS compatibility disabled, 1 if enabled
 */
int fdisk_dos_is_compatible(struct fdisk_label *lb)
{
	return ((struct fdisk_dos_label *) lb)->compatible;
}
