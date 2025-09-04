/* Malloc huge page support.  Generic implementation.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#ifndef _MALLOC_HUGEPAGES_H
#define _MALLOC_HUGEPAGES_H

#include <stddef.h>

/* Return the default transparent huge page size.  */
unsigned long int __malloc_default_thp_pagesize (void) attribute_hidden;

enum malloc_thp_mode_t
{
  malloc_thp_mode_always,
  malloc_thp_mode_madvise,
  malloc_thp_mode_never,
  malloc_thp_mode_not_supported
};

enum malloc_thp_mode_t __malloc_thp_mode (void) attribute_hidden;

/* Return the supported huge page size from the REQUESTED sizes on PAGESIZE
   along with the required extra mmap flags on FLAGS,  Requesting the value
   of 0 returns the default huge page size, otherwise the value will be
   matched against the sizes supported by the system.  */
void __malloc_hugepage_config (size_t requested, size_t *pagesize, int *flags)
     attribute_hidden;

#endif /* _MALLOC_HUGEPAGES_H */
