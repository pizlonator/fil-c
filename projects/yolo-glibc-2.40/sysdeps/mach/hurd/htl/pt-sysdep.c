/* System dependent pthreads code.  Hurd version.
   Copyright (C) 2000-2024 Free Software Foundation, Inc.
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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <mach.h>
#include <mach/mig_support.h>

#include <pt-internal.h>
#include <pthreadP.h>

static void
reset_pthread_total (void)
{
  /* Only current thread remains */
  __pthread_total = 1;
}

/* This function is called from the Hurd-specific startup code.  It
   should return a new stack pointer for the main thread.  The caller
   will switch to this new stack before doing anything serious.  */
static void
_init_routine (void *stack)
{
  struct __pthread *thread;
  int err;
  pthread_attr_t attr, *attrp = 0;

  if (GL (dl_pthread_threads) != NULL)
    /* Already initialized */
    return;

  /* Initialize the library.  */
  ___pthread_init ();

  if (stack != NULL)
    {
      /* We are given a stack, use it.  */

      /* Get the stack area information */
      vm_address_t addr = (vm_address_t) stack;
      vm_size_t vm_size;
      vm_prot_t prot, max_prot;
      vm_inherit_t inherit;
      boolean_t is_shared;
      memory_object_name_t obj;
      vm_offset_t offset;

      if (__vm_region (__mach_task_self (), &addr,
		     &vm_size, &prot, &max_prot, &inherit, &is_shared,
		     &obj, &offset) == KERN_SUCCESS)
	__mach_port_deallocate (__mach_task_self (), obj);
      else
	{
	  /* Uh.  Assume at least a page.  */
	  vm_size = __vm_page_size;
#if _STACK_GROWS_DOWN
	  addr = (vm_address_t) stack - vm_size;
#else
	  addr = (vm_address_t) stack + vm_size;
#endif
	}

      /* Avoid allocating another stack */
      attrp = &attr;
      __pthread_attr_init (attrp);
      __pthread_attr_setstack (attrp, (void *) addr, vm_size);
    }

  /* Create the pthread structure for the main thread (i.e. us).  */
  err = __pthread_create_internal (&thread, attrp, 0, 0);
  assert_perror (err);

  /* XXX The caller copies the command line arguments and the environment
     to the new stack.  Pretend it wasn't allocated so that it remains
     valid if the main thread terminates.  */
  thread->stack = 0;
#if TLS_TCB_AT_TP
  thread->tcb = THREAD_SELF;
#elif TLS_DTV_AT_TP
  /* Assuming THREAD_SELF is implemented as subtracting TLS_PRE_TCB_SIZE
     from the value of a thread pointer regsiter, this should optimize
     down to simply reading that register.  */
  thread->tcb = (tcbhead_t *) (((char *) THREAD_SELF) + TLS_PRE_TCB_SIZE);
#else
# error "Either TLS_TCB_AT_TP or TLS_DTV_AT_TP must be defined"
#endif

#ifndef PAGESIZE
  __pthread_default_attr.__guardsize = __vm_page_size;
#endif

  /* Copy over the thread-specific state */
  assert (!__pthread_init_thread.thread_specifics);
  memcpy (&thread->static_thread_specifics,
          &__pthread_init_thread.static_thread_specifics,
          sizeof (thread->static_thread_specifics));

  ___pthread_self = thread;

  /* Decrease the number of threads, to take into account that the
     signal thread (which will be created by the glibc startup code
     when we return from here) shouldn't be seen as a user thread.  */
  __pthread_total--;

  __pthread_atfork (NULL, NULL, reset_pthread_total);

  GL(dl_init_static_tls) = &__pthread_init_static_tls;

  /* Make MiG code thread aware.  */
  __mig_init (thread->stackaddr);
}

void
__pthread_initialize_minimal (void)
{
  _init_routine (__libc_stack_end);
}

#ifdef SHARED
__attribute__ ((constructor))
static void
dynamic_init_routine (void)
{
  _init_routine (__libc_stack_end);
}
#endif
