/*
 * fsetflags.c		- Set a file flags on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

/*
 * History:
 * 93/10/30	- Creation
 */

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "config.h"
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_EXT2_IOCTLS
#include <fcntl.h>
#include <sys/ioctl.h>
#endif

#include "e2p.h"

/*
 * Deal with lame glibc's that define this function without actually
 * implementing it.  Can you say "attractive nuisance", boys and girls?
 * I knew you could!
 */
#ifdef __linux__
#undef HAVE_CHFLAGS
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define OPEN_FLAGS (O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_NOFOLLOW)

int fsetflags (const char * name, unsigned long flags)
{
#if HAVE_CHFLAGS && !(APPLE_DARWIN && HAVE_EXT2_IOCTLS)
	unsigned long bsd_flags = 0;

#ifdef UF_IMMUTABLE
	if (flags & EXT2_IMMUTABLE_FL)
		bsd_flags |= UF_IMMUTABLE;
#endif
#ifdef UF_APPEND
	if (flags & EXT2_APPEND_FL)
		bsd_flags |= UF_APPEND;
#endif
#ifdef UF_NODUMP
	if (flags & EXT2_NODUMP_FL)
		bsd_flags |= UF_NODUMP;
#endif

	return chflags (name, bsd_flags);
#elif APPLE_DARWIN && HAVE_EXT2_IOCTLS
	int f = (int) flags;
	return syscall(SYS_fsctl, name, EXT2_IOC_SETFLAGS, &f, 0);
#elif HAVE_EXT2_IOCTLS
	struct stat buf;
	int fd, r, f, save_errno = 0;

	if (!stat(name, &buf) &&
	    !S_ISREG(buf.st_mode) && !S_ISDIR(buf.st_mode)) {
		errno = EOPNOTSUPP;
		return -1;
	}
	fd = open(name, OPEN_FLAGS);
	if (fd == -1) {
		if (errno == ELOOP || errno == ENXIO)
			errno = EOPNOTSUPP;
		return -1;
	}
	if (!fstat(fd, &buf) &&
	    !S_ISREG(buf.st_mode) && !S_ISDIR(buf.st_mode)) {
		close(fd);
		errno = EOPNOTSUPP;
		return -1;
	}
	f = (int) flags;
	r = ioctl(fd, EXT2_IOC_SETFLAGS, &f);
	if (r == -1) {
		if (errno == ENOTTY)
			errno = EOPNOTSUPP;
		save_errno = errno;
	}
	close(fd);
	if (save_errno)
		errno = save_errno;
	return r;
#else
	errno = EOPNOTSUPP;
	return -1;
#endif
}
