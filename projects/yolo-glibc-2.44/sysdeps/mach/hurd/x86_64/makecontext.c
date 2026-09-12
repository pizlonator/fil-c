/* Create new context.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <sysdep.h>
#include <stdarg.h>
#include <stdint.h>
#include <ucontext.h>

#include "ucontext_i.h"

/* This implementation can handle any ARGC value but only
   normal integer parameters.
   makecontext sets up a stack and the registers for the
   user context. The stack looks like this:
               +-----------------------+
               | next context          |
               +-----------------------+
               | parameter 7-n         |
	       +-----------------------+
	       | trampoline address    |
    %rsp ->    +-----------------------+

   The registers are set up like this:
     %rdi,%rsi,%rdx,%rcx,%r8,%r9: parameter 1 to 6
     %rbx   : address of next context
     %rsp   : stack pointer.
*/

/* XXX: This implementation currently only handles integer arguments.
   To handle long int and pointer arguments the va_arg arguments needs
   to be changed to long and also the stdlib/tst-setcontext.c file needs
   to be changed to pass long arguments to makecontext.  */


void
__makecontext (ucontext_t *ucp, void (*func) (void), int argc, ...)
{
  extern void __start_context (void) attribute_hidden;
  greg_t *sp;
  unsigned int idx_uc_link;
  va_list ap;
  int i;

  /* Generate room on stack for parameter if needed and uc_link.  */
  sp = (greg_t *) ((uintptr_t) ucp->uc_stack.ss_sp
		   + ucp->uc_stack.ss_size);
  sp -= (argc > 6 ? argc - 6 : 0) + 1;
  /* Align stack and make space for trampoline address.  */
  sp = (greg_t *) ((((uintptr_t) sp) & -16L) - 8);

  idx_uc_link = (argc > 6 ? argc - 6 : 0) + 1;

  /* Setup context ucp.  */
  /* Address to jump to.  */
  ucp->uc_mcontext.gregs[REG_RIP] = (uintptr_t) func;
  /* Setup rbx.*/
  ucp->uc_mcontext.gregs[REG_RBX] = (uintptr_t) &sp[idx_uc_link];
  ucp->uc_mcontext.gregs[REG_RSP] = (uintptr_t) sp;

  /* Setup stack.  */
  sp[0] = (uintptr_t) &__start_context;
  sp[idx_uc_link] = (uintptr_t) ucp->uc_link;

  va_start (ap, argc);
  /* Handle arguments.

     The standard says the parameters must all be int values.  This is
     an historic accident and would be done differently today.  For
     x86-64 all integer values are passed as 64-bit values and
     therefore extending the API to copy 64-bit values instead of
     32-bit ints makes sense.  It does not break existing
     functionality and it does not violate the standard which says
     that passing non-int values means undefined behavior.  */
  for (i = 0; i < argc; ++i)
    switch (i)
      {
      case 0:
	ucp->uc_mcontext.gregs[REG_RDI] = va_arg (ap, greg_t);
	break;
      case 1:
	ucp->uc_mcontext.gregs[REG_RSI] = va_arg (ap, greg_t);
	break;
      case 2:
	ucp->uc_mcontext.gregs[REG_RDX] = va_arg (ap, greg_t);
	break;
      case 3:
	ucp->uc_mcontext.gregs[REG_RCX] = va_arg (ap, greg_t);
	break;
      case 4:
	ucp->uc_mcontext.gregs[REG_R8] = va_arg (ap, greg_t);
	break;
      case 5:
	ucp->uc_mcontext.gregs[REG_R9] = va_arg (ap, greg_t);
	break;
      default:
	/* Put value on stack.  */
	sp[i - 5] = va_arg (ap, greg_t);
	break;
      }
  va_end (ap);
}


weak_alias (__makecontext, makecontext)
