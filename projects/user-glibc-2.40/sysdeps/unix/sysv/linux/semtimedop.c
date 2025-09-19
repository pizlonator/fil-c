/* Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <sys/sem.h>
#include <ipc_priv.h>
#include <sysdep.h>
#include <errno.h>
#include <pizlonated_syscalls.h>

/* Perform user-defined atomic operation of array of semaphores.  */
int
__semtimedop64 (int semid, struct sembuf *sops, size_t nsops,
		const struct __timespec64 *timeout)
{
  return zsys_semtimedop (semid, sops, nsops, timeout);
}
#if __TIMESIZE != 64
libc_hidden_def (__semtimedop64)

int
__semtimedop (int semid, struct sembuf *sops, size_t nsops,
	      const struct timespec *timeout)
{
  struct __timespec64 ts64, *pts64 = NULL;
  if (timeout != NULL)
    {
      ts64 = valid_timespec_to_timespec64 (*timeout);
      pts64 = &ts64;
    }
  return __semtimedop64 (semid, sops, nsops, pts64);
}
#endif
weak_alias (__semtimedop, semtimedop)
