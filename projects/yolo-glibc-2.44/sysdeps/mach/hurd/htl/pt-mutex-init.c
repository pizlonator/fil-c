/* pthread_mutex_init.  Hurd version.
   Copyright (C) 2016-2026 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <assert.h>
#include <pt-internal.h>
#include <pthreadP.h>
#include "pt-mutex.h"
#include <hurdlock.h>
#include <shlib-compat.h>

static const pthread_mutexattr_t dfl_attr = {
  .__prioceiling = 0,
  .__protocol = PTHREAD_PRIO_NONE,
  .__pshared = PTHREAD_PROCESS_PRIVATE,
  .__mutex_type = __PTHREAD_MUTEX_TIMED
};

int
__pthread_mutex_init (pthread_mutex_t *mtxp, const pthread_mutexattr_t *attrp)
{
  ASSERT_TYPE_SIZE (pthread_mutex_t, __SIZEOF_PTHREAD_MUTEX_T);

  if (attrp == NULL)
    attrp = &dfl_attr;

  mtxp->__flags = (attrp->__pshared == PTHREAD_PROCESS_SHARED
		   ? GSYNC_SHARED : 0) | ((attrp->__prioceiling
					   & PTHREAD_MUTEX_ROBUST)
					  ? PTHREAD_MUTEX_ROBUST : 0);

  mtxp->__type = (attrp->__mutex_type
		  + (attrp->__mutex_type != __PTHREAD_MUTEX_TIMED));

  mtxp->__owner_id = 0;
  mtxp->__shpid = 0;
  mtxp->__cnt = 0;
  mtxp->__lock = 0;

  return 0;
}
libc_hidden_def (__pthread_mutex_init)
versioned_symbol (libc, __pthread_mutex_init, pthread_mutex_init, GLIBC_2_21);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_21)
compat_symbol (libc, __pthread_mutex_init, pthread_mutex_init, GLIBC_2_12);
#endif
