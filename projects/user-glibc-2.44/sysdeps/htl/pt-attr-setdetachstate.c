/* pthread_attr_setdetachstate.  Generic version.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library;  if not, see
   <https://www.gnu.org/licenses/>.  */

#include <pthread.h>
#include <shlib-compat.h>
#include <pt-internal.h>

int
__pthread_attr_setdetachstate (pthread_attr_t *attr, int detachstate)
{
  switch (detachstate)
    {
    case PTHREAD_CREATE_DETACHED:
    case PTHREAD_CREATE_JOINABLE:
      attr->__detachstate = detachstate;
      break;
    default:
      return EINVAL;
    }

  return 0;
}

versioned_symbol (libc, __pthread_attr_setdetachstate, pthread_attr_setdetachstate, GLIBC_2_21);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_21)
compat_symbol (libc, __pthread_attr_setdetachstate, pthread_attr_setdetachstate, GLIBC_2_12);
#endif
