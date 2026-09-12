/* Utilities functions to name memory mappings.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

#ifndef __SETVMANAME_H
#define __SETVMANAME_H

#include <stdbool.h>

/* Set this to small value to not waste memory on systems, which do
 * not support VMA name. */
#define ANON_VMA_NAME_MAX_LEN 16

static inline bool
__is_decorate_maps_enabled (void)
{
  return false;
}

static inline
void __set_vma_name (void *start, size_t len, const char *name)
{
}

#endif
