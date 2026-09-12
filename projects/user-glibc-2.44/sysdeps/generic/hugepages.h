/* Huge page support.  Generic implementation.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#ifndef _HUGEPAGES_H
#define _HUGEPAGES_H

#include <stddef.h>

/* The THP segment load control mode.  */
enum dl_elf_thp_control_t
{
  /* To be enabled or disabled by GLIBC_TUNABLES.  */
  dl_elf_thp_control_default = 0,
  /* Enabled by GLIBC_TUNABLES=glibc.elf.thp=1.  */
  dl_elf_thp_control_enabled,
    /* Disabled by GLIBC_TUNABLES=glibc.elf.thp=0.  */
  dl_elf_thp_control_disabled
};

/* Return the default transparent huge page size.  */
unsigned long int __get_thp_size (void) attribute_hidden;

enum thp_mode_t
{
  thp_mode_unknown = 0,
  thp_mode_not_supported,
  thp_mode_always,
  thp_mode_madvise,
  thp_mode_never
};

enum thp_mode_t __get_thp_mode (void) attribute_hidden;

/* Return the supported huge page size from the REQUESTED sizes on PAGESIZE
   along with the required extra mmap flags on FLAGS,  Requesting the value
   of 0 returns the default huge page size, otherwise the value will be
   matched against the sizes supported by the system.  */
void __get_hugepage_config (size_t requested, size_t *pagesize, int *flags)
     attribute_hidden;

#ifndef MALLOC_DEFAULT_THP_PAGESIZE
# define MALLOC_DEFAULT_THP_PAGESIZE	0
#endif

#ifndef DL_MAP_DEFAULT_THP_PAGESIZE
# define DL_MAP_DEFAULT_THP_PAGESIZE	0
#endif

#ifndef MAX_THP_PAGESIZE
# define MAX_THP_PAGESIZE	(32 * 1024 * 1024)
#endif

/* If THP page size is above MAX_THP_PAGESIZE, return 0 to cap the THP
   size at MAX_THP_PAGESIZE to avoid over-aligning on systems with very
   large normal pages (like 64K pages with 512M huge pages).  */

static inline unsigned long int
get_capped_thp_size (void)
{
  unsigned long int size = __get_thp_size ();
  if (size > MAX_THP_PAGESIZE)
    size = 0;
  return size;
}

#endif /* _HUGEPAGES_H */
