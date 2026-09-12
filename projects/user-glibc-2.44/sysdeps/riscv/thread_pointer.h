/* __thread_pointer definition.  riscv version.
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

#include <features.h>

#if __GNUC_PREREQ (10, 3) || __glibc_clang_prereq (11, 0)
static inline void *
__thread_pointer (void)
{
  return __builtin_thread_pointer ();
}
#else
static inline void *
__thread_pointer (void)
{
  void *__thread_register;
  __asm__ ("mv %0, tp" : "=r" (__thread_register));
  return __thread_register;
}
#endif

#endif /* _SYS_THREAD_POINTER_H */
