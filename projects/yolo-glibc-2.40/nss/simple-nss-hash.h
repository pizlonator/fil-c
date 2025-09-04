/* __simple_nss_hash for testing nss_hash function
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

#ifndef _SIMPLE_NSS_HASH_H
#define _SIMPLE_NSS_HASH_H 1

#include <stdint.h>

/* For testing/benchmarking purposes.  Real implementation in
   nss/nss_hash.c.  */
static uint32_t
__attribute__ ((unused))
__simple_nss_hash (const void *keyarg, size_t len)
{
  const unsigned char *key;
  size_t i;
  uint32_t h = 0;
  key = keyarg;

  for (i = 0; i < len; ++i)
    h = *key++ + 65599 * h;

  return h;
}


#endif /* simple-nss-hash.h */
