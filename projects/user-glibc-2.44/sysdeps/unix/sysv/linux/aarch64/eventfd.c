/* An eventfd implementation for AArch64.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <sys/eventfd.h>
#include <sysdep.h>
#include <pizlonated_syscalls.h>

libc_hidden_proto (eventfd)

/* AArch64 does not have the legacy eventfd syscall, and the generic
   syscalls.list entry would otherwise turn eventfd into an ENOSYS stub
   while sysdeps/unix/sysv/linux/Makefile still expects eventfd.o to be
   built.  Implement eventfd in terms of the Fil-C runtime's zsys_eventfd
   (which the runtime implements with the host libc's eventfd, itself
   based on the eventfd2 syscall).  */

int
eventfd (unsigned int count, int flags)
{
  return zsys_eventfd (count, flags);
}
libc_hidden_def (eventfd)
