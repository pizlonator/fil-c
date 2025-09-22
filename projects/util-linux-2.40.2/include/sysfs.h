/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 */
#ifndef UTIL_LINUX_SYSFS_H
#define UTIL_LINUX_SYSFS_H


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <dirent.h>

#include "path.h"

/**
 * sysfs_devname_sys_to_dev:
 * @name: devname to be converted in place
 *
 * Linux kernel linux/drivers/base/core.c: device_get_devnode()
 * defines a replacement of '!' in the /sys device name by '/' in the
 * /dev device name. This helper replaces all occurrences of '!' in
 * @name by '/' to convert from /sys to /dev.
 */
static inline void sysfs_devname_sys_to_dev(char *name)
{
	char *c;

	if (name)
		while ((c = strchr(name, '!')))
			c[0] = '/';
}

/**
 * sysfs_devname_dev_to_sys:
 * @name: devname to be converted in place
 *
 * See sysfs_devname_sys_to_dev().
 */
static inline void sysfs_devname_dev_to_sys(char *name)
{
	char *c;

	if (name)
		while ((c = strchr(name, '/')))
			c[0] = '!';
}

struct sysfs_blkdev {
	dev_t	devno;
	struct path_cxt *parent;

	unsigned int	scsi_host,
			scsi_channel,
			scsi_target,
			scsi_lun;

	unsigned int	has_hctl   : 1,
			hctl_error : 1 ;
};

void ul_sysfs_init_debug(void);

struct path_cxt *ul_new_sysfs_path(dev_t devno, struct path_cxt *parent, const char *prefix);
int sysfs_blkdev_init_path(struct path_cxt *pc, dev_t devno, struct path_cxt *parent);

int sysfs_blkdev_set_parent(struct path_cxt *pc, struct path_cxt *parent);
struct path_cxt *sysfs_blkdev_get_parent(struct path_cxt *pc);

char *sysfs_blkdev_get_name(struct path_cxt *pc, char *buf, size_t bufsiz);
int sysfs_blkdev_is_partition_dirent(DIR *dir, struct dirent *d, const char *parent_name);
int sysfs_blkdev_count_partitions(struct path_cxt *pc, const char *devname);
dev_t sysfs_blkdev_partno_to_devno(struct path_cxt *pc, int partno);
char *sysfs_blkdev_get_slave(struct path_cxt *pc);
char *sysfs_blkdev_get_path(struct path_cxt *pc, char *buf, size_t bufsiz);
dev_t sysfs_blkdev_get_devno(struct path_cxt *pc);

char *sysfs_blkdev_get_devchain(struct path_cxt *pc, char *buf, size_t bufsz);
int sysfs_blkdev_next_subsystem(struct path_cxt *pc __attribute__((unused)), char *devchain, char **subsys);

int sysfs_blkdev_is_hotpluggable(struct path_cxt *pc);
int sysfs_blkdev_is_removable(struct path_cxt *pc);
int sysfs_blkdev_get_wholedisk( struct path_cxt *pc,
                                char *diskname,
                                size_t len,
                                dev_t *diskdevno);

int sysfs_devno_to_wholedisk(dev_t dev, char *diskname,
                             size_t len, dev_t *diskdevno);
int sysfs_devno_is_dm_private(dev_t devno, char **uuid);
int sysfs_devno_is_wholedisk(dev_t devno);

dev_t sysfs_devname_to_devno(const char *name);
dev_t __sysfs_devname_to_devno(const char *prefix, const char *name, const char *parent);
int sysfs_devname_is_hidden(const char *prefix, const char *name);

char *sysfs_devno_to_devpath(dev_t devno, char *buf, size_t bufsiz);
char *sysfs_devno_to_devname(dev_t devno, char *buf, size_t bufsiz);
int sysfs_devno_count_partitions(dev_t devno);

int sysfs_blkdev_scsi_get_hctl(struct path_cxt *pc, int *h, int *c, int *t, int *l);
char *sysfs_blkdev_scsi_host_strdup_attribute(struct path_cxt *pc,
                        const char *type, const char *attr);
int sysfs_blkdev_scsi_host_is(struct path_cxt *pc, const char *type);
int sysfs_blkdev_scsi_has_attribute(struct path_cxt *pc, const char *attr);
int sysfs_blkdev_scsi_path_contains(struct path_cxt *pc, const char *pattern);

char *sysfs_chrdev_devno_to_devname(dev_t devno, char *buf, size_t bufsiz);

enum sysfs_byteorder {
	SYSFS_BYTEORDER_LITTLE,
	SYSFS_BYTEORDER_BIG,
};

extern enum sysfs_byteorder sysfs_get_byteorder(struct path_cxt *pc);
extern int sysfs_get_address_bits(struct path_cxt *pc);

#endif /* UTIL_LINUX_SYSFS_H */
