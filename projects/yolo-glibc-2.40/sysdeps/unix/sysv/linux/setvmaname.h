/* Utilities functions to name memory mappings.
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

#ifndef __SETVMANAME_H
#define __SETVMANAME_H

/* Set the NAME to the anonymous memory map START with size of LEN.
   It assumes well-formatted input.  */
#if IS_IN(libc) || IS_IN(rtld)
void __set_vma_name (void *start, size_t len, const char *name)
  attribute_hidden;
#else
#include <sys/prctl.h>

static inline void __set_vma_name (void *start, size_t len, const char *name)
{
  prctl (PR_SET_VMA, PR_SET_VMA_ANON_NAME, start, len, name);
}
#endif

#endif
