/* File tree walker functions.  LFS version.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
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

#define FTW_NAME __ftw64
#define NFTW_NAME __nftw64
#define NFTW_OLD_NAME __old_nftw64
#define NFTW_NEW_NAME __new_nftw64
#define INO_T ino64_t
#define STRUCT_STAT stat64
#define LSTAT __lstat64
#define STAT __stat64
#define FSTATAT __fstatat64
#define FTW_FUNC_T __ftw64_func_t
#define NFTW_FUNC_T __nftw64_func_t

#define ftw __rename_ftw
#define nftw __rename_nftw

#include <shlib-compat.h>
#include "ftw-common.c"

#undef ftw
#undef nftw

weak_alias (__ftw64, ftw64)
versioned_symbol (libc, __new_nftw64, nftw64, GLIBC_2_3_3);

#if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_3_3)
compat_symbol (libc, __old_nftw64, nftw64, GLIBC_2_1);
#endif

#ifdef __OFF_T_MATCHES_OFF64_T
weak_alias (__ftw64, ftw)
versioned_symbol (libc, __new_nftw64, nftw, GLIBC_2_3_3);
# if SHLIB_COMPAT(libc, GLIBC_2_1, GLIBC_2_3_3)
compat_symbol (libc, __old_nftw64, nftw, GLIBC_2_1);
# endif
#endif
