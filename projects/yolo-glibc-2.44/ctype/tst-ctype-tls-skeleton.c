/* Test that <ctype.h> in a secondary namespace works.
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

/* Before this file is included, define DO_STATIC_TEST to 0 or 1.
   With 0, dlmopen is used for the test.  With 1, dlopen is used.  */

#include <stddef.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xdlfcn.h>
#include <support/xthread.h>

static int (*my_isalpha) (int);
static int (*my_toupper) (int);
static int (*my_tolower) (int);

static void *
checks (void *ignore)
{
  TEST_VERIFY (my_isalpha ('a'));
  TEST_VERIFY (!my_isalpha ('0'));
  TEST_COMPARE (my_toupper ('a'), 'A');
  TEST_COMPARE (my_toupper ('A'), 'A');
  TEST_COMPARE (my_tolower ('a'), 'a');
  TEST_COMPARE (my_tolower ('A'), 'a');
  return NULL;
}

static int
do_test (void)
{
  char *dso = xasprintf ("%s/ctype/tst-ctype-tls-mod.so", support_objdir_root);
#if DO_STATIC_TEST
  void *handle = xdlopen (dso, RTLD_LAZY);
#else
  void *handle = xdlmopen (LM_ID_NEWLM, dso, RTLD_LAZY);
#endif
  my_isalpha = xdlsym (handle, "my_isalpha");
  my_toupper = xdlsym (handle, "my_toupper");
  my_tolower = xdlsym (handle, "my_tolower");

  checks (NULL);
  xpthread_join (xpthread_create (NULL, checks, NULL));

  xdlclose (handle);
  free (dso);

  return 0;
}

#include <support/test-driver.c>
