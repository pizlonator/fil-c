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
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <dl-sysdep.h>
#include <dl-tls.h>
#include <tls.h>
#include <list.h>
#include <lowlevellock.h>
#include <futex-internal.h>
#include <kernel-features.h>
#include <nptl-stack.h>
#include <libc-lock.h>
#include <tls-internal.h>
#include <intprops.h>
#include <setvmaname.h>

/* Returns a usable stack for a new thread either by allocating a
   new stack or reusing a cached stack of sufficient size.
   ATTR must be non-NULL and point to a valid pthread_attr.
   PDP must be non-NULL.  */
static int
allocate_stack (const struct pthread_attr *attr, struct pthread **pdp)
{
  struct pthread *pd;

  /* Get memory for the stack.  */
  if (__glibc_unlikely (attr->flags & ATTR_FLAG_STACKADDR))
    {
      zerror("pthreadattr stack attributes not supported yet.");
      return ENOSYS;
    }
  else
    {
      pd = zgc_alloc(TLS_TCB_SIZE);
      
      /* We allocated the first block thread-specific data array.
         This address will not change for the lifetime of this
         descriptor.  */
      pd->specific[0] = pd->specific_1stblock;
      
      /* This is at least the second thread.  */
      pd->header.multiple_threads = 1;

#ifdef NEED_DL_SYSINFO
      SETUP_THREAD_SYSINFO (pd);
#endif
      
      /* Don't allow setxid until cloned.  
         FIXME: This is probably not needed. */
      pd->setxid_futex = -1;
    }

  /* Initialize the lock.  We have to do this unconditionally since the
     stillborn thread could be canceled while the lock is taken.  */
  pd->lock = LLL_LOCK_INITIALIZER;

  /* The robust mutex lists also need to be initialized
     unconditionally because the cleanup for the previous stack owner
     might have happened in the kernel.  */
  pd->robust_head.futex_offset = (offsetof (pthread_mutex_t, __data.__lock)
				  - offsetof (pthread_mutex_t,
					      __data.__list.__next));
  pd->robust_head.list_op_pending = NULL;
#if __PTHREAD_MUTEX_HAVE_PREV
  pd->robust_prev = &pd->robust_head;
#endif
  pd->robust_head.list = &pd->robust_head;

  /* We place the thread descriptor at the end of the stack.  */
  *pdp = pd;

  return 0;
}

/* Maximum supported name from initial kernel support, not exported
   by user API.  */
#define ANON_VMA_NAME_MAX_LEN 80

#define SET_STACK_NAME(__prefix, __stack, __stacksize, __tid)		\
  ({									\
     char __stack_name[sizeof (__prefix) +				\
		       INT_BUFSIZE_BOUND (unsigned int)];		\
     _Static_assert (sizeof __stack_name <= ANON_VMA_NAME_MAX_LEN,	\
		     "VMA name size larger than maximum supported");	\
     __snprintf (__stack_name, sizeof (__stack_name), __prefix "%u",	\
		 (unsigned int) __tid);					\
     __set_vma_name (__stack, __stacksize, __stack_name);		\
   })

/* Add or remove an associated name to the PD VMA stack.  */
static void
name_stack_maps (struct pthread *pd, bool set)
{
#if _STACK_GROWS_DOWN
  void *stack = pd->stackblock + pd->guardsize;
#else
  void *stack = pd->stackblock;
#endif
  size_t stacksize = pd->stackblock_size - pd->guardsize;

  if (!set)
    __set_vma_name (stack, stacksize, NULL);
  else
    {
      unsigned int tid = pd->tid;
      if (pd->user_stack)
	SET_STACK_NAME (" glibc: pthread user stack: ", stack, stacksize, tid);
      else
	SET_STACK_NAME (" glibc: pthread stack: ", stack, stacksize, tid);
    }
}
