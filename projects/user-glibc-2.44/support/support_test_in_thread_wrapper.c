/* Test-in-thread wrapper function for the test driver.
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
   <https://www.gnu.org/licenses/>.

   The support_test_in_thread_wrapper function defined here gets called from
   within support_test_main, but needs to be compiled separately.  This is
   done in order to avoid linking every test (irrespective of necessity)
   against pthread_create etc., which has implications for Hurd and
   statically linked tests.  */

#include <support/test-driver.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/xthread.h>

struct test_thread_args
{
  int argc;
  char **argv;
  const struct test_config *config;
  int ret;
};

static void *
test_thread (void *closure)
{
  struct test_thread_args *args = closure;

  if (args->config->test_function != NULL)
    args->ret = args->config->test_function ();
  else if (args->config->test_function_argv != NULL)
    args->ret = args->config->test_function_argv (args->argc, args->argv);
  else
    {
      printf ("error: no test function defined\n");
      exit (1);
    }
  return NULL;
}

static pthread_barrier_t barrier;

static void *
empty_thread (void *closure)
{
  /* Make sure that the alternate thread waits (and exists) till the main
     thread finishes running the test.  */
  xpthread_barrier_wait (&barrier);

  return NULL;
}

int
support_test_in_thread_wrapper (int argc, char **argv,
                                const struct test_config *config)
{
  pthread_t thread;
  struct test_thread_args closure = {.argc = argc,
                                     .argv = argv,
                                     .config = config};

  if (config->test_in_thread == TEST_THREAD_MAIN)
    {
      xpthread_barrier_init (&barrier, NULL, 2);
      thread = xpthread_create (NULL, empty_thread, NULL);

      /* Run the test in the main thread.  */
      test_thread (&closure);

      /* Signal to the alternate thread that it can return.  */
      xpthread_barrier_wait (&barrier);
      xpthread_join (thread);
    }
  else /* config->test_in_thread == TEST_THREAD_WORKER.  */
    {
      /* Run the test in an alternate thread.  */
      thread = xpthread_create (NULL, test_thread, &closure);
      xpthread_join (thread);
    }

  return closure.ret;
}
