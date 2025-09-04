/* Optimized memchr implementation for POWER10 LE.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>

# ifndef MEMCHR
#  define MEMCHR __memchr
# endif
# define M_VREG_ZERO v20
# define M_OFF_START_LOOP 256
# define MEMCHR_SUBTRACT_VECTORS \
	vsububm   v4,v4,v18;	    \
	vsububm   v5,v5,v18;	    \
	vsububm   v6,v6,v18;	    \
	vsububm   v7,v7,v18;
# define M_TAIL(vreg,increment)	   \
	vctzlsbb  r4,vreg;	   \
	cmpld     r5,r4;	   \
	ble       L(null);	   \
	addi	  r4,r4,increment; \
	add	  r3,r6,r4;	   \
	blr

/* TODO: Replace macros by the actual instructions when minimum binutils becomes
   >= 2.35.  This is used to keep compatibility with older versions.  */
#define M_VEXTRACTBM(rt,vrb)	 \
	.long(((4)<<(32-6))	 \
	      | ((rt)<<(32-11))	 \
	      | ((8)<<(32-16))	 \
	      | ((vrb)<<(32-21)) \
	      | 1602)

#define M_LXVP(xtp,dq,ra)		   \
	.long(((6)<<(32-6))		   \
	      | ((((xtp)-32)>>1)<<(32-10)) \
	      | ((1)<<(32-11))		   \
	      | ((ra)<<(32-16))		   \
	      | dq)

#define CHECK16B(vreg,offset,addr,label) \
	lxv	  vreg+32,offset(addr);	\
	vcmpequb. vreg,vreg,v18;	\
	bne	  cr6,L(label);		\
	cmpldi	  r5,16;		\
	ble	  L(null);		\
	addi	  r5,r5,-16;

/* Load 4 quadwords, merge into one VR for speed and check for NULLs.  r6 has #
   of bytes already checked.  */
#define CHECK64B(offset,addr,label)	    \
	M_LXVP(v4+32,offset,addr);	    \
	M_LXVP(v6+32,offset+32,addr);	    \
	MEMCHR_SUBTRACT_VECTORS;	    \
	vminub	  v14,v4,v5;		    \
	vminub	  v15,v6,v7;		    \
	vminub	  v16,v14,v15;		    \
	vcmpequb. v0,v16,M_VREG_ZERO;	    \
	beq	  cr6,$+12;		    \
	li	  r7,offset;		    \
	b     	  L(label);          	    \
	cmpldi	  r5,64;		    \
	ble	  L(null);		    \
	addi	  r5,r5,-64

/* Implements the function
   void *[r3] memchr (const void *s [r3], int c [r4], size_t n [r5]).  */

	.machine power9

ENTRY_TOCLESS (MEMCHR)
	CALL_MCOUNT 3

	cmpldi	r5,0
	beq	L(null)
	mr	r0,r5
	xori	r6,r4,0xff

	mtvsrd	v18+32,r4	/* matching char in v18  */
	mtvsrd	v19+32,r6	/* non matching char in v19  */

	vspltb	v18,v18,7	/* replicate  */
	vspltb	v19,v19,7	/* replicate  */
	vspltisb  M_VREG_ZERO,0

	/* Next 16B-aligned address. Prepare address for L(aligned).  */
	addi	  r6,r3,16
	clrrdi	  r6,r6,4

	/* Align data and fill bytes not loaded with non matching char.	 */
	lvx	  v0,0,r3
	lvsr	  v1,0,r3
	vperm	  v0,v19,v0,v1

	vcmpequb. v6,v0,v18
	bne	  cr6,L(found)
	sub	  r4,r6,r3
	cmpld	  r5,r4
	ble	  L(null)
	sub	  r5,r5,r4

	/* Test up to OFF_START_LOOP-16 bytes in 16B chunks.  The main loop is
	   optimized for longer strings, so checking the first bytes in 16B
	   chunks benefits a lot small strings.  */
	.p2align 5
L(aligned):
	cmpldi	r5,0
	beq     L(null)

	CHECK16B(v0,0,r6,tail1)
	CHECK16B(v1,16,r6,tail2)
	CHECK16B(v2,32,r6,tail3)
	CHECK16B(v3,48,r6,tail4)
	CHECK16B(v4,64,r6,tail5)
	CHECK16B(v5,80,r6,tail6)
	CHECK16B(v6,96,r6,tail7)
	CHECK16B(v7,112,r6,tail8)
	CHECK16B(v8,128,r6,tail9)
	CHECK16B(v9,144,r6,tail10)
	CHECK16B(v10,160,r6,tail11)
	CHECK16B(v0,176,r6,tail12)
	CHECK16B(v1,192,r6,tail13)
	CHECK16B(v2,208,r6,tail14)
	CHECK16B(v3,224,r6,tail15)

	cmpdi	cr5,r4,0	/* Check if c == 0.  This will be useful to
				   choose how we will perform the main loop.  */

	/* Prepare address for the loop.  */
	addi	  r4,r3,M_OFF_START_LOOP
	clrrdi	  r4,r4,6
	sub	  r6,r4,r3
	sub	  r5,r0,r6
	addi	  r6,r4,128

	/* If c == 0, use the loop without the vsububm.  */
	beq	cr5,L(loop)

	/* This is very similar to the block after L(loop), the difference is
	   that here MEMCHR_SUBTRACT_VECTORS is not empty, and we subtract
	   each byte loaded by the char we are looking for, this way we can keep
	   using vminub to merge the results and checking for nulls.  */
	.p2align 5
L(memchr_loop):
	CHECK64B(0,r4,pre_tail_64b)
	CHECK64B(64,r4,pre_tail_64b)
	addi	r4,r4,256

	CHECK64B(0,r6,tail_64b)
	CHECK64B(64,r6,tail_64b)
	addi	r6,r6,256

	CHECK64B(0,r4,pre_tail_64b)
	CHECK64B(64,r4,pre_tail_64b)
	addi	r4,r4,256

	CHECK64B(0,r6,tail_64b)
	CHECK64B(64,r6,tail_64b)
	addi	r6,r6,256

	b	L(memchr_loop)
	/* Switch to a more aggressive approach checking 64B each time.  Use 2
	   pointers 128B apart and unroll the loop once to make the pointer
	   updates and usages separated enough to avoid stalls waiting for
	   address calculation.  */
	.p2align 5
L(loop):
#undef MEMCHR_SUBTRACT_VECTORS
#define MEMCHR_SUBTRACT_VECTORS /* nothing */
	CHECK64B(0,r4,pre_tail_64b)
	CHECK64B(64,r4,pre_tail_64b)
	addi	  r4,r4,256

	CHECK64B(0,r6,tail_64b)
	CHECK64B(64,r6,tail_64b)
	addi	  r6,r6,256

	CHECK64B(0,r4,pre_tail_64b)
	CHECK64B(64,r4,pre_tail_64b)
	addi      r4,r4,256

	CHECK64B(0,r6,tail_64b)
	CHECK64B(64,r6,tail_64b)
	addi      r6,r6,256

	b	  L(loop)

	.p2align  5
L(pre_tail_64b):
	mr	r6,r4
L(tail_64b):
	/* OK, we found a null byte.  Let's look for it in the current 64-byte
	   block and mark it in its corresponding VR.  lxvp vx,0(ry) puts the
	   low 16B bytes into vx+1, and the high into vx, so the order here is
	   v5, v4, v7, v6.  */
	vcmpequb  v1,v5,M_VREG_ZERO
	vcmpequb  v2,v4,M_VREG_ZERO
	vcmpequb  v3,v7,M_VREG_ZERO
	vcmpequb  v4,v6,M_VREG_ZERO

	/* Take into account the other 64B blocks we had already checked.  */
	add	r6,r6,r7
	/* Extract first bit of each byte.  */
	M_VEXTRACTBM(r8,v1)
	M_VEXTRACTBM(r9,v2)
	M_VEXTRACTBM(r10,v3)
	M_VEXTRACTBM(r11,v4)

	/* Shift each value into their corresponding position.  */
	sldi	  r9,r9,16
	sldi	  r10,r10,32
	sldi	  r11,r11,48

	/* Merge the results.  */
	or	  r8,r8,r9
	or	  r9,r10,r11
	or	  r11,r9,r8

	cnttzd	  r0,r11	  /* Count trailing zeros before the match.  */
	cmpld     r5,r0
	ble	  L(null)
	add	  r3,r6,r0	  /* Compute final address.  */
	blr

	.p2align  5
L(tail1):
	M_TAIL(v0,0)

	.p2align  5
L(tail2):
	M_TAIL(v1,16)

	.p2align  5
L(tail3):
	M_TAIL(v2,32)

	.p2align  5
L(tail4):
	M_TAIL(v3,48)

	.p2align  5
L(tail5):
	M_TAIL(v4,64)

	.p2align  5
L(tail6):
	M_TAIL(v5,80)

	.p2align  5
L(tail7):
	M_TAIL(v6,96)

	.p2align  5
L(tail8):
	M_TAIL(v7,112)

	.p2align  5
L(tail9):
	M_TAIL(v8,128)

	.p2align  5
L(tail10):
	M_TAIL(v9,144)

	.p2align  5
L(tail11):
	M_TAIL(v10,160)

	.p2align  5
L(tail12):
	M_TAIL(v0,176)

	.p2align  5
L(tail13):
	M_TAIL(v1,192)

	.p2align  5
L(tail14):
	M_TAIL(v2,208)

	.p2align  5
L(tail15):
	M_TAIL(v3,224)

	.p2align  5
L(found):
	vctzlsbb  r7,v6
	cmpld     r5,r7
	ble       L(null)
	add       r3,r3,r7
	blr

	.p2align  5
L(null):
	li	r3,0
	blr

END (MEMCHR)

weak_alias (__memchr, memchr)
libc_hidden_builtin_def (memchr)
