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
#include <errno.h>
#include <libintl.h>
#include <list.h>
#include <pthreadP.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sysdep.h>
#include <unistd.h>
#include <stdio.h>

int
_dl_make_stack_executable (const void *stack_endp)
{
  __libc_fatal("refusing to make stack executable in _dl_make_stack_executable (Fil-C policy)\n");
  return 0;
}
