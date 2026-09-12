/* __thread_pointer definition.  hppa version.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _SYS_THREAD_POINTER_H
#define _SYS_THREAD_POINTER_H

static inline void *
__thread_pointer (void)
{
  void *__thread_register;
  __asm__ ("mfctl %%cr27, %0" : "=r" (__thread_register));
  return __thread_register;
}

/* We write to cr27, clobber r26 as the input argument, and clobber
   r31 as the link register.  */
static inline void
__set_thread_pointer(void *__thread_pointer)
{
  asm ( "ble	0xe0(%%sr2, %%r0)\n\t"
	"copy	%0, %%r26"
	: : "r" (__thread_pointer) : "r26", "r31" );
}

#endif /* _SYS_THREAD_POINTER_H */
