/* Huge Page support.  Generic implementation.
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

#include <hugepages.h>

unsigned long int
__get_thp_size (void)
{
  return 0;
}

enum thp_mode_t
__get_thp_mode (void)
{
  return thp_mode_not_supported;
}

void
__get_hugepage_config (size_t requested, size_t *pagesize, int *flags)
{
  *pagesize = 0;
  *flags = 0;
}
