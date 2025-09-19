/* Copyright (C) 2002-2024 Free Software Foundation, Inc.
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

#include <signal.h>
#include <pthreadP.h>
#include <sysdep.h>
#include <shlib-compat.h>
#include <pizlonated_syscalls.h>

int
__pthread_sigmask (int how, const sigset_t *newmask, sigset_t *oldmask)
{
  return ((zsys_sigprocmask (how, newmask, oldmask) < 0) ? errno : 0);
}
libc_hidden_def (__pthread_sigmask)

versioned_symbol (libc, __pthread_sigmask, pthread_sigmask, GLIBC_2_32);
#if SHLIB_COMPAT (libc, GLIBC_2_0, GLIBC_2_32)
strong_alias (__pthread_sigmask, __pthread_sigmask_2);
compat_symbol (libc, __pthread_sigmask_2, pthread_sigmask, GLIBC_2_0);
#endif
