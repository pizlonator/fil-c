/* Optimized strcmp implementation for PowerPC64/POWER10.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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


#define COMPARE_16(vreg1,vreg2,offset)  \
	lxv       vreg1+32,offset(r3);  \
	lxv       vreg2+32,offset(r4);	\
	vcmpnezb. v7,vreg1,vreg2;	\
	bne       cr6,L(different);     \

#define COMPARE_32(vreg1,vreg2,offset,label1,label2) \
	lxvp	  vreg1+32,offset(r3);               \
	lxvp	  vreg2+32,offset(r4);               \
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
	vcmpnezb. v7,v4,v5;              \
	vctzlsbb  r6,v7;                 \
	cmpld	  cr7,r6,len_reg;        \
	blt	  cr7,L(different);      \


	.machine  power10
ENTRY_TOCLESS (STRCMP, 4)
	li	 r11,16
	/* eq bit of cr1 used as swap status flag to indicate if
	source pointers were swapped.  */
	crclr	 4*cr1+eq
	andi.	 r7,r3,15
	sub	 r7,r11,r7	/* r7(nalign1) = 16 - (str1 & 15).  */
	andi.	 r9,r4,15
	sub	 r5,r11,r9	/* r5(nalign2) = 16 - (str2 & 15).  */
	cmpld	 cr7,r7,r5
	beq	 cr7,L(same_aligned)
	blt	 cr7,L(nalign1_min)
	/* Swap r3 and r4, and r7 and r5 such that r3 and r7 hold the
	pointer which is closer to the next 16B boundary so that only
	one CHECK_N_BYTES is needed before entering the loop below.  */
	mr	 r8,r4
	mr	 r4,r3
	mr	 r3,r8
	mr	 r12,r7
	mr	 r7,r5
	mr	 r5,r12
	crset	 4*cr1+eq	/* Set bit on swapping source pointers.  */

	.p2align 5
L(nalign1_min):
	CHECK_N_BYTES(r3,r4,r7)

	.p2align 5
L(s1_aligned):
	/* r9 and r5 is number of bytes to be read after and before
	 page boundary correspondingly.  */
	sub 	r5,r5,r7
	subfic	r9,r5,16
	/* Now let r7 hold the count of quadwords which can be
	checked without crossing a page boundary. quadword offset is
	(str2>>4)&0xFF.  */
	rlwinm	r7,r4,28,0xFF
	/* Below check is required only for first iteration. For second
	iteration and beyond, the new loop counter is always 255.  */
	cmpldi	r7,255
	beq	L(L3)
	/* Get the initial loop count by 255-((str2>>4)&0xFF).  */
	subfic  r11,r7,255

	.p2align 5
L(L1):
	mtctr	r11

	.p2align 5
L(L2):
	COMPARE_16(v4,v5,0)	/* Load 16B blocks using lxv.  */
	addi	r3,r3,16
	addi	r4,r4,16
	bdnz	L(L2)
	/* Cross the page boundary of s2, carefully.  */

	.p2align 5
L(L3):
	CHECK_N_BYTES(r3,r4,r5)
	CHECK_N_BYTES(r3,r4,r9)
	li 	r11,255		/* Load the new loop counter.  */
	b	L(L1)

	.p2align 5
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
	vctzlsbb r6,v7
	vextubrx r5,r6,v4
	vextubrx r4,r6,v5
	bt  	 4*cr1+eq,L(swapped)
	subf	 r3,r4,r5
	blr

	/* If src pointers were swapped, then swap the
	indices and calculate the return value.  */
L(swapped):
	subf     r3,r5,r4
	blr

	.p2align 5
L(32B_aligned_loop):
	COMPARE_32(v14,v16,0,tail1,tail2)
	COMPARE_32(v14,v16,32,tail1,tail2)
	COMPARE_32(v14,v16,64,tail1,tail2)
	COMPARE_32(v14,v16,96,tail1,tail2)
	addi	r3,r3,128
	addi	r4,r4,128
	b	L(32B_aligned_loop)

L(tail1): TAIL(v15,v17)
L(tail2): TAIL(v14,v16)

END (STRCMP)
libc_hidden_builtin_def (strcmp)
