/* Test deallocation of the environ pointer.
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

/* This test is not in the scope for POSIX or any other standard, but
   some applications assume that environ is a heap-allocated pointer
   after a call to setenv on an empty environment.  They also try to
   save and restore environ in an attempt to undo a temporary
   modification of the environment array, but this does not work if
   setenv was called before.

   Before including this file, these macros need to be defined
   to 0 or 1:

   DO_EARLY_SETENV  If 1, perform a setenv call before changing environ.
   DO_MALLOC        If 1, use a heap pointer for the empty environment.

   Note that this test will produce errors under valgrind and other
   memory tracers that call __libc_freeres because free (environ)
   deallocates a pointer still used internally.  */

#include <stdlib.h>
#include <unistd.h>
#include <support/check.h>
#include <support/support.h>

static void
check_rewritten (void)
{
  TEST_COMPARE_STRING (environ[0], "tst_environ_change_a=1");
  TEST_COMPARE_STRING (environ[1], "tst_environ_change_b=2");
  TEST_COMPARE_STRING (environ[2], NULL);
  TEST_COMPARE_STRING (getenv ("tst_environ_change_a"), "1");
  TEST_COMPARE_STRING (getenv ("tst_environ_change_b"), "2");
  TEST_COMPARE_STRING (getenv ("tst_environ_change_early"), NULL);
  TEST_COMPARE_STRING (getenv ("PATH"), NULL);
}

static int
do_test (void)
{
  TEST_COMPARE_STRING (getenv ("tst_environ_change_a"), NULL);
  TEST_COMPARE_STRING (getenv ("tst_environ_change_b"), NULL);
  TEST_COMPARE_STRING (getenv ("tst_environ_change_early_setenv"), NULL);
#if DO_EARLY_SETENV
  TEST_COMPARE (setenv ("tst_environ_change_early_setenv", "1", 1), 0);
#else
  /* Must come back after environ reset.  */
  char *original_path = xstrdup (getenv ("PATH"));
#endif

  char **save_environ = environ;
#if DO_MALLOC
  environ = xmalloc (sizeof (*environ));
#else
  char *environ_array[1];
  environ = environ_array;
#endif
  *environ = NULL;
  TEST_COMPARE (setenv ("tst_environ_change_a", "1", 1), 0);
  TEST_COMPARE (setenv ("tst_environ_change_b", "2", 1), 0);
#if !DO_EARLY_SETENV
  /* Early setenv results in reuse of the heap-allocated environ array
     that does not change as more pointers are added to it.  */
  TEST_VERIFY (environ != save_environ);
#endif
  check_rewritten ();

  bool check_environ = true;
#if DO_MALLOC
  /* Disable further checks if the free call clobbers the environ
     contents.  Whether that is the case depends on the internal
     setenv allocation policy and the heap layout.  */
  check_environ = environ != save_environ;
  /* Invalid: Causes internal use-after-free condition.  Yet this has
     to be supported for compatibility with some applications. */
  free (environ);
#endif

  environ = save_environ;

#if DO_EARLY_SETENV
  /* With an early setenv, the internal environ array was overwritten.
     Historically, this triggered a use-after-free problem because of
     the use of realloc internally in setenv, but it may appear as if
     the original environment had been restored.  In the current code,
     we can only support this if the free (environ) above call did not
     clobber the array, otherwise getenv will see invalid pointers.
     Due to the use-after-free, invalid pointers could be seen with
     the old implementation as well, but the triggering conditions
     were different.  */
  if (check_environ)
    check_rewritten ();
#else
  TEST_VERIFY (check_environ);
  TEST_COMPARE_STRING (getenv ("PATH"), original_path);
  TEST_COMPARE_STRING (getenv ("tst_environ_change_a"), NULL);
  TEST_COMPARE_STRING (getenv ("tst_environ_change_b"), NULL);
#endif

  return 0;
}

#include <support/test-driver.c>
