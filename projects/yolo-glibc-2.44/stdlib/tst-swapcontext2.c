/* Check setjmp/longjmp between user contexts.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <ucontext.h>
#include <stackinfo.h>

static ucontext_t uctx_main, uctx_func1, uctx_func2;
const char *str1 = "\e[31mswapcontext(&uctx_func1, &uctx_main)\e[0m";
const char *str2 = "\e[34mswapcontext(&uctx_func2, &uctx_main)\e[0m";
const char *fmt1 = "\e[31m";
const char *fmt2 = "\e[34m";
static jmp_buf jmpbuf;

#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

__attribute_optimization_barrier__
static void
func4(ucontext_t *uocp, ucontext_t *ucp, const char *str, const char *fmt)
{
  printf("      %sfunc4: %s\e[0m\n", fmt, str);
  if (swapcontext(uocp, ucp) == -1)
    handle_error("swapcontext");
  printf("      %sfunc4: returning\e[0m\n", fmt);
}

__attribute_optimization_barrier__
static void
func3(ucontext_t *uocp, ucontext_t *ucp, const char *str, const char *fmt)
{
  printf("    %sfunc3: func4(uocp, ucp, str)\e[0m\n", fmt);
  func4(uocp, ucp, str, fmt);
  printf("    %sfunc3: returning\e[0m\n", fmt);
}

__attribute_optimization_barrier__
static void
func1(void)
{
  while ( 1 )
    {
      printf("  \e[31mfunc1: func3(&uctx_func1, &uctx_main, str1)\e[0m\n");
      if (setjmp (jmpbuf) != 0)
	{
	  printf("    %sfunc1: from longjmp\e[0m\n", fmt1);
	  exit (EXIT_SUCCESS);
	}
      func3( &uctx_func1, &uctx_main, str1, fmt1);
    }
}

__attribute_optimization_barrier__
static void
func2(void)
{
  while ( 1 )
    {
      printf("  \e[34mfunc2: func3(&uctx_func2, &uctx_main, str2)\e[0m\n");
      func3(&uctx_func2, &uctx_main, str2, fmt2);
      printf("    %sfunc2: calling longjmp\e[0m\n", fmt2);
      longjmp (jmpbuf, 1);
    }
}

static int
do_test (void)
{
  /* ____longjmp_chk has  */
#if 0
#if _STACK_GROWS_DOWN
#define called_from(this, saved) ((this) < (saved))
#else
#define called_from(this, saved) ((this) > (saved))
#endif

 /* If "env" is from a frame that called us, we're all set.  */
  if (called_from(this_frame, saved_frame))
    __longjmp (env, val);
#endif
  /* Arrange stacks for uctx_func1 and uctx_func2 so that called_from
     is true when setjmp is called from uctx_func1 and longjmp is called
     from uctx_func2.  */
#if _STACK_GROWS_DOWN
# define UCTX_FUNC1_STACK	1
# define UCTX_FUNC2_STACK	0
#else
# define UCTX_FUNC1_STACK	0
# define UCTX_FUNC2_STACK	1
#endif
  char func_stack[2][16384];
  int i;

  if (getcontext(&uctx_func1) == -1)
    handle_error("getcontext");
  uctx_func1.uc_stack.ss_sp = func_stack[UCTX_FUNC1_STACK];
  uctx_func1.uc_stack.ss_size = sizeof (func_stack[UCTX_FUNC1_STACK]);
  uctx_func1.uc_link = &uctx_main;
  makecontext(&uctx_func1, func1, 0);

  if (getcontext(&uctx_func2) == -1)
    handle_error("getcontext");
  uctx_func2.uc_stack.ss_sp = func_stack[UCTX_FUNC2_STACK];
  uctx_func2.uc_stack.ss_size = sizeof (func_stack[UCTX_FUNC2_STACK]);
  uctx_func2.uc_link = &uctx_func1;
  makecontext(&uctx_func2, func2, 0);

  for ( i = 0; i < 2; i++ )
    {
      if (swapcontext(&uctx_main, &uctx_func1) == -1)
	handle_error("swapcontext");
      printf("        \e[35mmain: swapcontext(&uctx_main, &uctx_func2)\n\e[0m");
      if (swapcontext(&uctx_main, &uctx_func2) == -1)
	handle_error("swapcontext");
      printf("        \e[35mmain: swapcontext(&uctx_main, &uctx_func1)\n\e[0m");
    }

  exit(EXIT_FAILURE);
}

#include <support/test-driver.c>
