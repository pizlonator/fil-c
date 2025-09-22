/*
 * devno.c - find a particular device by its device number (major/minor)
 *
 * Copyright (C) 2000, 2001, 2003 Theodore Ts'o
 * Copyright (C) 2001 Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <dirent.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif
#include <fcntl.h>
#include <inttypes.h>

#include "blkidP.h"
#include "pathnames.h"
#include "sysfs.h"
#include "strutils.h"

static char *blkid_strconcat(const char *a, const char *b, const char *c)
{
	char *res, *p;
	size_t len, al, bl, cl;

	al = a ? strlen(a) : 0;
	bl = b ? strlen(b) : 0;
	cl = c ? strlen(c) : 0;

	len = al + bl + cl;
	if (!len)
		return NULL;
	p = res = malloc(len + 1);
	if (!res)
		return NULL;
	if (al)
		p = mempcpy(p, a, al);
	if (bl)
		p = mempcpy(p, b, bl);
	if (cl)
		p = mempcpy(p, c, cl);
	*p = '\0';
	return res;
}

/*
 * This function adds an entry to the directory list
 */
static void add_to_dirlist(const char *dir, const char *subdir,
				struct dir_list **list)
{
	struct dir_list *dp;

	dp = malloc(sizeof(struct dir_list));
	if (!dp)
		return;
	dp->name = subdir ? blkid_strconcat(dir, "/", subdir) :
		   dir ? strdup(dir) : NULL;

	if (!dp->name) {
		free(dp);
		return;
	}
	dp->next = *list;
	*list = dp;
}

/*
 * This function frees a directory list
 */
static void free_dirlist(struct dir_list **list)
{
	struct dir_list *dp, *next;

	for (dp = *list; dp; dp = next) {
		next = dp->next;
		free(dp->name);
		free(dp);
	}
	*list = NULL;
}

void blkid__scan_dir(char *dirname, dev_t devno, struct dir_list **list,
		     char **devname)
{
	DIR	*dir;
	struct dirent *dp;
	struct stat st;

	if ((dir = opendir(dirname)) == NULL)
		return;

	while ((dp = readdir(dir)) != NULL) {
#ifdef _DIRENT_HAVE_D_TYPE
		if (dp->d_type != DT_UNKNOWN && dp->d_type != DT_BLK &&
		    dp->d_type != DT_LNK && dp->d_type != DT_DIR)
			continue;
#endif
		if (dp->d_name[0] == '.' &&
		    ((dp->d_name[1] == 0) ||
		     ((dp->d_name[1] == '.') && (dp->d_name[2] == 0))))
			continue;

		if (fstatat(dirfd(dir), dp->d_name, &st, 0))
			continue;

		if (S_ISBLK(st.st_mode) && st.st_rdev == devno) {
			*devname = blkid_strconcat(dirname, "/", dp->d_name);
			DBG(DEVNO, ul_debug("found 0x%llx at %s", (long long)devno,
				   *devname));
			break;
		}

		if (!list || !S_ISDIR(st.st_mode))
			continue;

		/* add subdirectory (but not symlink) to the list */
#ifdef _DIRENT_HAVE_D_TYPE
		if (dp->d_type == DT_LNK)
			continue;
		if (dp->d_type == DT_UNKNOWN)
#endif
		{
			if (fstatat(dirfd(dir), dp->d_name, &st, AT_SYMLINK_NOFOLLOW) ||
			    !S_ISDIR(st.st_mode))
				continue;	/* symlink or fstatat() failed */
		}

		if (*dp->d_name == '.' || (
#ifdef _DIRENT_HAVE_D_TYPE
		    dp->d_type == DT_DIR &&
#endif
		    strcmp(dp->d_name, "shm") == 0))
			/* ignore /dev/.{udev,mount,mdadm} and /dev/shm */
			continue;

		add_to_dirlist(dirname, dp->d_name, list);
	}
	closedir(dir);
}

/* Directories where we will try to search for device numbers */
static const char *devdirs[] = { "/devices", "/devfs", "/dev", NULL };

/**
 * SECTION: misc
 * @title: Miscellaneous utils
 * @short_description: mix of various utils for low-level and high-level API
 */



static char *scandev_devno_to_devpath(dev_t devno)
{
	struct dir_list *list = NULL, *new_list = NULL;
	char *devname = NULL;
	const char **dir;

	/*
	 * Add the starting directories to search in reverse order of
	 * importance, since we are using a stack...
	 */
	for (dir = devdirs; *dir; dir++)
		add_to_dirlist(*dir, NULL, &list);

	while (list) {
		struct dir_list *current = list;

		list = list->next;
		DBG(DEVNO, ul_debug("directory %s", current->name));
		blkid__scan_dir(current->name, devno, &new_list, &devname);
		free(current->name);
		free(current);
		if (devname)
			break;
		/*
		 * If we're done checking at this level, descend to
		 * the next level of subdirectories. (breadth-first)
		 */
		if (list == NULL) {
			list = new_list;
			new_list = NULL;
		}
	}
	free_dirlist(&list);
	free_dirlist(&new_list);

	return devname;
}

/**
 * blkid_devno_to_devname:
 * @devno: device number
 *
 * This function finds the pathname to a block device with a given
 * device number.
 *
 * Returns: a pointer to allocated memory to the pathname on success,
 * and NULL on failure.
 */
char *blkid_devno_to_devname(dev_t devno)
{
	char *path;
	char buf[PATH_MAX];

	path = sysfs_devno_to_devpath(devno, buf, sizeof(buf));
	if (path)
		path = strdup(path);
	if (!path)
		path = scandev_devno_to_devpath(devno);

	if (!path) {
		DBG(DEVNO, ul_debug("blkid: couldn't find devno 0x%04lx",
			   (unsigned long) devno));
	} else {
		DBG(DEVNO, ul_debug("found devno 0x%04llx as %s", (long long)devno, path));
	}

	return path;
}


/**
 * blkid_devno_to_wholedisk:
 * @dev: device number
 * @diskname: buffer to return diskname (or NULL)
 * @len: diskname buffer size (or 0)
 * @diskdevno: pointer to returns devno of entire disk (or NULL)
 *
 * This function uses sysfs to convert the @devno device number to the *name*
 * of the whole disk. The function DOES NOT return full device name. The @dev
 * argument could be partition or whole disk -- both is converted.
 *
 * For example: sda1, 0x0801 --> sda, 0x0800
 *
 * For conversion to the full disk *path* use blkid_devno_to_devname(), for
 * example:
 *
 * <informalexample>
 *  <programlisting>
 *
 *	dev_t dev = 0x0801, disk;		// sda1 = 8:1
 *	char *diskpath, diskname[32];
 *
 *	blkid_devno_to_wholedisk(dev, diskname, sizeof(diskname), &disk);
 *	diskpath = blkid_devno_to_devname(disk);
 *
 *	// print "0x0801: sda, /dev/sda, 8:0
 *	printf("0x%x: %s, %s, %d:%d\n",
 *		dev, diskname, diskpath, major(disk), minor(disk));
 *
 *	free(diskpath);
 *
 *  </programlisting>
 * </informalexample>
 *
 * Returns: 0 on success or -1 in case of error.
 */
int blkid_devno_to_wholedisk(dev_t dev, char *diskname,
			size_t len, dev_t *diskdevno)
{
	return sysfs_devno_to_wholedisk( dev, diskname, len, diskdevno);
}

/*
 * Returns 1 if the @major number is associated with @drvname.
 */
int blkid_driver_has_major(const char *drvname, int drvmaj)
{
	FILE *f;
	char buf[128];
	int match = 0;

	f = fopen(_PATH_PROC_DEVICES, "r" UL_CLOEXECSTR);
	if (!f)
		return 0;

	while (fgets(buf, sizeof(buf), f)) {	/* skip to block dev section */
		if (strncmp("Block devices:\n", buf, sizeof(buf)) == 0)
			break;
	}

	while (fgets(buf, sizeof(buf), f)) {
		int maj;
		char name[64 + 1];

		if (sscanf(buf, "%d %64[^\n ]", &maj, name) != 2)
			continue;

		if (maj == drvmaj && strcmp(name, drvname) == 0) {
			match = 1;
			break;
		}
	}

	fclose(f);

	DBG(DEVNO, ul_debug("major %d %s associated with '%s' driver",
			drvmaj, match ? "is" : "is NOT", drvname));
	return match;
}

#ifdef TEST_PROGRAM
int main(int argc, char** argv)
{
	char	*devname, *tmp;
	char	diskname[PATH_MAX];
	int	devmaj, devmin;
	dev_t	devno, disk_devno;
	const char *errmsg = "Couldn't parse %s: %s\n";

	blkid_init_debug(BLKID_DEBUG_ALL);
	if ((argc != 2) && (argc != 3)) {
		fprintf(stderr, "Usage:\t%s device_number\n\t%s major minor\n"
			"Resolve a device number to a device name\n",
			argv[0], argv[0]);
		exit(1);
	}
	if (argc == 2) {
		devno = strtoul(argv[1], &tmp, 0);
		if (*tmp) {
			fprintf(stderr, errmsg, "device number", argv[1]);
			exit(1);
		}
	} else {
		devmaj = strtoul(argv[1], &tmp, 0);
		if (*tmp) {
			fprintf(stderr, errmsg, "major number", argv[1]);
			exit(1);
		}
		devmin = strtoul(argv[2], &tmp, 0);
		if (*tmp) {
			fprintf(stderr, errmsg, "minor number", argv[2]);
			exit(1);
		}
		devno = makedev(devmaj, devmin);
	}
	printf("Looking for device 0x%04llx\n", (long long)devno);
	devname = blkid_devno_to_devname(devno);
	free(devname);

	printf("Looking for whole-device for 0x%04llx\n", (long long)devno);
	if (blkid_devno_to_wholedisk(devno, diskname,
				sizeof(diskname), &disk_devno) == 0)
		printf("found devno 0x%04llx as /dev/%s\n", (long long) disk_devno, diskname);

	return 0;
}
#endif
