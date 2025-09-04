/* Check longjmp from user context to main context.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

/* When _FORTIFY_SOURCE is defined to 2, ____longjmp_chk is called,
   instead of longjmp.  ____longjmp_chk compares the relative stack
   values to decide if it is called from a stack frame which called
   setjmp.  If not, ____longjmp_chk assumes that an alternate signal
   stack is used.  Since comparing the relative stack values isn't
   reliable with user context, when there is no signal, ____longjmp_chk
   will fail.  Undefine _FORTIFY_SOURCE to avoid ____longjmp_chk.  */
#undef _FORTIFY_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <ucontext.h>
#include <unistd.h>

static jmp_buf jmpbuf;
static ucontext_t ctx;

static void f2 (void);

static void
__attribute__ ((noinline, noclone))
f1 (void)
{
  printf ("start f1\n");
  f2 ();
}

static void
__attribute__ ((noinline, noclone))
f2 (void)
{
  printf ("start f2\n");
  if (setcontext (&ctx) != 0)
    {
      printf ("%s: setcontext: %m\n", __FUNCTION__);
      exit (EXIT_FAILURE);
    }
}

static void
f3 (void)
{
  printf ("start f3\n");
  longjmp (jmpbuf, 1);
}

static int
__attribute__ ((noinline, noclone))
do_test_1 (void)
{
  char st1[32768];

  if (setjmp (jmpbuf) != 0)
    return 0;

  puts ("making contexts");
  if (getcontext (&ctx) != 0)
    {
      printf ("%s: getcontext: %m\n", __FUNCTION__);
      exit (EXIT_FAILURE);
    }
  ctx.uc_stack.ss_sp = st1;
  ctx.uc_stack.ss_size = sizeof st1;
  ctx.uc_link = NULL;
  makecontext (&ctx, (void (*) (void)) f3, 0);
  f1 ();
  puts ("FAIL: returned from f1 ()");
  exit (EXIT_FAILURE);
}

static int
do_test (void)
{
  return do_test_1 ();
}

#include <support/test-driver.c>
