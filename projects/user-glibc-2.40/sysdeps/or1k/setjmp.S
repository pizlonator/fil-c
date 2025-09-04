/* setjmp, OpenRISC version.
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
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <sysdep.h>
#define _ASM
#define _SETJMP_H
#include <bits/setjmp.h>

ENTRY (_setjmp)
	l.addi	r4, r0, 0     /* Set second argument to 0.  */
	l.j	.Local__sigsetjmp
	 l.nop
END (_setjmp)
libc_hidden_def (_setjmp)

ENTRY (setjmp)
	l.addi	r4, r0, 1     /* Set second argument to 1.  */
	l.j	.Local__sigsetjmp
	 l.nop
END (setjmp)

ENTRY (__sigsetjmp)
.Local__sigsetjmp:
	l.sw	0(r3), r1
	l.sw	4(r3), r2
	l.sw	8(r3), r9
	l.sw	12(r3), r10
	l.sw	16(r3), r14
	l.sw	20(r3), r16
	l.sw	24(r3), r18
	l.sw	28(r3), r20
	l.sw	32(r3), r22
	l.sw	36(r3), r24
	l.sw	40(r3), r26
	l.sw	44(r3), r28
	l.sw	48(r3), r30
	l.j	__sigjmp_save
	 l.nop
END (__sigsetjmp)
hidden_def (__sigsetjmp)
