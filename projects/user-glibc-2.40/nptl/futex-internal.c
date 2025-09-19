/* futex helper functions for glibc-internal use.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <sysdep.h>
#include <time.h>
#include <futex-internal.h>
#include <kernel-features.h>

int
__futex_abstimed_wait64 (unsigned int* futex_word, unsigned int expected,
                         clockid_t clockid,
                         const struct __timespec64* abstime, int private)
{
  return zsys_futex_timedwait ((volatile int *) futex_word, expected, clockid,
                               abstime, __lll_zsys_private_arg (private));
}
libc_hidden_def (__futex_abstimed_wait64)

int
__futex_abstimed_wait_cancelable64 (unsigned int* futex_word,
                                    unsigned int expected, clockid_t clockid,
                                    const struct __timespec64* abstime,
                                    int private)
{
  return __futex_abstimed_wait64 (futex_word, expected, clockid, abstime,
                                  private);
}
libc_hidden_def (__futex_abstimed_wait_cancelable64)

int
__futex_lock_pi64 (int *futex_word, clockid_t clockid,
		   const struct __timespec64 *abstime, int private)
{
  ZASSERT(clockid == CLOCK_REALTIME); /* FIXME: It would be easy to support monotonic times. */
  return zsys_futex_lock_pi ((volatile int *) futex_word,
                             __lll_zsys_private_arg (private), abstime);
}
