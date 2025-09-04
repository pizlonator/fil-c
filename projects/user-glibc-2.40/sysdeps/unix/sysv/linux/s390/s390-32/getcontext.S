/* Copyright (C) 2001-2024 Free Software Foundation, Inc.
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
#include <features.h>

#include "ucontext_i.h"

#include <shlib-compat.h>

/*  __getcontext (const ucontext_t *ucp)

  Saves the machine context in UCP such that when it is activated,
  it appears as if __getcontext() returned again.

  This implementation is intended to be used for *synchronous* context
  switches only.  Therefore, it does not have to save anything
  other than the PRESERVED state.  */

ENTRY(__getcontext)
	lr	%r0,%r2

	/* rt_sigprocmask (SIG_BLOCK, NULL, &sc->sc_mask, sigsetsize).  */
	la	%r4,SC_MASK(%r2)
	la      %r2,SIG_BLOCK
	slr	%r3,%r3
	lhi	%r5,_NSIG8
	lhi	%r1,SYS_ify(rt_sigprocmask)
	svc	0

	/* Store fpu context.  */
	lr	%r1,%r0
	stfpc   SC_FPC(%r1)
	std     %f0,SC_FPRS(%r1)
	std     %f1,SC_FPRS+8(%r1)
	std     %f2,SC_FPRS+16(%r1)
	std     %f3,SC_FPRS+24(%r1)
	std     %f4,SC_FPRS+32(%r1)
	std     %f5,SC_FPRS+40(%r1)
	std     %f6,SC_FPRS+48(%r1)
	std     %f7,SC_FPRS+56(%r1)
	std     %f8,SC_FPRS+64(%r1)
	std     %f9,SC_FPRS+72(%r1)
	std     %f10,SC_FPRS+80(%r1)
	std     %f11,SC_FPRS+88(%r1)
	std     %f12,SC_FPRS+96(%r1)
	std     %f13,SC_FPRS+104(%r1)
	std     %f14,SC_FPRS+112(%r1)
	std     %f15,SC_FPRS+120(%r1)

	/* Set __getcontext return value to 0.  */
	slr     %r2,%r2

	/* Store access registers.  */
	stam    %a0,%a15,SC_ACRS(%r1)

	/* Store general purpose registers.  */
	stm     %r0,%r15,SC_GPRS(%r1)

	/* Store psw mask to 0x0 and addr to return address.  Then the address
	   can be retrieved from the ucontext structure in the same way as if it
	   is created by kernel and passed to a signal-handler.  */
	st	%r2,SC_PSW+0(%r1)
	st	%r14,SC_PSW+4(%r1)

	/* Return.  */
	br	%r14
END(__getcontext)

#if defined SHARED && SHLIB_COMPAT (libc, GLIBC_2_19, GLIBC_2_20)
/* In glibc release 2.19 a new version of getcontext was introduced,
   but was reverted before 2.20. Thus both versions are the same function.  */
weak_alias (__getcontext, __v1__getcontext)
weak_alias (__getcontext, __v2__getcontext)
versioned_symbol (libc, __v1__getcontext, getcontext, GLIBC_2_1)
compat_symbol (libc, __v2__getcontext, getcontext, GLIBC_2_19)

#else

weak_alias (__getcontext, getcontext)

#endif /* !(defined SHARED && SHLIB_COMPAT (libc, GLIBC_2_19, GLIBC_2_20))  */
