/* Stack executability handling for GNU dynamic linker.  Linux version.
   Copyright (C) 2003-2026 Free Software Foundation, Inc.
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

#include <ldsodefs.h>
#include <stackinfo.h>

int
_dl_make_stack_executable (const void *stack_endp)
{
  /* This gives us the highest/lowest page that needs to be changed.  */
  uintptr_t page = ((uintptr_t) stack_endp
		    & -(intptr_t) GLRO(dl_pagesize));

  if (__mprotect ((void *) page, GLRO(dl_pagesize),
		  PROT_READ | PROT_WRITE | PROT_EXEC
#if _STACK_GROWS_DOWN
		  | PROT_GROWSDOWN
#elif _STACK_GROWS_UP
		  | PROT_GROWSUP
#endif
		  ) != 0)
    return errno;

  /* Remember that we changed the permission.  */
  GL(dl_stack_prot_flags) |= PROT_EXEC;

  return 0;
}
