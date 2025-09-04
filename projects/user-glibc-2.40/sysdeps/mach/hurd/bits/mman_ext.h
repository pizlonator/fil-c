/* System-specific extensions of <sys/mman.h>, Hurd version.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#ifndef _SYS_MMAN_H
# error "Never include <bits/mman_ext.h> directly; use <sys/mman.h> instead."
#endif

#ifdef __USE_GNU
# define SHM_ANON	((const char *) 1)

# define MAP_32BIT	0x1000	/* Map in the lower 2 GB.  */
# define MAP_EXCL	0x4000	/* With MAP_FIXED, don't replace existing mappings.  */

# define MAP_TRYFIXED		(MAP_FIXED | MAP_EXCL)	/* BSD name.  */
# define MAP_FIXED_NOREPLACE	(MAP_FIXED | MAP_EXCL)	/* Linux name.  */
#endif /* __USE_GNU  */
