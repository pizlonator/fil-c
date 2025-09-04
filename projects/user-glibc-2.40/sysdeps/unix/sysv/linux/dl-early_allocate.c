/* Early memory allocation for the dynamic loader.  Generic version.
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

/* Mark symbols hidden in static PIE for early self relocation to work.  */
#if BUILD_PIE_DEFAULT
# pragma GCC visibility push(hidden)
#endif
#include <startup.h>

#include <ldsodefs.h>
#include <stddef.h>
#include <string.h>
#include <sysdep.h>
#include <unistd.h>

#include <brk_call.h>
#include <mmap_call.h>

/* Defined in brk.c.  */
extern void *__curbrk;

void *
_dl_early_allocate (size_t size)
{
  void *result;

  if (__curbrk != NULL)
    /* If the break has been initialized, brk must have run before,
       so just call it once more.  */
    {
      result = __sbrk (size);
      if (result == (void *) -1)
        result = NULL;
    }
  else
    {
      /* If brk has not been invoked, there is no need to update
         __curbrk.  The first call to brk will take care of that.  */
      void *previous = __brk_call (0);
      result = __brk_call (previous + size);
      if (result == previous)
        result = NULL;
      else
        result = previous;
    }

  /* If brk fails, fall back to mmap.  This can happen due to
     unfortunate ASLR layout decisions and kernel bugs, particularly
     for static PIE.  */
  if (result == NULL)
    {
      long int ret;
      int prot = PROT_READ | PROT_WRITE;
      int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef __NR_mmap2
      ret = MMAP_CALL_INTERNAL (mmap2, 0, size, prot, flags, -1, 0);
#else
      ret = MMAP_CALL_INTERNAL (mmap, 0, size, prot, flags, -1, 0);
#endif
      if (INTERNAL_SYSCALL_ERROR_P (ret))
        result = NULL;
      else
        result = (void *) ret;
    }

  return result;
}
