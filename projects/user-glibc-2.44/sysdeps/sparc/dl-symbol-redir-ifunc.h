/* Symbol redirection for loader/static initialization code.  SPARC version.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#ifndef _DL_IFUNC_GENERIC_H
#define _DL_IFUNC_GENERIC_H

/* The mem* IFUNCs only exist on a multi-arch sparcv9/sparc64 build.  */
#if defined (__sparc_v9__) && defined (USE_MULTIARCH)

asm ("memset = __memset_ultra1");
# ifndef SHARED
asm ("memcpy = __memcpy_ultra1");
asm ("memmove = __memmove_ultra1");
asm ("mempcpy = __mempcpy_ultra1");
asm ("__mempcpy = __mempcpy_ultra1");
# endif

#endif

#endif
