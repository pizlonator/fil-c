/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

/* For 2.6, use the cleaned up header to get the 64 bit API. */
// Commented out per Rob's request
//# include "fix_u32.h" /* some old toolchains need __u64 for linux/loop.h */
# include <linux/loop.h>
typedef struct loop_info64 bb_loop_info;
# define BB_LOOP_SET_STATUS LOOP_SET_STATUS64
# define BB_LOOP_GET_STATUS LOOP_GET_STATUS64

#else

/* For 2.4 and earlier, use the 32 bit API (and don't trust the headers) */
/* Stuff stolen from linux/loop.h for 2.4 and earlier kernels */
# include <linux/posix_types.h>
# define LO_NAME_SIZE        64
# define LO_KEY_SIZE         32
# define LOOP_SET_FD         0x4C00
# define LOOP_CLR_FD         0x4C01
# define BB_LOOP_SET_STATUS  0x4C02
# define BB_LOOP_GET_STATUS  0x4C03
typedef struct {
	int                lo_number;
	__kernel_dev_t     lo_device;
	unsigned long      lo_inode;
	__kernel_dev_t     lo_rdevice;
	int                lo_offset;
	int                lo_encrypt_type;
	int                lo_encrypt_key_size;
	int                lo_flags;
	char               lo_file_name[LO_NAME_SIZE];
	unsigned char      lo_encrypt_key[LO_KEY_SIZE];
	unsigned long      lo_init[2];
	char               reserved[4];
} bb_loop_info;
#endif

char* FAST_FUNC query_loop(const char *device)
{
	int fd;
	bb_loop_info loopinfo;
	char *dev = NULL;

	fd = open(device, O_RDONLY);
	if (fd >= 0) {
		if (ioctl(fd, BB_LOOP_GET_STATUS, &loopinfo) == 0) {
			dev = xasprintf("%"OFF_FMT"u %s", (off_t) loopinfo.lo_offset,
					(char *)loopinfo.lo_file_name);
		}
		close(fd);
	}

	return dev;
}

int FAST_FUNC del_loop(const char *device)
{
	int fd, rc;

	fd = open(device, O_RDONLY);
	if (fd < 0)
		return fd; /* -1 */
	rc = ioctl(fd, LOOP_CLR_FD, 0);
	close(fd);

	return rc;
}

/* Obtain an unused loop device number */
int FAST_FUNC get_free_loop(void)
{
	int fd;
	int loopdevno;

	fd = open("/dev/loop-control", O_RDWR | O_CLOEXEC);
	if (fd == -1)
		return fd - 1; /* -2: "no /dev/loop-control" */

#ifndef LOOP_CTL_GET_FREE
# define LOOP_CTL_GET_FREE 0x4C82
#endif
	loopdevno = ioctl(fd, LOOP_CTL_GET_FREE);
	close(fd);
	return loopdevno; /* can be -1 if error */
}

static int get_next_free_loop(char *dev, int id)
{
	int loopdevno;

	loopdevno = get_free_loop();
	if (loopdevno != -1) {
		/* loopdevno is -2 (use id) or >= 0 (use id = loopdevno): */
		if (loopdevno >= 0)
			id = loopdevno;
		sprintf(dev, LOOP_FORMAT, id);
	}
	return loopdevno;
}

#if ENABLE_TRY_LOOP_CONFIGURE || ENABLE_LOOP_CONFIGURE
# define LOOP_CONFIGURE 0x4C0A
struct bb_loop_config {
	uint32_t fd;
	uint32_t block_size;
	struct loop_info64 info;
	uint64_t __reserved[8];
};
#endif

static int set_loopdev_params(int lfd,
		int ffd, const char *file,
		unsigned long long offset,
		unsigned long long sizelimit,
		unsigned flags)
{
	int rc;
#if ENABLE_TRY_LOOP_CONFIGURE || ENABLE_LOOP_CONFIGURE
	struct bb_loop_config lconfig;
# define loopinfo lconfig.info
#else
	bb_loop_info loopinfo;
#endif

	rc = ioctl(lfd, BB_LOOP_GET_STATUS, &loopinfo);

	/* If device is free, try to claim it */
	if (rc && errno == ENXIO) {
#if ENABLE_TRY_LOOP_CONFIGURE || ENABLE_LOOP_CONFIGURE
		memset(&lconfig, 0, sizeof(lconfig));
#else
		memset(&loopinfo, 0, sizeof(loopinfo));
#endif
		safe_strncpy((char *)loopinfo.lo_file_name, file, LO_NAME_SIZE);
		loopinfo.lo_offset = offset;
		loopinfo.lo_sizelimit = sizelimit;
		/*
		 * LO_FLAGS_READ_ONLY is not set because RO is controlled
		 * by open type of the lfd.
		 */
		loopinfo.lo_flags = (flags & ~BB_LO_FLAGS_READ_ONLY);

#if ENABLE_TRY_LOOP_CONFIGURE || ENABLE_LOOP_CONFIGURE
		lconfig.fd = ffd;
		rc = ioctl(lfd, LOOP_CONFIGURE, &lconfig);
		if (rc == 0)
			return rc; /* SUCCESS! */
# if ENABLE_TRY_LOOP_CONFIGURE
		if (errno != EINVAL)
			return rc; /* error other than old kernel */
		/* Old kernel, fall through into old way to do it: */
# endif
#endif
#if ENABLE_TRY_LOOP_CONFIGURE || ENABLE_NO_LOOP_CONFIGURE
		/* Associate free loop device with file */
		rc = ioctl(lfd, LOOP_SET_FD, ffd);
		if (rc != 0) {
			/* Ouch... race: the device already has a fd */
			return rc;
		}
		rc = ioctl(lfd, BB_LOOP_SET_STATUS, &loopinfo);
		if (rc != 0 && (loopinfo.lo_flags & BB_LO_FLAGS_AUTOCLEAR)) {
			/* Old kernel, does not support LO_FLAGS_AUTOCLEAR? */
			/* (this code path is not tested) */
			loopinfo.lo_flags -= BB_LO_FLAGS_AUTOCLEAR;
			rc = ioctl(lfd, BB_LOOP_SET_STATUS, &loopinfo);
		}
		if (rc == 0)
			return rc; /* SUCCESS! */
		/* failure, undo LOOP_SET_FD */
		ioctl(lfd, LOOP_CLR_FD, 0); // actually, 0 param is unnecessary
#endif
	}
	return -1;
#undef loopinfo
}

/* Returns opened fd to the loop device, <0 on error.
 * *device is loop device to use, or if *device==NULL finds a loop device to
 * mount it on and sets *device to a strdup of that loop device name.
 */
int FAST_FUNC set_loop(char **device, const char *file, unsigned long long offset,
		unsigned long long sizelimit, unsigned flags)
{
	char dev[LOOP_NAMESIZE];
	char *try;
	struct stat statbuf;
	int i, lfd, ffd, mode, rc;

	/* Open the file.  Barf if this doesn't work.  */
	mode = (flags & BB_LO_FLAGS_READ_ONLY) ? O_RDONLY : O_RDWR;
 open_ffd:
	ffd = open(file, mode);
	if (ffd < 0) {
		if (mode != O_RDONLY) {
			mode = O_RDONLY;
			goto open_ffd;
		}
		return -errno;
	}

	try = *device;
	if (!try) {
		try = dev;
	}

	/* Find a loop device */
	/* 0xfffff is a max possible minor number in Linux circa 2010 */
	for (i = 0; i <= 0xfffff; i++) {
		if (!*device) {
			rc = get_next_free_loop(dev, i);
			if (rc == -1)
				break; /* no free loop devices (or other error in LOOP_CTL_GET_FREE) */
			if (rc >= 0)
				/* /dev/loop-control gave us the next free /dev/loopN */
				goto open_lfd;
			/* else: sequential /dev/loopN, needs to be tested/maybe_created */
		}

		IF_FEATURE_MOUNT_LOOP_CREATE(errno = 0;)
		if (stat(try, &statbuf) != 0 || !S_ISBLK(statbuf.st_mode)) {
			if (ENABLE_FEATURE_MOUNT_LOOP_CREATE
			 && errno == ENOENT
			 && (!*device)
			) {
				/* Node doesn't exist, try to create it */
				if (mknod(dev, S_IFBLK|0644, makedev(7, i)) == 0)
					goto open_lfd;
			}
			/* Ran out of block devices, return failure */
			rc = -1;
			break;
		}
 open_lfd:
		/* Open the sucker and check its loopiness */
		lfd = rc = open(try, mode);
		if (lfd < 0 && errno == EROFS) {
			mode = O_RDONLY;
			lfd = rc = open(try, mode);
		}
		if (lfd < 0) {
			if (errno == ENXIO) {
				/* Happens if loop module is not loaded */
				/* rc is -1; */
				break;
			}
			goto try_next_loopN;
		}

		rc = set_loopdev_params(lfd, ffd, file, offset, sizelimit, flags);
		if (rc == 0) {
			/* SUCCESS! */
			if (!*device)
				*device = xstrdup(dev);
			/* Note: mount asks for LO_FLAGS_AUTOCLEAR loopdev.
			 * Closing LO_FLAGS_AUTOCLEARed lfd before mount
			 * is wrong (would free the loop device!),
			 * this is why we return without closing it.
			 */
			rc = lfd; /* return this */
			break;
		}
		close(lfd);
 try_next_loopN:
		rc = -1;
		if (*device) /* was looking for a particular "/dev/loopN"? */
			break; /* yes, do not try other names */
	} /* for() */

	close(ffd);
	return rc;
}
