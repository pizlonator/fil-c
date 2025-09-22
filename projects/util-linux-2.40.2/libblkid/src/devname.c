/*
 * devname.c - get a dev by its device inode name
 *
 * Copyright (C) Andries Brouwer
 * Copyright (C) 1999, 2000, 2001, 2002, 2003 Theodore Ts'o
 * Copyright (C) 2001 Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <string.h>
#include <limits.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <dirent.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <time.h>

#include "blkidP.h"

#include "canonicalize.h"		/* $(top_srcdir)/include */
#include "pathnames.h"
#include "sysfs.h"
#include "fileutils.h"

/*
 * Find a dev struct in the cache by device name, if available.
 *
 * If there is no entry with the specified device name, and the create
 * flag is set, then create an empty device entry.
 */
blkid_dev blkid_get_dev(blkid_cache cache, const char *devname, int flags)
{
	blkid_dev dev = NULL, tmp;
	struct list_head *p, *pnext;
	char *cn = NULL;

	if (!cache || !devname)
		return NULL;

	/* search by name */
	list_for_each(p, &cache->bic_devs) {
		tmp = list_entry(p, struct blkid_struct_dev, bid_devs);
		if (strcmp(tmp->bid_name, devname) != 0)
			continue;
		dev = tmp;
		break;
	}

	/* try canonicalize the name */
	if (!dev && (cn = canonicalize_path(devname))) {
		if (strcmp(cn, devname) != 0) {
			DBG(DEVNAME, ul_debug("search canonical %s", cn));
			list_for_each(p, &cache->bic_devs) {
				tmp = list_entry(p, struct blkid_struct_dev, bid_devs);
				if (strcmp(tmp->bid_name, cn) != 0)
					continue;
				dev = tmp;

				/* update name returned by blkid_dev_devname() */
				free(dev->bid_xname);
				dev->bid_xname = strdup(devname);
				break;
			}
		} else {
			free(cn);
			cn = NULL;
		}
	}

	if (!dev && (flags & BLKID_DEV_CREATE)) {
		if (access(devname, F_OK) < 0)
			goto done;
		dev = blkid_new_dev();
		if (!dev)
			goto done;
		dev->bid_time = (uintmax_t)1 << (sizeof(time_t) * 8 - 1);
		if (cn) {
			dev->bid_name = cn;
			dev->bid_xname = strdup(devname);
			cn = NULL;	/* see free() below */
		} else
			dev->bid_name = strdup(devname);

		dev->bid_cache = cache;
		list_add_tail(&dev->bid_devs, &cache->bic_devs);
		cache->bic_flags |= BLKID_BIC_FL_CHANGED;
	}

	if (flags & BLKID_DEV_VERIFY) {
		dev = blkid_verify(cache, dev);
		if (!dev || !(dev->bid_flags & BLKID_BID_FL_VERIFIED))
			goto done;
		/*
		 * If the device is verified, then search the blkid
		 * cache for any entries that match on the type, uuid,
		 * and label, and verify them; if a cache entry can
		 * not be verified, then it's stale and so we remove
		 * it.
		 */
		list_for_each_safe(p, pnext, &cache->bic_devs) {
			blkid_dev dev2 = list_entry(p, struct blkid_struct_dev, bid_devs);
			if (dev2->bid_flags & BLKID_BID_FL_VERIFIED)
				continue;
			if (!dev->bid_type || !dev2->bid_type ||
			    strcmp(dev->bid_type, dev2->bid_type) != 0)
				continue;
			if (dev->bid_label && dev2->bid_label &&
			    strcmp(dev->bid_label, dev2->bid_label) != 0)
				continue;
			if (dev->bid_uuid && dev2->bid_uuid &&
			    strcmp(dev->bid_uuid, dev2->bid_uuid) != 0)
				continue;
			if ((dev->bid_label && !dev2->bid_label) ||
			    (!dev->bid_label && dev2->bid_label) ||
			    (dev->bid_uuid && !dev2->bid_uuid) ||
			    (!dev->bid_uuid && dev2->bid_uuid))
				continue;
			dev2 = blkid_verify(cache, dev2);
			if (dev2 && !(dev2->bid_flags & BLKID_BID_FL_VERIFIED))
				blkid_free_dev(dev2);
		}
	}
done:
	if (dev)
		DBG(DEVNAME, ul_debug("%s requested, found %s in cache", devname, dev->bid_name));
	free(cn);
	return dev;
}

/* Directories where we will try to search for device names */
static const char *dirlist[] = { "/dev", "/devfs", "/devices", NULL };

/*
 * Return 1 if the device is a device-mapper 'leaf' node
 * not holding any other devices in its hierarchy.
 */
static int is_dm_leaf(const char *devname)
{
	DIR *dir;
	char path[NAME_MAX + 18 + 1];
	int ret;

	snprintf(path, sizeof(path), "/sys/block/%s/holders", devname);
	if ((dir = opendir(path)) == NULL)
		return 0;

	ret = xreaddir(dir) == NULL ? 1 : 0;	/* 'leaf' has no entries */

	closedir(dir);
	return ret;
}

/*
 * Probe a single block device to add to the device cache.
 */
static void probe_one(blkid_cache cache, const char *ptname,
		      dev_t devno, int pri, int only_if_new, int removable)
{
	blkid_dev dev = NULL;
	struct list_head *p, *pnext;
	const char **dir;
	char *devname = NULL;

	/* See if we already have this device number in the cache. */
	list_for_each_safe(p, pnext, &cache->bic_devs) {
		blkid_dev tmp = list_entry(p, struct blkid_struct_dev,
					   bid_devs);
		if (tmp->bid_devno == devno) {
			if (only_if_new && !access(tmp->bid_name, F_OK))
				return;
			dev = blkid_verify(cache, tmp);
			if (dev && (dev->bid_flags & BLKID_BID_FL_VERIFIED))
				break;
			dev = NULL;
		}
	}
	if (dev && dev->bid_devno == devno)
		goto set_pri;

	/* Try to translate private device-mapper dm-<N> names
	 * to standard /dev/mapper/<name>.
	 */
	if (!strncmp(ptname, "dm-", 3) && isdigit(ptname[3])) {
		devname = canonicalize_dm_name(ptname);
		if (!devname)
			blkid__scan_dir("/dev/mapper", devno, NULL, &devname);
		if (devname)
			goto get_dev;
	}

	/*
	 * Take a quick look at /dev/ptname for the device number.  We check
	 * all of the likely device directories.  If we don't find it, or if
	 * the stat information doesn't check out, use blkid_devno_to_devname()
	 * to find it via an exhaustive search for the device major/minor.
	 */
	for (dir = dirlist; *dir; dir++) {
		struct stat st;
		char device[256];

		snprintf(device, sizeof(device), "%s/%s", *dir, ptname);
		if ((dev = blkid_get_dev(cache, device, BLKID_DEV_FIND)) &&
		    dev->bid_devno == devno)
			goto set_pri;

		if (stat(device, &st) == 0 &&
		    (S_ISBLK(st.st_mode) ||
		     (S_ISCHR(st.st_mode) && !strncmp(ptname, "ubi", 3))) &&
		    st.st_rdev == devno) {
			devname = strdup(device);
			goto get_dev;
		}
	}
	/* Do a short-cut scan of /dev/mapper first */
	if (!devname)
		blkid__scan_dir("/dev/mapper", devno, NULL, &devname);
	if (!devname) {
		devname = blkid_devno_to_devname(devno);
		if (!devname)
			return;
	}

get_dev:
	dev = blkid_get_dev(cache, devname, BLKID_DEV_NORMAL);
	free(devname);

set_pri:
	if (dev) {
		if (pri)
			dev->bid_pri = pri;
		else if (!strncmp(dev->bid_name, "/dev/mapper/", 12)) {
			dev->bid_pri = BLKID_PRI_DM;
			if (is_dm_leaf(ptname))
				dev->bid_pri += 5;
		} else if (!strncmp(ptname, "md", 2))
			dev->bid_pri = BLKID_PRI_MD;
		if (removable)
			dev->bid_flags |= BLKID_BID_FL_REMOVABLE;
	}
}

#define PROC_PARTITIONS "/proc/partitions"
#define VG_DIR		"/proc/lvm/VGs"

/*
 * This function initializes the UUID cache with devices from the LVM
 * proc hierarchy.  We currently depend on the names of the LVM
 * hierarchy giving us the device structure in /dev.  (XXX is this a
 * safe thing to do?)
 */
#ifdef VG_DIR
static dev_t lvm_get_devno(const char *lvm_device)
{
	FILE *lvf;
	char buf[1024];
	int ma, mi;
	dev_t ret = 0;

	DBG(DEVNAME, ul_debug("opening %s", lvm_device));
	if ((lvf = fopen(lvm_device, "r" UL_CLOEXECSTR)) == NULL) {
		DBG(DEVNAME, ul_debug("%s: (%d) %m", lvm_device, errno));
		return 0;
	}

	while (fgets(buf, sizeof(buf), lvf)) {
		if (sscanf(buf, "device: %d:%d", &ma, &mi) == 2) {
			ret = makedev(ma, mi);
			break;
		}
	}
	fclose(lvf);

	return ret;
}

static void lvm_probe_all(blkid_cache cache, int only_if_new)
{
	DIR		*vg_list;
	struct dirent	*vg_iter;
	int		vg_len = strlen(VG_DIR);
	dev_t		dev;

	if ((vg_list = opendir(VG_DIR)) == NULL)
		return;

	DBG(DEVNAME, ul_debug("probing LVM devices under %s", VG_DIR));

	while ((vg_iter = readdir(vg_list)) != NULL) {
		DIR		*lv_list;
		char		*vdirname;
		char		*vg_name;
		struct dirent	*lv_iter;
		size_t		len;

		vg_name = vg_iter->d_name;
		if (!strcmp(vg_name, ".") || !strcmp(vg_name, ".."))
			continue;
		len = vg_len + strlen(vg_name) + 8;
		vdirname = malloc(len);
		if (!vdirname)
			goto exit;
		snprintf(vdirname, len, "%s/%s/LVs", VG_DIR, vg_name);

		lv_list = opendir(vdirname);
		free(vdirname);
		if (lv_list == NULL)
			continue;

		while ((lv_iter = readdir(lv_list)) != NULL) {
			char		*lv_name, *lvm_device;

			lv_name = lv_iter->d_name;
			if (!strcmp(lv_name, ".") || !strcmp(lv_name, ".."))
				continue;

			len = vg_len + strlen(vg_name) + strlen(lv_name) + 8;
			lvm_device = malloc(len);
			if (!lvm_device) {
				closedir(lv_list);
				goto exit;
			}
			snprintf(lvm_device, len, "%s/%s/LVs/%s", VG_DIR, vg_name,
				lv_name);
			dev = lvm_get_devno(lvm_device);
			snprintf(lvm_device, len, "%s/%s", vg_name, lv_name);
			DBG(DEVNAME, ul_debug("Probe LVM dev %s: devno 0x%04X",
						  lvm_device,
						  (unsigned int) dev));
			probe_one(cache, lvm_device, dev, BLKID_PRI_LVM,
				  only_if_new, 0);
			free(lvm_device);
		}
		closedir(lv_list);
	}
exit:
	closedir(vg_list);
}
#endif

static void
ubi_probe_all(blkid_cache cache, int only_if_new)
{
	const char **dirname;

	for (dirname = dirlist; *dirname; dirname++) {
		DIR		*dir;
		struct dirent	*iter;

		DBG(DEVNAME, ul_debug("probing UBI volumes under %s",
					  *dirname));

		dir = opendir(*dirname);
		if (dir == NULL)
			continue ;

		while ((iter = readdir(dir)) != NULL) {
			char		*name;
			struct stat	st;
			dev_t		dev;

			name = iter->d_name;
#ifdef _DIRENT_HAVE_D_TYPE
			if (iter->d_type != DT_UNKNOWN &&
			    iter->d_type != DT_CHR && iter->d_type != DT_LNK)
				continue;
#endif
			if (!strcmp(name, ".") || !strcmp(name, "..") ||
			    !strstr(name, "ubi"))
				continue;
			if (!strcmp(name, "ubi_ctrl"))
				continue;
			if (fstatat(dirfd(dir), name, &st, 0))
				continue;

			dev = st.st_rdev;

			if (!S_ISCHR(st.st_mode) || !minor(dev))
				continue;
			DBG(DEVNAME, ul_debug("Probe UBI vol %s/%s: devno 0x%04X",
				  *dirname, name, (int) dev));
			probe_one(cache, name, dev, BLKID_PRI_UBI, only_if_new, 0);
		}
		closedir(dir);
	}
}

/*
 * This function uses /sys to read all block devices in way compatible with
 * /proc/partitions (like the original libblkid implementation)
 */
static int
sysfs_probe_all(blkid_cache cache, int only_if_new, int only_removable)
{
	DIR *sysfs;
	struct dirent *dev;

	sysfs = opendir(_PATH_SYS_BLOCK);
	if (!sysfs)
		return -BLKID_ERR_SYSFS;

	DBG(DEVNAME, ul_debug(" probe /sys/block"));

	/* scan /sys/block */
	while ((dev = xreaddir(sysfs))) {
		DIR *dir = NULL;
		dev_t devno;
		size_t nparts = 0;
		unsigned int maxparts = 0, removable = 0;
		struct dirent *part;
		struct path_cxt *pc = NULL;
		uint64_t size = 0;

		DBG(DEVNAME, ul_debug("checking %s", dev->d_name));

		devno = sysfs_devname_to_devno(dev->d_name);
		if (!devno)
			goto next;
		pc = ul_new_sysfs_path(devno, NULL, NULL);
		if (!pc)
			goto next;

		if (ul_path_read_u64(pc, &size, "size") != 0)
			size = 0;
		if (ul_path_read_u32(pc, &removable, "removable") != 0)
			removable = 0;

		/* ignore empty devices */
		if (!size)
			goto next;

		/* accept removable if only removable requested */
		if (only_removable) {
			if (!removable)
				goto next;

		/* emulate /proc/partitions
		 * -- ignore empty devices and non-partitionable removable devices */
		} else {
			if (ul_path_read_u32(pc, &maxparts, "ext_range") != 0)
				maxparts = 0;
			if (!maxparts && removable)
				goto next;
		}

		DBG(DEVNAME, ul_debug("read device name %s", dev->d_name));

		dir = ul_path_opendir(pc, NULL);
		if (!dir)
			goto next;

		/* read /sys/block/<name>/ do get partitions */
		while ((part = xreaddir(dir))) {
			dev_t partno;

			if (!sysfs_blkdev_is_partition_dirent(dir, part, dev->d_name))
				continue;

			/* ignore extended partitions
			 * -- recount size to blocks like /proc/partitions */
			if (ul_path_readf_u64(pc, &size, "%s/size", part->d_name) == 0
			    && (size >> 1) == 1)
				continue;
			partno = __sysfs_devname_to_devno(NULL, part->d_name, dev->d_name);
			if (!partno)
				continue;

			DBG(DEVNAME, ul_debug(" Probe partition dev %s, devno 0x%04X",
                                   part->d_name, (unsigned int) partno));
			nparts++;
			probe_one(cache, part->d_name, partno, 0, only_if_new, 0);
		}

		if (!nparts) {
			/* add non-partitioned whole disk to cache */
			DBG(DEVNAME, ul_debug(" Probe whole dev %s, devno 0x%04X",
				   dev->d_name, (unsigned int) devno));
			probe_one(cache, dev->d_name, devno, 0, only_if_new, 0);
		} else {
			/* remove partitioned whole-disk from cache */
			struct list_head *p, *pnext;

			list_for_each_safe(p, pnext, &cache->bic_devs) {
				blkid_dev tmp = list_entry(p, struct blkid_struct_dev,
							bid_devs);
				if (tmp->bid_devno == devno) {
					DBG(DEVNAME, ul_debug(" freeing %s", tmp->bid_name));
					blkid_free_dev(tmp);
					cache->bic_flags |= BLKID_BIC_FL_CHANGED;
					break;
				}
			}
		}
	next:
		if (dir)
			closedir(dir);
		if (pc)
			ul_unref_path(pc);
	}

	closedir(sysfs);
	return 0;
}

/*
 * Read the device data for all available block devices in the system.
 */
static int probe_all(blkid_cache cache, int only_if_new, int update_interval)
{
	int rc;

	if (!cache)
		return -BLKID_ERR_PARAM;

	if (cache->bic_flags & BLKID_BIC_FL_PROBED &&
	    time(NULL) - cache->bic_time < BLKID_PROBE_INTERVAL) {
		DBG(PROBE, ul_debug("don't re-probe [delay < %d]", BLKID_PROBE_INTERVAL));
		return 0;
	}

	blkid_read_cache(cache);
#ifdef VG_DIR
	lvm_probe_all(cache, only_if_new);
#endif
	ubi_probe_all(cache, only_if_new);

	rc = sysfs_probe_all(cache, only_if_new, 0);

	/* Don't mark the change as "probed" if /sys not avalable */
	if (update_interval && rc == 0) {
		cache->bic_time = time(NULL);
		cache->bic_flags |= BLKID_BIC_FL_PROBED;
	}

	blkid_flush_cache(cache);
	return 0;
}

/**
 * blkid_probe_all:
 * @cache: cache handler
 *
 * Probes all block devices.
 *
 * Returns: 0 on success, or number less than zero in case of error.
 */
int blkid_probe_all(blkid_cache cache)
{
	int ret;

	DBG(PROBE, ul_debug("Begin blkid_probe_all()"));
	ret = probe_all(cache, 0, 1);
	DBG(PROBE, ul_debug("End blkid_probe_all() [rc=%d]", ret));
	return ret;
}

/**
 * blkid_probe_all_new:
 * @cache: cache handler
 *
 * Probes all new block devices.
 *
 * Returns: 0 on success, or number less than zero in case of error.
 */
int blkid_probe_all_new(blkid_cache cache)
{
	int ret;

	DBG(PROBE, ul_debug("Begin blkid_probe_all_new()"));
	ret = probe_all(cache, 1, 0);
	DBG(PROBE, ul_debug("End blkid_probe_all_new() [rc=%d]", ret));
	return ret;
}

/**
 * blkid_probe_all_removable:
 * @cache: cache handler
 *
 * The libblkid probing is based on devices from /proc/partitions by default.
 * This file usually does not contain removable devices (e.g. CDROMs) and this kind
 * of devices are invisible for libblkid.
 *
 * This function adds removable block devices to @cache (probing is based on
 * information from the /sys directory). Don't forget that removable devices
 * (floppies, CDROMs, ...) could be pretty slow. It's very bad idea to call
 * this function by default.
 *
 * Note that devices which were detected by this function won't be written to
 * blkid.tab cache file.
 *
 * Returns: 0 on success, or number less than zero in case of error.
 */
int blkid_probe_all_removable(blkid_cache cache)
{
	int ret;

	DBG(PROBE, ul_debug("Begin blkid_probe_all_removable()"));
	ret = sysfs_probe_all(cache, 0, 1);
	DBG(PROBE, ul_debug("End blkid_probe_all_removable() [rc=%d]", ret));
	return ret;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	blkid_cache cache = NULL;
	int ret;

	blkid_init_debug(BLKID_DEBUG_ALL);
	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n"
			"Probe all devices and exit\n", argv[0]);
		exit(1);
	}
	if ((ret = blkid_get_cache(&cache, "/dev/null")) != 0) {
		fprintf(stderr, "%s: error creating cache (%d)\n",
			argv[0], ret);
		exit(1);
	}
	if (blkid_probe_all(cache) < 0)
		printf("%s: error probing devices\n", argv[0]);

	if (blkid_probe_all_removable(cache) < 0)
		printf("%s: error probing removable devices\n", argv[0]);

	blkid_put_cache(cache);
	return (0);
}
#endif
