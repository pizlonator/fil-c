/* Definitions for POSIX memory map interface.  Linux/HPPA version.
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _SYS_MMAN_H
# error "Never use <bits/mman.h> directly; include <sys/mman.h> instead."
#endif

/* These are taken from the kernel definitions.  */

/* Other flags.  */
#define __MAP_ANONYMOUS	0x10		/* Don't use a file */

/* These are Linux-specific.  */
#define MAP_DENYWRITE	0x0800		/* ETXTBSY */
#define MAP_EXECUTABLE	0x1000		/* Mark it as an executable */
#define MAP_LOCKED	0x2000		/* Pages are locked */
#define MAP_NORESERVE	0x4000		/* Don't check for reservations */
#define MAP_GROWSDOWN	0x8000		/* Stack-like segment */
#define MAP_POPULATE	0x10000		/* Populate (prefault) pagetables */
#define MAP_NONBLOCK	0x20000		/* Do not block on IO */
#define MAP_STACK	0x40000		/* Create for process/thread stacks */
#define MAP_HUGETLB	0x80000		/* Create a huge page mapping */
#define MAP_FIXED_NOREPLACE 0x100000	/* MAP_FIXED but do not unmap
					   underlying mapping.  */

#include <bits/mman-linux.h>

#undef MAP_TYPE
#define MAP_TYPE	0x2b		/* Mask for type of mapping */

#undef MAP_FIXED
#define MAP_FIXED	0x04		/* Interpret addr exactly */

/* Flags to "msync"  */
#undef MS_SYNC
#define MS_SYNC		1		/* Synchronous memory sync */
#undef MS_ASYNC
#define MS_ASYNC	2		/* Sync memory asynchronously */
#undef MS_INVALIDATE
#define MS_INVALIDATE	4		/* Invalidate the caches */
