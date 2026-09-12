/* Stack executability handling for GNU dynamic linker.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
#include <dl-tunables.h>

void
_dl_handle_execstack_tunable (void)
{
  switch (TUNABLE_GET (glibc, rtld, execstack, int32_t, NULL))
    {
    case stack_tunable_mode_disable:
      if ((__glibc_unlikely (GL(dl_stack_prot_flags)) & PROT_EXEC))
	_dl_fatal_printf (
"Fatal glibc error: executable stack is not allowed\n");
      break;

    case stack_tunable_mode_force:
      if (_dl_make_stack_executable (__libc_stack_end) != 0)
	_dl_fatal_printf (
"Fatal glibc error: cannot enable executable stack as tunable requires");
      break;
    }
}
