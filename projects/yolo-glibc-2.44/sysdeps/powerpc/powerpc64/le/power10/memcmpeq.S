/* Optimized __memcmpeq implementation for POWER10.
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

#include <sysdep.h>

#define COMPARE_32(vr1,vr2,offset)\
	lxvp      32+vr1,offset(r3);	\
	lxvp      32+vr2,offset(r4);	\
	vxor      v4,vr1,vr2;		\
	vxor      v5,vr1+1,vr2+1;	\
	vor       v19,v19,v4;		\
	vor       v19,v19,v5;

/* int [r3] __memcmpeq (const char *s1 [r3], const char *s2 [r4],
					size_t size [r5])
   Returns 0 if equal, 1 if not equal (no lexicographic comparison) */

#ifndef MEMCMPEQ
# define MEMCMPEQ __memcmpeq
#endif
	.machine  power10
ENTRY_TOCLESS (MEMCMPEQ, 4)
	CALL_MCOUNT 3

	/* Fast path: size == 0 */
	cmpdi	cr7,r5,0
	beq	cr7,L(finish)

	/* Fast path: same pointer */
	cmpd	cr7,r3,r4
	beq	cr7,L(finish)

	cmpldi	cr6,r5,64
	bgt	cr6,L(loop_head)

/* Compare 64 bytes. This section is used for lengths <= 64 and for the last
   bytes for larger lengths.  */
L(last_compare):
	li	r8,16

	sldi	r9,r5,56
	sldi	r8,r8,56
	addi	r6,r3,16
	addi	r7,r4,16

	/* Align up to 16 bytes.  */
	lxvl	32+v0,r3,r9
	lxvl	32+v2,r4,r9

	/* Branch to not_equal if any bytes differ (CR6 set by vcmpneb.).
	   Branch to finish if no bytes remain (CR0.LT set when r9 went
	   negative after sub.).  */
	sub.	  r9,r9,r8
	vcmpneb.  v4,v0,v2
	bne	  cr6,L(not_equal)
	bt	  4*cr0+lt,L(finish)

	addi	  r3,r3,32
	addi	  r4,r4,32

	lxvl	  32+v1,r6,r9
	lxvl	  32+v3,r7,r9
	sub.	  r9,r9,r8
	vcmpneb.  v5,v1,v3
	bne	  cr6,L(not_equal)
	bt	  4*cr0+lt,L(finish)

	addi	  r6,r3,16
	addi	  r7,r4,16

	lxvl	  32+v6,r3,r9
	lxvl	  32+v8,r4,r9
	sub.	  r9,r9,r8
	vcmpneb.  v4,v6,v8
	bne	  cr6,L(not_equal)
	bt	  4*cr0+lt,L(finish)

	lxvl	  32+v7,r6,r9
	lxvl	  32+v9,r7,r9
	vcmpneb.  v5,v7,v9
	bne	  cr6,L(not_equal)

L(finish):
	/* The contents are equal.  */
	li	r3,0
	blr

L(not_equal):
	li	r3,1
	blr

L(loop_head):
	/* Calculate how many loops to run.  */
	srdi.	r8,r5,7
	beq	L(loop_tail)
	mtctr	r8

	vxor	v18,v18,v18
	vxor	v19,v19,v19
	.p2align 5
L(loop_128):
	COMPARE_32(v0,v2,0)
	COMPARE_32(v6,v8,32)
	vcmpneb. v17,v19,v18
	bne	cr6,L(not_equal)
	COMPARE_32(v10,v12,64)
	COMPARE_32(v14,v16,96)

	vcmpneb. v17,v19,v18
	bne	cr6,L(not_equal)

	addi	r3,r3,128
	addi	r4,r4,128
	bdnz	L(loop_128)

	/* Account loop comparisons.  */
	clrldi.  r5,r5,57
	beq	 L(finish)

/* Compares 64 bytes if length is still bigger than 64 bytes.  */
	.p2align 5
L(loop_tail):
	/* Initialize accumulator for tail */
	vxor	v18,v18,v18
	vxor	v19,v19,v19

	cmpldi	r5,64
	ble	L(last_compare)

	COMPARE_32(v0,v2,0)
	COMPARE_32(v6,v8,32)

	vcmpneb. v17,v19,v18
	bne	cr6,L(not_equal)

	addi	r3,r3,64
	addi	r4,r4,64
	subi	r5,r5,64
	b	L(last_compare)

END (MEMCMPEQ)

libc_hidden_def (MEMCMPEQ)
