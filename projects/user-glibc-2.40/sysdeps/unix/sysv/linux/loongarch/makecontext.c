/* Create new context.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <sys/asm.h>
#include <sys/ucontext.h>
#include <stdarg.h>
#include <assert.h>

void
__makecontext (ucontext_t *ucp, void (*func) (void), int argc, long int a0,
	       long int a1, long int a2, long int a3, long int a4, ...)
{
  extern void __start_context (void) attribute_hidden;
  unsigned long int *sp;

  _Static_assert(LARCH_REG_NARGS == 8,
		 "__makecontext assumes 8 argument registers");

  /* Set up the stack.  */
  sp = (unsigned long int *)
       (((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size) & ALMASK);

  /* Set up the register context.
     ra = s0 = 0, terminating the stack for backtracing purposes.
     s1 = the function we must call.
     s2 = the subsequent context to run.  */
  ucp->uc_mcontext.__gregs[LARCH_REG_RA] = (uintptr_t) 0;
  ucp->uc_mcontext.__gregs[LARCH_REG_S0] = (uintptr_t) 0;
  ucp->uc_mcontext.__gregs[LARCH_REG_S1] = (uintptr_t) func;
  ucp->uc_mcontext.__gregs[LARCH_REG_S2] = (uintptr_t) ucp->uc_link;
  ucp->uc_mcontext.__gregs[LARCH_REG_SP] = (uintptr_t) sp;
  ucp->uc_mcontext.__pc = (uintptr_t) &__start_context;

  /* Put args in a0-a7, then put any remaining args on the stack.  */
  ucp->uc_mcontext.__gregs[LARCH_REG_A0 + 0] = (uintptr_t) a0;
  ucp->uc_mcontext.__gregs[LARCH_REG_A0 + 1] = (uintptr_t) a1;
  ucp->uc_mcontext.__gregs[LARCH_REG_A0 + 2] = (uintptr_t) a2;
  ucp->uc_mcontext.__gregs[LARCH_REG_A0 + 3] = (uintptr_t) a3;
  ucp->uc_mcontext.__gregs[LARCH_REG_A0 + 4] = (uintptr_t) a4;

  if (__glibc_unlikely (argc > 5))
    {
      va_list vl;
      va_start (vl, a4);

      long int reg_args = argc < LARCH_REG_NARGS ? argc : LARCH_REG_NARGS;
      for (long int i = 5; i < reg_args; i++)
	ucp->uc_mcontext.__gregs[LARCH_REG_A0 + i] = va_arg (vl, unsigned long int);

      long int stack_args = argc - reg_args;
      if (stack_args > 0)
	{
	  sp = (unsigned long int *)
	       (((uintptr_t) sp - stack_args * sizeof (long int)) & ALMASK);
	  ucp->uc_mcontext.__gregs[LARCH_REG_SP] = (uintptr_t) sp;
	  for (long int i = 0; i < stack_args; i++)
	    sp[i] = va_arg (vl, unsigned long int);
	}

      va_end (vl);
    }
}

weak_alias (__makecontext, makecontext)
