/* Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

#include <setjmp.h>
#include <stddef.h>
#include <signal.h>
#include <pizlonated_runtime.h>

#pragma clang diagnostic ignored "-Winvalid-noreturn"

void __libc_siglongjmp (sigjmp_buf env, int val)
{
  zlongjmp (*(zjmp_buf**)env, val);
}

#ifndef __libc_siglongjmp
# ifndef __libc_longjmp
/* __libc_longjmp is a private interface for cancellation implementation
   in libpthread.  */
strong_alias (__libc_siglongjmp, __libc_longjmp)
# endif
weak_alias (__libc_siglongjmp, _longjmp)
weak_alias (__libc_siglongjmp, longjmp)
weak_alias (__libc_siglongjmp, siglongjmp)
weak_alias (__libc_siglongjmp, __longjmp_chk)
#endif
