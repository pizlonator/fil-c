/* Create new context.  OpenRISC version.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#include <shlib-compat.h>
#include <sysdep.h>
#include <stdarg.h>
#include <stdint.h>
#include <ucontext.h>

/* makecontext sets up a stack and the registers for the
   user context.  The stack looks like this:

		+-----------------------+
		| padding as required   |
		+-----------------------+
    sp ->       | parameters 7 to n     |
		+-----------------------+

   The registers are set up like this:
     r3-r8  : parameters 1 to 6
     r14    : uc_link
     r1     : stack pointer
     r2     : frame pointer, set to NULL
*/
static void
do_makecontext (ucontext_t *ucp, void (*startcontext) (void),
		void (*func) (void), int argc, va_list ap)
{
  unsigned long int *sp;
  int i;

  sp = (unsigned long int *)
    ((uintptr_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);

  /* Allocate stack arguments.  */
  sp -= argc < 6 ? 0 : argc - 6;

  /* Keep the stack aligned.  */
  sp = (unsigned long int *) (((uintptr_t) sp) & -4L);

  /* Keep uc_link in r14.  */
  ucp->uc_mcontext.__gprs[14] = (uintptr_t) ucp->uc_link;
  /* Return address points to function startcontext.  */
  ucp->uc_mcontext.__gprs[9] = (uintptr_t) startcontext;
  /* Frame pointer is null.  */
  ucp->uc_mcontext.__gprs[2] = (uintptr_t) 0;
  /* Restart in user-space starting at 'func'.  */
  ucp->uc_mcontext.__gprs[11] = (uintptr_t) func;
  /* Set stack pointer.  */
  ucp->uc_mcontext.__gprs[1] = (uintptr_t) sp;

  for (i = 0; i < argc; ++i)
    if (i < 6)
      ucp->uc_mcontext.__gprs[i + 3] = va_arg (ap, unsigned long int);
    else
      sp[i - 6] = va_arg (ap, unsigned long int);
}

void
__makecontext (ucontext_t *ucp, void (*func) (void), int argc, ...)
{
  extern void __startcontext (void);
  va_list ap;

  va_start (ap, argc);
  do_makecontext (ucp, &__startcontext, func, argc, ap);
  va_end (ap);
}

versioned_symbol (libc, __makecontext, makecontext, GLIBC_2_40);

#if SHLIB_COMPAT (libc, GLIBC_2_35, GLIBC_2_40)

/* Define a compat version of makecontext for glibc's before the fpcsr
   field was added to mcontext_t.  The offset sigmask changed with this
   introduction, the change was done because glibc's definition of
   ucontext_t was initially defined incompatible with the Linux
   definition of ucontext_t.  We keep the compatability definition to
   allow getcontext, setcontext and swapcontext to work in older
   binaries.  */

void
__makecontext_nofpcsr (ucontext_t *ucp, void (*func) (void), int argc, ...)
{
  extern void __startcontext_nofpcsr (void);
  va_list ap;

  va_start (ap, argc);
  do_makecontext (ucp, &__startcontext_nofpcsr, func, argc, ap);
  va_end (ap);
}

compat_symbol (libc, __makecontext_nofpcsr, makecontext, GLIBC_2_35);

#endif
