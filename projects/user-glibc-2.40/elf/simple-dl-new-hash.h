/* __simple_dl_new_hash for testing true elf symbol lookup.
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

#ifndef _SIMPLE_DL_NEW_HASH_H
#define _SIMPLE_DL_NEW_HASH_H 1

#include <stdint.h>

/* For testing/benchmarking purposes.  Real implementation in
   sysdeps/generic/dl-new-hash.h.  */
static uint32_t
__attribute__ ((unused))
__simple_dl_new_hash (const char *s)
{
  uint32_t h = 5381;
  for (unsigned char c = *s; c != '\0'; c = *++s)
    h = h * 33 + c;
  return h;
}

#endif /* simple-dl-new-hash.h */
