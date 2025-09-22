/*
 * lsfd(1) - list file descriptors
 *
 * Copyright (C) 2022 Red Hat, Inc. All rights reserved.
 * Written by Masatake YAMATO <yamato@redhat.com>
 *
 * Very generally based on lsof(8) by Victor A. Abell <abe@purdue.edu>
 * It supports multiple OSes. lsfd specializes to Linux.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* lsfd_decode_file_flags() is for decoding `flags' field of
 * /proc/$pid/fdinfo/$fd. Bits of the field have name defined
 * in fctl.h.
 * A system on which lsfd is built may have multiple
 * fctl.h files:
 *
 * - /usr/include/asm/fcntl.h         (a part of Linux kernel)
 * - /usr/include/asm-generic/fcntl.h (a part of Linux kernel)
 * - /usr/include/fcntl.h             (a part of glibc)
 * - /usr/include/bits/fcntl.h        (a part of glibc)
 *
 * For decoding purpose, /usr/include/asm/fcntl.h or
 * /usr/include/asm-generic/fcntl.h is needed.
 *
 * /usr/include/bits/fcntl.h and /usr/include/fcntl.h are
 * not suitable for decoding. They should not be included.
 * /usr/include/fcntl.h includes /usr/include/bits/fcntl.h.
 */

#if defined HAVE_ASM_FCNTL_H
#include <asm/fcntl.h>
#elif defined HAVE_ASM_GENERIC_FCNTL_H
#include <asm-generic/fcntl.h>
#else
#error "kernel's fcntl.h is not available"
#endif

#include <stddef.h>		/* for size_t */
struct ul_buffer;

void lsfd_decode_file_flags(struct ul_buffer *buf, int flags);

/* We cannot include buffer.h because buffer.h includes
 * /usr/include/fcntl.h indirectly. */
extern int ul_buffer_is_empty(struct ul_buffer *buf);
extern int ul_buffer_append_data(struct ul_buffer *buf, const char *data, size_t sz);
extern int ul_buffer_append_string(struct ul_buffer *buf, const char *str);

void lsfd_decode_file_flags(struct ul_buffer *buf, int flags)
{
#define SET_FLAG_FULL(L,s)						\
	do {								\
		if (flags & (L)) {					\
			if (!ul_buffer_is_empty(buf))			\
				ul_buffer_append_data(buf, ",", 1);	\
			ul_buffer_append_string(buf, #s);		\
		}							\
	} while (0)

#define SET_FLAG(L,s) SET_FLAG_FULL(O_##L,s)

#ifdef O_WRONLY
	SET_FLAG(WRONLY,wronly);
#endif

#ifdef O_RDWR
	SET_FLAG(RDWR,rdwr);
#endif

#ifdef O_CREAT
	SET_FLAG(CREAT,creat);
#endif

#ifdef O_EXCL
	SET_FLAG(EXCL,excl);
#endif

#ifdef O_NOCTTY
	SET_FLAG(NOCTTY,noctty);
#endif

#ifdef O_APPEND
	SET_FLAG(APPEND,append);
#endif

#ifdef O_NONBLOCK
	SET_FLAG(NONBLOCK,nonblock);
#endif

#ifdef O_DSYNC
	SET_FLAG(DSYNC,dsync);
#endif

#ifdef FASYNC
	SET_FLAG_FULL(FASYNC,fasync);
#endif

#ifdef O_DIRECT
	SET_FLAG(DIRECT,direct);
#endif

#ifdef O_LARGEFILE
	SET_FLAG(LARGEFILE,largefile);
#endif

#ifdef O_DIRECTORY
	SET_FLAG(DIRECTORY,directory);
#endif

#ifdef O_NOFOLLOW
	SET_FLAG(NOFOLLOW,nofollow);
#endif

#ifdef O_NOATIME
	SET_FLAG(NOATIME,noatime);
#endif

#ifdef O_CLOEXEC
	SET_FLAG(CLOEXEC,cloexec);
#endif

#ifdef __O_SYNC
	SET_FLAG_FULL(__O_SYNC,_sync);
#endif

#ifdef O_PATH
	SET_FLAG(PATH,path);
#endif

#ifdef __O_TMPFILE
	SET_FLAG_FULL(__O_TMPFILE,_tmpfile);
#endif

}
