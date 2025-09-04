/* pthread_atfork supports handlers that call pthread_atfork or dlclose.
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
   <https://www.gnu.org/licenses/>.  */

#include <support/xdlfcn.h>
#include <stdio.h>
#include <support/xthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <support/xunistd.h>
#include <support/check.h>
#include <stdlib.h>

static void *
thread_func (void *x)
{
  return NULL;
}

static unsigned int second_atfork_handler_runcount = 0;

static void
second_atfork_handler (void)
{
  second_atfork_handler_runcount++;
}

static void *h = NULL;

static unsigned int atfork_handler_runcount = 0;

static void
prepare (void)
{
  /* These atfork handlers are registered while atfork handlers are being
     executed and thus will not be executed during the corresponding
     fork.  */
  TEST_VERIFY_EXIT (pthread_atfork (second_atfork_handler,
                                    second_atfork_handler,
                                    second_atfork_handler) == 0);

  /* This will de-register the atfork handlers registered by the dlopen'd
     library and so they will not be executed.  */
  if (h != NULL)
    {
      xdlclose (h);
      h = NULL;
    }

  atfork_handler_runcount++;
}

static void
after (void)
{
  atfork_handler_runcount++;
}

static int
do_test (void)
{
  /* Make sure __libc_single_threaded is 0.  */
  pthread_attr_t attr;
  xpthread_attr_init (&attr);
  xpthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
  xpthread_create (&attr, thread_func, NULL);

  void (*reg_atfork_handlers) (void);

  h = xdlopen ("tst-atfork4mod.so", RTLD_LAZY);

  reg_atfork_handlers = xdlsym (h, "reg_atfork_handlers");

  reg_atfork_handlers ();

  /* We register our atfork handlers *after* loading the module so that our
     prepare handler is called first at fork, where we then dlclose the
     module before its prepare handler has a chance to be called.  */
  TEST_VERIFY_EXIT (pthread_atfork (prepare, after, after) == 0);

  pid_t pid = xfork ();

  /* Both the parent and the child processes should observe this.  */
  TEST_VERIFY_EXIT (atfork_handler_runcount == 2);
  TEST_VERIFY_EXIT (second_atfork_handler_runcount == 0);

  if (pid > 0)
    {
      int childstat;

      xwaitpid (-1, &childstat, 0);
      TEST_VERIFY_EXIT (WIFEXITED (childstat)
                        && WEXITSTATUS (childstat) == 0);

      /* This time, the second set of atfork handlers should also be called
         since the handlers are already in place before fork is called.  */

      pid = xfork ();

      TEST_VERIFY_EXIT (atfork_handler_runcount == 4);
      TEST_VERIFY_EXIT (second_atfork_handler_runcount == 2);

      if (pid > 0)
        {
          xwaitpid (-1, &childstat, 0);
          TEST_VERIFY_EXIT (WIFEXITED (childstat)
                            && WEXITSTATUS (childstat) == 0);
        }
    }

  return 0;
}

#include <support/test-driver.c>
