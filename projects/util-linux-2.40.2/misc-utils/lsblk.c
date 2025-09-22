/*
 * lsblk(8) - list block devices
 *
 * Copyright (C) 2010-2018 Red Hat, Inc. All rights reserved.
 * Written by Milan Broz <gmazyland@gmail.com>
 *            Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <locale.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <assert.h>

#include <blkid.h>

#include "c.h"
#include "pathnames.h"
#include "blkdev.h"
#include "canonicalize.h"
#include "nls.h"
#include "xalloc.h"
#include "strutils.h"
#include "sysfs.h"
#include "closestream.h"
#include "optutils.h"
#include "fileutils.h"
#include "loopdev.h"
#include "buffer.h"
#include "colors.h"
#include "column-list-table.h"

#include "lsblk.h"

UL_DEBUG_DEFINE_MASK(lsblk);
UL_DEBUG_DEFINE_MASKNAMES(lsblk) = UL_DEBUG_EMPTY_MASKNAMES;

#define LSBLK_EXIT_SOMEOK 64
#define LSBLK_EXIT_ALLFAILED 32

static int column_id_to_number(int id);

/* column IDs */
enum {
	COL_ALIOFF = 0,
	COL_IDLINK,
	COL_ID,
	COL_DALIGN,
	COL_DAX,
	COL_DGRAN,
	COL_DISKSEQ,
	COL_DMAX,
	COL_DZERO,
	COL_FSAVAIL,
	COL_FSROOTS,
	COL_FSSIZE,
	COL_FSTYPE,
	COL_FSUSED,
	COL_FSUSEPERC,
	COL_FSVERSION,
	COL_GROUP,
	COL_HCTL,
	COL_HOTPLUG,
	COL_KNAME,
	COL_LABEL,
	COL_LOGSEC,
	COL_MAJMIN,
	COL_MAJ,
	COL_MIN,
	COL_MINIO,
	COL_MODE,
	COL_MODEL,
	COL_MQ,
	COL_NAME,
	COL_OPTIO,
	COL_OWNER,
	COL_PARTFLAGS,
	COL_PARTLABEL,
	COL_PARTN,
	COL_PARTTYPE,
	COL_PARTTYPENAME,
	COL_PARTUUID,
	COL_PATH,
	COL_PHYSEC,
	COL_PKNAME,
	COL_PTTYPE,
	COL_PTUUID,
	COL_RA,
	COL_RAND,
	COL_REV,
	COL_RM,
	COL_RO,
	COL_ROTA,
	COL_RQ_SIZE,
	COL_SCHED,
	COL_SERIAL,
	COL_SIZE,
	COL_START,
	COL_STATE,
	COL_SUBSYS,
	COL_TARGET,
	COL_TARGETS,
	COL_TRANSPORT,
	COL_TYPE,
	COL_UUID,
	COL_VENDOR,
	COL_WSAME,
	COL_WWN,
	COL_ZONED,
	COL_ZONE_SZ,
	COL_ZONE_WGRAN,
	COL_ZONE_APP,
	COL_ZONE_NR,
	COL_ZONE_OMAX,
	COL_ZONE_AMAX,
};

/* basic table settings */
enum {
	LSBLK_ASCII =		(1 << 0),
	LSBLK_RAW =		(1 << 1),
	LSBLK_NOHEADINGS =	(1 << 2),
	LSBLK_EXPORT =		(1 << 3),
	LSBLK_TREE =		(1 << 4),
	LSBLK_JSON =		(1 << 5),
	LSBLK_SHELLVAR =	(1 << 6)
};

/* Types used for qsort() and JSON */
enum {
	COLTYPE_STR	= 0,	/* default */
	COLTYPE_NUM	= 1,	/* always u64 number */
	COLTYPE_SORTNUM = 2,	/* string on output, u64 for qsort() */
	COLTYPE_SIZE	= 3,	/* srring by default, number when --bytes */
	COLTYPE_BOOL	= 4	/* 0 or 1 */
};

/* column names */
struct colinfo {
	const char	* const name;	/* header */
	double		whint;		/* width hint (N < 1 is in percent of termwidth) */
	int		flags;		/* SCOLS_FL_* */
	const char	*help;
	int		type;		/* COLTYPE_* */
};

/* columns descriptions */
static const struct colinfo infos[] = {
	[COL_ALIOFF] = { "ALIGNMENT", 6, SCOLS_FL_RIGHT, N_("alignment offset"), COLTYPE_NUM },
	[COL_ID] = { "ID", 0.1, SCOLS_FL_NOEXTREMES, N_("udev ID (based on ID-LINK)") },
	[COL_IDLINK] = { "ID-LINK", 0.1, SCOLS_FL_NOEXTREMES, N_("the shortest udev /dev/disk/by-id link name") },
	[COL_DALIGN] = { "DISC-ALN", 6, SCOLS_FL_RIGHT, N_("discard alignment offset"), COLTYPE_NUM },
	[COL_DAX] = { "DAX", 1, SCOLS_FL_RIGHT, N_("dax-capable device"), COLTYPE_BOOL },
	[COL_DGRAN] = { "DISC-GRAN", 6, SCOLS_FL_RIGHT, N_("discard granularity, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_DISKSEQ] = { "DISK-SEQ", 1, SCOLS_FL_RIGHT, N_("disk sequence number"), COLTYPE_NUM },
	[COL_DMAX] = { "DISC-MAX", 6, SCOLS_FL_RIGHT, N_("discard max bytes, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_DZERO] = { "DISC-ZERO", 1, SCOLS_FL_RIGHT, N_("discard zeroes data"), COLTYPE_BOOL },
	[COL_FSAVAIL] = { "FSAVAIL", 5, SCOLS_FL_RIGHT, N_("filesystem size available for unprivileged users, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_FSROOTS] = { "FSROOTS", 0.1, SCOLS_FL_WRAP, N_("mounted filesystem roots") },
	[COL_FSSIZE] = { "FSSIZE", 5, SCOLS_FL_RIGHT, N_("filesystem size, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_FSTYPE] = { "FSTYPE", 0.1, SCOLS_FL_TRUNC, N_("filesystem type") },
	[COL_FSUSED] = { "FSUSED", 5, SCOLS_FL_RIGHT, N_("filesystem size used, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_FSUSEPERC] = { "FSUSE%", 3, SCOLS_FL_RIGHT, N_("filesystem use percentage") },
	[COL_FSVERSION] = { "FSVER", 0.1, SCOLS_FL_TRUNC, N_("filesystem version") },
	[COL_GROUP] = { "GROUP", 0.1, SCOLS_FL_TRUNC, N_("group name") },
	[COL_HCTL] = { "HCTL", 10, 0, N_("Host:Channel:Target:Lun for SCSI") },
	[COL_HOTPLUG] = { "HOTPLUG", 1, SCOLS_FL_RIGHT, N_("removable or hotplug device (usb, pcmcia, ...)"), COLTYPE_BOOL },
	[COL_KNAME] = { "KNAME", 0.3, 0, N_("internal kernel device name") },
	[COL_LABEL] = { "LABEL", 0.1, 0, N_("filesystem LABEL") },
	[COL_LOGSEC] = { "LOG-SEC", 7, SCOLS_FL_RIGHT, N_("logical sector size"), COLTYPE_NUM },
	[COL_MAJMIN] = { "MAJ:MIN", 6, 0, N_("major:minor device number"), COLTYPE_SORTNUM },
	[COL_MAJ] = { "MAJ", 3, 0, N_("major device number"), COLTYPE_SORTNUM },
	[COL_MIN] = { "MIN", 3, 0, N_("minor device number"), COLTYPE_SORTNUM },
	[COL_MINIO] = { "MIN-IO", 6, SCOLS_FL_RIGHT, N_("minimum I/O size"), COLTYPE_NUM },
	[COL_MODEL] = { "MODEL", 0.1, SCOLS_FL_TRUNC, N_("device identifier") },
	[COL_MODE] = { "MODE", 10, 0, N_("device node permissions") },
	[COL_MQ] = { "MQ", 3, SCOLS_FL_RIGHT, N_("device queues") },
	[COL_NAME] = { "NAME", 0.25, SCOLS_FL_NOEXTREMES, N_("device name") },
	[COL_OPTIO] = { "OPT-IO", 6, SCOLS_FL_RIGHT, N_("optimal I/O size"), COLTYPE_NUM },
	[COL_OWNER] = { "OWNER", 0.1, SCOLS_FL_TRUNC, N_("user name"), },
	[COL_PARTFLAGS] = { "PARTFLAGS", 36,  0, N_("partition flags") },
	[COL_PARTLABEL] = { "PARTLABEL", 0.1, 0, N_("partition LABEL") },
	[COL_PARTN] = { "PARTN", 2, SCOLS_FL_RIGHT, N_("partition number as read from the partition table"), COLTYPE_NUM },
	[COL_PARTTYPENAME]  = { "PARTTYPENAME",  0.1,  0, N_("partition type name") },
	[COL_PARTTYPE] = { "PARTTYPE", 36,  0, N_("partition type code or UUID") },
	[COL_PARTUUID] = { "PARTUUID", 36,  0, N_("partition UUID") },
	[COL_PATH] = { "PATH", 0.3, 0, N_("path to the device node") },
	[COL_PHYSEC] = { "PHY-SEC", 7, SCOLS_FL_RIGHT, N_("physical sector size"), COLTYPE_NUM },
	[COL_PKNAME] = { "PKNAME", 0.3, 0, N_("internal parent kernel device name") },
	[COL_PTTYPE] = { "PTTYPE", 0.1, 0, N_("partition table type") },
	[COL_PTUUID] = { "PTUUID", 36,  0, N_("partition table identifier (usually UUID)") },
	[COL_RAND] = { "RAND", 1, SCOLS_FL_RIGHT, N_("adds randomness"), COLTYPE_BOOL },
	[COL_RA] = { "RA", 3, SCOLS_FL_RIGHT, N_("read-ahead of the device"), COLTYPE_NUM },
	[COL_REV] = { "REV", 4, SCOLS_FL_RIGHT, N_("device revision") },
	[COL_RM] = { "RM", 1, SCOLS_FL_RIGHT, N_("removable device"), COLTYPE_BOOL },
	[COL_ROTA] = { "ROTA", 1, SCOLS_FL_RIGHT, N_("rotational device"), COLTYPE_BOOL },
	[COL_RO] = { "RO", 1, SCOLS_FL_RIGHT, N_("read-only device"), COLTYPE_BOOL },
	[COL_RQ_SIZE]= { "RQ-SIZE", 5, SCOLS_FL_RIGHT, N_("request queue size"), COLTYPE_NUM },
	[COL_SCHED] = { "SCHED", 0.1, 0, N_("I/O scheduler name") },
	[COL_SERIAL] = { "SERIAL", 0.1, SCOLS_FL_TRUNC, N_("disk serial number") },
	[COL_SIZE] = { "SIZE", 5, SCOLS_FL_RIGHT, N_("size of the device, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_START] = { "START", 5, SCOLS_FL_RIGHT, N_("partition start offset (in 512-byte sectors)"), COLTYPE_NUM },
	[COL_STATE] = { "STATE", 7, SCOLS_FL_TRUNC, N_("state of the device") },
	[COL_SUBSYS] = { "SUBSYSTEMS", 0.1, SCOLS_FL_NOEXTREMES, N_("de-duplicated chain of subsystems") },
	[COL_TARGETS] = { "MOUNTPOINTS", 0.10, SCOLS_FL_WRAP | SCOLS_FL_NOEXTREMES,  N_("all locations where device is mounted") },
	[COL_TARGET] = { "MOUNTPOINT", 0.10, SCOLS_FL_TRUNC | SCOLS_FL_NOEXTREMES, N_("where the device is mounted") },
	[COL_TRANSPORT] = { "TRAN", 6, 0, N_("device transport type") },
	[COL_TYPE] = { "TYPE", 4, 0, N_("device type") },
	[COL_UUID] = { "UUID", 36,  0, N_("filesystem UUID") },
	[COL_VENDOR] = { "VENDOR", 0.1, SCOLS_FL_TRUNC, N_("device vendor") },
	[COL_WSAME] = { "WSAME", 6, SCOLS_FL_RIGHT, N_("write same max bytes, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_WWN] = { "WWN", 18, 0, N_("unique storage identifier") },
	[COL_ZONED] = { "ZONED", 0.3, 0, N_("zone model") },
	[COL_ZONE_SZ] = { "ZONE-SZ", 9, SCOLS_FL_RIGHT, N_("zone size, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_ZONE_WGRAN] = { "ZONE-WGRAN", 10, SCOLS_FL_RIGHT, N_("zone write granularity, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_ZONE_APP] = { "ZONE-APP", 11, SCOLS_FL_RIGHT, N_("zone append max bytes, use <number> if --bytes is given"), COLTYPE_SIZE },
	[COL_ZONE_NR] = { "ZONE-NR", 8, SCOLS_FL_RIGHT, N_("number of zones"), COLTYPE_NUM },
	[COL_ZONE_OMAX] = { "ZONE-OMAX", 10, SCOLS_FL_RIGHT, N_("maximum number of open zones"), COLTYPE_NUM },
	[COL_ZONE_AMAX] = { "ZONE-AMAX", 10, SCOLS_FL_RIGHT, N_("maximum number of active zones"), COLTYPE_NUM },
};

/* "userdata" used by callback for libsmartcols filter */
struct filler_data {
	struct lsblk_device *dev;
	struct lsblk_device *parent;
};

struct lsblk *lsblk;	/* global handler */

/*
 * columns[] array specifies all currently wanted output column. The columns
 * are defined by infos[] array and you can specify (on command line) each
 * column twice. That's enough, dynamically allocated array of the columns is
 * unnecessary overkill and over-engineering in this case
 */
static int columns[ARRAY_SIZE(infos) * 2];
static size_t ncolumns;

static inline void add_column(int id)
{
	if (ncolumns >= ARRAY_SIZE(columns))
		errx(EXIT_FAILURE, _("too many columns specified, "
				     "the limit is %zu columns"),
				ARRAY_SIZE(columns) - 1);
	columns[ ncolumns++ ] =  id;
}

static inline void add_uniq_column(int id)
{
	if (column_id_to_number(id) < 0)
		add_column(id);
}

static void lsblk_init_debug(void)
{
	__UL_INIT_DEBUG_FROM_ENV(lsblk, LSBLK_DEBUG_, 0, LSBLK_DEBUG);
}

/*
 * exclude/include devices filter based on major device numbers
 */
static int excludes[256];
static size_t nexcludes;

static int includes[256];
static size_t nincludes;

static int is_maj_excluded(int maj)
{
	size_t i;

	assert(ARRAY_SIZE(excludes) > nexcludes);

	if (!nexcludes)
		return 0;	/* filter not enabled, device not excluded */

	for (i = 0; i < nexcludes; i++) {
		if (excludes[i] == maj) {
			DBG(FILTER, ul_debug("exclude: maj=%d", maj));
			return 1;
		}
	}
	return 0;
}

static int is_maj_included(int maj)
{
	size_t i;

	assert(ARRAY_SIZE(includes) > nincludes);

	if (!nincludes)
		return 1;	/* filter not enabled, device is included */

	for (i = 0; i < nincludes; i++) {
		if (includes[i] == maj) {
			DBG(FILTER, ul_debug("include: maj=%d", maj));
			return 1;
		}
	}
	return 0;
}

/* Converts column sequential number to column ID (COL_*) */
static int get_column_id(int num)
{
	assert(num >= 0);
	assert((size_t) num < ncolumns);
	assert(columns[num] < (int) ARRAY_SIZE(infos));
	return columns[num];
}

/* Returns column description for the column sequential number */
static const struct colinfo *get_column_info(int num)
{
	return &infos[ get_column_id(num) ];
}

/* Converts column name (as defined in the infos[] to the column ID */
static int column_name_to_id(const char *name, size_t namesz)
{
	size_t i;

	/* name as diplayed for users */
	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const char *cn = infos[i].name;

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}

	/* name as used in expressions, JSON output etc. */
	if (strnchr(name, namesz, '_')) {
		char *buf = NULL;
		size_t bufsz = 0;

		for (i = 0; i < ARRAY_SIZE(infos); i++) {
			if (scols_shellvar_name(infos[i].name, &buf, &bufsz) != 0)
				continue;
			if (!strncasecmp(name, buf, namesz) && !*(buf + namesz)) {
				free(buf);
				return i;
			}
		}
		free(buf);
	}

	warnx(_("unknown column: %s"), name);
	return -1;
}

/* Converts column ID (COL_*) to column sequential number */
static int column_id_to_number(int id)
{
	size_t i;

	for (i = 0; i < ncolumns; i++)
		if (columns[i] == id)
			return i;
	return -1;
}

/* Checks for DM prefix in the device name */
static int is_dm(const char *name)
{
	return strncmp(name, "dm-", 3) ? 0 : 1;
}

/* Returns full pat to the device node (TODO: what about sysfs_blkdev_get_path()) */
static char *get_device_path(struct lsblk_device *dev)
{
	char path[PATH_MAX];

	assert(dev);
	assert(dev->name);

	if (is_dm(dev->name))
		return __canonicalize_dm_name(lsblk->sysroot, dev->name);

	snprintf(path, sizeof(path), "/dev/%s", dev->name);
	sysfs_devname_sys_to_dev(path);
	return xstrdup(path);
}

static int is_readonly_device(struct lsblk_device *dev)
{
	int fd, ro = 0;

	if (ul_path_scanf(dev->sysfs, "ro", "%d", &ro) == 1)
		return ro;

	/* fallback if "ro" attribute does not exist */
	fd = open(dev->filename, O_RDONLY);
	if (fd != -1) {
		if (ioctl(fd, BLKROGET, &ro) != 0)
			ro = 0;
		close(fd);
	}
	return ro;
}

static char *get_scheduler(struct lsblk_device *dev)
{
	char buf[128];
	char *p, *res = NULL;

	if (ul_path_read_buffer(dev->sysfs, buf, sizeof(buf), "queue/scheduler") == 0)
		return NULL;
	p = strchr(buf, '[');
	if (p) {
		res = p + 1;
		p = strchr(res, ']');
		if (p) {
			*p = '\0';
			res = xstrdup(res);
		} else
			res = NULL;
	}
	return res;
}

static char *get_type(struct lsblk_device *dev)
{
	char *res = NULL, *p;

	if (device_is_partition(dev))
		return xstrdup("part");

	if (is_dm(dev->name)) {
		char *dm_uuid = NULL;

		/* The DM_UUID prefix should be set to subsystem owning
		 * the device - LVM, CRYPT, DMRAID, MPATH, PART */
		if (ul_path_read_string(dev->sysfs, &dm_uuid, "dm/uuid") > 0
		    && dm_uuid) {
			char *tmp = dm_uuid;
			char *dm_uuid_prefix = strsep(&tmp, "-");

			if (dm_uuid_prefix) {
				/* kpartx hack to remove partition number */
				if (strncasecmp(dm_uuid_prefix, "part", 4) == 0)
					dm_uuid_prefix[4] = '\0';

				res = xstrdup(dm_uuid_prefix);
			}
		}

		free(dm_uuid);
		if (!res)
			/* No UUID or no prefix - just mark it as DM device */
			res = xstrdup("dm");

	} else if (!strncmp(dev->name, "loop", 4)) {
		res = xstrdup("loop");

	} else if (!strncmp(dev->name, "md", 2)) {
		char *md_level = NULL;

		ul_path_read_string(dev->sysfs, &md_level, "md/level");
		res = md_level ? md_level : xstrdup("md");

	} else {
		const char *type = NULL;
		int x = 0;

		if (ul_path_read_s32(dev->sysfs, &x, "device/type") == 0)
			type = blkdev_scsi_type_to_name(x);
		if (!type)
			type = "disk";
		res = xstrdup(type);
	}

	for (p = res; p && *p; p++)
		*p = tolower((unsigned char) *p);
	return res;
}

/* Thanks to lsscsi code for idea of detection logic used here */
static const char *get_transport(struct lsblk_device *dev)
{
	struct path_cxt *sysfs = dev->sysfs;
	char *attr = NULL;
	const char *trans = NULL;


	/* SCSI - Serial Peripheral Interface */
	if (sysfs_blkdev_scsi_host_is(sysfs, "spi"))
		trans = "spi";

	/* FC/FCoE - Fibre Channel / Fibre Channel over Ethernet */
	else if (sysfs_blkdev_scsi_host_is(sysfs, "fc")) {
		attr = sysfs_blkdev_scsi_host_strdup_attribute(sysfs, "fc", "symbolic_name");
		if (!attr)
			return NULL;
		trans = strstr(attr, " over ") ? "fcoe" : "fc";
		free(attr);
	}

	/* SAS - Serial Attached SCSI */
	else if (sysfs_blkdev_scsi_host_is(sysfs, "sas") ||
		 sysfs_blkdev_scsi_has_attribute(sysfs, "sas_device"))
		trans = "sas";


	/* SBP - Serial Bus Protocol (FireWire) */
	else if (sysfs_blkdev_scsi_has_attribute(sysfs, "ieee1394_id"))
		trans = "sbp";

	/* iSCSI */
	else if (sysfs_blkdev_scsi_host_is(sysfs, "iscsi"))
		trans ="iscsi";

	/* USB - Universal Serial Bus */
	else if (sysfs_blkdev_scsi_path_contains(sysfs, "usb"))
		trans = "usb";

	/* ATA, SATA */
	else if (sysfs_blkdev_scsi_host_is(sysfs, "scsi")) {
		attr = sysfs_blkdev_scsi_host_strdup_attribute(sysfs, "scsi", "proc_name");
		if (!attr)
			return NULL;
		if (!strncmp(attr, "ahci", 4) || !strncmp(attr, "sata", 4))
			trans = "sata";
		else if (strstr(attr, "ata"))
			trans = "ata";
		free(attr);

	} else if (strncmp(dev->name, "nvme", 4) == 0) {
		trans = "nvme";
	} else if (strncmp(dev->name, "vd", 2) == 0)
		trans = "virtio";
	else if (strncmp(dev->name, "mmcblk", 6) == 0)
		trans = "mmc";

	return trans;
}

static char *get_subsystems(struct lsblk_device *dev)
{
	char path[PATH_MAX];
	char *sub, *chain, *res = NULL;
	size_t len = 0, last = 0;

	chain = sysfs_blkdev_get_devchain(dev->sysfs, path, sizeof(path));
	if (!chain)
		return NULL;

	while (sysfs_blkdev_next_subsystem(dev->sysfs, chain, &sub) == 0) {
		size_t sz;

		/* don't create "block:scsi:scsi", but "block:scsi" */
		if (len && strcmp(res + last, sub) == 0) {
			free(sub);
			continue;
		}

		sz = strlen(sub);
		res = xrealloc(res, len + sz + 2);
		if (len)
			res[len++] = ':';

		memcpy(res + len, sub, sz + 1);
		last = len;
		len += sz;
		free(sub);
	}

	return res;
}


#define is_parsable(_l)	(scols_table_is_raw((_l)->table) || \
			 scols_table_is_export((_l)->table) || \
			 scols_table_is_json((_l)->table))

static char *mk_name(const char *name)
{
	char *p;
	if (!name)
		return NULL;
	if (lsblk->paths)
		xasprintf(&p, "/dev/%s", name);
	else
		p = xstrdup(name);
	if (p)
		sysfs_devname_sys_to_dev(p);
	return p;
}

static char *mk_dm_name(const char *name)
{
	char *p;
	if (!name)
		return NULL;
	if (lsblk->paths)
		xasprintf(&p, "/dev/mapper/%s", name);
	else
		p = xstrdup(name);
	return p;
}

/* stores data to scols cell userdata (invisible and independent on output)
 * to make the original values accessible for sort functions
 */
static void set_rawdata_u64(struct libscols_line *ln, int col, uint64_t x)
{
	struct libscols_cell *ce = scols_line_get_cell(ln, col);
	uint64_t *data;

	if (!ce)
		return;
	data = xmalloc(sizeof(uint64_t));
	*data = x;
	scols_cell_set_userdata(ce, data);
}

/* do not modify *data on any error */
static void str2u64(const char *str, uint64_t *data)
{
	uintmax_t num;
	char *end = NULL;

	errno = 0;
	if (str == NULL || *str == '\0')
		return;
	num = strtoumax(str, &end, 10);

	if (errno || str == end || (end && *end))
		return;
	*data = num;
}

static void unref_line_rawdata(struct libscols_line *ln, struct libscols_table *tb)
{
	size_t i;

	for (i = 0; i < ncolumns; i++) {
		struct libscols_column *cl = scols_table_get_column(tb, i);
		struct libscols_cell *ce;
		void *data;

		if (cl != lsblk->sort_col && !scols_column_has_data_func(cl))
			continue;

		ce = scols_line_get_column_cell(ln, cl);
		data = scols_cell_get_userdata(ce);
		free(data);
	}
}

static void unref_table_rawdata(struct libscols_table *tb)
{
	struct libscols_iter *itr;
	struct libscols_line *ln;

	if (!tb || !lsblk->rawdata)
		return;

	itr = scols_new_iter(SCOLS_ITER_FORWARD);
	if (!itr)
		return;
	while (scols_table_next_line(tb, itr, &ln) == 0)
		unref_line_rawdata(ln, tb);

	scols_free_iter(itr);
}

static char *get_vfs_attribute(struct lsblk_device *dev, int id)
{
	char *sizestr;
	uint64_t vfs_attr = 0;

	if (!dev->fsstat.f_blocks) {
		const char *mnt = lsblk_device_get_mountpoint(dev);
		if (!mnt || dev->is_swap)
			return NULL;
		if (statvfs(mnt, &dev->fsstat) != 0)
			return NULL;
	}

	switch(id) {
	case COL_FSSIZE:
		vfs_attr = dev->fsstat.f_frsize * dev->fsstat.f_blocks;
		break;
	case COL_FSAVAIL:
		vfs_attr = dev->fsstat.f_frsize * dev->fsstat.f_bavail;
		break;
	case COL_FSUSED:
		vfs_attr = dev->fsstat.f_frsize * (dev->fsstat.f_blocks - dev->fsstat.f_bfree);
		break;
	case COL_FSUSEPERC:
		if (dev->fsstat.f_blocks == 0)
			return xstrdup("-");

		xasprintf(&sizestr, "%.0f%%",
				(double)(dev->fsstat.f_blocks - dev->fsstat.f_bfree) /
				dev->fsstat.f_blocks * 100);
		return sizestr;
	}

	if (!vfs_attr)
		sizestr = xstrdup("0");
	else if (lsblk->bytes)
		xasprintf(&sizestr, "%ju", vfs_attr);
	else
		sizestr = size_to_human_string(SIZE_SUFFIX_1LETTER, vfs_attr);

	return sizestr;
}

static struct stat *device_get_stat(struct lsblk_device *dev)
{
	if (!dev->st.st_rdev
	    && stat(dev->filename, &dev->st) != 0)
		return NULL;

	return &dev->st;
}

static int is_removable_device(struct lsblk_device *dev, struct lsblk_device *parent)
{
	struct path_cxt *pc;

	if (dev->removable != -1)
		goto done;

	dev->removable = sysfs_blkdev_is_removable(dev->sysfs);

	if (!dev->removable && parent) {
		pc = sysfs_blkdev_get_parent(dev->sysfs);
		if (!pc)
			goto done;

		if (pc == parent->sysfs)
			/* dev is partition and parent is whole-disk  */
			dev->removable = is_removable_device(parent, NULL);
		else
			/* parent is something else, use sysfs parent */
			dev->removable = sysfs_blkdev_is_removable(pc);
	}
done:
	if (dev->removable == -1)
		dev->removable = 0;
	return dev->removable;
}

static uint64_t device_get_discard_granularity(struct lsblk_device *dev)
{
	if (dev->discard_granularity == (uint64_t) -1
	    && ul_path_read_u64(dev->sysfs, &dev->discard_granularity,
			        "queue/discard_granularity") != 0)
		dev->discard_granularity = 0;

	return dev->discard_granularity;
}

static void device_read_bytes(struct lsblk_device *dev, char *path, char **str,
			      uint64_t *rawdata)
{
	uint64_t x;

	if (lsblk->bytes) {
		ul_path_read_string(dev->sysfs, str, path);
		if (rawdata)
			str2u64(*str, rawdata);
		return;
	}

	if (ul_path_read_u64(dev->sysfs, &x, path) == 0) {
		*str = size_to_human_string(SIZE_SUFFIX_1LETTER, x);
		if (rawdata)
			*rawdata = x;
	}
}

static void process_mq(struct lsblk_device *dev, char **str)
{
	unsigned int queues = 0;

	DBG(DEV, ul_debugobj(dev, "%s: process mq", dev->name));

	queues = ul_path_count_dirents(dev->sysfs, "mq");
	if (!queues) {
		*str = xstrdup("1");
		DBG(DEV, ul_debugobj(dev, "%s: no mq supported, use a single queue", dev->name));
		return;
	}

	DBG(DEV, ul_debugobj(dev, "%s: has %d queues", dev->name, queues));
	xasprintf(str, "%3u", queues);
}

/*
 * Generates data (string) for column specified by column ID for specified device. If rawdata
 * is not NULL then returns number usable to sort the column if the data are available for the
 * column.
 */
static char *device_get_data(
		struct lsblk_device *dev,		/* device */
		struct lsblk_device *parent,		/* device parent as defined in the tree */
		int id,					/* column ID (COL_*) */
		uint64_t *rawdata,			/* returns sort data as number */
		size_t *datasiz)
{
	struct lsblk_devprop *prop = NULL;
	char *str = NULL;

	switch(id) {
	case COL_NAME:
		str = dev->dm_name ? mk_dm_name(dev->dm_name) :	mk_name(dev->name);
		break;
	case COL_KNAME:
		str = mk_name(dev->name);
		break;
	case COL_PKNAME:
		if (parent)
			str = mk_name(parent->name);
		break;
	case COL_PATH:
		if (dev->filename)
			str = xstrdup(dev->filename);
		break;
	case COL_OWNER:
		if (lsblk->sysroot)
			prop = lsblk_device_get_properties(dev);
		if (prop && prop->owner) {
			str = xstrdup(prop->owner);
		} else {
			struct stat *st = device_get_stat(dev);
			struct passwd *pw = st ? getpwuid(st->st_uid) : NULL;
			if (pw)
				str = xstrdup(pw->pw_name);
		}
		break;
	case COL_GROUP:
		if (lsblk->sysroot)
			prop = lsblk_device_get_properties(dev);
		if (prop && prop->group) {
			str = xstrdup(prop->group);
		} else {
			struct stat *st = device_get_stat(dev);
			struct group *gr = st ? getgrgid(st->st_gid) : NULL;
			if (gr)
				str = xstrdup(gr->gr_name);
		}
		break;
	case COL_MODE:
		if (lsblk->sysroot)
			prop = lsblk_device_get_properties(dev);
		if (prop && prop->mode) {
			str = xstrdup(prop->mode);
		} else {
			struct stat *st = device_get_stat(dev);
			char md[11] = { '\0' };

			if (st)
				str = xstrdup(xstrmode(st->st_mode, md));
		}
		break;
	case COL_MAJMIN:
		if (is_parsable(lsblk))
			xasprintf(&str, "%u:%u", dev->maj, dev->min);
		else
			xasprintf(&str, "%3u:%-3u", dev->maj, dev->min);
		if (rawdata)
			*rawdata = makedev(dev->maj, dev->min);
		break;
	case COL_MAJ:
		xasprintf(&str, "%u", dev->maj);
		if (rawdata)
			*rawdata = dev->maj;
		break;
	case COL_MIN:
		xasprintf(&str, "%u", dev->min);
		if (rawdata)
			*rawdata = dev->min;
		break;
	case COL_FSTYPE:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->fstype)
			str = xstrdup(prop->fstype);
		break;
	case COL_FSSIZE:
	case COL_FSAVAIL:
	case COL_FSUSED:
	case COL_FSUSEPERC:
		str = get_vfs_attribute(dev, id);
		break;
	case COL_FSVERSION:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->fsversion)
			str = xstrdup(prop->fsversion);
		break;
	case COL_TARGET:
	{
		const char *p = lsblk_device_get_mountpoint(dev);
		if (p)
			str = xstrdup(p);
		break;
	}
	case COL_TARGETS:
	{
		size_t i, n = 0;
		struct ul_buffer buf = UL_INIT_BUFFER;
		struct libmnt_fs **fss = lsblk_device_get_filesystems(dev, &n);

		for (i = 0; i < n; i++) {
			struct libmnt_fs *fs = fss[i];
			if (mnt_fs_is_swaparea(fs))
				ul_buffer_append_string(&buf, "[SWAP]");
			else
				ul_buffer_append_string(&buf, mnt_fs_get_target(fs));
			ul_buffer_append_data(&buf, "\0", 1);
		}
		str = ul_buffer_get_data(&buf, datasiz, NULL);
		break;
	}
	case COL_FSROOTS:
	{
		size_t i, n = 0;
		struct ul_buffer buf = UL_INIT_BUFFER;
		struct libmnt_fs **fss = lsblk_device_get_filesystems(dev, &n);

		for (i = 0; i < n; i++) {
			struct libmnt_fs *fs = fss[i];
			const char *root = mnt_fs_get_root(fs);
			if (mnt_fs_is_swaparea(fs))
				continue;
			ul_buffer_append_string(&buf, root ? root : "/");
			ul_buffer_append_data(&buf, "\0", 1);
		}
		str = ul_buffer_get_data(&buf, datasiz, NULL);
		break;
	}
	case COL_LABEL:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->label)
			str = xstrdup(prop->label);
		break;
	case COL_UUID:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->uuid)
			str = xstrdup(prop->uuid);
		break;
	case COL_PTUUID:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->ptuuid)
			str = xstrdup(prop->ptuuid);
		break;
	case COL_PTTYPE:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->pttype)
			str = xstrdup(prop->pttype);
		break;
	case COL_PARTTYPE:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->parttype)
			str = xstrdup(prop->parttype);
		break;
	case COL_PARTTYPENAME:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->parttype && prop->pttype) {
			const char *x = lsblk_parttype_code_to_string(
						prop->parttype, prop->pttype);
			if (x)
				str = xstrdup(x);
		}
		break;
	case COL_PARTLABEL:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->partlabel)
			str = xstrdup(prop->partlabel);
		break;
	case COL_PARTUUID:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->partuuid)
			str = xstrdup(prop->partuuid);
		break;
	case COL_PARTFLAGS:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->partflags)
			str = xstrdup(prop->partflags);
		break;
	case COL_PARTN:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->partn)
			str = xstrdup(prop->partn);
		break;
	case COL_WWN:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->wwn)
			str = xstrdup(prop->wwn);
		break;
	case COL_IDLINK:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->idlink)
			str = xstrdup(prop->idlink);
		break;
	case COL_ID:
		prop = lsblk_device_get_properties(dev);
		if (prop && prop->idlink) {
			/* skip bus/subsystem prefix */
			const char *p = strchr(prop->idlink, '-');
			str = p && *(p + 1) ? xstrdup(p+1) : xstrdup(prop->idlink);
		}
		break;
	case COL_RA:
		ul_path_read_string(dev->sysfs, &str, "queue/read_ahead_kb");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_RO:
		str = xstrdup(is_readonly_device(dev) ? "1" : "0");
		break;
	case COL_RM:
		str = xstrdup(is_removable_device(dev, parent) ? "1" : "0");
		break;
	case COL_HOTPLUG:
		str = sysfs_blkdev_is_hotpluggable(dev->sysfs) ? xstrdup("1") : xstrdup("0");
		break;
	case COL_ROTA:
		ul_path_read_string(dev->sysfs, &str, "queue/rotational");
		break;
	case COL_RAND:
		ul_path_read_string(dev->sysfs, &str, "queue/add_random");
		break;
	case COL_MODEL:
		if (!device_is_partition(dev) && dev->nslaves == 0) {
			prop = lsblk_device_get_properties(dev);
			if (prop && prop->model)
				str = xstrdup(prop->model);
			else
				ul_path_read_string(dev->sysfs, &str, "device/model");
		}
		break;
	case COL_SERIAL:
		if (!device_is_partition(dev) && dev->nslaves == 0) {
			prop = lsblk_device_get_properties(dev);
			if (prop && prop->serial)
				str = xstrdup(prop->serial);
			else
				ul_path_read_string(dev->sysfs, &str, "device/serial");
		}
		break;
	case COL_REV:
		if (!device_is_partition(dev) && dev->nslaves == 0) {
			prop = lsblk_device_get_properties(dev);
			if (prop && prop->revision)
				str = xstrdup(prop->revision);
			else
				ul_path_read_string(dev->sysfs, &str, "device/rev");
		}
		break;
	case COL_VENDOR:
		if (!device_is_partition(dev) && dev->nslaves == 0)
			ul_path_read_string(dev->sysfs, &str, "device/vendor");
		break;
	case COL_SIZE:
		if (lsblk->bytes)
			xasprintf(&str, "%ju", dev->size);
		else
			str = size_to_human_string(SIZE_SUFFIX_1LETTER, dev->size);
		if (rawdata)
			*rawdata = dev->size;
		break;
	case COL_START:
		ul_path_read_string(dev->sysfs, &str, "start");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_STATE:
		if (!device_is_partition(dev) && !dev->dm_name)
			ul_path_read_string(dev->sysfs, &str, "device/state");
		else if (dev->dm_name) {
			int x = 0;
			if (ul_path_read_s32(dev->sysfs, &x, "dm/suspended") == 0)
				str = xstrdup(x ? "suspended" : "running");
		}
		break;
	case COL_ALIOFF:
		ul_path_read_string(dev->sysfs, &str, "alignment_offset");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_MINIO:
		ul_path_read_string(dev->sysfs, &str, "queue/minimum_io_size");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_OPTIO:
		ul_path_read_string(dev->sysfs, &str, "queue/optimal_io_size");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_PHYSEC:
		ul_path_read_string(dev->sysfs, &str, "queue/physical_block_size");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_LOGSEC:
		ul_path_read_string(dev->sysfs, &str, "queue/logical_block_size");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_SCHED:
		str = get_scheduler(dev);
		break;
	case COL_RQ_SIZE:
		ul_path_read_string(dev->sysfs, &str, "queue/nr_requests");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_TYPE:
		str = get_type(dev);
		break;
	case COL_HCTL:
	{
		int h, c, t, l;
		if (sysfs_blkdev_scsi_get_hctl(dev->sysfs, &h, &c, &t, &l) == 0)
			xasprintf(&str, "%d:%d:%d:%d", h, c, t, l);
		break;
	}
	case COL_TRANSPORT:
	{
		const char *trans = get_transport(dev);
		if (trans)
			str = xstrdup(trans);
		break;
	}
	case COL_SUBSYS:
		str = get_subsystems(dev);
		break;
	case COL_DALIGN:
		if (device_get_discard_granularity(dev) > 0)
			ul_path_read_string(dev->sysfs, &str, "discard_alignment");
		if (!str)
			str = xstrdup("0");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_DGRAN:
		if (lsblk->bytes) {
			ul_path_read_string(dev->sysfs, &str, "queue/discard_granularity");
			if (rawdata)
				str2u64(str, rawdata);
		} else {
			uint64_t x = device_get_discard_granularity(dev);
			str = size_to_human_string(SIZE_SUFFIX_1LETTER, x);
			if (rawdata)
				*rawdata = x;
		}
		break;
	case COL_DMAX:
		device_read_bytes(dev, "queue/discard_max_bytes", &str, rawdata);
		break;
	case COL_DZERO:
		if (device_get_discard_granularity(dev) > 0)
			ul_path_read_string(dev->sysfs, &str, "queue/discard_zeroes_data");
		if (!str)
			str = xstrdup("0");
		break;
	case COL_WSAME:
		device_read_bytes(dev, "queue/write_same_max_bytes", &str, rawdata);
		if (!str)
			str = xstrdup("0");
		break;
	case COL_ZONED:
		ul_path_read_string(dev->sysfs, &str, "queue/zoned");
		break;
	case COL_ZONE_SZ:
	{
		uint64_t x;

		if (ul_path_read_u64(dev->sysfs, &x, "queue/chunk_sectors") == 0) {
			x <<= 9;
			if (lsblk->bytes)
				xasprintf(&str, "%ju", x);
			else
				str = size_to_human_string(SIZE_SUFFIX_1LETTER, x);
			if (rawdata)
				*rawdata = x;
		}
		break;
	}
	case COL_ZONE_WGRAN:
		device_read_bytes(dev, "queue/zone_write_granularity", &str, rawdata);
		break;
	case COL_ZONE_APP:
		device_read_bytes(dev, "queue/zone_append_max_bytes", &str, rawdata);
		break;
	case COL_ZONE_NR:
		ul_path_read_string(dev->sysfs, &str, "queue/nr_zones");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_ZONE_OMAX:
		ul_path_read_string(dev->sysfs, &str, "queue/max_open_zones");
		if (!str)
			str = xstrdup("0");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_ZONE_AMAX:
		ul_path_read_string(dev->sysfs, &str, "queue/max_active_zones");
		if (!str)
			str = xstrdup("0");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	case COL_DAX:
		ul_path_read_string(dev->sysfs, &str, "queue/dax");
		break;
	case COL_MQ:
		process_mq(dev, &str);
		break;
	case COL_DISKSEQ:
		ul_path_read_string(dev->sysfs, &str, "diskseq");
		if (rawdata)
			str2u64(str, rawdata);
		break;
	};

	return str;
}

static void device_fill_scols_cell(struct lsblk_device *dev,
				   struct lsblk_device *parent,
				   struct libscols_line *ln,
				   size_t colnum)
{
	struct libscols_cell *ce;
	struct libscols_column *cl = scols_table_get_column(lsblk->table, colnum);
	char *data;
	size_t datasiz = 0;
	int rc, id = get_column_id(colnum);

	if (lsblk->sort_id == id || scols_column_has_data_func(cl)) {
		uint64_t rawdata = (uint64_t) -1;

		data = device_get_data(dev, parent, id, &rawdata, &datasiz);
		if (data && rawdata != (uint64_t) -1)
			set_rawdata_u64(ln, colnum, rawdata);
	} else
		data = device_get_data(dev, parent, id, NULL, &datasiz);

	if (!data)
		return;
	DBG(DEV, ul_debugobj(dev, " refer data[%zu]=\"%s\"", colnum, data));
	ce = scols_line_get_cell(ln, colnum);
	if (!ce)
		return;
	rc = datasiz ? scols_cell_refer_memory(ce, data, datasiz)
		     : scols_cell_refer_data(ce, data);
	if (rc)
		err(EXIT_FAILURE, _("failed to add output data"));
}

static int filter_filler_cb(
		 struct libscols_filter *fltr __attribute__((__unused__)),
		 struct libscols_line *ln,
		 size_t colnum,
		 void *userdata)
{
	struct filler_data *fid = (struct filler_data *) userdata;

	device_fill_scols_cell(fid->dev, fid->parent, ln, colnum);
	return 0;
}

/*
 * Adds data for all wanted columns about the device to the smartcols table
 */
static void device_to_scols(
			struct lsblk_device *dev,
			struct lsblk_device *parent,
			struct libscols_table *tab,
			struct libscols_line *parent_line)
{
	size_t i;
	struct libscols_line *ln;
	struct lsblk_iter itr;
	struct lsblk_device *child = NULL;
	int link_group = 0, nocount = 0;


	DBG(DEV, ul_debugobj(dev, "add '%s' to scols", dev->name));
	ON_DBG(DEV, if (ul_path_isopen_dirfd(dev->sysfs)) ul_debugobj(dev, " %s ---> is open!", dev->name));

	if (!parent && dev->wholedisk)
		parent = dev->wholedisk;

	/* Do not print device more than once on --list if tree order is not requested */
	if (!(lsblk->flags & LSBLK_TREE) && !lsblk->force_tree_order && dev->is_printed)
		return;

	if (lsblk->merge && list_count_entries(&dev->parents) > 1) {
		if (!lsblk_device_is_last_parent(dev, parent))
			return;
		link_group = 1;
	}

	ln = scols_table_new_line(tab, link_group ? NULL : parent_line);
	if (!ln)
		err(EXIT_FAILURE, _("failed to allocate output line"));

	dev->is_printed = 1;

	/* filter lines, smartcols filter can ask for data */
	if (lsblk->filter) {
		int status = 0;
		struct filler_data fid = {
			.dev = dev,
			.parent = parent
		};

		scols_filter_set_filler_cb(lsblk->filter,
				filter_filler_cb, (void *) &fid);

		if (scols_line_apply_filter(ln, lsblk->filter, &status))
			err(EXIT_FAILURE, _("failed to apply filter"));
		if (status == 0) {
			struct libscols_line *x = scols_line_get_parent(ln);

			if (x)
				scols_line_remove_child(x, ln);
			unref_line_rawdata(ln, tab);
			scols_table_remove_line(tab, ln);
			ln = NULL;
		}
	}

	/* read column specific data and set it to smartcols table line */
	for (i = 0; ln && i < ncolumns; i++) {
		if (scols_line_is_filled(ln, i))
			continue;
		device_fill_scols_cell(dev, parent, ln, i);
	}

	if (ln && link_group) {
		struct lsblk_device *p;
		struct libscols_line *gr = parent_line;

		/* Merge all my parents to the one group */
		DBG(DEV, ul_debugobj(dev, " grouping parents [--merge]"));
		lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);
		while (lsblk_device_next_parent(dev, &itr, &p) == 0) {
			if (!p->scols_line) {
				DBG(DEV, ul_debugobj(dev, " *** ignore '%s' no scols line yet", p->name));
				continue;
			}
			DBG(DEV, ul_debugobj(dev, " group '%s'", p->name));
			scols_table_group_lines(tab, p->scols_line, gr, 0);
		}

		/* Link the group -- this makes group->child connection */
		DBG(DEV, ul_debugobj(dev, " linking the group [--merge]"));
		scols_line_link_group(ln, gr, 0);
	}

	/* The same device could be printed more than once, don't use it in counter */
	if (dev->scols_line)
		nocount = 1;

	dev->scols_line = ln;

	if (dev->npartitions == 0)
		/* For partitions we often read from parental whole-disk sysfs,
		 * otherwise we can close */
		ul_path_close_dirfd(dev->sysfs);

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);
	while (lsblk_device_next_child(dev, &itr, &child) == 0) {
		DBG(DEV, ul_debugobj(dev, "%s -> continue to child", dev->name));
		device_to_scols(child, dev, tab, ln);
		DBG(DEV, ul_debugobj(dev, "%s <- child done", dev->name));
	}

	/* apply highligther */
	if (ln && lsblk->hlighter) {
		int status = 0;

		if (scols_line_apply_filter(ln, lsblk->hlighter, &status) == 0
		    && status)
			scols_line_set_color(ln, lsblk->hlighter_seq);
	}

	/* apply counters */
	if (!nocount) {
		for (i = 0; ln && i < lsblk->ncts; i++)
			scols_line_apply_filter(ln, lsblk->ct_filters[i], NULL);
	}

	/* Let's be careful with number of open files */
	ul_path_close_dirfd(dev->sysfs);
}

/*
 * Walks on tree and adds one line for each device to the smartcols table
 */
static void devtree_to_scols(struct lsblk_devtree *tr, struct libscols_table *tab)
{
	struct lsblk_iter itr;
	struct lsblk_device *dev = NULL;

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (lsblk_devtree_next_root(tr, &itr, &dev) == 0)
		device_to_scols(dev, NULL, tab, NULL);
}

static int ignore_empty(struct lsblk_device *dev)
{
	/* show all non-empty devices */
	if (dev->size)
		return 0;

	if (lsblk->noempty && dev->size == 0)
		return 1;

	/* ignore empty loop devices without backing file */
	if (dev->maj == LOOPDEV_MAJOR &&
	    !loopdev_has_backing_file(dev->filename))
		return 1;

	return 0;
}

/*
 * Reads very basic information about the device from sysfs into the device struct
 */
static int initialize_device(struct lsblk_device *dev,
		    struct lsblk_device *wholedisk,
		    const char *name)
{
	dev_t devno;

	DBG(DEV, ul_debugobj(dev, "initialize %s [wholedisk=%p %s]",
			name, wholedisk, wholedisk ? wholedisk->name : ""));

	if (sysfs_devname_is_hidden(lsblk->sysroot, name)) {
		DBG(DEV, ul_debugobj(dev, "%s: hidden, ignore", name));
		return -1;
	}

	dev->name = xstrdup(name);

	if (wholedisk) {
		dev->wholedisk = wholedisk;
		lsblk_ref_device(wholedisk);
	}

	dev->filename = get_device_path(dev);
	if (!dev->filename) {
		DBG(DEV, ul_debugobj(dev, "%s: failed to get device path", dev->name));
		return -1;
	}
	DBG(DEV, ul_debugobj(dev, "%s: filename=%s", dev->name, dev->filename));

	devno = __sysfs_devname_to_devno(lsblk->sysroot, dev->name, wholedisk ? wholedisk->name : NULL);
	if (!devno) {
		DBG(DEV, ul_debugobj(dev, "%s: unknown device name", dev->name));
		return -1;
	}

	dev->sysfs = ul_new_sysfs_path(devno, wholedisk ? wholedisk->sysfs : NULL, lsblk->sysroot);
	if (!dev->sysfs) {
		DBG(DEV, ul_debugobj(dev, "%s: failed to initialize sysfs handler", dev->name));
		return -1;
	}

	dev->maj = major(devno);
	dev->min = minor(devno);
	dev->size = 0;

	if (ul_path_read_u64(dev->sysfs, &dev->size, "size") == 0)	/* in sectors */
		dev->size <<= 9;					/* in bytes */

	/* Ignore devices of zero size */
	if (!lsblk->all_devices && ignore_empty(dev)) {
		DBG(DEV, ul_debugobj(dev, "zero size device -- ignore"));
		return -1;
	}
	if (is_dm(dev->name)) {
		ul_path_read_string(dev->sysfs, &dev->dm_name, "dm/name");
		if (!dev->dm_name) {
			DBG(DEV, ul_debugobj(dev, "%s: failed to get dm name", dev->name));
			return -1;
		}
	}

	dev->npartitions = sysfs_blkdev_count_partitions(dev->sysfs, dev->name);
	dev->nholders = ul_path_count_dirents(dev->sysfs, "holders");
	dev->nslaves = ul_path_count_dirents(dev->sysfs, "slaves");

	DBG(DEV, ul_debugobj(dev, "%s: npartitions=%d, nholders=%d, nslaves=%d",
			dev->name, dev->npartitions, dev->nholders, dev->nslaves));

	/* ignore non-SCSI devices */
	if (lsblk->scsi && sysfs_blkdev_scsi_get_hctl(dev->sysfs, NULL, NULL, NULL, NULL)) {
		DBG(DEV, ul_debugobj(dev, "non-scsi device -- ignore"));
		return -1;
	}

	/* ignore non-NVMe devices */
	if (lsblk->nvme) {
		const char *transport = get_transport(dev);

		if (!transport || strcmp(transport, "nvme")) {
			DBG(DEV, ul_debugobj(dev, "non-nvme device -- ignore"));
			return -1;
		}
	}

	/* ignore non-virtio devices */
	if (lsblk->virtio) {
		const char *transport = get_transport(dev);

		if (!transport || strcmp(transport, "virtio")) {
			DBG(DEV, ul_debugobj(dev, "non-virtio device -- ignore"));
			return -1;
		}
	}

	DBG(DEV, ul_debugobj(dev, "%s: context successfully initialized", dev->name));
	return 0;
}

static struct lsblk_device *devtree_get_device_or_new(struct lsblk_devtree *tr,
					       struct lsblk_device *disk,
					       const char *name)
{
	struct lsblk_device *dev = lsblk_devtree_get_device(tr, name);

	if (!dev) {
		dev = lsblk_new_device();
		if (!dev)
			err(EXIT_FAILURE, _("failed to allocate device"));

		if (initialize_device(dev, disk, name) != 0) {
			lsblk_unref_device(dev);
			return NULL;
		}
		lsblk_devtree_add_device(tr, dev);
		lsblk_unref_device(dev);		/* keep it referenced by devtree only */
	} else
		DBG(DEV, ul_debugobj(dev, "%s: already processed", name));

	return dev;
}

static struct lsblk_device *devtree_pktcdvd_get_dep(
			struct lsblk_devtree *tr,
			struct lsblk_device *dev,
			int want_slave)
{
	char buf[PATH_MAX], *name;
	dev_t devno;

	devno = lsblk_devtree_pktcdvd_get_mate(tr,
			makedev(dev->maj, dev->min), !want_slave);
	if (!devno)
		return NULL;

	name = sysfs_devno_to_devname(devno, buf, sizeof(buf));
	if (!name)
		return NULL;

	return devtree_get_device_or_new(tr, NULL, name);
}

static int process_dependencies(
			struct lsblk_devtree *tr,
			struct lsblk_device *dev,
			int do_partitions);

/*
 * Read devices from whole-disk device into tree
 */
static int process_partitions(struct lsblk_devtree *tr, struct lsblk_device *disk)
{
	DIR *dir;
	struct dirent *d;

	assert(disk);

	/*
	 * Do not process further if there are no partitions for
	 * this device or the device itself is a partition.
	 */
	if (!disk->npartitions || device_is_partition(disk))
		return -EINVAL;

	DBG(DEV, ul_debugobj(disk, "%s: probe whole-disk for partitions", disk->name));

	dir = ul_path_opendir(disk->sysfs, NULL);
	if (!dir)
		err(EXIT_FAILURE, _("failed to open device directory in sysfs"));

	while ((d = xreaddir(dir))) {
		struct lsblk_device *part;

		if (!(sysfs_blkdev_is_partition_dirent(dir, d, disk->name)))
			continue;

		DBG(DEV, ul_debugobj(disk, "  checking %s", d->d_name));

		part = devtree_get_device_or_new(tr, disk, d->d_name);
		if (!part)
			continue;

		if (lsblk_device_new_dependence(disk, part) == 0)
			process_dependencies(tr, part, 0);

		ul_path_close_dirfd(part->sysfs);
	}

	/* For partitions we need parental (whole-disk) sysfs directory pretty
	 * often, so close it now when all is done */
	ul_path_close_dirfd(disk->sysfs);

	DBG(DEV, ul_debugobj(disk, "probe whole-disk for partitions -- done"));
	closedir(dir);
	return 0;
}

static char *get_wholedisk_from_partition_dirent(DIR *dir, struct dirent *d, char *buf, size_t bufsz)
{
	char *p;
	int len;

	if ((len = readlinkat(dirfd(dir), d->d_name, buf, bufsz - 1)) < 0)
		return 0;

	buf[len] = '\0';

	/* The path ends with ".../<device>/<partition>" */
	p = strrchr(buf, '/');
	if (!p)
		return NULL;
	*p = '\0';

	p = strrchr(buf, '/');
	if (!p)
		return NULL;
	p++;

	return p;
}

/*
 * Reads slaves/holders and partitions for specified device into device tree
 */
static int process_dependencies(
			struct lsblk_devtree *tr,
			struct lsblk_device *dev,
			int do_partitions)
{
	DIR *dir;
	struct dirent *d;
	const char *depname;
	struct lsblk_device *dep = NULL;

	assert(dev);

	if (lsblk->nodeps)
		return 0;

	/* read all or specified partition */
	if (do_partitions && dev->npartitions)
		process_partitions(tr, dev);

	DBG(DEV, ul_debugobj(dev, "%s: reading dependencies", dev->name));

	if (!(lsblk->inverse ? dev->nslaves : dev->nholders)) {
		DBG(DEV, ul_debugobj(dev, " ignore (no slaves/holders)"));
		goto done;
	}

	depname = lsblk->inverse ? "slaves" : "holders";
	dir = ul_path_opendir(dev->sysfs, depname);
	if (!dir) {
		DBG(DEV, ul_debugobj(dev, " ignore (no slaves/holders directory)"));
		goto done;
	}
	ul_path_close_dirfd(dev->sysfs);

	DBG(DEV, ul_debugobj(dev, " %s: checking for '%s' dependence", dev->name, depname));

	while ((d = xreaddir(dir))) {
		struct lsblk_device *disk = NULL;

		/* Is the dependency a partition? */
		if (sysfs_blkdev_is_partition_dirent(dir, d, NULL)) {

			char buf[PATH_MAX];
			char *diskname;

			DBG(DEV, ul_debugobj(dev, " %s: dependence is partition", d->d_name));

			diskname = get_wholedisk_from_partition_dirent(dir, d, buf, sizeof(buf));
			if (diskname)
				disk = devtree_get_device_or_new(tr, NULL, diskname);
			if (!disk) {
				DBG(DEV, ul_debugobj(dev, "  ignore no wholedisk ???"));
				goto next;
			}

			dep = devtree_get_device_or_new(tr, disk, d->d_name);
			if (!dep)
				goto next;

			if (lsblk_device_new_dependence(dev, dep) == 0)
				process_dependencies(tr, dep, 1);

			if (lsblk->inverse
			    && lsblk_device_new_dependence(dep, disk) == 0)
				process_dependencies(tr, disk, 0);
		}
		/* The dependency is a whole device. */
		else {
			DBG(DEV, ul_debugobj(dev, " %s: %s: dependence is whole-disk",
								dev->name, d->d_name));

			dep = devtree_get_device_or_new(tr, NULL, d->d_name);
			if (!dep)
				goto next;

			if (lsblk_device_new_dependence(dev, dep) == 0)
				/* For inverse tree we don't want to show partitions
				 * if the dependence is on whole-disk */
				process_dependencies(tr, dep, lsblk->inverse ? 0 : 1);
		}
next:
		if (dep && dep->sysfs)
			ul_path_close_dirfd(dep->sysfs);
		if (disk && disk->sysfs)
			ul_path_close_dirfd(disk->sysfs);
	}
	closedir(dir);
done:
	dep = devtree_pktcdvd_get_dep(tr, dev, lsblk->inverse);

	if (dep && lsblk_device_new_dependence(dev, dep) == 0) {
		lsblk_devtree_remove_root(tr, dep);
		process_dependencies(tr, dep, lsblk->inverse ? 0 : 1);
	}

	return 0;
}

/*
 * Defines the device as root node in the device tree and walks on all dependencies of the device.
 */
static int __process_one_device(struct lsblk_devtree *tr, char *devname, dev_t devno)
{
	struct lsblk_device *dev = NULL;
	struct lsblk_device *disk = NULL;
	char buf[PATH_MAX + 1], *name = NULL, *diskname = NULL;
	int real_part = 0, rc = -EINVAL;

	if (devno == 0 && devname) {
		struct stat st;

		DBG(DEV, ul_debug("%s: reading alone device", devname));

		if (stat(devname, &st) || !S_ISBLK(st.st_mode)) {
			warnx(_("%s: not a block device"), devname);
			goto leave;
		}
		devno = st.st_rdev;
	} else if (devno) {
		DBG(DEV, ul_debug("%d:%d: reading alone device", major(devno), minor(devno)));
	} else {
		assert(devno || devname);
		return -EINVAL;
	}

	/* TODO: sysfs_devno_to_devname() internally initializes path_cxt, it
	 * would be better to use ul_new_sysfs_path() + sysfs_blkdev_get_name()
	 * and reuse path_cxt for initialize_device()
	 */
	name = sysfs_devno_to_devname(devno, buf, sizeof(buf));
	if (!name) {
		if (devname)
			warn(_("%s: failed to get sysfs name"), devname);
		goto leave;
	}
	name = xstrdup(name);

	if (!strncmp(name, "dm-", 3)) {
		/* dm mapping is never a real partition! */
		real_part = 0;
	} else {
		dev_t diskno = 0;

		if (blkid_devno_to_wholedisk(devno, buf, sizeof(buf), &diskno)) {
			warn(_("%s: failed to get whole-disk device number"), name);
			goto leave;
		}
		diskname = buf;
		real_part = devno != diskno;
	}

	if (!real_part) {
		/*
		 * Device is not a partition.
		 */
		DBG(DEV, ul_debug(" non-partition"));

		dev = devtree_get_device_or_new(tr, NULL, name);
		if (!dev)
			goto leave;

		lsblk_devtree_add_root(tr, dev);
		process_dependencies(tr, dev, !lsblk->inverse);
	} else {
		/*
		 * Partition, read sysfs name of the disk device
		 */
		DBG(DEV, ul_debug(" partition"));

		disk = devtree_get_device_or_new(tr, NULL, diskname);
		if (!disk)
			goto leave;

		dev = devtree_get_device_or_new(tr, disk, name);
		if (!dev)
			goto leave;

		lsblk_devtree_add_root(tr, dev);
		process_dependencies(tr, dev, 1);

		if (lsblk->inverse
		    && lsblk_device_new_dependence(dev, disk) == 0)
			process_dependencies(tr, disk, 0);
		else
			ul_path_close_dirfd(disk->sysfs);
	}

	rc = 0;
leave:
	if (dev && dev->sysfs)
		ul_path_close_dirfd(dev->sysfs);
	if (disk && disk->sysfs)
		ul_path_close_dirfd(disk->sysfs);
	free(name);
	return rc;
}

static int process_one_device(struct lsblk_devtree *tr, char *devname)
{
	assert(devname);
	return __process_one_device(tr, devname, 0);
}

/*
 * The /sys/block contains only root devices, and no partitions. It seems more
 * simple to scan /sys/dev/block where are all devices without exceptions to get
 * top-level devices for the reverse tree.
 */
static int process_all_devices_inverse(struct lsblk_devtree *tr)
{
	DIR *dir;
	struct dirent *d;
	struct path_cxt *pc = ul_new_path(_PATH_SYS_DEVBLOCK);

	assert(lsblk->inverse);

	if (!pc)
		err(EXIT_FAILURE, _("failed to allocate /sys handler"));

	ul_path_set_prefix(pc, lsblk->sysroot);
	dir = ul_path_opendir(pc, NULL);
	if (!dir)
		goto done;

	DBG(DEV, ul_debug("iterate on " _PATH_SYS_DEVBLOCK));

	while ((d = xreaddir(dir))) {
		dev_t devno;
		int maj, min;

		DBG(DEV, ul_debug(" %s dentry", d->d_name));

		if (sscanf(d->d_name, "%d:%d", &maj, &min) != 2)
			continue;
		devno = makedev(maj, min);

		if (is_maj_excluded(maj) || !is_maj_included(maj))
			continue;
		if (ul_path_countf_dirents(pc, "%s/holders", d->d_name) != 0)
			continue;
		if (sysfs_devno_count_partitions(devno) != 0)
			continue;
		__process_one_device(tr, NULL, devno);
	}

	closedir(dir);
done:
	ul_unref_path(pc);
	DBG(DEV, ul_debug("iterate on " _PATH_SYS_DEVBLOCK " -- done"));
	return 0;
}

/*
 * Reads root nodes (devices) from /sys/block into devices tree
 */
static int process_all_devices(struct lsblk_devtree *tr)
{
	DIR *dir;
	struct dirent *d;
	struct path_cxt *pc;

	assert(lsblk->inverse == 0);

	pc = ul_new_path(_PATH_SYS_BLOCK);
	if (!pc)
		err(EXIT_FAILURE, _("failed to allocate /sys handler"));

	ul_path_set_prefix(pc, lsblk->sysroot);
	dir = ul_path_opendir(pc, NULL);
	if (!dir)
		goto done;

	DBG(DEV, ul_debug("iterate on " _PATH_SYS_BLOCK));

	while ((d = xreaddir(dir))) {
		struct lsblk_device *dev = NULL;

		DBG(DEV, ul_debug(" %s dentry", d->d_name));
		dev = devtree_get_device_or_new(tr, NULL, d->d_name);
		if (!dev)
			goto next;

		/* remove unwanted devices */
		if (is_maj_excluded(dev->maj) || !is_maj_included(dev->maj)) {
			DBG(DEV, ul_debug(" %s: ignore (by filter)", d->d_name));
			lsblk_devtree_remove_device(tr, dev);
			dev = NULL;
			goto next;
		}

		if (dev->nslaves) {
			DBG(DEV, ul_debug(" %s: ignore (in-middle)", d->d_name));
			goto next;
		}

		lsblk_devtree_add_root(tr, dev);
		process_dependencies(tr, dev, 1);
next:
		/* Let's be careful with number of open files */
		if (dev && dev->sysfs)
			ul_path_close_dirfd(dev->sysfs);
	}

	closedir(dir);
done:
	ul_unref_path(pc);
	DBG(DEV, ul_debug("iterate on " _PATH_SYS_BLOCK " -- done"));
	return 0;
}

/*
 * Parses major numbers as specified on lsblk command line
 */
static void parse_excludes(const char *str0)
{
	const char *str = str0;

	while (str && *str) {
		char *end = NULL;
		unsigned long n;

		errno = 0;
		n = strtoul(str, &end, 10);

		if (end == str || (end && *end && *end != ','))
			errx(EXIT_FAILURE, _("failed to parse list '%s'"), str0);
		if (errno != 0 && (n == ULONG_MAX || n == 0))
			err(EXIT_FAILURE, _("failed to parse list '%s'"), str0);
		excludes[nexcludes++] = n;

		if (nexcludes == ARRAY_SIZE(excludes))
			/* TRANSLATORS: The standard value for %d is 256. */
			errx(EXIT_FAILURE, _("the list of excluded devices is "
					"too large (limit is %d devices)"),
					(int)ARRAY_SIZE(excludes));

		str = end && *end ? end + 1 : NULL;
	}
}

/*
 * Parses major numbers as specified on lsblk command line
 * (TODO: what about refactor and merge parse_excludes() and parse_includes().)
 */
static void parse_includes(const char *str0)
{
	const char *str = str0;

	while (str && *str) {
		char *end = NULL;
		unsigned long n;

		errno = 0;
		n = strtoul(str, &end, 10);

		if (end == str || (end && *end && *end != ','))
			errx(EXIT_FAILURE, _("failed to parse list '%s'"), str0);
		if (errno != 0 && (n == ULONG_MAX || n == 0))
			err(EXIT_FAILURE, _("failed to parse list '%s'"), str0);
		includes[nincludes++] = n;

		if (nincludes == ARRAY_SIZE(includes))
			/* TRANSLATORS: The standard value for %d is 256. */
			errx(EXIT_FAILURE, _("the list of included devices is "
					"too large (limit is %d devices)"),
					(int)ARRAY_SIZE(includes));
		str = end && *end ? end + 1 : NULL;
	}
}

/*
 * see set_rawdata_u64() and columns initialization in main()
 */
static int cmp_u64_cells(struct libscols_cell *a,
			 struct libscols_cell *b,
			 __attribute__((__unused__)) void *data)
{
	uint64_t *adata = (uint64_t *) scols_cell_get_userdata(a),
		 *bdata = (uint64_t *) scols_cell_get_userdata(b);

	if (adata == NULL && bdata == NULL)
		return 0;
	if (adata == NULL)
		return -1;
	if (bdata == NULL)
		return 1;
	return *adata == *bdata ? 0 : *adata >= *bdata ? 1 : -1;
}

static void *get_u64_cell(const struct libscols_column *cl __attribute__((__unused__)),
			  struct libscols_cell *ce,
			  void *data __attribute__((__unused__)))
{
	return scols_cell_get_userdata(ce);
}

static void device_set_dedupkey(
			struct lsblk_device *dev,
			struct lsblk_device *parent,
			int id)
{
	struct lsblk_iter itr;
	struct lsblk_device *child = NULL;

	dev->dedupkey = device_get_data(dev, parent, id, NULL, NULL);
	if (dev->dedupkey)
		DBG(DEV, ul_debugobj(dev, "%s: de-duplication key: %s", dev->name, dev->dedupkey));

	if (dev->npartitions == 0)
		/* For partitions we often read from parental whole-disk sysfs,
		 * otherwise we can close */
		ul_path_close_dirfd(dev->sysfs);

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (lsblk_device_next_child(dev, &itr, &child) == 0)
		device_set_dedupkey(child, dev, id);

	/* Let's be careful with number of open files */
	ul_path_close_dirfd(dev->sysfs);
}

static void devtree_set_dedupkeys(struct lsblk_devtree *tr, int id)
{
	struct lsblk_iter itr;
	struct lsblk_device *dev = NULL;

	lsblk_reset_iter(&itr, LSBLK_ITER_FORWARD);

	while (lsblk_devtree_next_root(tr, &itr, &dev) == 0)
		device_set_dedupkey(dev, NULL, id);
}

static struct libscols_filter *new_filter(const char *query)
{
	struct libscols_filter *f;

	f = scols_new_filter(NULL);
	if (!f)
		err(EXIT_FAILURE, _("failed to allocate filter"));
	if (query && scols_filter_parse_string(f, query) != 0)
		errx(EXIT_FAILURE, _("failed to parse \"%s\": %s"), query,
				scols_filter_get_errmsg(f));
	return f;
}

static struct libscols_filter *new_counter_filter(const char *query)
{
	lsblk->ct_filters = xreallocarray(lsblk->ct_filters, lsblk->ncts + 1,
					sizeof(struct libscols_filter *));

	lsblk->ct_filters[lsblk->ncts] = new_filter(query);
	lsblk->ncts++;

	return lsblk->ct_filters[lsblk->ncts - 1];
}

static void set_counter_properties(const char *str0)
{
	struct libscols_filter *fltr = NULL;
	struct libscols_counter *ct;
	char *p, *str = xstrdup(str0);
	char *name = NULL, *param = NULL, *func = NULL;

	for (p = strtok(str, ":"); p != NULL; p = strtok((char *)0, ":")) {
		if (!name)
			name = p;
		else if (!param)
			param = p;
		else if (!func)
			func = p;
		else
			errx(EXIT_FAILURE, _("unexpected counter specification: %s"), str0);
	}

	if (!name)
		errx(EXIT_FAILURE, _("counter not properly specified"));

	/* use the latest counter filter (--ct-filter) or create empty */
	if (lsblk->ncts)
		fltr = lsblk->ct_filters[lsblk->ncts - 1];
	else
		fltr = new_counter_filter(NULL);

	ct = scols_filter_new_counter(fltr);
	if (!ct)
		err(EXIT_FAILURE, _("failed to allocate counter"));

	scols_counter_set_name(ct, name);
	if (param)
		scols_counter_set_param(ct, param);
	if (func) {
		int x;

		if (strcmp(func, "max") == 0)
			x = SCOLS_COUNTER_MAX;
		else if (strcmp(func, "min") == 0)
			x = SCOLS_COUNTER_MIN;
		else if (strcmp(func, "sum") == 0)
			x = SCOLS_COUNTER_SUM;
		else if (strcmp(func, "count") == 0)
			x = SCOLS_COUNTER_COUNT;
		else
			errx(EXIT_FAILURE, _("unsupported counter type: %s"), func);

		scols_counter_set_func(ct, x);
	}
	free(str);
}

static void print_counters(void)
{
	struct libscols_iter *itr;
	size_t i;

	fputc('\n', stdout);
	fputs(_("Summary:\n"), stdout);

	itr = scols_new_iter(SCOLS_ITER_FORWARD);
	if (!itr)
		err(EXIT_FAILURE, _("failed to allocate iterator"));

	for (i = 0; i < lsblk->ncts; i++) {
		struct libscols_filter *fltr = lsblk->ct_filters[i];
		struct libscols_counter *ct = NULL;

		scols_reset_iter(itr, SCOLS_ITER_FORWARD);
		while (scols_filter_next_counter(fltr, itr, &ct) == 0) {
			printf("%16llu %s\n",
					scols_counter_get_result(ct),
					scols_counter_get_name(ct));
		}
	}

	scols_free_iter(itr);
}

static void set_column_type(const struct colinfo *ci, struct libscols_column *cl, int fl)
{
	switch (ci->type) {
	case COLTYPE_SIZE:
		/* See init_scols_filter(), it may overwrite the type */
		if (!lsblk->bytes)
			break;
		/* fallthrough */
	case COLTYPE_NUM:
		scols_column_set_json_type(cl, SCOLS_JSON_NUMBER);
		scols_column_set_data_type(cl, SCOLS_DATA_U64);
		return;
	case COLTYPE_BOOL:
		scols_column_set_json_type(cl, SCOLS_JSON_BOOLEAN);
		scols_column_set_data_type(cl, SCOLS_DATA_BOOLEAN);
		return;
	default:
		break;
	}

	/* default */
	if (fl & SCOLS_FL_WRAP)
		scols_column_set_json_type(cl, SCOLS_JSON_ARRAY_STRING);
	else
		scols_column_set_json_type(cl, SCOLS_JSON_STRING);

	scols_column_set_data_type(cl, SCOLS_DATA_STRING);
}

static void init_scols_filter(struct libscols_table *tb, struct libscols_filter *f)
{
	struct libscols_iter *itr;
	const char *name = NULL;
	int nerrs = 0;

	itr = scols_new_iter(SCOLS_ITER_FORWARD);
	if (!itr)
		err(EXIT_FAILURE, _("failed to allocate iterator"));

	while (scols_filter_next_holder(f, itr, &name, 0) == 0) {
		struct libscols_column *col = scols_table_get_column_by_name(tb, name);
		int id = column_name_to_id(name, strlen(name));
		const struct colinfo *ci = id >= 0 ? &infos[id] : NULL;

		if (!ci) {
			nerrs++;
			continue;	/* report all unknown columns */
		}
		if (!col) {
			add_column(id);
			col = scols_table_new_column(lsblk->table, ci->name,
						     ci->whint, SCOLS_FL_HIDDEN);
			if (!col)
				err(EXIT_FAILURE,_("failed to allocate output column"));

			set_column_type(ci, col, ci->flags);
		}

		/* For sizes use rawdata (u64) rather than strings from table */
		if (ci->type == COLTYPE_SIZE
		    && !lsblk->bytes
		    && !scols_column_has_data_func(col)) {

			scols_column_set_data_type(col, SCOLS_DATA_U64);
			scols_column_set_data_func(col, get_u64_cell, NULL);
			lsblk->rawdata = 1;
		}

		scols_filter_assign_column(f, itr, name, col);
	}

	scols_free_iter(itr);

	if (!nerrs)
		return;

	errx(EXIT_FAILURE, _("failed to initialize filter"));
}


static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;

	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] [<device> ...]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("List information about block devices.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -A, --noempty        don't print empty devices\n"), out);
	fputs(_(" -D, --discard        print discard capabilities\n"), out);
	fputs(_(" -E, --dedup <column> de-duplicate output by <column>\n"), out);
	fputs(_(" -I, --include <list> show only devices with specified major numbers\n"), out);
	fputs(_(" -J, --json           use JSON output format\n"), out);
	fputs(_(" -M, --merge          group parents of sub-trees (usable for RAIDs, Multi-path)\n"), out);
	fputs(_(" -O, --output-all     output all columns\n"), out);
	fputs(_(" -P, --pairs          use key=\"value\" output format\n"), out);
	fputs(_(" -Q, --filter <expr>  print only lines maching the expression\n"), out);
	fputs(_("     --highlight <expr> colorize lines maching the expression\n"), out);
	fputs(_("     --ct-filter <expr> restrict the next counter\n"), out);
	fputs(_("     --ct <name>[:<param>[:<func>]] define a custom counter\n"), out);
	fputs(_(" -S, --scsi           output info about SCSI devices\n"), out);
	fputs(_(" -N, --nvme           output info about NVMe devices\n"), out);
	fputs(_(" -v, --virtio         output info about virtio devices\n"), out);
	fputs(_(" -T, --tree[=<column>] use tree format output\n"), out);
	fputs(_(" -a, --all            print all devices\n"), out);
	fputs(_(" -b, --bytes          print SIZE in bytes rather than in human readable format\n"), out);
	fputs(_(" -d, --nodeps         don't print slaves or holders\n"), out);
	fputs(_(" -e, --exclude <list> exclude devices by major number (default: RAM disks)\n"), out);
	fputs(_(" -f, --fs             output info about filesystems\n"), out);
	fputs(_(" -i, --ascii          use ascii characters only\n"), out);
	fputs(_(" -l, --list           use list format output\n"), out);
	fputs(_(" -m, --perms          output info about permissions\n"), out);
	fputs(_(" -n, --noheadings     don't print headings\n"), out);
	fputs(_(" -o, --output <list>  output columns (see --list-columns)\n"), out);
	fputs(_(" -p, --paths          print complete device path\n"), out);
	fputs(_(" -r, --raw            use raw output format\n"), out);
	fputs(_(" -s, --inverse        inverse dependencies\n"), out);
	fputs(_(" -t, --topology       output info about topology\n"), out);
	fputs(_(" -w, --width <num>    specifies output width as number of characters\n"), out);
	fputs(_(" -x, --sort <column>  sort output by <column>\n"), out);
	fputs(_(" -y, --shell          use column names to be usable as shell variable identifiers\n"), out);
	fputs(_(" -z, --zoned          print zone related information\n"), out);
	fputs(_("     --sysroot <dir>  use specified directory as system root\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(_(" -H, --list-columns   list the available columns\n"), out);
	fprintf(out, USAGE_HELP_OPTIONS(22));

	fprintf(out, USAGE_MAN_TAIL("lsblk(8)"));

	exit(EXIT_SUCCESS);
}


static void __attribute__((__noreturn__)) list_colunms(void)
{
	size_t i;
	struct libscols_table *tb = xcolumn_list_table_new("lsblk-columns", stdout,
						lsblk->flags & LSBLK_RAW,
						lsblk->flags & LSBLK_JSON);

	for (i = 0; i < ARRAY_SIZE(infos); i++) {
		const struct colinfo *ci = &infos[i];

		xcolumn_list_table_append_line(tb, ci->name,
			ci->type == COLTYPE_SIZE ? -1 :
			ci->type == COLTYPE_NUM  ? SCOLS_JSON_NUMBER :
			ci->type == COLTYPE_BOOL ? SCOLS_JSON_BOOLEAN :
			ci->flags & SCOLS_FL_WRAP ? SCOLS_JSON_ARRAY_STRING :
				SCOLS_JSON_STRING,
			ci->type == COLTYPE_SIZE ? "<string|number>" : NULL,
		       _(ci->help));
	}


	scols_print_table(tb);
	scols_unref_table(tb);

	exit(EXIT_SUCCESS);
}

static void check_sysdevblock(void)
{
	if (access(_PATH_SYS_DEVBLOCK, R_OK) != 0)
		err(EXIT_FAILURE, _("failed to access sysfs directory: %s"),
		    _PATH_SYS_DEVBLOCK);
}

int main(int argc, char *argv[])
{
	struct lsblk _ls = {
		.sort_id = -1,
		.dedup_id = -1,
		.flags = LSBLK_TREE,
		.tree_id = COL_NAME
	};
	struct lsblk_devtree *tr = NULL;
	int c, status = EXIT_FAILURE, collist = 0;
	char *outarg = NULL;
	size_t i;
	unsigned int width = 0;
	int force_tree = 0, has_tree_col = 0;

	enum {
		OPT_SYSROOT = CHAR_MAX + 1,
		OPT_COUNTER_FILTER,
		OPT_COUNTER,
		OPT_HIGHLIGHT,
	};

	static const struct option longopts[] = {
		{ "all",	no_argument,       NULL, 'a' },
		{ "bytes",      no_argument,       NULL, 'b' },
		{ "nodeps",     no_argument,       NULL, 'd' },
		{ "noempty",    no_argument,       NULL, 'A' },
		{ "discard",    no_argument,       NULL, 'D' },
		{ "dedup",      required_argument, NULL, 'E' },
		{ "zoned",      no_argument,       NULL, 'z' },
		{ "help",	no_argument,       NULL, 'h' },
		{ "json",       no_argument,       NULL, 'J' },
		{ "output",     required_argument, NULL, 'o' },
		{ "output-all", no_argument,       NULL, 'O' },
		{ "filter",     required_argument, NULL, 'Q' },
		{ "highlight",  required_argument, NULL, OPT_HIGHLIGHT },
		{ "merge",      no_argument,       NULL, 'M' },
		{ "perms",      no_argument,       NULL, 'm' },
		{ "noheadings",	no_argument,       NULL, 'n' },
		{ "list",       no_argument,       NULL, 'l' },
		{ "ascii",	no_argument,       NULL, 'i' },
		{ "raw",        no_argument,       NULL, 'r' },
		{ "inverse",	no_argument,       NULL, 's' },
		{ "fs",         no_argument,       NULL, 'f' },
		{ "exclude",    required_argument, NULL, 'e' },
		{ "include",    required_argument, NULL, 'I' },
		{ "topology",   no_argument,       NULL, 't' },
		{ "paths",      no_argument,       NULL, 'p' },
		{ "pairs",      no_argument,       NULL, 'P' },
		{ "scsi",       no_argument,       NULL, 'S' },
		{ "nvme",       no_argument,       NULL, 'N' },
		{ "virtio",     no_argument,       NULL, 'v' },
		{ "sort",	required_argument, NULL, 'x' },
		{ "sysroot",    required_argument, NULL, OPT_SYSROOT },
		{ "shell",      no_argument,       NULL, 'y' },
		{ "tree",       optional_argument, NULL, 'T' },
		{ "version",    no_argument,       NULL, 'V' },
		{ "width",	required_argument, NULL, 'w' },
		{ "ct-filter",  required_argument, NULL, OPT_COUNTER_FILTER },
		{ "ct",         required_argument, NULL, OPT_COUNTER },
		{ "list-columns", no_argument,     NULL, 'H' },
		{ NULL, 0, NULL, 0 },
	};

	static const ul_excl_t excl[] = {       /* rows and cols in ASCII order */
		{ 'D','O' },
		{ 'I','e' },
		{ 'J', 'P', 'r' },
		{ 'O','S' },
		{ 'O','f' },
		{ 'O','m' },
		{ 'O','o' },
		{ 'O','t' },
		{ 'P','T', 'l','r' },
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	lsblk = &_ls;

	lsblk_init_debug();
	scols_init_debug(0);

	while((c = getopt_long(argc, argv,
				"AabdDzE:e:fHhJlNnMmo:OpPQ:iI:rstVvST::w:x:y",
				longopts, NULL)) != -1) {

		err_exclusive_options(c, longopts, excl, excl_st);

		switch(c) {
		case 'A':
			lsblk->noempty = 1;
			break;
		case 'a':
			lsblk->all_devices = 1;
			break;
		case 'b':
			lsblk->bytes = 1;
			break;
		case 'd':
			lsblk->nodeps = 1;
			break;
		case 'D':
			add_uniq_column(COL_NAME);
			add_uniq_column(COL_DALIGN);
			add_uniq_column(COL_DGRAN);
			add_uniq_column(COL_DMAX);
			add_uniq_column(COL_DZERO);
			break;
		case 'z':
			add_uniq_column(COL_NAME);
			add_uniq_column(COL_ZONED);
			add_uniq_column(COL_ZONE_SZ);
			add_uniq_column(COL_ZONE_NR);
			add_uniq_column(COL_ZONE_AMAX);
			add_uniq_column(COL_ZONE_OMAX);
			add_uniq_column(COL_ZONE_APP);
			add_uniq_column(COL_ZONE_WGRAN);
			break;
		case 'e':
			parse_excludes(optarg);
			break;
		case 'J':
			lsblk->flags |= LSBLK_JSON;
			break;
		case 'l':
			lsblk->flags &= ~LSBLK_TREE; /* disable the default */
			break;
		case 'M':
			lsblk->merge = 1;
			break;
		case 'n':
			lsblk->flags |= LSBLK_NOHEADINGS;
			break;
		case 'o':
			outarg = optarg;
			break;
		case 'O':
			for (ncolumns = 0 ; ncolumns < ARRAY_SIZE(infos); ncolumns++)
				columns[ncolumns] = ncolumns;
			break;
		case 'p':
			lsblk->paths = 1;
			break;
		case 'P':
			lsblk->flags |= LSBLK_EXPORT;
			lsblk->flags &= ~LSBLK_TREE;	/* disable the default */
			break;
		case 'Q':
			lsblk->filter = new_filter(optarg);
			lsblk->flags &= ~LSBLK_TREE; /* disable the default */
			break;
		case 'y':
			lsblk->flags |= LSBLK_SHELLVAR;
			break;
		case 'i':
			lsblk->flags |= LSBLK_ASCII;
			break;
		case 'I':
			parse_includes(optarg);
			break;
		case 'r':
			lsblk->flags &= ~LSBLK_TREE;	/* disable the default */
			lsblk->flags |= LSBLK_RAW;		/* enable raw */
			break;
		case 's':
			lsblk->inverse = 1;
			break;
		case 'f':
			add_uniq_column(COL_NAME);
			add_uniq_column(COL_FSTYPE);
			add_uniq_column(COL_FSVERSION);
			add_uniq_column(COL_LABEL);
			add_uniq_column(COL_UUID);
			add_uniq_column(COL_FSAVAIL);
			add_uniq_column(COL_FSUSEPERC);
			add_uniq_column(COL_TARGETS);
			break;
		case 'm':
			add_uniq_column(COL_NAME);
			add_uniq_column(COL_SIZE);
			add_uniq_column(COL_OWNER);
			add_uniq_column(COL_GROUP);
			add_uniq_column(COL_MODE);
			break;
		case 't':
			add_uniq_column(COL_NAME);
			add_uniq_column(COL_ALIOFF);
			add_uniq_column(COL_MINIO);
			add_uniq_column(COL_OPTIO);
			add_uniq_column(COL_PHYSEC);
			add_uniq_column(COL_LOGSEC);
			add_uniq_column(COL_ROTA);
			add_uniq_column(COL_SCHED);
			add_uniq_column(COL_RQ_SIZE);
			add_uniq_column(COL_RA);
			add_uniq_column(COL_WSAME);
			break;
		case 'S':
			lsblk->nodeps = 1;
			lsblk->scsi = 1;
			add_uniq_column(COL_NAME);
			add_uniq_column(COL_HCTL);
			add_uniq_column(COL_TYPE);
			add_uniq_column(COL_VENDOR);
			add_uniq_column(COL_MODEL);
			add_uniq_column(COL_REV);
			add_uniq_column(COL_SERIAL);
			add_uniq_column(COL_TRANSPORT);
			break;
		case 'N':
			lsblk->nodeps = 1;
			lsblk->nvme = 1;
			add_uniq_column(COL_NAME);
			add_uniq_column(COL_TYPE);
			add_uniq_column(COL_MODEL);
			add_uniq_column(COL_SERIAL);
			add_uniq_column(COL_REV);
			add_uniq_column(COL_TRANSPORT);
			add_uniq_column(COL_RQ_SIZE);
			add_uniq_column(COL_MQ);
			break;
		case 'v':
			lsblk->nodeps = 1;
			lsblk->virtio = 1;
			add_uniq_column(COL_NAME);
			add_uniq_column(COL_TYPE);
			add_uniq_column(COL_TRANSPORT);
			add_uniq_column(COL_SIZE);
			add_uniq_column(COL_RQ_SIZE);
			add_uniq_column(COL_MQ);
			break;
		case 'T':
			force_tree = 1;
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				lsblk->tree_id = column_name_to_id(optarg, strlen(optarg));
			}
			break;
		case OPT_SYSROOT:
			lsblk->sysroot = optarg;
			break;
		case 'E':
			lsblk->dedup_id = column_name_to_id(optarg, strlen(optarg));
			if (lsblk->dedup_id >= 0)
				break;
			errtryhelp(EXIT_FAILURE);
			break;
		case 'w':
			width = strtou32_or_err(optarg, _("invalid output width number argument"));
			break;
		case 'x':
			lsblk->flags &= ~LSBLK_TREE; /* disable the default */
			lsblk->sort_id = column_name_to_id(optarg, strlen(optarg));
			if (lsblk->sort_id >= 0)
				break;
			errtryhelp(EXIT_FAILURE);
			break;

		case OPT_COUNTER_FILTER:
			new_counter_filter(optarg);
			break;
		case OPT_COUNTER:
			set_counter_properties(optarg);
			break;
		case OPT_HIGHLIGHT:
			lsblk->hlighter = new_filter(optarg);
			break;

		case 'H':
			collist = 1;
			break;
		case 'h':
			usage();
		case 'V':
			print_version(EXIT_SUCCESS);
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (collist)
		list_colunms();        /* print end exit */

	if (force_tree)
		lsblk->flags |= LSBLK_TREE;

	check_sysdevblock();

	if (!ncolumns) {
		add_column(COL_NAME);
		add_column(COL_MAJMIN);
		add_column(COL_RM);
		add_column(COL_SIZE);
		add_column(COL_RO);
		add_column(COL_TYPE);
		add_column(COL_TARGETS);
	}

	if (outarg && string_add_to_idarray(outarg, columns, ARRAY_SIZE(columns),
					 &ncolumns, column_name_to_id) < 0)
		return EXIT_FAILURE;

	if (lsblk->all_devices == 0 && nexcludes == 0 && nincludes == 0)
		excludes[nexcludes++] = 1;	/* default: ignore RAM disks */

	if (lsblk->sort_id < 0)
		/* Since Linux 4.8 we have sort devices by default, because
		 * /sys is no more sorted */
		lsblk->sort_id = COL_MAJMIN;

	/* For --{inverse,raw,pairs} --list we still follow parent->child relation */
	if (!(lsblk->flags & LSBLK_TREE)
	    && (lsblk->inverse || lsblk->flags & LSBLK_EXPORT || lsblk->flags & LSBLK_RAW))
		lsblk->force_tree_order = 1;

	if (lsblk->sort_id >= 0 && column_id_to_number(lsblk->sort_id) < 0) {
		/* the sort column is not between output columns -- add as hidden */
		add_column(lsblk->sort_id);
		lsblk->sort_hidden = 1;
	}

	if (lsblk->dedup_id >= 0 && column_id_to_number(lsblk->dedup_id) < 0) {
		/* the deduplication column is not between output columns -- add as hidden */
		add_column(lsblk->dedup_id);
		lsblk->dedup_hidden = 1;
	}

	lsblk_mnt_init();
	ul_path_init_debug();

	/*
	 * initialize output columns
	 */
	if (!(lsblk->table = scols_new_table()))
		errx(EXIT_FAILURE, _("failed to allocate output table"));
	scols_table_enable_raw(lsblk->table, !!(lsblk->flags & LSBLK_RAW));
	scols_table_enable_export(lsblk->table, !!(lsblk->flags & LSBLK_EXPORT));
	scols_table_enable_shellvar(lsblk->table, !!(lsblk->flags & LSBLK_SHELLVAR));
	scols_table_enable_ascii(lsblk->table, !!(lsblk->flags & LSBLK_ASCII));
	scols_table_enable_json(lsblk->table, !!(lsblk->flags & LSBLK_JSON));
	scols_table_enable_noheadings(lsblk->table, !!(lsblk->flags & LSBLK_NOHEADINGS));

	if (lsblk->flags & LSBLK_JSON)
		scols_table_set_name(lsblk->table, "blockdevices");
	if (width) {
		scols_table_set_termwidth(lsblk->table, width);
		scols_table_set_termforce(lsblk->table, SCOLS_TERMFORCE_ALWAYS);
	}

	for (i = 0; i < ncolumns; i++) {
		const struct colinfo *ci = get_column_info(i);
		struct libscols_column *cl;
		int id = get_column_id(i), fl = ci->flags;

		if ((lsblk->flags & LSBLK_TREE)
		    && has_tree_col == 0
		    && id == lsblk->tree_id) {
			fl |= SCOLS_FL_TREE;
			fl &= ~SCOLS_FL_RIGHT;
			has_tree_col = 1;
		}

		if (lsblk->sort_hidden && lsblk->sort_id == id)
			fl |= SCOLS_FL_HIDDEN;
		if (lsblk->dedup_hidden && lsblk->dedup_id == id)
			fl |= SCOLS_FL_HIDDEN;

		if (force_tree
		    && lsblk->flags & LSBLK_JSON
		    && has_tree_col == 0
		    && i + 1 == ncolumns)
			/* The "--tree --json" specified, but no column with
			 * SCOLS_FL_TREE yet; force it for the last column
			 */
			fl |= SCOLS_FL_TREE;

		cl = scols_table_new_column(lsblk->table, ci->name, ci->whint, fl);
		if (!cl) {
			warn(_("failed to allocate output column"));
			goto leave;
		}
		if (!lsblk->sort_col && lsblk->sort_id == id) {
			lsblk->sort_col = cl;
			lsblk->rawdata = 1;
			scols_column_set_cmpfunc(cl,
				ci->type == COLTYPE_NUM     ? cmp_u64_cells :
				ci->type == COLTYPE_SIZE    ? cmp_u64_cells :
			        ci->type == COLTYPE_SORTNUM ? cmp_u64_cells : scols_cmpstr_cells,
				NULL);
		}
		/* multi-line cells (now used for MOUNTPOINTS) */
		if (fl & SCOLS_FL_WRAP)
			scols_column_set_wrapfunc(cl, NULL, scols_wrapzero_nextchunk, NULL);

		set_column_type(ci, cl, fl);
	}

	if (lsblk->filter)
		init_scols_filter(lsblk->table, lsblk->filter);

	if (lsblk->hlighter && colors_init(UL_COLORMODE_AUTO, "lsblk") > 0) {
		lsblk->hlighter_seq = color_scheme_get_sequence("highlight-line", UL_COLOR_RED);
		scols_table_enable_colors(lsblk->table, 1);
		init_scols_filter(lsblk->table, lsblk->hlighter);
	}
	for (i = 0; i < lsblk->ncts; i++)
		init_scols_filter(lsblk->table, lsblk->ct_filters[i]);

	tr = lsblk_new_devtree();
	if (!tr)
		err(EXIT_FAILURE, _("failed to allocate device tree"));

	if (optind == argc) {
		int rc = lsblk->inverse ?
			process_all_devices_inverse(tr) :
			process_all_devices(tr);

		status = rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
	} else {
		int cnt = 0, cnt_err = 0;

		while (optind < argc) {
			if (process_one_device(tr, argv[optind++]) != 0)
				cnt_err++;
			cnt++;
		}
		status = cnt == 0	? EXIT_FAILURE :	/* nothing */
			 cnt == cnt_err	? LSBLK_EXIT_ALLFAILED :/* all failed */
			 cnt_err	? LSBLK_EXIT_SOMEOK :	/* some ok */
					  EXIT_SUCCESS;		/* all success */
	}

	if (lsblk->dedup_id > -1) {
		devtree_set_dedupkeys(tr, lsblk->dedup_id);
		lsblk_devtree_deduplicate_devices(tr);
	}

	devtree_to_scols(tr, lsblk->table);

	if (lsblk->sort_col)
		scols_sort_table(lsblk->table, lsblk->sort_col);
	if (lsblk->force_tree_order)
		scols_sort_table_by_tree(lsblk->table);

	scols_print_table(lsblk->table);

	if (lsblk->ncts)
		print_counters();
leave:
	if (lsblk->rawdata)
		unref_table_rawdata(lsblk->table);

	scols_unref_table(lsblk->table);
	scols_unref_filter(lsblk->filter);
	scols_unref_filter(lsblk->hlighter);

	for (i = 0; i < lsblk->ncts; i++)
		scols_unref_filter(lsblk->ct_filters[i]);
	free(lsblk->ct_filters);

	lsblk_mnt_deinit();
	lsblk_properties_deinit();
	lsblk_unref_devtree(tr);

	return status;
}
