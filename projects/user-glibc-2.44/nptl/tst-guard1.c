/* Basic tests for pthread guard area.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
#include <pthreaddef.h>
#include <setjmp.h>
#include <stackinfo.h>
#include <stdio.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/test-driver.h>
#include <support/xsignal.h>
#include <support/xthread.h>
#include <support/xunistd.h>
#include <support/check_mem_access.h>
#include <sys/mman.h>
#include <stdlib.h>

static long int pagesz;

static bool
try_read_buf (char *ptr)
{
  return check_mem_access (ptr, false);
}

static bool
try_write_buf (char *ptr)
{
  return check_mem_access (ptr, true);
}

static bool
try_read_write_buf (char *ptr)
{
  return try_read_buf (ptr) && try_write_buf(ptr);
}


/* Return the guard region of the current thread (it only makes sense on
   a thread created by pthread_created).  */

struct stack_t
{
  char *stack;
  size_t stacksize;
  char *guard;
  size_t guardsize;
};

static inline size_t
adjust_stacksize (size_t stacksize)
{
  /* For some ABIs, The guard page depends of the thread descriptor, which in
     turn rely  on the require static TLS.  The only supported _STACK_GROWS_UP
     ABI, hppa, defines TLS_DTV_AT_TP and it is not straightforward to
     calculate the guard region with current pthread APIs.  So to get a
     correct stack size assumes an extra page after the guard area.  */
#if _STACK_GROWS_DOWN
  return stacksize;
#elif _STACK_GROWS_UP
  return stacksize - pagesz;
#endif
}

struct stack_t
get_current_stack_info (void)
{
  pthread_attr_t attr;
  TEST_VERIFY_EXIT (pthread_getattr_np (pthread_self (), &attr) == 0);
  void *stack;
  size_t stacksize;
  TEST_VERIFY_EXIT (pthread_attr_getstack (&attr, &stack, &stacksize) == 0);
  size_t guardsize;
  TEST_VERIFY_EXIT (pthread_attr_getguardsize (&attr, &guardsize) == 0);
  /* The guardsize is reported as the current page size, although it might
     be adjusted to a larger value (aarch64 for instance).  */
  if (guardsize != 0 && guardsize < ARCH_MIN_GUARD_SIZE)
    guardsize = ARCH_MIN_GUARD_SIZE;

#if _STACK_GROWS_DOWN
  void *guard = guardsize ? stack - guardsize : 0;
#elif _STACK_GROWS_UP
  stacksize = adjust_stacksize (stacksize);
  void *guard = guardsize ? stack + stacksize  : 0;
#endif

  pthread_attr_destroy (&attr);

  return (struct stack_t) { stack, stacksize, guard, guardsize };
}

struct thread_args_t
{
  size_t stacksize;
  size_t guardsize;
};

struct thread_args_t
get_thread_args (const pthread_attr_t *attr)
{
  size_t stacksize;
  size_t guardsize;

  TEST_COMPARE (pthread_attr_getstacksize (attr, &stacksize), 0);
  TEST_COMPARE (pthread_attr_getguardsize (attr, &guardsize), 0);
  if (guardsize < ARCH_MIN_GUARD_SIZE)
    guardsize = ARCH_MIN_GUARD_SIZE;

  return (struct thread_args_t) { stacksize, guardsize };
}

static void
set_thread_args (pthread_attr_t *attr, const struct thread_args_t *args)
{
  xpthread_attr_setstacksize (attr, args->stacksize);
  xpthread_attr_setguardsize (attr, args->guardsize);
}

static void *
tf (void *closure)
{
  struct thread_args_t *args = closure;

  struct stack_t s = get_current_stack_info ();
  if (test_verbose)
    printf ("debug: [tid=%jd] stack = { .stack=%p, stacksize=%#zx, guard=%p, "
	    "guardsize=%#zx }\n",
	    (intmax_t) gettid (),
	    s.stack,
	    s.stacksize,
	    s.guard,
	    s.guardsize);

  if (args != NULL)
    {
      TEST_COMPARE (adjust_stacksize (args->stacksize), s.stacksize);
      TEST_COMPARE (args->guardsize, s.guardsize);
    }

  /* Ensure we can access the stack area.  */
  TEST_COMPARE (try_read_buf (s.stack), true);
  TEST_COMPARE (try_read_buf (&s.stack[s.stacksize / 2]), true);
  TEST_COMPARE (try_read_buf (&s.stack[s.stacksize - 1]), true);

  /* Check if accessing the guard area results in SIGSEGV.  */
  if (s.guardsize > 0)
    {
      TEST_COMPARE (try_read_write_buf (s.guard), false);
      TEST_COMPARE (try_read_write_buf (&s.guard[s.guardsize / 2]), false);
      TEST_COMPARE (try_read_write_buf (&s.guard[s.guardsize] - 1), false);
    }

  return NULL;
}

/* Test 1: caller provided stack without guard.  */
static void
do_test1 (void *closure)
{
  pthread_attr_t attr;
  xpthread_attr_init (&attr);

  size_t stacksize = support_small_thread_stack_size (false);
  void *stack = xmmap (0,
		       stacksize,
		       PROT_READ | PROT_WRITE,
		       MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK,
		       -1);
  xpthread_attr_setstack (&attr, stack, stacksize);
  xpthread_attr_setguardsize (&attr, 0);

  struct thread_args_t args = { stacksize, 0 };
  pthread_t t = xpthread_create (&attr, tf, &args);
  void *status = xpthread_join (t);
  TEST_VERIFY (status == 0);

  xpthread_attr_destroy (&attr);
  xmunmap (stack, stacksize);
}

/* Test 2: same as 1., but with a guard area.  */
static void
do_test2 (void *closure)
{
  pthread_attr_t attr;
  xpthread_attr_init (&attr);

  size_t stacksize = support_small_thread_stack_size (false);
  void *stack = xmmap (0,
		       stacksize,
		       PROT_READ | PROT_WRITE,
		       MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK,
		       -1);
  xpthread_attr_setstack (&attr, stack, stacksize);
  xpthread_attr_setguardsize (&attr, pagesz);

  struct thread_args_t args = { stacksize, 0 };
  pthread_t t = xpthread_create (&attr, tf, &args);
  void *status = xpthread_join (t);
  TEST_VERIFY (status == 0);

  xpthread_attr_destroy (&attr);
  xmunmap (stack, stacksize);
}

/* Test 3: pthread_create without a guard area.  */
static void
do_test3 (void *closure)
{
  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  struct thread_args_t args = get_thread_args (&attr);
  args.stacksize += args.guardsize;
  args.guardsize = 0;
  set_thread_args (&attr, &args);

  pthread_t t = xpthread_create (&attr, tf, &args);
  void *status = xpthread_join (t);
  TEST_VERIFY (status == 0);

  xpthread_attr_destroy (&attr);
}

/* Test 4: pthread_create with default values.  */
static void
do_test4 (void *closure)
{
  pthread_t t = xpthread_create (NULL, tf, NULL);
  void *status = xpthread_join (t);
  TEST_VERIFY (status == 0);
}

/* Test 5: pthread_create with non default stack and guard size value.  */
static void
do_test5 (void *closure)
{
  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  struct thread_args_t args = get_thread_args (&attr);
  args.guardsize += pagesz;
  args.stacksize += pagesz;
  set_thread_args (&attr, &args);

  pthread_t t = xpthread_create (&attr, tf, &args);
  void *status = xpthread_join (t);
  TEST_VERIFY (status == 0);

  xpthread_attr_destroy (&attr);
}

/* Test 6: thread with the required size (stack + guard) that matches the
   test 3, but with a larger guard area.  The pthread_create will need to
   increase the guard area.  */
static void
do_test6 (void *closure)
{
  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  struct thread_args_t args = get_thread_args (&attr);
  args.guardsize += pagesz;
  args.stacksize -= pagesz;
  set_thread_args (&attr, &args);

  pthread_t t = xpthread_create (&attr, tf, &args);
  void *status = xpthread_join (t);
  TEST_VERIFY (status == 0);

  xpthread_attr_destroy (&attr);
}

/* Test 7: pthread_create with default values, the requires size matches the
   one from test 3 and 6 (but with a reduced guard ares).  The
   pthread_create should use the cached stack from previous tests, but it
   would require to reduce the guard area.  */
static void
do_test7 (void *closure)
{
  pthread_t t = xpthread_create (NULL, tf, NULL);
  void *status = xpthread_join (t);
  TEST_VERIFY (status == 0);
}

static int
do_test (void)
{
  pagesz = sysconf (_SC_PAGESIZE);

  static const struct {
    const char *descr;
    void (*test) (void *);
  } tests[] = {
    { "user provided stack without guard", do_test1 },
    { "user provided stack with guard",    do_test2 },
    /* N.B: do_test3 should be before do_test4 to check if a new thread
       that uses the thread stack previously allocated without a guard
       page correctly sets up the guard pages even on a kernel without
       MADV_GUARD_INSTALL support (BZ 33356).  */
    { "default attribute without guard",   do_test3 },
    { "default attribute",                 do_test4 },
    /* Also checks if the guard is correctly removed from the cache thread
       stack.  */
    { "default attribute without guard",   do_test3 },
    { "non default stack and guard sizes", do_test5 },
    { "reused stack with larger guard",    do_test6 },
    { "reused stack with smaller guard",   do_test7 },
  };

  /* Run each test with a clean state.  */
  for (int i = 0; i < array_length (tests); i++)
    {
      printf ("debug: fork: test%01d: %s\n", i, tests[i].descr);
      struct support_capture_subprocess result =
	support_capture_subprocess (tests[i].test, NULL);
      support_capture_subprocess_check (&result, tests[i].descr, 0,
					sc_allow_none);
      support_capture_subprocess_free (&result);
    }

  /* And now run the same tests along with the thread stack cache.  */
  for (int i = 0; i < array_length (tests); i++)
    {
      printf ("debug: test%01d: %s\n", i, tests[i].descr);
      tests[i].test ( NULL);
    }

  return 0;
}

#include <support/test-driver.c>
