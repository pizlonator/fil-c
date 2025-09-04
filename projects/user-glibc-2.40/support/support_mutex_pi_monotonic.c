/* Returns whether Priority Inheritance support CLOCK_MONOTONIC.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <support/xthread.h>
#include <sys/syscall.h>
#include <unistd.h>

bool
support_mutex_pi_monotonic (void)
{
#ifdef __linux__
# ifndef __NR_futex
#  define __NR_futex __NR_futex_time64
# endif
  int r = syscall (__NR_futex, &(unsigned int){0}, 13 /* FUTEX_LOCK_PI2  */,
		   0, NULL);
  return r == 0;
#else
  return false;
#endif
}
