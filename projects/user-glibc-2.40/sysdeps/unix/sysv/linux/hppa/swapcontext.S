/* Swap to new context.
   Copyright (C) 2008-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include "ucontext_i.h"

	.text
ENTRY(__swapcontext)

	/* Copy rp to ret0 (r28).  */
	copy %rp,%ret0

	/* Create a frame.  */
	ldo 64(%sp),%sp
	.cfi_def_cfa_offset -64

	/* Save the current machine context to oucp.  */
	bl __getcontext,%rp

	/* Copy oucp to register ret1 (r29).  __getcontext saves and
	   restores it on a normal return.  It is restored from oR29
	   on reactivation.  */
	copy %r26,%ret1

	/* Pop frame.  */
	ldo -64(%sp),%sp
	.cfi_def_cfa_offset 0

	/* Load return pointer from oR28.  */
	ldw oR28(%ret1),%rp

	/* Return if error.  */
	or,= %r0,%ret0,%r0
	bv,n %r0(%rp)

	/* Load sc_sar flag.  */
	ldb oSAR(%ret1),%r20

	/* Return if oucp context has been reactivated.  */
	or,= %r0,%r20,%r0
	bv,n %r0(%rp)

	/* Mark sc_sar flag.  */
	ldi 1,%r20
	stb %r20,oSAR(%ret1)

	/* Activate the machine context in ucp.  */
	bl __setcontext,%rp
	ldw oR25(%ret1),%r26

	/* Load return pointer.  */
	ldw oR28(%ret1),%rp
	bv,n %r0(%rp)

END(__swapcontext)

weak_alias (__swapcontext, swapcontext)
