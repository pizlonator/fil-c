/* Debugger hook called after dynamic linker updates.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
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

/* This function exists solely to have a breakpoint set on it by the
   debugger.  The debugger is supposed to find this function's address by
   examining the r_brk member of struct r_debug, but GDB 4.15 in fact looks
   for this particular symbol name in the PT_INTERP file.  Therefore,
   this function must not be inlined.  */
void
_dl_debug_state (void)
{
}
rtld_hidden_def (_dl_debug_state)
