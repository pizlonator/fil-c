/* Optimized strcmp implementation for PowerPC64/POWER10.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#ifndef STRCMP
# define STRCMP strcmp
#endif

/* Implements the function
   int [r3] strcmp (const char *s1 [r3], const char *s2 [r4]).  */

/* TODO: Change this to actual instructions when minimum binutils is upgraded
   to 2.27.  Macros are defined below for these newer instructions in order
   to maintain compatibility.  */

#define LXVP(xtp,dq,ra)		     \
	.long(((6)<<(32-6))	     \
	| ((((xtp)-32)>>1)<<(32-10)) \
	| ((1)<<(32-11))	     \
	| ((ra)<<(32-16))	     \
	| dq)

#define COMPARE_16(vreg1,vreg2,offset)  \
	lxv       vreg1+32,offset(r3);  \
	lxv       vreg2+32,offset(r4);	\
	vcmpnezb. v7,vreg1,vreg2;	\
	bne       cr6,L(different);     \

#define COMPARE_32(vreg1,vreg2,offset,label1,label2) \
	LXVP(vreg1+32,offset,r3);                    \
	LXVP(vreg2+32,offset,r4);                    \
	vcmpnezb. v7,vreg1+1,vreg2+1;                \
	bne	  cr6,L(label1);                     \
	vcmpnezb. v7,vreg1,vreg2;                    \
	bne	  cr6,L(label2);                     \

#define TAIL(vreg1,vreg2)     \
	vctzlsbb r6,v7;	      \
	vextubrx r5,r6,vreg1; \
	vextubrx r4,r6,vreg2; \
	subf	 r3,r4,r5;    \
	blr;                  \

#define CHECK_N_BYTES(reg1,reg2,len_reg) \
	sldi	  r0,len_reg,56;         \
	lxvl	  32+v4,reg1,r0;         \
	lxvl	  32+v5,reg2,r0;         \
	add	  reg1,reg1,len_reg;     \
	add	  reg2,reg2,len_reg;     \
	vcmpnezb  v7,v4,v5;              \
	vctzlsbb  r6,v7;                 \
	cmpld	  cr7,r6,len_reg;        \
	blt	  cr7,L(different);      \

	/* TODO: change this to .machine power10 when the minimum required
	binutils allows it.  */

	.machine  power9
ENTRY_TOCLESS (STRCMP, 4)
	andi.	r7,r3,4095
	andi.	r8,r4,4095
	cmpldi	cr0,r7,4096-16
	cmpldi	cr1,r8,4096-16
	bgt	cr0,L(crosses)
	bgt	cr1,L(crosses)
	COMPARE_16(v4,v5,0)

L(crosses):
	andi.	r7,r3,15
	subfic	r7,r7,16	/* r7(nalign1) = 16 - (str1 & 15).  */
	andi.	r9,r4,15
	subfic	r5,r9,16	/* r5(nalign2) = 16 - (str2 & 15).  */
	cmpld	cr7,r7,r5
	beq	cr7,L(same_aligned)
	blt	cr7,L(nalign1_min)

	/* nalign2 is minimum and s2 pointer is aligned.  */
	CHECK_N_BYTES(r3,r4,r5)
	/* Are we on the 64B hunk which crosses a page?  */
	andi.	r10,r3,63	/* Determine offset into 64B hunk.  */
	andi.	r8,r3,15        /* The offset into the 16B hunk.  */
	neg	r7,r3
	andi.	r9,r7,15	/* Number of bytes after a 16B cross.  */
	rlwinm.	r7,r7,26,0x3F	/* ((r3-4096))>>6&63.  */
	beq	L(compare_64_pagecross)
	mtctr	r7
	b	L(compare_64B_unaligned)

	/* nalign1 is minimum and s1 pointer is aligned.  */
L(nalign1_min):
	CHECK_N_BYTES(r3,r4,r7)
	/* Are we on the 64B hunk which crosses a page?  */
	andi.	r10,r4,63	/* Determine offset into 64B hunk.  */
	andi.	r8,r4,15	/* The offset into the 16B hunk.  */
	neg	r7,r4
	andi.	r9,r7,15	/* Number of bytes after a 16B cross.  */
	rlwinm. r7,r7,26,0x3F	/* ((r4-4096))>>6&63.  */
	beq	L(compare_64_pagecross)
	mtctr	r7

	.p2align 5
L(compare_64B_unaligned):
	COMPARE_16(v4,v5,0)
	COMPARE_16(v4,v5,16)
	COMPARE_16(v4,v5,32)
	COMPARE_16(v4,v5,48)
	addi	r3,r3,64
	addi	r4,r4,64
	bdnz	L(compare_64B_unaligned)

	/* Cross the page boundary of s2, carefully. Only for first
	iteration we have to get the count of 64B blocks to be checked.
	From second iteration and beyond, loop counter is always 63.  */
L(compare_64_pagecross):
	li	r11, 63
	mtctr	r11
	cmpldi	r10,16
	ble	L(cross_4)
	cmpldi	r10,32
	ble	L(cross_3)
	cmpldi	r10,48
	ble	L(cross_2)
L(cross_1):
	CHECK_N_BYTES(r3,r4,r9)
	CHECK_N_BYTES(r3,r4,r8)
	COMPARE_16(v4,v5,0)
	COMPARE_16(v4,v5,16)
	COMPARE_16(v4,v5,32)
	addi	r3,r3,48
	addi	r4,r4,48
	b	L(compare_64B_unaligned)
L(cross_2):
	COMPARE_16(v4,v5,0)
	addi	r3,r3,16
	addi	r4,r4,16
	CHECK_N_BYTES(r3,r4,r9)
	CHECK_N_BYTES(r3,r4,r8)
	COMPARE_16(v4,v5,0)
	COMPARE_16(v4,v5,16)
	addi	r3,r3,32
	addi	r4,r4,32
	b	L(compare_64B_unaligned)
L(cross_3):
	COMPARE_16(v4,v5,0)
	COMPARE_16(v4,v5,16)
	addi	r3,r3,32
	addi	r4,r4,32
	CHECK_N_BYTES(r3,r4,r9)
	CHECK_N_BYTES(r3,r4,r8)
	COMPARE_16(v4,v5,0)
	addi	r3,r3,16
	addi	r4,r4,16
	b	L(compare_64B_unaligned)
L(cross_4):
	COMPARE_16(v4,v5,0)
	COMPARE_16(v4,v5,16)
	COMPARE_16(v4,v5,32)
	addi	r3,r3,48
	addi	r4,r4,48
	CHECK_N_BYTES(r3,r4,r9)
	CHECK_N_BYTES(r3,r4,r8)
	b	L(compare_64B_unaligned)

L(same_aligned):
	CHECK_N_BYTES(r3,r4,r7)
        /* Align s1 to 32B and adjust s2 address.
	   Use lxvp only if both s1 and s2 are 32B aligned.  */
	COMPARE_16(v4,v5,0)
	COMPARE_16(v4,v5,16)
	COMPARE_16(v4,v5,32)
	COMPARE_16(v4,v5,48)
	addi	r3,r3,64
	addi	r4,r4,64
	COMPARE_16(v4,v5,0)
	COMPARE_16(v4,v5,16)

	clrldi	r6,r3,59
	subfic	r5,r6,32
	add	r3,r3,r5
	add	r4,r4,r5
	andi.	r5,r4,0x1F
	beq	cr0,L(32B_aligned_loop)

	.p2align 5
L(16B_aligned_loop):
	COMPARE_16(v4,v5,0)
	COMPARE_16(v4,v5,16)
	COMPARE_16(v4,v5,32)
	COMPARE_16(v4,v5,48)
	addi	r3,r3,64
	addi	r4,r4,64
	b	L(16B_aligned_loop)

	/* Calculate and return the difference.  */
L(different):
	TAIL(v4,v5)

	.p2align 5
L(32B_aligned_loop):
	COMPARE_32(v14,v16,0,tail1,tail2)
	COMPARE_32(v18,v20,32,tail3,tail4)
	COMPARE_32(v22,v24,64,tail5,tail6)
	COMPARE_32(v26,v28,96,tail7,tail8)
	addi	r3,r3,128
	addi	r4,r4,128
	b	L(32B_aligned_loop)

L(tail1): TAIL(v15,v17)
L(tail2): TAIL(v14,v16)
L(tail3): TAIL(v19,v21)
L(tail4): TAIL(v18,v20)
L(tail5): TAIL(v23,v25)
L(tail6): TAIL(v22,v24)
L(tail7): TAIL(v27,v29)
L(tail8): TAIL(v26,v28)

END (STRCMP)
libc_hidden_builtin_def (strcmp)
