/* Copyright (C) 2004-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <mqueue.h>
#include <shlib-compat.h>
#include <rt-libc.h>

/* Add message pointed by MSG_PTR to message queue MQDES, stop blocking
   on full message queue if ABS_TIMEOUT expires.  */
int
__mq_timedsend (mqd_t mqdes, const char *msg_ptr, size_t msg_len,
	        unsigned int msg_prio, const struct timespec *abs_timeout)
{
  __set_errno (ENOSYS);
  return -1;
}
hidden_def (__mq_timedsend)
versioned_symbol (libc, __mq_timedsend, mq_timedsend, RT_IN_LIBC);
libc_hidden_ver (__mq_timedsend, mq_timedsend)
#if OTHER_SHLIB_COMPAT (librt, GLIBC_2_3_4, RT_IN_LIBC)
compat_symbol (librt, __mq_timedsend, mq_timedsend, GLIBC_2_3_4);
#endif
stub_warning (mq_timedsend)
