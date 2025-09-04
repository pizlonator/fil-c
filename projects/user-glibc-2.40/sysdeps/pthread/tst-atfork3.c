/* Check if pthread_atfork handler can call dlclose (BZ#24595).
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include <support/check.h>
#include <support/xthread.h>
#include <support/capture_subprocess.h>
#include <support/xdlfcn.h>

/* Check if pthread_atfork handlers do not deadlock when calling a function
   that might alter the internal fork handle list, such as dlclose.

   The test registers a callback set with pthread_atfork(), dlopen() a shared
   library (nptl/tst-atfork3mod.c), calls an exported symbol from the library
   (which in turn also registers atfork handlers), and calls fork to trigger
   the callbacks.  */

static void *handler;
static bool run_dlclose_prepare;
static bool run_dlclose_parent;
static bool run_dlclose_child;

static void
prepare (void)
{
  if (run_dlclose_prepare)
    xdlclose (handler);
}

static void
parent (void)
{
  if (run_dlclose_parent)
    xdlclose (handler);
}

static void
child (void)
{
  if (run_dlclose_child)
    xdlclose (handler);
}

static void
proc_func (void *closure)
{
}

static void
do_test_generic (bool dlclose_prepare, bool dlclose_parent, bool dlclose_child)
{
  run_dlclose_prepare = dlclose_prepare;
  run_dlclose_parent = dlclose_parent;
  run_dlclose_child = dlclose_child;

  handler = xdlopen ("tst-atfork3mod.so", RTLD_NOW);

  int (*atfork3mod_func)(void);
  atfork3mod_func = xdlsym (handler, "atfork3mod_func");

  atfork3mod_func ();

  struct support_capture_subprocess proc
    = support_capture_subprocess (proc_func, NULL);
  support_capture_subprocess_check (&proc, "tst-atfork3", 0, sc_allow_none);

  handler = atfork3mod_func = NULL;

  support_capture_subprocess_free (&proc);
}

static void *
thread_func (void *closure)
{
  return NULL;
}

static int
do_test (void)
{
  {
    /* Make the process acts as multithread.  */
    pthread_attr_t attr;
    xpthread_attr_init (&attr);
    xpthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    xpthread_create (&attr, thread_func, NULL);
  }

  TEST_COMPARE (pthread_atfork (prepare, parent, child), 0);

  do_test_generic (true  /* prepare */, false /* parent */, false /* child */);
  do_test_generic (false /* prepare */, true  /* parent */, false /* child */);
  do_test_generic (false /* prepare */, false /* parent */, true  /* child */);

  return 0;
}

#include <support/test-driver.c>
