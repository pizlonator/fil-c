/*
 * llseek.c -- stub calling the llseek system call
 *
 * Copyright (C) 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "config.h"
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef __MSDOS__
#include <io.h>
#endif
#include "et/com_err.h"
#include "ext2fs/ext2_io.h"

#ifdef __linux__

#include <linux/unistd.h>

static ext2_loff_t my_llseek (int fd, ext2_loff_t offset, int origin)
{
#if SIZEOF_OFF_T >= 8
	return lseek(fd, offset, origin);
#elif HAVE_LSEEK64_PROTOTYPE
	return lseek64(fd, offset, origin);
#elif HAVE_LLSEEK_PROTOTYPE
	return llseek(fd, offset, origin);
#elif defined(__NR__llseek)
	loff_t result;
	int retval;
	retval = syscall(__NR__llseek, fd,
		(unsigned long)(offset >> 32),
		(unsigned long)(offset & 0xffffffff),
		&result, origin);
	return (retval == -1 ? retval : result);
#else
	errno = ENOSYS;
	return -1;
#endif
}

ext2_loff_t ext2fs_llseek (int fd, ext2_loff_t offset, int origin)
{
	ext2_loff_t result;
	static int do_compat = 0;

	if (do_compat)
		goto fallback;

	result = my_llseek (fd, offset, origin);
	if (result == -1 && errno == ENOSYS) {
		/*
		 * Just in case this code runs on top of an old kernel
		 * which does not support the llseek system call
		 */
		do_compat++;
	fallback:
		if (offset < ((ext2_loff_t) 1 << ((sizeof(off_t)*8) -1)))
			return lseek(fd, (off_t) offset, origin);
		errno = EINVAL;
		return -1;
	}
	return result;
}

#else /* !linux */

#ifndef EINVAL
#define EINVAL EXT2_ET_INVALID_ARGUMENT
#endif

ext2_loff_t ext2fs_llseek (int fd, ext2_loff_t offset, int origin)
{
#if defined(HAVE_LSEEK64) && defined(HAVE_LSEEK64_PROTOTYPE)
	return lseek64 (fd, offset, origin);
#else
	if ((sizeof(off_t) < sizeof(ext2_loff_t)) &&
	    (offset >= ((ext2_loff_t) 1 << ((sizeof(off_t)*8) -1)))) {
		errno = EINVAL;
		return -1;
	}
	return lseek (fd, (off_t) offset, origin);
#endif
}

#endif 	/* linux */


