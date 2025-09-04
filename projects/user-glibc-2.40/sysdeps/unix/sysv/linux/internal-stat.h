/* Internal stat definitions.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <sysdep.h>
#include <stat_t64_cp.h>
#include <kernel_stat.h>
#include <kstat_cp.h>

#if (__WORDSIZE == 32 \
     && (!defined __SYSCALL_WORDSIZE || __SYSCALL_WORDSIZE == 32)) \
     || defined STAT_HAS_TIME32 \
     || (!defined __NR_newfstatat && !defined __NR_fstatat64)
# define FSTATAT_USE_STATX 1
#else
# define FSTATAT_USE_STATX 0
#endif
