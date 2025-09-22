/*
 * fgetflags.c		- Get a file flags on an ext2 file system
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

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define OPEN_FLAGS (O_RDONLY|O_NONBLOCK|O_LARGEFILE|O_NOFOLLOW)

int fgetflags (const char * name, unsigned long * flags)
{
#if HAVE_STAT_FLAGS && !(APPLE_DARWIN && HAVE_EXT2_IOCTLS)
	struct stat buf;

	if (stat (name, &buf) == -1)
		return -1;

	*flags = 0;
#ifdef UF_IMMUTABLE
	if (buf.st_flags & UF_IMMUTABLE)
		*flags |= EXT2_IMMUTABLE_FL;
#endif
#ifdef UF_APPEND
	if (buf.st_flags & UF_APPEND)
		*flags |= EXT2_APPEND_FL;
#endif
#ifdef UF_NODUMP
	if (buf.st_flags & UF_NODUMP)
		*flags |= EXT2_NODUMP_FL;
#endif

	return 0;
#elif APPLE_DARWIN && HAVE_EXT2_IOCTLS
	int f, save_errno = 0;

	f = -1;
	save_errno = syscall(SYS_fsctl, name, EXT2_IOC_GETFLAGS, &f, 0);
	*flags = f;
	return (save_errno);
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
	r = ioctl(fd, EXT2_IOC_GETFLAGS, &f);
	if (r == -1) {
		if (errno == ENOTTY)
			errno = EOPNOTSUPP;
		save_errno = errno;
	}
	*flags = f;
	close(fd);
	if (save_errno)
		errno = save_errno;
	return r;
#else
	errno = EOPNOTSUPP;
	return -1;
#endif
}
