/* File tree traversal functions LFS version.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <sys/types.h>

#ifndef __OFF_T_MATCHES_OFF64_T
# include "ftw-common.c"

versioned_symbol (libc, __new_nftw, nftw, GLIBC_2_3_3);
#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_3_3)
compat_symbol (libc, __old_nftw, nftw, GLIBC_2_1);
#endif

#endif
