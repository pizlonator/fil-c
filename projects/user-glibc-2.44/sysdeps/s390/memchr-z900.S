/* Search a character in a block of memory.  64 bit S/390 version.
   Copyright (C) 2001-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* INPUT PARAMETERS
     %r2 = address to memory area
     %r3 = character to find
     %r4 = number of bytes to search.  */

#include <ifunc-memchr.h>
#include "sysdep.h"
#include "asm-syntax.h"

#if HAVE_MEMCHR_Z900
	.text
ENTRY(MEMCHR_Z900)
	lghi  %r0,0xff
	ngr   %r0,%r3
	lgr   %r1,%r2
	la    %r2,0(%r4,%r1)
0:	srst  %r2,%r1
	jo    0b
	brc   13,1f
	slgr  %r2,%r2
1:	br    %r14
END(MEMCHR_Z900)

# if ! HAVE_MEMCHR_IFUNC
strong_alias (MEMCHR_Z900, __memchr)
weak_alias (__memchr, memchr)
# endif

# if defined SHARED && IS_IN (libc)
strong_alias (MEMCHR_Z900, __GI_memchr)
# endif
#endif
