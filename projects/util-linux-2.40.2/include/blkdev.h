/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com>
 */
#ifndef BLKDEV_H
#define BLKDEV_H

#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_IOCCOM_H
# include <sys/ioccom.h> /* for _IO macro on e.g. Solaris */
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

#ifdef HAVE_SYS_MKDEV_H
# include <sys/mkdev.h>		/* major and minor on Solaris */
#endif

#define DEFAULT_SECTOR_SIZE       512

#ifdef __linux__
/* very basic ioctls, should be available everywhere */
# ifndef BLKROSET
#  define BLKROSET   _IO(0x12,93)	/* set device read-only (0 = read-write) */
#  define BLKROGET   _IO(0x12,94)	/* get read-only status (0 = read_write) */
#  define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#  define BLKGETSIZE _IO(0x12,96)	/* return device size /512 (long *arg) */
#  define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#  define BLKRASET   _IO(0x12,98)	/* set read ahead for block device */
#  define BLKRAGET   _IO(0x12,99)	/* get current read ahead setting */
#  define BLKFRASET  _IO(0x12,100)	/* set filesystem (mm/filemap.c) read-ahead */
#  define BLKFRAGET  _IO(0x12,101)	/* get filesystem (mm/filemap.c) read-ahead */
#  define BLKSECTSET _IO(0x12,102)	/* set max sectors per request (ll_rw_blk.c) */
#  define BLKSECTGET _IO(0x12,103)	/* get max sectors per request (ll_rw_blk.c) */
#  define BLKSSZGET  _IO(0x12,104)	/* get block device sector size */

/* ioctls introduced in 2.2.16, removed in 2.5.58 */
#  define BLKELVGET  _IOR(0x12,106,size_t) /* elevator get */
#  define BLKELVSET  _IOW(0x12,107,size_t) /* elevator set */

#  define BLKBSZGET  _IOR(0x12,112,size_t)
#  define BLKBSZSET  _IOW(0x12,113,size_t)
# endif /* !BLKROSET */

# ifndef BLKGETSIZE64
#  define BLKGETSIZE64 _IOR(0x12,114,size_t) /* return device size in bytes (u64 *arg) */
# endif

/* block device topology ioctls, introduced in 2.6.32 (commit ac481c20) */
# ifndef BLKIOMIN
#  define BLKIOMIN   _IO(0x12,120)
#  define BLKIOOPT   _IO(0x12,121)
#  define BLKALIGNOFF _IO(0x12,122)
#  define BLKPBSZGET _IO(0x12,123)
# endif

/* discard zeroes support, introduced in 2.6.33 (commit 98262f27) */
# ifndef BLKDISCARDZEROES
#  define BLKDISCARDZEROES _IO(0x12,124)
# endif

/* disk sequence number, introduced in 5.15 (commit 7957d93b) */
# ifndef BLKGETDISKSEQ
#  define BLKGETDISKSEQ _IOR(0x12, 128, uint64_t)
# endif

/* filesystem freeze, introduced in 2.6.29 (commit fcccf502) */
# ifndef FIFREEZE
#  define FIFREEZE   _IOWR('X', 119, int)    /* Freeze */
#  define FITHAW     _IOWR('X', 120, int)    /* Thaw */
# endif

/* uniform CD-ROM information */
# ifndef CDROM_GET_CAPABILITY
#  define CDROM_GET_CAPABILITY 0x5331
# endif

#endif /* __linux */


#ifdef APPLE_DARWIN
# define BLKGETSIZE DKIOCGETBLOCKCOUNT32
#endif

#ifndef HDIO_GETGEO
# ifdef __linux__
#  define HDIO_GETGEO 0x0301
# endif

struct hd_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;	/* truncated */
	unsigned long start;
};
#endif /* HDIO_GETGEO */


/* are we working with block device? */
int is_blkdev(int fd);

/* open block device or file */
int open_blkdev_or_file(const struct stat *st, const char *name, const int oflag);

/* Determine size in bytes */
off_t blkdev_find_size (int fd);

/* get size in bytes */
int blkdev_get_size(int fd, unsigned long long *bytes);

/* get 512-byte sector count */
int blkdev_get_sectors(int fd, unsigned long long *sectors);

/* get hardware sector size */
int blkdev_get_sector_size(int fd, int *sector_size);

/* specifies whether or not the device is misaligned */
int blkdev_is_misaligned(int fd);

/* get physical block device size */
int blkdev_get_physector_size(int fd, int *sector_size);

/* is the device cdrom capable? */
int blkdev_is_cdrom(int fd);

/* get device's geometry - legacy */
int blkdev_get_geometry(int fd, unsigned int *h, unsigned int *s);

/* SCSI device types.  Copied almost as-is from kernel header
 * (include/scsi/scsi_proto.h). */
#define SCSI_TYPE_DISK			0x00
#define SCSI_TYPE_TAPE			0x01
#define SCSI_TYPE_PRINTER		0x02
#define SCSI_TYPE_PROCESSOR		0x03	/* HP scanners use this */
#define SCSI_TYPE_WORM			0x04	/* Treated as ROM by our system */
#define SCSI_TYPE_ROM			0x05
#define SCSI_TYPE_SCANNER		0x06
#define SCSI_TYPE_MOD			0x07	/* Magneto-optical disk - treated as SCSI_TYPE_DISK */
#define SCSI_TYPE_MEDIUM_CHANGER	0x08
#define SCSI_TYPE_COMM			0x09	/* Communications device */
#define SCSI_TYPE_RAID			0x0c
#define SCSI_TYPE_ENCLOSURE		0x0d	/* Enclosure Services Device */
#define SCSI_TYPE_RBC			0x0e
#define SCSI_TYPE_OSD			0x11
#define SCSI_TYPE_NO_LUN		0x7f

/* convert scsi type code to name */
const char *blkdev_scsi_type_to_name(int type);

int blkdev_lock(int fd, const char *devname, const char *lockmode);
#ifdef HAVE_LINUX_BLKZONED_H
struct blk_zone_report *blkdev_get_zonereport(int fd, uint64_t sector, uint32_t nzones);
#else
static inline struct blk_zone_report *blkdev_get_zonereport(int fd __attribute__((unused)),
							    uint64_t sector __attribute__((unused)),
							    uint32_t nzones __attribute__((unused)))
{
	return NULL;
}
#endif

#endif /* BLKDEV_H */
