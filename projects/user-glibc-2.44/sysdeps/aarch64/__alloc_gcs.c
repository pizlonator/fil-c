/* AArch64 GCS allocation.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.

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

#include "aarch64-gcs.h"

#include <sysdep.h>
#include <unistd.h>
#include <sys/mman.h>

#ifndef SHADOW_STACK_SET_TOKEN
# define SHADOW_STACK_SET_TOKEN (1UL << 0)
# define SHADOW_STACK_SET_MARKER (1UL << 1)
#endif

static void *
map_shadow_stack (void *addr, size_t size, unsigned long flags)
{
  return (void *) INLINE_SYSCALL_CALL (map_shadow_stack, addr, size, flags);
}

#define GCS_MAX_SIZE (1UL << 31)
#define GCS_ALTSTACK_RESERVE 160

void *
__alloc_gcs (size_t stack_size, struct gcs_record *gcs)
{
  size_t size = (stack_size / 2 + GCS_ALTSTACK_RESERVE) & -8UL;
  if (size > GCS_MAX_SIZE)
    size = GCS_MAX_SIZE;

  unsigned long flags = SHADOW_STACK_SET_MARKER | SHADOW_STACK_SET_TOKEN;
  void *base = map_shadow_stack (NULL, size, flags);
  if (base == MAP_FAILED)
    return NULL;

  uint64_t *gcsp = (uint64_t *) ((char *) base + size);
  /* Skip end of GCS token.  */
  gcsp--;
  /* Verify GCS cap token.  */
  gcsp--;
  if (((uint64_t)gcsp & 0xfffffffffffff000) + 1 != *gcsp)
    {
      __munmap (base, size);
      return NULL;
    }

  if (gcs != NULL)
    {
      gcs->gcs_base = base;
      gcs->gcs_token = gcsp;
      gcs->gcs_size = size;
    }

  /* Return the target GCS pointer for context switch.  */
  return gcsp + 1;
}
