/* Test program for making nonexecutable stacks executable
   on load of a DSO that requires executable stacks.

   Copyright (C) 2003-2026 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <error.h>
#include <stackinfo.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <support/xdlfcn.h>
#include <support/xthread.h>
#include <support/check.h>
#include <support/xstdio.h>

/* The DEFAULT_RWX_STACK controls whether the toolchain enables an executable
   stack for the testcase (which does not contain features that might require
   an executable stack, such as nested function).
   Some ABIs do require an executable stack, even if the toolchain supports
   non-executable stack.  In this cases the DEFAULT_RWX_STACK can be
   overridden.  */
#ifndef DEFAULT_RWX_STACK
# define DEFAULT_RWX_STACK 0
#else
static void
deeper (void (*f) (void))
{
  char stack[1100 * 1024];
  explicit_bzero (stack, sizeof stack);
  (*f) ();
  memfrob (stack, sizeof stack);
}
#endif

#if USE_PTHREADS
# if DEFAULT_RWX_STACK
static void *
tryme_thread (void *f)
{
  (*((void (*) (void)) f)) ();

  return 0;
}
# endif

static pthread_barrier_t startup_barrier, go_barrier;
static void *
waiter_thread (void *arg)
{
  xpthread_barrier_wait (&startup_barrier);
  xpthread_barrier_wait (&go_barrier);

# if DEFAULT_RWX_STACK
  void **f = arg;
  (*((void (*) (void)) *f)) ();
# else
  abort ();
# endif

  return 0;
}
#endif

static bool allow_execstack = true;


static int
do_test (void)
{
  /* Check whether SELinux is enabled and disallows executable stacks.  */
  FILE *fp = fopen ("/selinux/enforce", "r");
  if (fp != NULL)
    {
      char *line = NULL;
      size_t linelen = 0;

      bool enabled = false;
      ssize_t n = getline (&line, &linelen, fp);
      if (n > 0 && line[0] != '0')
	enabled = true;

      fclose (fp);

      if (enabled)
	{
	  fp = fopen ("/selinux/booleans/allow_execstack", "r");
	  if (fp != NULL)
	    {
	      n = getline (&line, &linelen, fp);
	      if (n > 0 && line[0] == '0')
		allow_execstack = false;
	    }

	  fclose (fp);
	}
    }

  printf ("executable stacks %sallowed\n", allow_execstack ? "" : "not ");

#if USE_PTHREADS || DEFAULT_RWX_STACK
  static void *f;		/* Address of this is used in other threads. */
#endif

#if USE_PTHREADS
  /* Create some threads while stacks are nonexecutable.  */
  #define N 5

  xpthread_barrier_init (&startup_barrier, NULL, N + 1);
  xpthread_barrier_init (&go_barrier, NULL, N + 1);

  for (int i = 0; i < N; ++i)
    xpthread_create (NULL, &waiter_thread, &f);

  /* Make sure they are all there using their stacks.  */
  xpthread_barrier_wait (&startup_barrier);
  puts ("threads waiting");
#endif

#if USE_PTHREADS && DEFAULT_RWX_STACK
  void *old_stack_addr, *new_stack_addr;
  size_t stack_size;
  pthread_t me = pthread_self ();
  pthread_attr_t attr;
  TEST_VERIFY_EXIT (pthread_getattr_np (me, &attr) == 0);
  TEST_VERIFY_EXIT (pthread_attr_getstack (&attr, &old_stack_addr,
					    &stack_size) == 0);
# if _STACK_GROWS_DOWN
    old_stack_addr += stack_size;
# else
    old_stack_addr -= stack_size;
# endif
#endif

  /* Loading this module should force stacks to become executable.  */
#if USE_PTHREADS
  const char *soname = "tst-execstack-threads-mod.so";
#else
  const char *soname = "tst-execstack-mod.so";
#endif
  void *h = dlopen (soname, RTLD_LAZY);
#if !DEFAULT_RWX_STACK
  TEST_VERIFY_EXIT (h == NULL);
#else
  TEST_VERIFY_EXIT (h != NULL);

  f = xdlsym (h, "tryme");

  /* Test if that really made our stack executable.
     The `tryme' function should crash if not.  */

  (*((void (*) (void)) f)) ();

#if USE_PTHREADS
  TEST_VERIFY_EXIT (pthread_getattr_np (me, &attr) == 0);
  TEST_VERIFY_EXIT (pthread_attr_getstack (&attr, &new_stack_addr,
					    &stack_size) == 0);

# if _STACK_GROWS_DOWN
    new_stack_addr += stack_size;
#  else
    new_stack_addr -= stack_size;
#  endif

  /* It is possible that the dlopen'd module may have been mmapped just below
     the stack.  The stack size is taken as MIN(stack rlimit size, end of last
     vma) in pthread_getattr_np.  If rlimit is set high enough, it is possible
     that the size may have changed.  A subsequent call to
     pthread_attr_getstack returns the size and (bottom - size) as the
     stacksize and stackaddr respectively.  If the size changes due to the
     above, then both stacksize and stackaddr can change, but the stack bottom
     should remain the same, which is computed as stackaddr + stacksize.  */
  TEST_VERIFY_EXIT (old_stack_addr == new_stack_addr);
  printf ("Stack address remains the same: %p\n", old_stack_addr);
# endif

  /* Test that growing the stack region gets new executable pages too.  */
  deeper ((void (*) (void)) f);

# if USE_PTHREADS
  /* Test that a fresh thread now gets an executable stack.  */
  xpthread_create (NULL, &tryme_thread, f);

  puts ("threads go");
  /* The existing threads' stacks should have been changed.
     Let them run to test it.  */
  xpthread_barrier_wait (&go_barrier);

  pthread_exit ((void *) (long int) (! allow_execstack));
# endif
#endif

  return ! allow_execstack;
}

#include <support/test-driver.c>
