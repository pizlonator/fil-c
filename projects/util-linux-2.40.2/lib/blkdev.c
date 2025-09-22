/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com>
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>

#ifdef HAVE_LINUX_FD_H
#include <linux/fd.h>
#endif

#ifdef HAVE_LINUX_BLKZONED_H
#include <linux/blkzoned.h>
#endif

#ifdef HAVE_SYS_DISKLABEL_H
#include <sys/disklabel.h>
#endif

#ifdef HAVE_SYS_DISK_H
# include <sys/disk.h>
#endif

#ifndef EBADFD
# define EBADFD 77		/* File descriptor in bad state */
#endif

#include "all-io.h"
#include "blkdev.h"
#include "c.h"
#include "linux_version.h"
#include "fileutils.h"
#include "nls.h"

static long
blkdev_valid_offset (int fd, off_t offset) {
	char ch;

	if (lseek (fd, offset, 0) < 0)
		return 0;
	if (read_all (fd, &ch, 1) < 1)
		return 0;
	return 1;
}

int is_blkdev(int fd)
{
	struct stat st;
	return (fstat(fd, &st) == 0 && S_ISBLK(st.st_mode));
}

off_t
blkdev_find_size (int fd) {
	off_t high, low = 0;

	for (high = 1024; blkdev_valid_offset (fd, high); ) {
		if (high == SINT_MAX(off_t)) {
			errno = EFBIG;
			return -1;
		}

		low = high;

		if (high >= SINT_MAX(off_t)/2)
			high = SINT_MAX(off_t);
		else
			high *= 2;
	}

	while (low < high - 1)
	{
		off_t mid = (low + high) / 2;

		if (blkdev_valid_offset (fd, mid))
			low = mid;
		else
			high = mid;
	}
	blkdev_valid_offset (fd, 0);
	return (low + 1);
}

/* get size in bytes */
int
blkdev_get_size(int fd, unsigned long long *bytes)
{
#ifdef DKIOCGETBLOCKCOUNT
	/* Apple Darwin */
	if (ioctl(fd, DKIOCGETBLOCKCOUNT, bytes) >= 0) {
		*bytes <<= 9;
		return 0;
	}
#endif

#ifdef BLKGETSIZE64
	if (ioctl(fd, BLKGETSIZE64, bytes) >= 0)
		return 0;
#endif

#ifdef BLKGETSIZE
	{
		unsigned long size;

		if (ioctl(fd, BLKGETSIZE, &size) >= 0) {
			*bytes = ((unsigned long long)size << 9);
			return 0;
		}
	}

#endif /* BLKGETSIZE */

#ifdef DIOCGMEDIASIZE
	/* FreeBSD */
	if (ioctl(fd, DIOCGMEDIASIZE, bytes) >= 0)
		return 0;
#endif

#ifdef FDGETPRM
	{
		struct floppy_struct this_floppy;

		if (ioctl(fd, FDGETPRM, &this_floppy) >= 0) {
			*bytes = ((unsigned long long) this_floppy.size) << 9;
			return 0;
		}
	}
#endif /* FDGETPRM */

#if defined(HAVE_SYS_DISKLABEL_H) && defined(DIOCGDINFO)
	{
		/*
		 * This code works for FreeBSD 4.11 i386, except for the full device
		 * (such as /dev/ad0). It doesn't work properly for newer FreeBSD
		 * though. FreeBSD >= 5.0 should be covered by the DIOCGMEDIASIZE
		 * above however.
		 *
		 * Note that FreeBSD >= 4.0 has disk devices as unbuffered (raw,
		 * character) devices, so we need to check for S_ISCHR, too.
		 */
		int part = -1;
		struct disklabel lab;
		struct partition *pp;
		struct stat st;

		if ((fstat(fd, &st) >= 0) &&
		    (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode)))
			part = st.st_rdev & 7;

		if (part >= 0 && (ioctl(fd, DIOCGDINFO, (char *)&lab) >= 0)) {
			pp = &lab.d_partitions[part];
			if (pp->p_size) {
				 *bytes = pp->p_size << 9;
				 return 0;
			}
		}
	}
#endif /* defined(HAVE_SYS_DISKLABEL_H) && defined(DIOCGDINFO) */

	{
		struct stat st;

		if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
			*bytes = st.st_size;
			return 0;
		}
		if (!S_ISBLK(st.st_mode)) {
			errno = ENOTBLK;
			return -1;
		}
	}

	*bytes = blkdev_find_size(fd);
	return 0;
}

/* get 512-byte sector count */
int
blkdev_get_sectors(int fd, unsigned long long *sectors)
{
	unsigned long long bytes;

	if (blkdev_get_size(fd, &bytes) == 0) {
		*sectors = (bytes >> 9);
		return 0;
	}

	return -1;
}

/*
 * Get logical sector size.
 *
 * This is the smallest unit the storage device can
 * address. It is typically 512 bytes.
 */
#ifdef BLKSSZGET
int blkdev_get_sector_size(int fd, int *sector_size)
{
	if (ioctl(fd, BLKSSZGET, sector_size) >= 0)
		return 0;
	return -1;
}
#else
int blkdev_get_sector_size(int fd __attribute__((__unused__)), int *sector_size)
{
	*sector_size = DEFAULT_SECTOR_SIZE;
	return 0;
}
#endif

/*
 * Get physical block device size. The BLKPBSZGET is supported since Linux
 * 2.6.32. For old kernels is probably the best to assume that physical sector
 * size is the same as logical sector size.
 *
 * Example:
 *
 * rc = blkdev_get_physector_size(fd, &physec);
 * if (rc || physec == 0) {
 *	rc = blkdev_get_sector_size(fd, &physec);
 *	if (rc)
 *		physec = DEFAULT_SECTOR_SIZE;
 * }
 */
#ifdef BLKPBSZGET
int blkdev_get_physector_size(int fd, int *sector_size)
{
	if (ioctl(fd, BLKPBSZGET, sector_size) >= 0)
    {
		return 0;
    }
	return -1;
}
#else
int blkdev_get_physector_size(int fd __attribute__((__unused__)), int *sector_size)
{
	*sector_size = DEFAULT_SECTOR_SIZE;
	return 0;
}
#endif

/*
 * Return the alignment status of a device
 */
#ifdef BLKALIGNOFF
int blkdev_is_misaligned(int fd)
{
	int aligned;

	if (ioctl(fd, BLKALIGNOFF, &aligned) < 0)
		return 0;			/* probably kernel < 2.6.32 */
	/*
	 * Note that kernel returns -1 as alignment offset if no compatible
	 * sizes and alignments exist for stacked devices
	 */
	return aligned != 0 ? 1 : 0;
}
#else
int blkdev_is_misaligned(int fd __attribute__((__unused__)))
{
	return 0;
}
#endif

int open_blkdev_or_file(const struct stat *st, const char *name, const int oflag)
{
	int fd;

	if (S_ISBLK(st->st_mode)) {
		fd = open(name, oflag | O_EXCL);
	} else
		fd = open(name, oflag);
	if (-1 < fd && !is_same_inode(fd, st)) {
		close(fd);
		errno = EBADFD;
		return -1;
	}
	if (-1 < fd && S_ISBLK(st->st_mode) && blkdev_is_misaligned(fd))
		warnx(_("warning: %s is misaligned"), name);
	return fd;
}

#ifdef CDROM_GET_CAPABILITY
int blkdev_is_cdrom(int fd)
{
	int ret;

	if ((ret = ioctl(fd, CDROM_GET_CAPABILITY, NULL)) < 0)
		return 0;

	return ret;
}
#else
int blkdev_is_cdrom(int fd __attribute__((__unused__)))
{
	return 0;
}
#endif

/*
 * Get kernel's interpretation of the device's geometry.
 *
 * Returns the heads and sectors - but not cylinders
 * as it's truncated for disks with more than 65535 tracks.
 *
 * Note that this is deprecated in favor of LBA addressing.
 */
#ifdef HDIO_GETGEO
int blkdev_get_geometry(int fd, unsigned int *h, unsigned int *s)
{
	struct hd_geometry geometry;

	if (ioctl(fd, HDIO_GETGEO, &geometry) == 0) {
		*h = geometry.heads;
		*s = geometry.sectors;
		return 0;
	}
#else
int blkdev_get_geometry(int fd __attribute__((__unused__)),
		unsigned int *h, unsigned int *s)
{
	*h = 0;
	*s = 0;
#endif
	return -1;
}

/*
 * Convert scsi type to human readable string.
 */
const char *blkdev_scsi_type_to_name(int type)
{
	switch (type) {
	case SCSI_TYPE_DISK:
		return "disk";
	case SCSI_TYPE_TAPE:
		return "tape";
	case SCSI_TYPE_PRINTER:
		return "printer";
	case SCSI_TYPE_PROCESSOR:
		return "processor";
	case SCSI_TYPE_WORM:
		return "worm";
	case SCSI_TYPE_ROM:
		return "rom";
	case SCSI_TYPE_SCANNER:
		return "scanner";
	case SCSI_TYPE_MOD:
		return "mo-disk";
	case SCSI_TYPE_MEDIUM_CHANGER:
		return "changer";
	case SCSI_TYPE_COMM:
		return "comm";
	case SCSI_TYPE_RAID:
		return "raid";
	case SCSI_TYPE_ENCLOSURE:
		return "enclosure";
	case SCSI_TYPE_RBC:
		return "rbc";
	case SCSI_TYPE_OSD:
		return "osd";
	case SCSI_TYPE_NO_LUN:
		return "no-lun";
	default:
		break;
	}
	return NULL;
}

/* return 0 on success */
int blkdev_lock(int fd, const char *devname, const char *lockmode)
{
	int oper, rc, msg = 0;

	if (!lockmode)
		lockmode = getenv("LOCK_BLOCK_DEVICE");
	if (!lockmode)
		return 0;

	if (strcasecmp(lockmode, "yes") == 0 ||
	    strcmp(lockmode, "1") == 0)
		oper = LOCK_EX;

	else if (strcasecmp(lockmode, "nonblock") == 0)
		oper = LOCK_EX | LOCK_NB;

	else if (strcasecmp(lockmode, "no") == 0 ||
		 strcmp(lockmode, "0") == 0)
		return 0;
	else {
		warnx(_("unsupported lock mode: %s"), lockmode);
		return -EINVAL;
	}

	if (!(oper & LOCK_NB)) {
		/* Try non-block first to provide message */
		rc = flock(fd, oper | LOCK_NB);
		if (rc == 0)
			return 0;
		if (rc != 0 && errno == EWOULDBLOCK) {
			fprintf(stderr, _("%s: %s: device already locked, waiting to get lock ... "),
					program_invocation_short_name, devname);
			msg = 1;
		}
	}
	rc = flock(fd, oper);
	if (rc != 0) {
		switch (errno) {
		case EWOULDBLOCK: /* LOCK_NB */
			warnx(_("%s: device already locked"), devname);
			break;
		default:
			warn(_("%s: failed to get lock"), devname);
		}
	} else if (msg)
		fprintf(stderr, _("OK\n"));
	return rc;
}

#ifdef HAVE_LINUX_BLKZONED_H
struct blk_zone_report *blkdev_get_zonereport(int fd, uint64_t sector, uint32_t nzones)
{
	struct blk_zone_report *rep;
	size_t rep_size;
	int ret;

	rep_size = sizeof(struct blk_zone_report) + sizeof(struct blk_zone) * 2;
	rep = calloc(1, rep_size);
	if (!rep)
		return NULL;

	rep->sector = sector;
	rep->nr_zones = nzones;

	ret = ioctl(fd, BLKREPORTZONE, rep);
	if (ret || rep->nr_zones != nzones) {
		free(rep);
		return NULL;
	}

	return rep;
}
#endif


#ifdef TEST_PROGRAM_BLKDEV
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
int
main(int argc, char **argv)
{
	unsigned long long bytes;
	unsigned long long sectors;
	int sector_size, phy_sector_size;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s device\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if ((fd = open(argv[1], O_RDONLY|O_CLOEXEC)) < 0)
		err(EXIT_FAILURE, "open %s failed", argv[1]);

	if (blkdev_get_size(fd, &bytes) < 0)
		err(EXIT_FAILURE, "blkdev_get_size() failed");
	if (blkdev_get_sectors(fd, &sectors) < 0)
		err(EXIT_FAILURE, "blkdev_get_sectors() failed");
	if (blkdev_get_sector_size(fd, &sector_size) < 0)
		err(EXIT_FAILURE, "blkdev_get_sector_size() failed");
	if (blkdev_get_physector_size(fd, &phy_sector_size) < 0)
		err(EXIT_FAILURE, "blkdev_get_physector_size() failed");

	printf("          bytes: %llu\n", bytes);
	printf("        sectors: %llu\n", sectors);
	printf("    sector size: %d\n", sector_size);
	printf("phy-sector size: %d\n", phy_sector_size);

	return EXIT_SUCCESS;
}
#endif /* TEST_PROGRAM_BLKDEV */
