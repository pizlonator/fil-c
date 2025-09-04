/* PLT rewrite helper function.  Linux/x86-64 version.
   Copyright (C) 2024 Free Software Foundation, Inc.

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

#include <stdbool.h>
#include <sys/mman.h>

static __always_inline bool
dl_plt_rewrite_supported (void)
{
  /* PLT rewrite is enabled.  Check if mprotect works.  */
  void *plt = (void *) INTERNAL_SYSCALL_CALL (mmap, NULL, 4096,
					      PROT_READ | PROT_WRITE,
					      MAP_PRIVATE | MAP_ANONYMOUS,
					      -1, 0);
  if (__glibc_unlikely (plt == MAP_FAILED))
    return false;

  /* Touch the PROT_READ | PROT_WRITE page.  */
  *(int32_t *) plt = 1;

  /* If the updated PROT_READ | PROT_WRITE page can be changed to
     PROT_EXEC | PROT_READ, rewrite PLT.  */
  bool status = (INTERNAL_SYSCALL_CALL (mprotect, plt, 4096,
					PROT_EXEC | PROT_READ) == 0);

  INTERNAL_SYSCALL_CALL (munmap, plt, 4096);

  return status;
}
