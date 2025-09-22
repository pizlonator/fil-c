/*
 * fdiskP.h - private library header file
 *
 * Copyright (C) 2012 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#ifndef _LIBFDISK_PRIVATE_H
#define _LIBFDISK_PRIVATE_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid.h>

#include "c.h"
#include "libfdisk.h"

#include "list.h"
#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

/*
 * Debug
 */
#define LIBFDISK_DEBUG_HELP	(1 << 0)
#define LIBFDISK_DEBUG_INIT	(1 << 1)
#define LIBFDISK_DEBUG_CXT	(1 << 2)
#define LIBFDISK_DEBUG_LABEL    (1 << 3)
#define LIBFDISK_DEBUG_ASK      (1 << 4)
#define LIBFDISK_DEBUG_PART	(1 << 6)
#define LIBFDISK_DEBUG_PARTTYPE	(1 << 7)
#define LIBFDISK_DEBUG_TAB	(1 << 8)
#define LIBFDISK_DEBUG_SCRIPT	(1 << 9)
#define LIBFDISK_DEBUG_WIPE	(1 << 10)
#define LIBFDISK_DEBUG_ITEM	(1 << 11)
#define LIBFDISK_DEBUG_GPT	(1 << 12)
#define LIBFDISK_DEBUG_ALL	0xFFFF

UL_DEBUG_DECLARE_MASK(libfdisk);
#define DBG(m, x)	__UL_DBG(libfdisk, LIBFDISK_DEBUG_, m, x)
#define ON_DBG(m, x)	__UL_DBG_CALL(libfdisk, LIBFDISK_DEBUG_, m, x)
#define DBG_FLUSH	__UL_DBG_FLUSH(libfdisk, LIBFDISK_DEBUG_)

#define UL_DEBUG_CURRENT_MASK	UL_DEBUG_MASK(libfdisk)
#include "debugobj.h"

/*
 * NLS -- the library has to be independent on main program, so define
 * UL_TEXTDOMAIN_EXPLICIT before you include nls.h.
 *
 * Now we use util-linux.po (=PACKAGE), rather than maintain the texts
 * in the separate libfdisk.po file.
 */
#define LIBFDISK_TEXTDOMAIN	PACKAGE
#define UL_TEXTDOMAIN_EXPLICIT	LIBFDISK_TEXTDOMAIN
#include "nls.h"


#ifdef TEST_PROGRAM
struct fdisk_test {
	const char	*name;
	int		(*body)(struct fdisk_test *ts, int argc, char *argv[]);
	const char	*usage;
};

/* test.c */
extern int fdisk_run_test(struct fdisk_test *tests, int argc, char *argv[]);
#endif

#define FDISK_GPT_NPARTITIONS_DEFAULT	128

/*
 * Generic iterator
 */
struct fdisk_iter {
        struct list_head        *p;		/* current position */
        struct list_head        *head;		/* start position */
	int			direction;	/* FDISK_ITER_{FOR,BACK}WARD */
};

#define IS_ITER_FORWARD(_i)	((_i)->direction == FDISK_ITER_FORWARD)
#define IS_ITER_BACKWARD(_i)	((_i)->direction == FDISK_ITER_BACKWARD)

#define FDISK_ITER_INIT(itr, list) \
	do { \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(list)->next : (list)->prev; \
		(itr)->head = (list); \
	} while(0)

#define FDISK_ITER_ITERATE(itr, res, restype, member) \
	do { \
		res = list_entry((itr)->p, restype, member); \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(itr)->p->next : (itr)->p->prev; \
	} while(0)

/*
 * Partition types
 */
struct fdisk_parttype {
	unsigned int	code;		/* type as number or zero */
	char		*name;		/* description */
	char		*typestr;	/* type as string or NULL */

	unsigned int	flags;		/* FDISK_PARTTYPE_* flags */
	int		refcount;	/* reference counter for allocated types */
};

enum {
	FDISK_PARTTYPE_UNKNOWN		= (1 << 1),
	FDISK_PARTTYPE_INVISIBLE	= (1 << 2),
	FDISK_PARTTYPE_ALLOCATED	= (1 << 3)
};

#define fdisk_parttype_is_invisible(_x)	((_x) && ((_x)->flags & FDISK_PARTTYPE_INVISIBLE))
#define fdisk_parttype_is_allocated(_x)	((_x) && ((_x)->flags & FDISK_PARTTYPE_ALLOCATED))

/*
 * Shortcut (used for partition types)
 */
struct fdisk_shortcut {
	const char	*shortcut;	/* shortcut, usually one letter (e.h. "H") */
	const char	*alias;		/* human readable alias (e.g. "home") */
	const char	*data;		/* for example partition type */

	unsigned int    deprecated : 1;
};

struct fdisk_partition {
	int		refcount;		/* reference counter */

	size_t		partno;			/* partition number */
	size_t		parent_partno;		/* for logical partitions */

	fdisk_sector_t	start;			/* first sectors */
	fdisk_sector_t	size;			/* size in sectors */

	int		movestart;		/* FDISK_MOVE_* (scripts only) */
	int		resize;			/* FDISK_RESIZE_* (scripts only) */

	char		*name;			/* partition name */
	char		*uuid;			/* partition UUID */
	char		*attrs;			/* partition flags/attributes converted to string */
	struct fdisk_parttype	*type;		/* partition type */

	char		*fstype;		/* filesystem type */
	char		*fsuuid;		/* filesystem uuid  */
	char		*fslabel;		/* filesystem label */

	struct list_head	parts;		/* list of partitions */

	/* extra fields for partition_to_string() */
	char		start_post;		/* start postfix  (e.g. '+') */
	char		end_post;		/* end postfix */
	char		size_post;		/* size postfix */

	uint64_t	fsize;			/* bsd junk */
	uint64_t	bsize;
	uint64_t	cpg;

	char		*start_chs;		/* start C/H/S in string */
	char		*end_chs;		/* end C/H/S in string */

	unsigned int	boot;			/* MBR: bootable */

	unsigned int	container : 1,			/* container partition (e.g. extended partition) */
			end_follow_default : 1,		/* use default end */
			freespace : 1,			/* this is free space */
			partno_follow_default : 1,	/* use default partno */
			size_explicit : 1,		/* don't align the size */
			start_follow_default : 1,	/* use default start */
			fs_probed : 1,			/* already probed by blkid */
			used : 1,			/* partition already used */
			wholedisk : 1;			/* special system partition */
};

enum {
	FDISK_MOVE_NONE = 0,
	FDISK_MOVE_DOWN = -1,
	FDISK_MOVE_UP = 1
};

enum {
	FDISK_RESIZE_NONE = 0,
	FDISK_RESIZE_REDUCE = -1,
	FDISK_RESIZE_ENLARGE = 1
};

#define FDISK_INIT_UNDEF(_x)	((_x) = (__typeof__(_x)) -1)
#define FDISK_IS_UNDEF(_x)	((_x) == (__typeof__(_x)) -1)

struct fdisk_table {
	struct list_head	parts;		/* partitions */
	int			refcount;
	size_t			nents;		/* number of partitions */
};

/*
 * Legacy CHS based geometry
 */
struct fdisk_geometry {
	unsigned int heads;
	fdisk_sector_t sectors;
	fdisk_sector_t cylinders;
};

/*
 * Label specific operations
 */
struct fdisk_label_operations {
	/* probe disk label */
	int (*probe)(struct fdisk_context *cxt);
	/* write in-memory changes to disk */
	int (*write)(struct fdisk_context *cxt);
	/* verify the partition table */
	int (*verify)(struct fdisk_context *cxt);
	/* create new disk label */
	int (*create)(struct fdisk_context *cxt);
	/* returns offset and size of the 'n' part of the PT */
	int (*locate)(struct fdisk_context *cxt, int n, const char **name,
		      uint64_t *offset, size_t *size);
	/* reorder partitions */
	int (*reorder)(struct fdisk_context *cxt);
	/* get details from label */
	int (*get_item)(struct fdisk_context *cxt, struct fdisk_labelitem *item);
	/* set disk label ID */
	int (*set_id)(struct fdisk_context *cxt, const char *str);


	/* new partition */
	int (*add_part)(struct fdisk_context *cxt, struct fdisk_partition *pa,
						size_t *partno);
	/* delete partition */
	int (*del_part)(struct fdisk_context *cxt, size_t partnum);

	/* fill in partition struct */
	int (*get_part)(struct fdisk_context *cxt, size_t n,
						struct fdisk_partition *pa);
	/* modify partition */
	int (*set_part)(struct fdisk_context *cxt, size_t n,
						struct fdisk_partition *pa);

	/* return state of the partition */
	int (*part_is_used)(struct fdisk_context *cxt, size_t partnum);

	int (*part_toggle_flag)(struct fdisk_context *cxt, size_t i, unsigned long flag);

	/* refresh alignment setting */
	int (*reset_alignment)(struct fdisk_context *cxt);

	/* free in-memory label stuff */
	void (*free)(struct fdisk_label *lb);

	/* deinit in-memory label stuff */
	void (*deinit)(struct fdisk_label *lb);
};

/*
 * The fields describes how to display libfdisk_partition
 */
struct fdisk_field {
	int		id;		/* FDISK_FIELD_* */
	const char	*name;		/* field name */
	double		width;		/* field width (compatible with libsmartcols whint) */
	int		flags;		/* FDISK_FIELDFL_* */
};

/* note that the defaults is to display a column always */
enum {
	FDISK_FIELDFL_DETAIL	= (1 << 1),	/* only display if fdisk_is_details() */
	FDISK_FIELDFL_EYECANDY	= (1 << 2),	/* don't display if fdisk_is_details() */
	FDISK_FIELDFL_NUMBER	= (1 << 3),	/* column display numbers */
};

/*
 * Generic label
 */
struct fdisk_label {
	const char		*name;		/* label name */
	enum fdisk_labeltype	id;		/* FDISK_DISKLABEL_* */
	const struct fdisk_parttype *parttypes;	/* supported partitions types */
	size_t			nparttypes;	/* number of items in parttypes[] */

	const struct fdisk_shortcut *parttype_cuts;	/* partition type shortcuts */
	size_t			nparttype_cuts;	/* number of items in parttype_cuts */

	size_t			nparts_max;	/* maximal number of partitions */
	size_t			nparts_cur;	/* number of currently used partitions */

	int			flags;		/* FDISK_LABEL_FL_* flags */

	struct fdisk_geometry	geom_min;	/* minimal geometry */
	struct fdisk_geometry	geom_max;	/* maximal geometry */

	unsigned int		changed:1,	/* label has been modified */
				disabled:1;	/* this driver is disabled at all */

	const struct fdisk_field *fields;	/* all possible fields */
	size_t			nfields;

	const struct fdisk_label_operations *op;
};


/* label driver flags */
enum {
	FDISK_LABEL_FL_REQUIRE_GEOMETRY = (1 << 2),
	FDISK_LABEL_FL_INCHARS_PARTNO   = (1 << 3)
};

/* label allocators */
extern struct fdisk_label *fdisk_new_gpt_label(struct fdisk_context *cxt);
extern struct fdisk_label *fdisk_new_dos_label(struct fdisk_context *cxt);
extern struct fdisk_label *fdisk_new_bsd_label(struct fdisk_context *cxt);
extern struct fdisk_label *fdisk_new_sgi_label(struct fdisk_context *cxt);
extern struct fdisk_label *fdisk_new_sun_label(struct fdisk_context *cxt);


struct ask_menuitem {
	char	key;
	const char	*name;
	const char	*desc;

	struct ask_menuitem *next;
};

/* fdisk dialog -- note that nothing from this stuff will be directly exported,
 * we will have get/set() function for everything.
 */
struct fdisk_ask {
	int		type;		/* FDISK_ASKTYPE_* */
	char		*query;

	int		refcount;

	union {
		/* FDISK_ASKTYPE_{NUMBER,OFFSET} */
		struct ask_number {
			uint64_t	hig;		/* high limit */
			uint64_t	low;		/* low limit */
			uint64_t	dfl;		/* default */
			uint64_t	result;
			uint64_t	base;		/* for relative results */
			uint64_t	unit;		/* unit for offsets */
			const char	*range;		/* by library generated list */
			unsigned int	relative :1,
					inchars  :1,
					wrap_negative	:1;
		} num;
		/* FDISK_ASKTYPE_{WARN,WARNX,..} */
		struct ask_print {
			const char	*mesg;
			int		errnum;		/* errno */
		} print;
		/* FDISK_ASKTYPE_YESNO */
		struct ask_yesno {
			int		result;		/* TRUE or FALSE */
		} yesno;
		/* FDISK_ASKTYPE_STRING */
		struct ask_string {
			char		*result;	/* allocated */
		} str;
		/* FDISK_ASKTYPE_MENU */
		struct ask_menu {
			int		dfl;		/* default menu item */
			int		result;
			struct ask_menuitem *first;
		} menu;
	} data;
};

struct fdisk_context {
	int dev_fd;         /* device descriptor */
	char *dev_path;     /* device path */
	char *dev_model;    /* on linux /sys/block/<name>/device/model or NULL */
	struct stat dev_st; /* stat(2) result */

	int refcount;

	unsigned char *firstsector; /* buffer with master boot record */
	unsigned long firstsector_bufsz;


	/* topology */
	unsigned long io_size;		/* I/O size used by fdisk */
	unsigned long optimal_io_size;	/* optional I/O returned by device */
	unsigned long min_io_size;	/* minimal I/O size */
	unsigned long phy_sector_size;	/* physical size */
	unsigned long sector_size;	/* logical size */
	unsigned long alignment_offset;

	unsigned int readonly : 1,		/* don't write to the device */
		     display_in_cyl_units : 1,	/* for obscure labels */
		     display_details : 1,	/* expert display mode */
		     protect_bootbits : 1,	/* don't zeroize first sector */
		     pt_collision : 1,		/* another PT detected by libblkid */
		     no_disalogs : 1,		/* disable dialog-driven partititoning */
		     dev_model_probed : 1,	/* tried to read from sys */
		     is_priv : 1,		/* open by libfdisk */
		     is_excl : 1,		/* open with O_EXCL */
		     listonly : 1;		/* list partition, nothing else */

	char *collision;			/* name of already existing FS/PT */
	struct list_head wipes;			/* list of areas to wipe before write */

	int sizeunit;				/* SIZE fields, FDISK_SIZEUNIT_* */

	/* alignment */
	unsigned long grain;		/* alignment unit */
	fdisk_sector_t first_lba;		/* recommended begin of the first partition */
	fdisk_sector_t last_lba;		/* recommended end of last partition */

	/* geometry */
	fdisk_sector_t total_sectors;	/* in logical sectors */
	struct fdisk_geometry geom;

	/* user setting to overwrite device default */
	struct fdisk_geometry user_geom;
	unsigned long user_pyh_sector;
	unsigned long user_log_sector;
	unsigned long user_grain;

	struct fdisk_label *label;	/* current label, pointer to labels[] */

	size_t nlabels;			/* number of initialized label drivers */
	struct fdisk_label *labels[8];	/* all supported labels,
					 * FIXME: use any enum rather than hardcoded number */

	int	(*ask_cb)(struct fdisk_context *, struct fdisk_ask *, void *);	/* fdisk dialogs callback */
	void	*ask_data;		/* ask_cb() data */

	struct fdisk_context	*parent;	/* for nested PT */
	struct fdisk_script	*script;	/* what we want to follow */
};

/* table */
enum {
	FDISK_DIFF_UNCHANGED = 0,
	FDISK_DIFF_REMOVED,
	FDISK_DIFF_ADDED,
	FDISK_DIFF_MOVED,
	FDISK_DIFF_RESIZED
};
extern int fdisk_diff_tables(struct fdisk_table *a, struct fdisk_table *b,
				struct fdisk_iter *itr,
				struct fdisk_partition **res, int *change);
extern void fdisk_debug_print_table(struct fdisk_table *tb);


/* context.c */
extern int __fdisk_switch_label(struct fdisk_context *cxt,
				    struct fdisk_label *lb);
extern int fdisk_missing_geometry(struct fdisk_context *cxt);

/* alignment.c */
fdisk_sector_t fdisk_scround(struct fdisk_context *cxt, fdisk_sector_t num);
fdisk_sector_t fdisk_cround(struct fdisk_context *cxt, fdisk_sector_t num);

extern int fdisk_discover_geometry(struct fdisk_context *cxt);
extern int fdisk_discover_topology(struct fdisk_context *cxt);

extern int fdisk_has_user_device_geometry(struct fdisk_context *cxt);
extern int fdisk_apply_user_device_properties(struct fdisk_context *cxt);
extern int fdisk_apply_label_device_properties(struct fdisk_context *cxt);
extern void fdisk_zeroize_device_properties(struct fdisk_context *cxt);

/* utils.c */
extern int fdisk_init_firstsector_buffer(struct fdisk_context *cxt,
			unsigned int protect_off, unsigned int protect_size);
extern int fdisk_read_firstsector(struct fdisk_context *cxt);

/* label.c */
extern int fdisk_probe_labels(struct fdisk_context *cxt);
extern void fdisk_deinit_label(struct fdisk_label *lb);

struct fdisk_labelitem {
	int		refcount;	/* reference counter */
	int		id;		/* <label>_ITEM_* */
	char		type;		/* s = string, j = uint64 */
	const char	*name;		/* human readable name */

	union {
		char		*str;
		uint64_t	num64;
	} data;
};

/* Use only internally for non-allocated items, never use
 * refcouting for such items!
 */
#define FDISK_LABELITEM_INIT	{ .type = 0, .refcount = 0 }

/* ask.c */
struct fdisk_ask *fdisk_new_ask(void);
void fdisk_reset_ask(struct fdisk_ask *ask);
int fdisk_ask_set_query(struct fdisk_ask *ask, const char *str);
int fdisk_ask_set_type(struct fdisk_ask *ask, int type);
int fdisk_do_ask(struct fdisk_context *cxt, struct fdisk_ask *ask);
int fdisk_ask_number_set_range(struct fdisk_ask *ask, const char *range);
int fdisk_ask_number_set_default(struct fdisk_ask *ask, uint64_t dflt);
int fdisk_ask_number_set_low(struct fdisk_ask *ask, uint64_t low);
int fdisk_ask_number_set_high(struct fdisk_ask *ask, uint64_t high);
int fdisk_ask_number_set_base(struct fdisk_ask *ask, uint64_t base);
int fdisk_ask_number_set_unit(struct fdisk_ask *ask, uint64_t unit);
int fdisk_ask_number_is_relative(struct fdisk_ask *ask);
int fdisk_ask_number_set_wrap_negative(struct fdisk_ask *ask, int wrap_negative);
int fdisk_ask_menu_set_default(struct fdisk_ask *ask, int dfl);
int fdisk_ask_menu_add_item(struct fdisk_ask *ask, int key,
			const char *name, const char *desc);
int fdisk_ask_print_set_errno(struct fdisk_ask *ask, int errnum);
int fdisk_ask_print_set_mesg(struct fdisk_ask *ask, const char *mesg);
int fdisk_info_new_partition(
			struct fdisk_context *cxt,
			int num, fdisk_sector_t start, fdisk_sector_t stop,
			struct fdisk_parttype *t);

/* dos.c */
extern struct dos_partition *fdisk_dos_get_partition(
				struct fdisk_context *cxt,
				size_t i);

/* wipe.c */
void fdisk_free_wipe_areas(struct fdisk_context *cxt);
int fdisk_set_wipe_area(struct fdisk_context *cxt, uint64_t start, uint64_t size, int enable);
int fdisk_do_wipe(struct fdisk_context *cxt);
int fdisk_has_wipe_area(struct fdisk_context *cxt, uint64_t start, uint64_t size);
int fdisk_check_collisions(struct fdisk_context *cxt);

/* parttype.c */
const char *fdisk_label_translate_type_shortcut(const struct fdisk_label *lb, char *cut);

#endif /* _LIBFDISK_PRIVATE_H */
