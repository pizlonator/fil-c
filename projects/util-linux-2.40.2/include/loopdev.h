/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_LOOPDEV_H
#define UTIL_LINUX_LOOPDEV_H

#include "sysfs.h"

/*
 * loop_info.lo_encrypt_type
 */
#define LO_CRYPT_NONE	0
#define LO_CRYPT_XOR	1
#define LO_CRYPT_DES	2
#define LO_CRYPT_CRYPTOAPI 18

#define LOOP_SET_FD		0x4C00
#define LOOP_CLR_FD		0x4C01
/*
 * Obsolete (kernel < 2.6)
 *
 * #define LOOP_SET_STATUS	0x4C02
 * #define LOOP_GET_STATUS	0x4C03
 */
#define LOOP_SET_STATUS64	0x4C04
#define LOOP_GET_STATUS64	0x4C05
/* #define LOOP_CHANGE_FD	0x4C06 */
#define LOOP_SET_CAPACITY	0x4C07
#define LOOP_SET_DIRECT_IO	0x4C08
#define LOOP_SET_BLOCK_SIZE	0x4C09

/* /dev/loop-control interface */
#ifndef LOOP_CTL_ADD
# define LOOP_CTL_ADD		0x4C80
# define LOOP_CTL_REMOVE	0x4C81
# define LOOP_CTL_GET_FREE	0x4C82
#endif

/*
 * loop_info.lo_flags
 */
enum {
	LO_FLAGS_READ_ONLY  = 1,
	LO_FLAGS_USE_AOPS   = 2,
	LO_FLAGS_AUTOCLEAR  = 4,	/* kernel >= 2.6.25 */
	LO_FLAGS_PARTSCAN   = 8,	/* kernel >= 3.2 */
	LO_FLAGS_DIRECT_IO  = 16,	/* kernel >= 4.2 */
};

#define LO_NAME_SIZE	64
#define LO_KEY_SIZE	32

/*
 * Linux LOOP_{SET,GET}_STATUS64 ioctl struct
 */
struct loop_info64 {
	uint64_t	lo_device;
	uint64_t	lo_inode;
	uint64_t	lo_rdevice;
	uint64_t	lo_offset;
	uint64_t	lo_sizelimit; /* bytes, 0 == max available */
	uint32_t	lo_number;
	uint32_t	lo_encrypt_type;
	uint32_t	lo_encrypt_key_size;
	uint32_t	lo_flags;
	uint8_t		lo_file_name[LO_NAME_SIZE];
	uint8_t		lo_crypt_name[LO_NAME_SIZE];
	uint8_t		lo_encrypt_key[LO_KEY_SIZE];
	uint64_t	lo_init[2];
};

#ifndef LOOP_CONFIGURE
/*
 * Since Linux v5.8-rc1 (commit 3448914e8cc550ba792d4ccc74471d1ca4293aae)
 */
# define LOOP_CONFIGURE		0x4C0A
struct loop_config {
  uint32_t fd;
  uint32_t block_size;
  struct loop_info64 info;
  uint64_t __reserved[8];
};
#endif

#define LOOPDEV_MAJOR		7	/* loop major number */
#define LOOPDEV_DEFAULT_NNODES	8	/* default number of loop devices */

struct loopdev_iter {
	FILE		*proc;		/* /proc/partitions */
	DIR		*sysblock;	/* /sys/block */
	int		ncur;		/* current position */
	int		*minors;	/* ary of minor numbers (when scan whole /dev) */
	int		nminors;	/* number of items in *minors */
	int		ct_perm;	/* count permission problems */
	int		ct_succ;	/* count number of detected devices */

	unsigned int	done:1;		/* scanning done */
	unsigned int	default_check:1;/* check first LOOPDEV_NLOOPS */
	int		flags;		/* LOOPITER_FL_* flags */
};

enum {
	LOOPITER_FL_FREE	= (1 << 0),
	LOOPITER_FL_USED	= (1 << 1)
};

/*
 * handler for work with loop devices
 */
struct loopdev_cxt {
	char		device[128];	/* device path (e.g. /dev/loop<N>) */
	char		*filename;	/* backing file for loopcxt_set_... */
	int		fd;		/* open(/dev/looo<N>) */
	dev_t		devno;		/* loop device devno from /sys */
	mode_t		mode;		/* fd mode O_{RDONLY,RDWR} */
	uint64_t	blocksize;	/* used by loopcxt_setup_device() */

	int		flags;		/* LOOPDEV_FL_* flags */
	unsigned int	has_info:1;	/* .info contains data */
	unsigned int	extra_check:1;	/* unusual stuff for iterator */
	unsigned int	info_failed:1;	/* LOOP_GET_STATUS ioctl failed */
	unsigned int    control_ok:1;	/* /dev/loop-control success */
	unsigned int	is_lost:1;	/* device in /sys, but missing in /dev */

	struct path_cxt		*sysfs; /* pointer to /sys/dev/block/<maj:min>/ */
	struct loop_config	config;	/* for GET/SET ioctl */
	struct loopdev_iter	iter;	/* scans /sys or /dev for used/free devices */
};

#define UL_LOOPDEVCXT_EMPTY { .fd = -1  }

/*
 * loopdev_cxt.flags
 */
enum {
	LOOPDEV_FL_OFFSET	= (1 << 4),
	LOOPDEV_FL_NOSYSFS	= (1 << 5),
	LOOPDEV_FL_NOIOCTL	= (1 << 6),
	LOOPDEV_FL_DEVSUBDIR	= (1 << 7),
	LOOPDEV_FL_CONTROL	= (1 << 8),	/* system with /dev/loop-control */
	LOOPDEV_FL_SIZELIMIT	= (1 << 9)
};

/*
 * High-level
 */
extern int loopmod_supports_partscan(void);

extern int is_loopdev(const char *device);
extern int loopdev_is_autoclear(const char *device);

extern char *loopdev_get_backing_file(const char *device);
extern dev_t loopcxt_get_devno(struct loopdev_cxt *lc);
extern int loopdev_has_backing_file(const char *device);
extern int loopcxt_is_lost(struct loopdev_cxt *lc);
extern int loopdev_is_used(const char *device, const char *filename,
			   uint64_t offset, uint64_t sizelimit, int flags);
extern char *loopdev_find_by_backing_file(const char *filename,
				uint64_t offset, uint64_t sizelimit, int flags);
extern int loopcxt_find_unused(struct loopdev_cxt *lc);
extern int loopdev_delete(const char *device);
extern int loopdev_count_by_backing_file(const char *filename, char **loopdev);

/*
 * Low-level
 */
extern int loopcxt_init(struct loopdev_cxt *lc, int flags)
				__attribute__ ((warn_unused_result));
extern void loopcxt_deinit(struct loopdev_cxt *lc);

extern int loopcxt_set_device(struct loopdev_cxt *lc, const char *device)
				__attribute__ ((warn_unused_result));
extern int loopcxt_has_device(struct loopdev_cxt *lc);
extern int loopcxt_add_device(struct loopdev_cxt *lc);
extern char *loopcxt_strdup_device(struct loopdev_cxt *lc);
extern const char *loopcxt_get_device(struct loopdev_cxt *lc);
extern struct loop_info64 *loopcxt_get_info(struct loopdev_cxt *lc);

extern int loopcxt_get_fd(struct loopdev_cxt *lc);
extern int loopcxt_set_fd(struct loopdev_cxt *lc, int fd, mode_t mode);

extern int loopcxt_init_iterator(struct loopdev_cxt *lc, int flags);
extern int loopcxt_deinit_iterator(struct loopdev_cxt *lc);
extern int loopcxt_next(struct loopdev_cxt *lc);

extern int loopcxt_setup_device(struct loopdev_cxt *lc);
extern int loopcxt_delete_device(struct loopdev_cxt *lc);

extern int loopcxt_ioctl_status(struct loopdev_cxt *lc);
extern int loopcxt_ioctl_capacity(struct loopdev_cxt *lc);
extern int loopcxt_ioctl_dio(struct loopdev_cxt *lc, unsigned long use_dio);
extern int loopcxt_ioctl_blocksize(struct loopdev_cxt *lc, uint64_t blocksize);

int loopcxt_set_offset(struct loopdev_cxt *lc, uint64_t offset);
int loopcxt_set_sizelimit(struct loopdev_cxt *lc, uint64_t sizelimit);
int loopcxt_set_blocksize(struct loopdev_cxt *lc, uint64_t blocksize);
int loopcxt_set_flags(struct loopdev_cxt *lc, uint32_t flags);
int loopcxt_set_backing_file(struct loopdev_cxt *lc, const char *filename);
int loopcxt_set_refname(struct loopdev_cxt *lc, const char *refname);

extern char *loopcxt_get_backing_file(struct loopdev_cxt *lc);
extern char *loopcxt_get_refname(struct loopdev_cxt *lc);
extern int loopcxt_get_backing_devno(struct loopdev_cxt *lc, dev_t *devno);
extern int loopcxt_get_backing_inode(struct loopdev_cxt *lc, ino_t *ino);
extern int loopcxt_get_offset(struct loopdev_cxt *lc, uint64_t *offset);
extern int loopcxt_get_blocksize(struct loopdev_cxt *lc, uint64_t *blocksize);
extern int loopcxt_get_sizelimit(struct loopdev_cxt *lc, uint64_t *size);
extern int loopcxt_get_encrypt_type(struct loopdev_cxt *lc, uint32_t *type);
extern const char *loopcxt_get_crypt_name(struct loopdev_cxt *lc);
extern int loopcxt_is_autoclear(struct loopdev_cxt *lc);
extern int loopcxt_is_readonly(struct loopdev_cxt *lc);
extern int loopcxt_is_dio(struct loopdev_cxt *lc);
extern int loopcxt_is_partscan(struct loopdev_cxt *lc);
extern int loopcxt_find_by_backing_file(struct loopdev_cxt *lc,
				const char *filename,
				uint64_t offset, uint64_t sizelimit,
				int flags);
extern int loopcxt_find_overlap(struct loopdev_cxt *lc,
				const char *filename,
				uint64_t offset, uint64_t sizelimit);

extern int loopcxt_is_used(struct loopdev_cxt *lc,
                    struct stat *st,
                    const char *backing_file,
                    uint64_t offset,
                    uint64_t sizelimit,
                    int flags);

#endif /* UTIL_LINUX_LOOPDEV_H */
