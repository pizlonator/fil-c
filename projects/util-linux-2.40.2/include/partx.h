/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_PARTX_H
#define UTIL_LINUX_PARTX_H

#include <sys/ioctl.h>
#include <linux/blkpg.h>
#include <stdint.h>

#ifndef BLKPG_ADD_PARTITION
# define BLKPG_ADD_PARTITION	1
#endif

#ifndef BLKPG_DEL_PARTITION
# define BLKPG_DEL_PARTITION	2
#endif

#ifndef BLKPG_RESIZE_PARTITION
# define BLKPG_RESIZE_PARTITION	3		/* since Linux 3.6 */
#endif


#define INIT_BLKPG_PARTITION(_partno, _start, _size) {	\
		.pno    = (_partno),			\
		.start  = (_start) << 9,		\
		.length = (_size) << 9,			\
		.devname[0] = 0,			\
		.volname[0] = 0				\
}

#define INIT_BLKPG_ARG(_action, _part) {		\
		.op      = (_action),			\
		.flags   = 0,				\
		.datalen = sizeof(*(_part)),		\
		.data = (_part)				\
}


static inline int partx_del_partition(int fd, unsigned int partno)
{
	struct blkpg_partition p = INIT_BLKPG_PARTITION(partno, 0, 0);
	struct blkpg_ioctl_arg a = INIT_BLKPG_ARG(BLKPG_DEL_PARTITION, &p);

	return ioctl(fd, BLKPG, &a);
}

static inline int partx_add_partition(int fd, int partno,
			uint64_t start, uint64_t size)
{
	struct blkpg_partition p = INIT_BLKPG_PARTITION(partno, start, size);
	struct blkpg_ioctl_arg a = INIT_BLKPG_ARG(BLKPG_ADD_PARTITION, &p);

	return ioctl(fd, BLKPG, &a);
}

static inline int partx_resize_partition(int fd, int partno,
			uint64_t start, uint64_t size)
{
	struct blkpg_partition p = INIT_BLKPG_PARTITION(partno, start, size);
	struct blkpg_ioctl_arg a = INIT_BLKPG_ARG(BLKPG_RESIZE_PARTITION, &p);

	return ioctl(fd, BLKPG, &a);
}

#endif /*  UTIL_LINUX_PARTX_H */
