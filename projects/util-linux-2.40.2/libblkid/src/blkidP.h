/*
 * blkidP.h - Internal interfaces for libblkid
 *
 * Copyright (C) 2001 Andreas Dilger
 * Copyright (C) 2003 Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#ifndef _BLKID_BLKIDP_H
#define _BLKID_BLKIDP_H

/* Always confirm that /dev/disk-by symlinks match with LABEL/UUID on device */
/* #define CONFIG_BLKID_VERIFY_UDEV 1 */

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#ifndef UUID_STR_LEN
# define UUID_STR_LEN   37
#endif

#include "c.h"
#include "bitops.h"	/* $(top_srcdir)/include/ */
#include "blkdev.h"

#include "debug.h"
#include "blkid.h"
#include "list.h"
#include "encode.h"

/*
 * This describes the attributes of a specific device.
 * We can traverse all of the tags by bid_tags (linking to the tag bit_names).
 * The bid_label and bid_uuid fields are shortcuts to the LABEL and UUID tag
 * values, if they exist.
 */
struct blkid_struct_dev
{
	struct list_head	bid_devs;	/* All devices in the cache */
	struct list_head	bid_tags;	/* All tags for this device */
	blkid_cache		bid_cache;	/* Dev belongs to this cache */
	char			*bid_name;	/* Device real path (as used in cache) */
	char			*bid_xname;	/* Device path as used by application (maybe symlink..) */
	char			*bid_type;	/* Preferred device TYPE */
	int			bid_pri;	/* Device priority */
	dev_t			bid_devno;	/* Device major/minor number */
	time_t			bid_time;	/* Last update time of device */
	suseconds_t		bid_utime;	/* Last update time (microseconds) */
	unsigned int		bid_flags;	/* Device status bitflags */
	char			*bid_label;	/* Shortcut to device LABEL */
	char			*bid_uuid;	/* Shortcut to binary UUID */
};

#define BLKID_BID_FL_VERIFIED	0x0001	/* Device data validated from disk */
#define BLKID_BID_FL_INVALID	0x0004	/* Device is invalid */
#define BLKID_BID_FL_REMOVABLE	0x0008	/* Device added by blkid_probe_all_removable() */

/*
 * Each tag defines a NAME=value pair for a particular device.  The tags
 * are linked via bit_names for a single device, so that traversing the
 * names list will get you a list of all tags associated with a device.
 * They are also linked via bit_values for all devices, so one can easily
 * search all tags with a given NAME for a specific value.
 */
struct blkid_struct_tag
{
	struct list_head	bit_tags;	/* All tags for this device */
	struct list_head	bit_names;	/* All tags with given NAME */
	char			*bit_name;	/* NAME of tag (shared) */
	char			*bit_val;	/* value of tag */
	blkid_dev		bit_dev;	/* pointer to device */
};
typedef struct blkid_struct_tag *blkid_tag;

/*
 * Chain IDs
 */
enum {
	BLKID_CHAIN_SUBLKS,	/* FS/RAID superblocks (enabled by default) */
	BLKID_CHAIN_TOPLGY,	/* Block device topology */
	BLKID_CHAIN_PARTS,	/* Partition tables */

	BLKID_NCHAINS		/* number of chains */
};

struct blkid_chain {
	const struct blkid_chaindrv *driver;	/* chain driver */

	int		enabled;	/* boolean */
	int		flags;		/* BLKID_<chain>_* */
	int		binary;		/* boolean */
	int		idx;		/* index of the current prober (or -1) */
	unsigned long	*fltr;		/* filter or NULL */
	void		*data;		/* private chain data or NULL */
};

/*
 * Chain driver
 */
struct blkid_chaindrv {
	const size_t	id;		/* BLKID_CHAIN_* */
	const char	*name;		/* name of chain (for debug purpose) */
	const int	dflt_flags;	/* default chain flags */
	const int	dflt_enabled;	/* default enabled boolean */
	int		has_fltr;	/* boolean */

	const struct blkid_idinfo **idinfos; /* description of probing functions */
	const size_t	nidinfos;	/* number of idinfos */

	/* driver operations */
	int		(*probe)(blkid_probe, struct blkid_chain *);
	int		(*safeprobe)(blkid_probe, struct blkid_chain *);
	void		(*free_data)(blkid_probe, void *);
};

/* chains */
extern const struct blkid_chaindrv superblocks_drv;
extern const struct blkid_chaindrv topology_drv;
extern const struct blkid_chaindrv partitions_drv;

/*
 * Low-level probe result
 */
struct blkid_prval
{
	const char	*name;		/* value name */
	unsigned char	*data;		/* value data */
	size_t		len;		/* length of value data */

	struct blkid_chain	*chain;		/* owner */
	struct list_head	prvals;		/* list of results */
};

/*
 * Filesystem / Raid magic strings
 */
struct blkid_idmag
{
	const char	*magic;		/* magic string */
	unsigned int	len;		/* length of magic */
	unsigned int	hint;		/* hint for prober */

	const char	*hoff;		/* hint which contains byte offset to kboff */
	long		kboff;		/* kilobyte offset of superblock */
	unsigned int	sboff;		/* byte offset within superblock */

	int		is_zoned;	/* indicate magic location is calculated based on zone position  */
	long		zonenum;	/* zone number which has superblock */
	long		kboff_inzone;	/* kilobyte offset of superblock in a zone */
};

/*
 * Filesystem / Raid description
 */
struct blkid_idinfo
{
	const char	*name;		/* fs, raid or partition table name */
	int		usage;		/* BLKID_USAGE_* flag */
	int		flags;		/* BLKID_IDINFO_* flags */
	int		minsz;		/* minimal device size */

					/* probe function */
	int		(*probefunc)(blkid_probe pr, const struct blkid_idmag *mag);

	struct blkid_idmag	magics[];	/* NULL or array with magic strings */
};

#define BLKID_NONE_MAGIC	{{ NULL }}

/*
 * tolerant FS - can share the same device with more filesystems (e.g. typical
 * on CD-ROMs). We need this flag to detect ambivalent results (e.g. valid fat
 * and valid linux swap on the same device).
 */
#define BLKID_IDINFO_TOLERANT	(1 << 1)

struct blkid_bufinfo {
	unsigned char		*data;
	uint64_t		off;
	uint64_t		len;
	struct list_head	bufs;	/* list of buffers */
};

/*
 * Probing hint
 */
struct blkid_hint {
	char			*name;
	uint64_t		value;
	struct list_head	hints;
};

/*
 * Low-level probing control struct
 */
struct blkid_struct_probe
{
	int			fd;		/* device file descriptor */
	uint64_t		off;		/* begin of data on the device */
	uint64_t		size;		/* end of data on the device */
	uint64_t		io_size;	/* optimal size of IO */

	dev_t			devno;		/* device number (st.st_rdev) */
	dev_t			disk_devno;	/* devno of the whole-disk or 0 */
	unsigned int		blkssz;		/* sector size (BLKSSZGET ioctl) */
	mode_t			mode;		/* struct stat.sb_mode */
	uint64_t		zone_size;	/* zone size (BLKGETZONESZ ioctl) */

	int			flags;		/* private library flags */
	int			prob_flags;	/* always zeroized by blkid_do_*() */

	uint64_t		wipe_off;	/* begin of the wiped area */
	uint64_t		wipe_size;	/* size of the wiped area */
	struct blkid_chain	*wipe_chain;	/* superblock, partition, ... */

	struct list_head	buffers;	/* list of buffers */
	struct list_head	prunable_buffers;	/* list of prunable buffers */
	struct list_head	hints;

	struct blkid_chain	chains[BLKID_NCHAINS];	/* array of chains */
	struct blkid_chain	*cur_chain;		/* current chain */

	struct list_head	values;		/* results */

	struct blkid_struct_probe *parent;	/* for clones */
	struct blkid_struct_probe *disk_probe;	/* whole-disk probing */
};

/* private flags library flags */
#define BLKID_FL_PRIVATE_FD	(1 << 1)	/* see blkid_new_probe_from_filename() */
#define BLKID_FL_TINY_DEV	(1 << 2)	/* <= 1.47MiB (floppy or so) */
#define BLKID_FL_CDROM_DEV	(1 << 3)	/* is a CD/DVD drive */
#define BLKID_FL_NOSCAN_DEV	(1 << 4)	/* do not scan this device */
#define BLKID_FL_MODIF_BUFF	(1 << 5)	/* cached buffers has been modified */
#define BLKID_FL_OPAL_LOCKED	(1 << 6)	/* OPAL device is locked (I/O errors) */

/* private per-probing flags */
#define BLKID_PROBE_FL_IGNORE_PT (1 << 1)	/* ignore partition table */

extern blkid_probe blkid_clone_probe(blkid_probe parent);
extern blkid_probe blkid_probe_get_wholedisk_probe(blkid_probe pr);

/*
 * Evaluation methods (for blkid_eval_* API)
 */
enum {
	BLKID_EVAL_UDEV = 0,
	BLKID_EVAL_SCAN,

	__BLKID_EVAL_LAST
};

/*
 * Library config options
 */
struct blkid_config {
	int eval[__BLKID_EVAL_LAST];	/* array with EVALUATION=<udev,cache> options */
	int nevals;			/* number of elems in eval array */
	int uevent;			/* SEND_UEVENT=<yes|not> option */
	char *cachefile;		/* CACHE_FILE=<path> option */
};

extern struct blkid_config *blkid_read_config(const char *filename)
			__ul_attribute__((warn_unused_result));
extern void blkid_free_config(struct blkid_config *conf);

/*
 * Minimum number of seconds between device probes, even when reading
 * from the cache.  This is to avoid re-probing all devices which were
 * just probed by another program that does not share the cache.
 */
#define BLKID_PROBE_MIN		2

/*
 * Time in seconds an entry remains verified in the in-memory cache
 * before being reverified (in case of long-running processes that
 * keep a cache in memory and continue to use it for a long time).
 */
#define BLKID_PROBE_INTERVAL	200

/* This describes an entire blkid cache file and probed devices.
 * We can traverse all of the found devices via bic_list.
 * We can traverse all of the tag types by bic_tags, which hold empty tags
 * for each tag type.  Those tags can be used as list_heads for iterating
 * through all devices with a specific tag type (e.g. LABEL).
 */
struct blkid_struct_cache
{
	struct list_head	bic_devs;	/* List head of all devices */
	struct list_head	bic_tags;	/* List head of all tag types */
	time_t			bic_time;	/* Last probe time */
	time_t			bic_ftime;	/* Mod time of the cachefile */
	unsigned int		bic_flags;	/* Status flags of the cache */
	char			*bic_filename;	/* filename of cache */
	blkid_probe		probe;		/* low-level probing stuff */
};

#define BLKID_BIC_FL_PROBED	0x0002	/* We probed /proc/partition devices */
#define BLKID_BIC_FL_CHANGED	0x0004	/* Cache has changed from disk */

/* config file */
#define BLKID_CONFIG_FILE	"/etc/blkid.conf"

/* cache file on systemds with /run */
#define BLKID_RUNTIME_TOPDIR	"/run"
#define BLKID_RUNTIME_DIR	BLKID_RUNTIME_TOPDIR "/blkid"
#define BLKID_CACHE_FILE	BLKID_RUNTIME_DIR "/blkid.tab"

/* old systems */
#define BLKID_CACHE_FILE_OLD	"/etc/blkid.tab"

#define BLKID_ERR_IO	 5
#define BLKID_ERR_SYSFS	 9
#define BLKID_ERR_MEM	12
#define BLKID_ERR_CACHE	14
#define BLKID_ERR_DEV	19
#define BLKID_ERR_PARAM	22
#define BLKID_ERR_BIG	27

/*
 * Priority settings for different types of devices
 */
#define BLKID_PRI_UBI	50
#define BLKID_PRI_DM	40
#define BLKID_PRI_EVMS	30
#define BLKID_PRI_LVM	20
#define BLKID_PRI_MD	10

#define BLKID_DEBUG_HELP	(1 << 0)
#define BLKID_DEBUG_INIT	(1 << 1)
#define BLKID_DEBUG_CACHE	(1 << 2)
#define BLKID_DEBUG_CONFIG	(1 << 3)
#define BLKID_DEBUG_DEV		(1 << 4)
#define BLKID_DEBUG_DEVNAME	(1 << 5)
#define BLKID_DEBUG_DEVNO	(1 << 6)
#define BLKID_DEBUG_EVALUATE	(1 << 7)
#define BLKID_DEBUG_LOWPROBE	(1 << 8)
#define BLKID_DEBUG_PROBE	(1 << 9)
#define BLKID_DEBUG_READ	(1 << 10)
#define BLKID_DEBUG_SAVE	(1 << 11)
#define BLKID_DEBUG_TAG		(1 << 12)
#define BLKID_DEBUG_BUFFER	(1 << 13)
#define BLKID_DEBUG_ALL		0xFFFF		/* (1 << 16) aka FFFF is expected by API */

UL_DEBUG_DECLARE_MASK(libblkid);
#define DBG(m, x)	__UL_DBG(libblkid, BLKID_DEBUG_, m, x)
#define ON_DBG(m, x)    __UL_DBG_CALL(libblkid, BLKID_DEBUG_, m, x)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(libblkid)
#include "debugobj.h"

extern void blkid_debug_dump_dev(blkid_dev dev);


/* devno.c */
struct dir_list {
	char	*name;
	struct dir_list *next;
};
extern void blkid__scan_dir(char *, dev_t, struct dir_list **, char **)
			__attribute__((nonnull(1,4)));
extern int blkid_driver_has_major(const char *drvname, int drvmaj)
			__attribute__((warn_unused_result));

/* read.c */
extern void blkid_read_cache(blkid_cache cache)
			__attribute__((nonnull));

/* save.c */
extern int blkid_flush_cache(blkid_cache cache)
			__attribute__((nonnull));

/* cache */
extern char *blkid_safe_getenv(const char *arg)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern char *blkid_get_cache_filename(struct blkid_config *conf)
			__attribute__((warn_unused_result));
/*
 * Functions to create and find a specific tag type: tag.c
 */
extern void blkid_free_tag(blkid_tag tag);
extern blkid_tag blkid_find_tag_dev(blkid_dev dev, const char *type)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern int blkid_set_tag(blkid_dev dev, const char *name,
			 const char *value, const int vlength)
			__attribute__((nonnull(1,2)));

/*
 * Functions to create and find a specific tag type: dev.c
 */
extern blkid_dev blkid_new_dev(void)
			__attribute__((warn_unused_result));
extern void blkid_free_dev(blkid_dev dev);

/* probe.c */
extern int blkid_probe_is_tiny(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));
extern int blkid_probe_is_cdrom(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));
extern int blkdid_probe_is_opal_locked(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern const unsigned char *blkid_probe_get_buffer(blkid_probe pr,
                                uint64_t off, uint64_t len)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern const unsigned char *blkid_probe_get_sector(blkid_probe pr, unsigned int sector)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern int blkid_probe_get_dimension(blkid_probe pr,
	                uint64_t *off, uint64_t *size)
			__attribute__((nonnull));

extern int blkid_probe_set_dimension(blkid_probe pr,
	                uint64_t off, uint64_t size)
			__attribute__((nonnull));

extern int blkid_probe_get_idmag(blkid_probe pr, const struct blkid_idinfo *id,
			uint64_t *offset, const struct blkid_idmag **res)
			__attribute__((nonnull(1)));

extern void blkid_probe_prune_buffers(blkid_probe pr);

/* returns superblock according to 'struct blkid_idmag' */
extern const unsigned char *blkid_probe_get_sb_buffer(blkid_probe pr, const struct blkid_idmag *mag, size_t size);
#define blkid_probe_get_sb(_pr, _mag, type) \
			((const type *) blkid_probe_get_sb_buffer((_pr), _mag, sizeof(type)))

extern uint64_t blkid_probe_get_idmag_off(blkid_probe pr, const struct blkid_idmag *mag)
			__attribute__((nonnull));

extern blkid_partlist blkid_probe_get_partlist(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern int blkid_probe_is_covered_by_pt(blkid_probe pr,
					uint64_t offset, uint64_t size)
			__attribute__((warn_unused_result));

extern void blkid_probe_chain_reset_values(blkid_probe pr, struct blkid_chain *chn)
			__attribute__((nonnull));
extern int blkid_probe_chain_save_values(blkid_probe pr,
				       struct blkid_chain *chn,
			               struct list_head *vals)
			__attribute__((nonnull));

extern struct blkid_prval *blkid_probe_assign_value(blkid_probe pr,
					const char *name)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern void blkid_probe_free_value(struct blkid_prval *v);


extern void blkid_probe_append_values_list(blkid_probe pr,
				    struct list_head *vals)
			__attribute__((nonnull));

extern void blkid_probe_free_values_list(struct list_head *vals);

extern struct blkid_chain *blkid_probe_get_chain(blkid_probe pr)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern struct blkid_prval *__blkid_probe_get_value(blkid_probe pr, int num)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern struct blkid_prval *__blkid_probe_lookup_value(blkid_probe pr, const char *name)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern unsigned long *blkid_probe_get_filter(blkid_probe pr, int chain, int create)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern int __blkid_probe_invert_filter(blkid_probe pr, int chain)
			__attribute__((nonnull));
extern int __blkid_probe_reset_filter(blkid_probe pr, int chain)
			__attribute__((nonnull));
extern int __blkid_probe_filter_types(blkid_probe pr, int chain, int flag, char *names[])
			__attribute__((nonnull));

extern void *blkid_probe_get_binary_data(blkid_probe pr, struct blkid_chain *chn)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));

extern struct blkid_prval *blkid_probe_new_val(void)
			__attribute__((warn_unused_result));
extern int blkid_probe_set_value(blkid_probe pr, const char *name,
				const unsigned char *data, size_t len)
			__attribute__((nonnull));
extern int blkid_probe_value_set_data(struct blkid_prval *v,
				const unsigned char *data, size_t len)
			__attribute__((nonnull));

extern int blkid_probe_vsprintf_value(blkid_probe pr, const char *name,
				const char *fmt, va_list ap)
			__attribute__((nonnull));

extern int blkid_probe_sprintf_value(blkid_probe pr, const char *name,
				const char *fmt, ...)
			__attribute__((nonnull))
			__attribute__ ((__format__ (__printf__, 3, 4)));

extern int blkid_probe_set_magic(blkid_probe pr, uint64_t offset,
				size_t len, const unsigned char *magic)
			__attribute__((nonnull));

extern int blkid_probe_verify_csum(blkid_probe pr, uint64_t csum, uint64_t expected)
			__attribute__((nonnull));
extern int blkid_probe_verify_csum_buf(blkid_probe pr, size_t n, const void *csum,
		const void *expected) __attribute__((nonnull));

extern void blkid_unparse_uuid(const unsigned char *uuid, char *str, size_t len)
			__attribute__((nonnull));
extern int blkid_uuid_is_empty(const unsigned char *buf, size_t len);

extern size_t blkid_rtrim_whitespace(unsigned char *str)
			__attribute__((nonnull));
extern size_t blkid_ltrim_whitespace(unsigned char *str)
			__attribute__((nonnull));

extern void blkid_probe_set_wiper(blkid_probe pr, uint64_t off,
				  uint64_t size)
			__attribute__((nonnull));
extern int blkid_probe_is_wiped(blkid_probe pr, struct blkid_chain **chn,
		                uint64_t off, uint64_t size)
			__attribute__((nonnull))
			__attribute__((warn_unused_result));
extern void blkid_probe_use_wiper(blkid_probe pr, uint64_t off, uint64_t size)
			__attribute__((nonnull));

extern int blkid_probe_get_hint(blkid_probe pr, const char *name, uint64_t *value)
			__attribute__((nonnull(1,2)))
			__attribute__((warn_unused_result));

extern int blkid_probe_get_partitions_flags(blkid_probe pr)
			__attribute__((nonnull));

/* filter bitmap macros */
#define blkid_bmp_wordsize		(8 * sizeof(unsigned long))
#define blkid_bmp_idx_bit(item)		(1UL << ((item) % blkid_bmp_wordsize))
#define blkid_bmp_idx_byte(item)	((item) / blkid_bmp_wordsize)

#define blkid_bmp_set_item(bmp, item)	\
		((bmp)[ blkid_bmp_idx_byte(item) ] |= blkid_bmp_idx_bit(item))

#define blkid_bmp_unset_item(bmp, item)	\
		((bmp)[ blkid_bmp_idx_byte(item) ] &= ~blkid_bmp_idx_bit(item))

#define blkid_bmp_get_item(bmp, item)	\
		((bmp)[ blkid_bmp_idx_byte(item) ] & blkid_bmp_idx_bit(item))

#define blkid_bmp_nwords(max_items) \
		(((max_items) + blkid_bmp_wordsize) / blkid_bmp_wordsize)

#define blkid_bmp_nbytes(max_items) \
		(blkid_bmp_nwords(max_items) * sizeof(unsigned long))

#endif /* _BLKID_BLKIDP_H */
