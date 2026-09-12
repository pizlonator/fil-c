/* Test plural expression evaluation hardening.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>

static int
do_test (void)
{
  unsetenv ("OUTPUT_CHARSET");

  /* Use a real locale so that the active locale is not "C".  */
  TEST_VERIFY (setenv ("LC_ALL", "de_DE.UTF-8", 1) == 0);
  xsetlocale (LC_ALL, "");
  /* Set a dummy langauge to override lookup.  */
  TEST_VERIFY (setenv ("LANGUAGE", "ll", 1) == 0);

  /* Test 1: Stack overflow in plural evaluation.
     The .mo file has a plural expression with 5000 levels of nesting
     like !(1-(!(1-(...(n!=1)...)))).  Before the fix, plural_eval()
     used unbounded recursion and would crash with SIGSEGV on threads
     with small stacks.  After the fix, EVAL_MAXDEPTH=100 causes
     plural_eval_recurse() to return PE_STACKOVF, and plural_lookup()
     falls back to index 0 (the singular form).  */

  TEST_VERIFY (bindtextdomain ("plural-depth", OBJPFX "domaindir") != NULL);
  TEST_VERIFY (textdomain ("plural-depth") != NULL);

  /* ngettext must not crash.  The return value depends on whether
     the depth limit is hit (falls back to index 0) or the expression
     evaluates successfully.  Either result is acceptable.  */

  const char *tr = ngettext ("X", "Y", 42);
  TEST_VERIFY (tr != NULL);
  TEST_VERIFY (strcmp (tr, "x") == 0 || strcmp (tr, "y") == 0);

  /* Test 2: Division by zero in plural evaluation.
     The .mo file has plural expression (n!=1)+1/(n!=1729).
     For n=1729, (n!=1729) is 0, so 1/0 triggers division by zero.
     Before the fix, this raised SIGFPE.  After the fix,
     plural_eval_recurse() returns PE_INTDIV, and plural_lookup()
     falls back to index 0.  */

  TEST_VERIFY (bindtextdomain ("plural-divzero", OBJPFX "domaindir") != NULL);
  TEST_VERIFY (textdomain ("plural-divzero") != NULL);

  /* ngettext with n=1729 must not crash with SIGFPE.  */
  tr = ngettext ("X", "Y", 1729);
  TEST_VERIFY (tr != NULL);
  TEST_VERIFY (strcmp (tr, "x") == 0 || strcmp (tr, "y") == 0
	       || strcmp (tr, "z") == 0);

  return 0;
}

#include <support/test-driver.c>
