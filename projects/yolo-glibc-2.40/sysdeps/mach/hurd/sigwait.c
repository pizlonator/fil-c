/* Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <hurd.h>

/* Select any of pending signals from SET or wait for any to arrive.  */
int
__sigwait (const sigset_t *set, int *sig)
{
  int ret;

  ret = __sigtimedwait (set, NULL, NULL);

  if (ret < 0)
    return -1;

  if (!ret)
    return __hurd_fail(EAGAIN);

  *sig = ret;
  return 0;
}
libc_hidden_def (__sigwait)
weak_alias (__sigwait, sigwait)
