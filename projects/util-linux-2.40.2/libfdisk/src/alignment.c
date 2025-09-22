
#ifdef HAVE_LIBBLKID
#include <blkid.h>
#endif
#include "blkdev.h"

#include "fdiskP.h"

/**
 * SECTION: alignment
 * @title: Alignment
 * @short_description: functions to align partitions and work with disk topology and geometry
 *
 * The libfdisk aligns the end of the partitions to make it possible to align
 * the next partition to the "grain" (see fdisk_get_grain_size()). The grain is
 * usually 1MiB (or more for devices where optimal I/O is greater than 1MiB).
 *
 * It means that the library does not align strictly to physical sector size
 * (or minimal or optimal I/O), but it uses greater granularity. It makes
 * partition tables more portable. If you copy disk layout from 512-sector to
 * 4K-sector device, all partitions are still aligned to physical sectors.
 *
 * This unified concept also makes partition tables more user friendly, all
 * tables look same, LBA of the first partition is 2048 sectors everywhere, etc.
 *
 * It's recommended to not change any alignment or device properties. All is
 * initialized by default by fdisk_assign_device().
 *
 * Note that terminology used by libfdisk is:
 *   - device properties: I/O limits (topology), geometry, sector size, ...
 *   - alignment: first, last LBA, grain, ...
 *
 * The alignment setting may be modified by disk label driver.
 */

/*
 * Alignment according to logical granularity (usually 1MiB)
 */
static int lba_is_aligned(struct fdisk_context *cxt, uintmax_t lba)
{
	unsigned long granularity = max(cxt->phy_sector_size, cxt->min_io_size);
	uintmax_t offset;

	if (cxt->grain > granularity)
		granularity = cxt->grain;

	offset = (lba * cxt->sector_size) % granularity;

	return !((granularity + cxt->alignment_offset - offset) % granularity);
}

/*
 * Alignment according to physical device topology (usually minimal i/o size)
 */
static int lba_is_phy_aligned(struct fdisk_context *cxt, fdisk_sector_t lba)
{
	unsigned long granularity = max(cxt->phy_sector_size, cxt->min_io_size);
	uintmax_t offset = (lba * cxt->sector_size) % granularity;

	return !((granularity + cxt->alignment_offset - offset) % granularity);
}

/**
 * fdisk_align_lba:
 * @cxt: context
 * @lba: address to align
 * @direction: FDISK_ALIGN_{UP,DOWN,NEAREST}
 *
 * This function aligns @lba to the "grain" (see fdisk_get_grain_size()). If the
 * device uses alignment offset then the result is moved according the offset
 * to be on the physical boundary.
 *
 * Returns: alignment LBA.
 */
fdisk_sector_t fdisk_align_lba(struct fdisk_context *cxt, fdisk_sector_t lba, int direction)
{
	fdisk_sector_t res;

	if (lba_is_aligned(cxt, lba))
		res = lba;
	else {
		fdisk_sector_t sects_in_phy = cxt->grain / cxt->sector_size;

		if (lba < cxt->first_lba)
			res = cxt->first_lba;

		else if (direction == FDISK_ALIGN_UP)
			res = ((lba + sects_in_phy) / sects_in_phy) * sects_in_phy;

		else if (direction == FDISK_ALIGN_DOWN)
			res = (lba / sects_in_phy) * sects_in_phy;

		else /* FDISK_ALIGN_NEAREST */
			res = ((lba + sects_in_phy / 2) / sects_in_phy) * sects_in_phy;

		if (cxt->alignment_offset && !lba_is_aligned(cxt, res) &&
		    res > cxt->alignment_offset / cxt->sector_size) {
			/*
			 * apply alignment_offset
			 *
			 * On disk with alignment compensation physical blocks starts
			 * at LBA < 0 (usually LBA -1). It means we have to move LBA
			 * according the offset to be on the physical boundary.
			 */
			/* fprintf(stderr, "LBA: %llu apply alignment_offset\n", res); */
			res -= (max(cxt->phy_sector_size, cxt->min_io_size) -
					cxt->alignment_offset) / cxt->sector_size;

			if (direction == FDISK_ALIGN_UP && res < lba)
				res += sects_in_phy;
		}
	}
/*
	if (lba != res)
		DBG(CXT, ul_debugobj(cxt, "LBA %12ju aligned-%s %12ju [grain=%lus]",
				(uintmax_t) lba,
				direction == FDISK_ALIGN_UP ? "up  " :
				direction == FDISK_ALIGN_DOWN ? "down" : "near",
				(uintmax_t) res,
				cxt->grain / cxt->sector_size));
	else
		DBG(CXT, ul_debugobj(cxt, "LBA %12ju already aligned", (uintmax_t)lba));
*/
	return res;
}

/**
 * fdisk_align_lba_in_range:
 * @cxt: context
 * @lba: LBA
 * @start: range start
 * @stop: range stop
 *
 * Align @lba, the result has to be between @start and @stop
 *
 * Returns: aligned LBA
 */
fdisk_sector_t fdisk_align_lba_in_range(struct fdisk_context *cxt,
				  fdisk_sector_t lba, fdisk_sector_t start, fdisk_sector_t stop)
{
	fdisk_sector_t res;

	/*DBG(CXT, ul_debugobj(cxt, "LBA: align in range <%ju..%ju>", (uintmax_t) start, (uintmax_t) stop));*/

	if (start + (cxt->grain / cxt->sector_size) <= stop) {
		start = fdisk_align_lba(cxt, start, FDISK_ALIGN_UP);
		stop = fdisk_align_lba(cxt, stop, FDISK_ALIGN_DOWN);
	}

	if (start + (cxt->grain / cxt->sector_size) > stop) {
		DBG(CXT, ul_debugobj(cxt, "LBA: area smaller than grain, don't align"));
		res = lba;
		goto done;
	}

	lba = fdisk_align_lba(cxt, lba, FDISK_ALIGN_NEAREST);

	if (lba < start)
		res = start;
	else if (lba > stop)
		res = stop;
	else
		res = lba;
done:
	DBG(CXT, ul_debugobj(cxt, "%ju in range <%ju..%ju> aligned to %ju",
				(uintmax_t) lba,
				(uintmax_t) start,
				(uintmax_t) stop,
				(uintmax_t) res));
	return res;
}

/**
 * fdisk_lba_is_phy_aligned:
 * @cxt: context
 * @lba: LBA to check
 *
 * Check if the @lba is aligned to physical sector boundary.
 *
 * Returns: 1 if aligned.
 */
int fdisk_lba_is_phy_aligned(struct fdisk_context *cxt, fdisk_sector_t lba)
{
	return lba_is_phy_aligned(cxt, lba);
}

static unsigned long get_sector_size(struct fdisk_context *cxt)
{
	int sect_sz;

	if (!fdisk_is_regfile(cxt) &&
	    !blkdev_get_sector_size(cxt->dev_fd, &sect_sz))
		return (unsigned long) sect_sz;

	return DEFAULT_SECTOR_SIZE;
}

static void recount_geometry(struct fdisk_context *cxt)
{
	if (!cxt->geom.heads)
		cxt->geom.heads = 255;
	if (!cxt->geom.sectors)
		cxt->geom.sectors = 63;

	cxt->geom.cylinders = cxt->total_sectors /
		(cxt->geom.heads * cxt->geom.sectors);
}

/**
 * fdisk_override_geometry:
 * @cxt: fdisk context
 * @cylinders: user specified cylinders
 * @heads: user specified heads
 * @sectors: user specified sectors
 *
 * Overrides auto-discovery. The function fdisk_reset_device_properties()
 * restores the original setting.
 *
 * The difference between fdisk_override_geometry() and fdisk_save_user_geometry()
 * is that saved user geometry is persistent setting and it's applied always
 * when device is assigned to the context or device properties are reset.
 *
 * Returns: 0 on success, < 0 on error.
 */
int fdisk_override_geometry(struct fdisk_context *cxt,
			    unsigned int cylinders,
			    unsigned int heads,
			    unsigned int sectors)
{
	if (!cxt)
		return -EINVAL;
	if (heads)
		cxt->geom.heads = heads;
	if (sectors)
		cxt->geom.sectors = sectors;

	if (cylinders)
		cxt->geom.cylinders = cylinders;
	else
		recount_geometry(cxt);

	fdisk_reset_alignment(cxt);

	DBG(CXT, ul_debugobj(cxt, "override C/H/S: %u/%u/%u",
		(unsigned) cxt->geom.cylinders,
		(unsigned) cxt->geom.heads,
		(unsigned) cxt->geom.sectors));

	return 0;
}

/**
 * fdisk_save_user_geometry:
 * @cxt: context
 * @cylinders: C
 * @heads: H
 * @sectors: S
 *
 * Save user defined geometry to use it for partitioning.
 *
 * The user properties are applied by fdisk_assign_device() or
 * fdisk_reset_device_properties().

 * Returns: <0 on error, 0 on success.
 */
int fdisk_save_user_geometry(struct fdisk_context *cxt,
			    unsigned int cylinders,
			    unsigned int heads,
			    unsigned int sectors)
{
	if (!cxt)
		return -EINVAL;

	if (heads)
		cxt->user_geom.heads = heads > 256 ? 0 : heads;
	if (sectors)
		cxt->user_geom.sectors = sectors >= 64 ? 0 : sectors;
	if (cylinders)
		cxt->user_geom.cylinders = cylinders;

	DBG(CXT, ul_debugobj(cxt, "user C/H/S: %u/%u/%u",
				(unsigned) cxt->user_geom.cylinders,
				(unsigned) cxt->user_geom.heads,
				(unsigned) cxt->user_geom.sectors));

	return 0;
}

/**
 * fdisk_save_user_sector_size:
 * @cxt: context
 * @phy: physical sector size
 * @log: logical sector size
 *
 * Save user defined sector sizes to use it for partitioning.
 *
 * The user properties are applied by fdisk_assign_device() or
 * fdisk_reset_device_properties().
 *
 * Returns: <0 on error, 0 on success.
 */
int fdisk_save_user_sector_size(struct fdisk_context *cxt,
				unsigned int phy,
				unsigned int log)
{
	if (!cxt)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "user phy/log sector size: %u/%u", phy, log));

	cxt->user_pyh_sector = phy;
	cxt->user_log_sector = log;

	return 0;
}

/**
 * fdisk_save_user_grain:
 * @cxt: context
 * @grain: size in bytes (>= 512, multiple of 512)
 *
 * Save user define grain size. The size is used to align partitions.
 *
 * The default is 1MiB (or optimal I/O size if greater than 1MiB). It's strongly
 * recommended to use the default.
 *
 * The smallest possible granularity for partitioning is physical sector size
 * (or minimal I/O size; the bigger number win). If the user's @grain size is
 * too small then the smallest possible granularity is used. It means
 * fdisk_save_user_grain(cxt, 512) forces libfdisk to use grain as small as
 * possible.
 *
 * The setting is applied by fdisk_assign_device() or
 * fdisk_reset_device_properties().
 *
 * Returns: <0 on error, 0 on success.
 */
int fdisk_save_user_grain(struct fdisk_context *cxt, unsigned long grain)
{
	if (!cxt || grain % 512)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "user grain size: %lu", grain));
	cxt->user_grain = grain;
	return 0;
}

/**
 * fdisk_has_user_device_properties:
 * @cxt: context
 *
 * Returns: 1 if user specified any properties
 */
int fdisk_has_user_device_properties(struct fdisk_context *cxt)
{
	return (cxt->user_pyh_sector || cxt->user_log_sector ||
		cxt->user_grain ||
		fdisk_has_user_device_geometry(cxt));
}

int fdisk_has_user_device_geometry(struct fdisk_context *cxt)
{
	return (cxt->user_geom.heads || cxt->user_geom.sectors || cxt->user_geom.cylinders);
}

int fdisk_apply_user_device_properties(struct fdisk_context *cxt)
{
	if (!cxt)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "applying user device properties"));

	if (cxt->user_pyh_sector)
		cxt->phy_sector_size = cxt->user_pyh_sector;
	if (cxt->user_log_sector) {
		uint64_t old_total = cxt->total_sectors;
		uint64_t old_secsz = cxt->sector_size;

		cxt->sector_size = cxt->min_io_size =
			cxt->io_size = cxt->user_log_sector;

		if (cxt->sector_size != old_secsz) {
			cxt->total_sectors = (old_total * (old_secsz/512)) / (cxt->sector_size >> 9);
			DBG(CXT, ul_debugobj(cxt, "new total sectors: %ju", (uintmax_t)cxt->total_sectors));
		}
	}

	if (cxt->user_geom.heads)
		cxt->geom.heads = cxt->user_geom.heads;
	if (cxt->user_geom.sectors)
		cxt->geom.sectors = cxt->user_geom.sectors;

	if (cxt->user_geom.cylinders)
		cxt->geom.cylinders = cxt->user_geom.cylinders;
	else if (cxt->user_geom.heads || cxt->user_geom.sectors)
		recount_geometry(cxt);

	fdisk_reset_alignment(cxt);

	if (cxt->user_grain) {
		unsigned long granularity = max(cxt->phy_sector_size, cxt->min_io_size);

		cxt->grain = cxt->user_grain < granularity ? granularity : cxt->user_grain;
		DBG(CXT, ul_debugobj(cxt, "new grain: %lu", cxt->grain));
	}

	if (cxt->firstsector_bufsz != cxt->sector_size)
		fdisk_read_firstsector(cxt);

	DBG(CXT, ul_debugobj(cxt, "new C/H/S: %u/%u/%u",
		(unsigned) cxt->geom.cylinders,
		(unsigned) cxt->geom.heads,
		(unsigned) cxt->geom.sectors));
	DBG(CXT, ul_debugobj(cxt, "new log/phy sector size: %u/%u",
		(unsigned) cxt->sector_size,
		(unsigned) cxt->phy_sector_size));

	return 0;
}

void fdisk_zeroize_device_properties(struct fdisk_context *cxt)
{
	assert(cxt);

	cxt->io_size = 0;
	cxt->optimal_io_size = 0;
	cxt->min_io_size = 0;
	cxt->phy_sector_size = 0;
	cxt->sector_size = 0;
	cxt->alignment_offset = 0;
	cxt->grain = 0;
	cxt->first_lba = 0;
	cxt->last_lba = 0;
	cxt->total_sectors = 0;

	memset(&cxt->geom, 0, sizeof(struct fdisk_geometry));
}

/**
 * fdisk_reset_device_properties:
 * @cxt: context
 *
 * Resets and discovery topology (I/O limits), geometry, re-read the first
 * rector on the device if necessary and apply user device setting (geometry
 * and sector size), then initialize alignment according to label driver (see
 * fdisk_reset_alignment()).
 *
 * You don't have to use this function by default, fdisk_assign_device() is
 * smart enough to initialize all necessary setting.
 *
 * Returns: 0 on success, <0 on error.
 */
int fdisk_reset_device_properties(struct fdisk_context *cxt)
{
	int rc;

	if (!cxt)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "*** resetting device properties"));

	fdisk_zeroize_device_properties(cxt);
	fdisk_discover_topology(cxt);
	fdisk_discover_geometry(cxt);

	rc = fdisk_read_firstsector(cxt);
	if (rc)
		return rc;

	fdisk_apply_user_device_properties(cxt);
	return 0;
}

/*
 * Generic (label independent) geometry
 */
int fdisk_discover_geometry(struct fdisk_context *cxt)
{
	fdisk_sector_t nsects = 0;
	unsigned int h = 0, s = 0;

	assert(cxt);
	assert(cxt->geom.heads == 0);

	DBG(CXT, ul_debugobj(cxt, "%s: discovering geometry...", cxt->dev_path));

	if (fdisk_is_regfile(cxt))
		cxt->total_sectors = cxt->dev_st.st_size / cxt->sector_size;
	else {
		/* get number of 512-byte sectors, and convert it the real sectors */
		if (!blkdev_get_sectors(cxt->dev_fd, (unsigned long long *) &nsects))
			cxt->total_sectors = (nsects / (cxt->sector_size >> 9));

		/* what the kernel/bios thinks the geometry is */
		blkdev_get_geometry(cxt->dev_fd, &h, &s);
	}

	DBG(CXT, ul_debugobj(cxt, "total sectors: %ju (ioctl=%ju)",
				(uintmax_t) cxt->total_sectors,
				(uintmax_t) nsects));

	cxt->geom.cylinders = 0;
	cxt->geom.heads = h;
	cxt->geom.sectors = s;

	/* obtained heads and sectors */
	recount_geometry(cxt);

	DBG(CXT, ul_debugobj(cxt, "result: C/H/S: %u/%u/%u",
			       (unsigned) cxt->geom.cylinders,
			       (unsigned) cxt->geom.heads,
			       (unsigned) cxt->geom.sectors));
	return 0;
}

int fdisk_discover_topology(struct fdisk_context *cxt)
{
#ifdef HAVE_LIBBLKID
	blkid_probe pr;
#endif
	assert(cxt);
	assert(cxt->sector_size == 0);

	DBG(CXT, ul_debugobj(cxt, "%s: discovering topology...", cxt->dev_path));
#ifdef HAVE_LIBBLKID
	DBG(CXT, ul_debugobj(cxt, "initialize libblkid prober"));

	pr = blkid_new_probe();
	if (pr && blkid_probe_set_device(pr, cxt->dev_fd, 0, 0) == 0) {
		blkid_topology tp = blkid_probe_get_topology(pr);

		if (tp) {
			cxt->min_io_size = blkid_topology_get_minimum_io_size(tp);
			cxt->optimal_io_size = blkid_topology_get_optimal_io_size(tp);
			cxt->phy_sector_size = blkid_topology_get_physical_sector_size(tp);
			cxt->alignment_offset = blkid_topology_get_alignment_offset(tp);

			/* I/O size used by fdisk */
			cxt->io_size = cxt->optimal_io_size;
			if (!cxt->io_size)
				/* optimal I/O is optional, default to minimum IO */
				cxt->io_size = cxt->min_io_size;

			if (cxt->io_size && cxt->phy_sector_size) {
				if (cxt->io_size == 33553920) {
					/* 33553920 (32 MiB - 512) is always a controller error */
					DBG(CXT, ul_debugobj(cxt, "ignore bad I/O size 33553920"));
					cxt->io_size = cxt->phy_sector_size;
				} else if ((cxt->io_size % cxt->phy_sector_size) != 0) {
					/* ignore optimal I/O if not aligned to phy.sector size */
					DBG(CXT, ul_debugobj(cxt, "ignore misaligned I/O size"));
					cxt->io_size = cxt->phy_sector_size;
				}
			}

		}
	}
	blkid_free_probe(pr);
#endif

	cxt->sector_size = get_sector_size(cxt);
	if (!cxt->phy_sector_size) /* could not discover physical size */
		cxt->phy_sector_size = cxt->sector_size;

	/* no blkid or error, use default values */
	if (!cxt->min_io_size)
		cxt->min_io_size = cxt->sector_size;
	if (!cxt->io_size)
		cxt->io_size = cxt->sector_size;

	DBG(CXT, ul_debugobj(cxt, "result: log/phy sector size: %ld/%ld",
			cxt->sector_size, cxt->phy_sector_size));
	DBG(CXT, ul_debugobj(cxt, "result: fdisk/optimal/minimal io: %ld/%ld/%ld",
		       cxt->io_size, cxt->optimal_io_size, cxt->min_io_size));
	return 0;
}

static int has_topology(struct fdisk_context *cxt)
{
	/*
	 * Assume that the device provides topology info if
	 * optimal_io_size is set or alignment_offset is set or
	 * minimum_io_size is not power of 2.
	 */
	if (cxt &&
	    (cxt->optimal_io_size ||
	     cxt->alignment_offset ||
	     !is_power_of_2(cxt->min_io_size)))
		return 1;
	return 0;
}

/*
 * The LBA of the first partition is based on the device geometry and topology.
 * This offset is generic (and recommended) for all labels.
 *
 * Returns: 0 on error or number of logical sectors.
 */
static fdisk_sector_t topology_get_first_lba(struct fdisk_context *cxt)
{
	fdisk_sector_t x = 0, res;

	if (!cxt)
		return 0;

	if (!cxt->io_size)
		fdisk_discover_topology(cxt);

	/*
	 * Align the begin of partitions to:
	 *
	 * a) topology
	 *  a2) alignment offset
	 *  a1) or physical sector (minimal_io_size, aka "grain")
	 *
	 * b) or default to 1MiB (2048 sectors, Windows Vista default)
	 *
	 * c) or for very small devices use 1 phy.sector
	 */
	if (has_topology(cxt)) {
		if (cxt->alignment_offset)
			x = cxt->alignment_offset;
		else if (cxt->io_size > 2048 * 512)
			x = cxt->io_size;
	}
	/* default to 1MiB */
	if (!x)
		x = 2048 * 512;

	res = x / cxt->sector_size;

	/* don't use huge offset on small devices */
	if (cxt->total_sectors <= res * 4)
		res = cxt->phy_sector_size / cxt->sector_size;

	return res;
}

static unsigned long topology_get_grain(struct fdisk_context *cxt)
{
	unsigned long res;

	if (!cxt)
		return 0;

	if (!cxt->io_size)
		fdisk_discover_topology(cxt);

	res = cxt->io_size;

	/* use 1MiB grain always when possible */
	if (res < 2048 * 512)
		res = 2048 * 512;

	/* don't use huge grain on small devices */
	if (cxt->total_sectors <= (res * 4 / cxt->sector_size))
		res = cxt->phy_sector_size;

	return res;
}

/* apply label alignment setting to the context -- if not sure use
 * fdisk_reset_alignment()
 */
int fdisk_apply_label_device_properties(struct fdisk_context *cxt)
{
	int rc = 0;

	if (cxt->label && cxt->label->op->reset_alignment) {
		DBG(CXT, ul_debugobj(cxt, "applying label device properties..."));
		rc = cxt->label->op->reset_alignment(cxt);
	}
	return rc;
}

/**
 * fdisk_reset_alignment:
 * @cxt: fdisk context
 *
 * Resets alignment setting to the default and label specific values. This
 * function does not change device properties (I/O limits, geometry etc.).
 *
 * Returns: 0 on success, < 0 in case of error.
 */
int fdisk_reset_alignment(struct fdisk_context *cxt)
{
	int rc = 0;

	if (!cxt)
		return -EINVAL;

	DBG(CXT, ul_debugobj(cxt, "resetting alignment..."));

	/* default */
	cxt->grain = topology_get_grain(cxt);
	cxt->first_lba = topology_get_first_lba(cxt);
	cxt->last_lba = cxt->total_sectors - 1;

	/* overwrite default by label stuff */
	rc = fdisk_apply_label_device_properties(cxt);

	DBG(CXT, ul_debugobj(cxt, "alignment reset to: "
			    "first LBA=%ju, last LBA=%ju, grain=%lu [rc=%d]",
			    (uintmax_t) cxt->first_lba, (uintmax_t) cxt->last_lba,
			    cxt->grain,	rc));
	return rc;
}


fdisk_sector_t fdisk_scround(struct fdisk_context *cxt, fdisk_sector_t num)
{
	fdisk_sector_t un = fdisk_get_units_per_sector(cxt);
	return (num + un - 1) / un;
}

fdisk_sector_t fdisk_cround(struct fdisk_context *cxt, fdisk_sector_t num)
{
	return fdisk_use_cylinders(cxt) ?
			(num / fdisk_get_units_per_sector(cxt)) + 1 : num;
}

