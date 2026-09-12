/* AArch64 MTE (Memory Tagging Extension) declarations.
   Copyright (C) 2020-2026 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#ifndef _AARCH64_MTE_H
#define _AARCH64_MTE_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

/* Assign a new (random) tag to a pointer P (does not adjust the
   allocation tag on the memory addressed).  */
static __always_inline __attribute_maybe_unused__ void *
__mte_new_tag (void *p)
{
  register void *x0 asm ("x0") = p;
  register uintptr_t x1 asm ("x1");
  /* Guarantee that the new tag is not the same as now.  */
  asm (".inst 0x9adf1401 /* gmi x1, x0, xzr */\n"
       ".inst 0x9ac11000 /* irg x0, x0, x1 */" : "+r" (x0), "=r" (x1));
  return x0;
}

/* Clears logical tag in the input pointer.  */
static __always_inline __attribute_maybe_unused__ void *
__mte_clear_tag (void *p)
{
  return (void *)((uintptr_t)p & ~(0xfull << 56ull));
}

/* Convert address P to a pointer that is tagged correctly for that
   location (logical tag in the returned pointer will be the same
   as the allocation tag in the addressed memory).  */
static __always_inline __attribute_maybe_unused__ void *
__mte_get_tag (void *p)
{
  register void *x0 asm ("x0") = p;
  asm (".inst 0xd9600000 /* ldg x0, [x0] */" : "+r" (x0));
  return x0;
}

/* Set the tags for a region of memory, which must have size and alignment
   that are multiples of MTE_GRANULE_SIZE.  Size cannot be zero.  */
void *__mte_tag_region (void *, size_t);

/* Optimized equivalent to __mte_tag_region followed by memset to 0.  */
void *__mte_tag_region_zero (void *, size_t);

#endif /* _AARCH64_MTE_H */
