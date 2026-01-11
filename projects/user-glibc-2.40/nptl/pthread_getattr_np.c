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

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "pthreadP.h"
#include <lowlevellock.h>
#include <ldsodefs.h>
#include <pizlonated_runtime.h>

int
__pthread_getattr_np (pthread_t thread_id, pthread_attr_t *attr)
{
  struct pthread *thread = (struct pthread *) thread_id;

  /* Prepare the new thread attribute.  */
  int ret = __pthread_attr_init (attr);
  if (ret != 0)
    return ret;

  struct pthread_attr *iattr = (struct pthread_attr *) attr;

  lll_lock (thread->lock, LLL_PRIVATE);

  /* The thread library is responsible for keeping the values in the
     thread descriptor up-to-date in case the user changes them.  */
  memcpy (&iattr->schedparam, &thread->schedparam,
	  sizeof (struct sched_param));
  iattr->schedpolicy = thread->schedpolicy;

  /* Clear the flags work.  */
  iattr->flags = thread->flags;

  /* The thread might be detached by now.  */
  if (IS_DETACHED (thread))
    iattr->flags |= ATTR_FLAG_DETACHSTATE;

  /* This is the guardsize after adjusting it.  */
  iattr->guardsize = thread->reported_guardsize;

  /* The sizes are subject to alignment.  */
  iattr->stackaddr = zthread_stack_top(thread->zthread);
  iattr->stacksize = (char*)iattr->stackaddr - (char*)zthread_stack_limit(thread->zthread);

  iattr->flags |= ATTR_FLAG_STACKADDR;

  if (ret == 0)
    {
      size_t size = 16;
      cpu_set_t *cpuset = NULL;

      do
	{
	  size <<= 1;

	  void *newp = realloc (cpuset, size);
	  if (newp == NULL)
	    {
	      ret = ENOMEM;
	      break;
	    }
	  cpuset = (cpu_set_t *) newp;

	  ret = __pthread_getaffinity_np (thread_id, size, cpuset);
	}
      /* Pick some ridiculous upper limit.  Is 8 million CPUs enough?  */
      while (ret == EINVAL && size < 1024 * 1024);

      if (ret == 0)
	ret = __pthread_attr_setaffinity_np (attr, size, cpuset);
      else if (ret == ENOSYS)
	/* There is no such functionality.  */
	ret = 0;
      free (cpuset);
    }

  lll_unlock (thread->lock, LLL_PRIVATE);

  if (ret != 0)
    __pthread_attr_destroy (attr);

  return ret;
}
versioned_symbol (libc, __pthread_getattr_np, pthread_getattr_np, GLIBC_2_32);

#if SHLIB_COMPAT (libc, GLIBC_2_2_3, GLIBC_2_32)
strong_alias (__pthread_getattr_np, __pthread_getattr_np_alias)
compat_symbol (libc, __pthread_getattr_np_alias,
	       pthread_getattr_np, GLIBC_2_2_3);
#endif
