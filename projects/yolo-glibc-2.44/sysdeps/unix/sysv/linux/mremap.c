/* Remap a virtual memory address.  Linux specific syscall.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <sys/mman.h>
#include <sysdep.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>

#define MREMAP_KNOWN_BITS \
  (MREMAP_MAYMOVE \
   | MREMAP_FIXED \
   | MREMAP_DONTUNMAP)

void *
__mremap (void *addr, size_t old_len, size_t new_len, int flags, ...)
{
  va_list va;
  void *new_addr = NULL;

  if (flags & ~(MREMAP_KNOWN_BITS))
    {
      __set_errno (EINVAL);
      return MAP_FAILED;
    }

  if (flags & (MREMAP_FIXED | MREMAP_DONTUNMAP))
    {
      va_start (va, flags);
      new_addr = va_arg (va, void *);
      va_end (va);
    }

  return (void *) INLINE_SYSCALL_CALL (mremap, addr, old_len, new_len, flags,
				       new_addr);
}
libc_hidden_def (__mremap)
weak_alias (__mremap, mremap)
