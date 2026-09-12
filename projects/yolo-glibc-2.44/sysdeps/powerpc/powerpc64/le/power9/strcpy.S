/* Optimized strcpy implementation for PowerPC64/POWER9.
   Copyright (C) 2020-2026 Free Software Foundation, Inc.
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

#ifdef USE_AS_STPCPY
# ifndef STPCPY
#   define FUNC_NAME __stpcpy
# else
#   define FUNC_NAME STPCPY
# endif
#else
# ifndef STRCPY
#  define FUNC_NAME strcpy
# else
#  define FUNC_NAME STRCPY
# endif
#endif  /* !USE_AS_STPCPY  */

/* Implements the function

   char * [r3] strcpy (char *dest [r3], const char *src [r4])

   or

   char * [r3] stpcpy (char *dest [r3], const char *src [r4])

   if USE_AS_STPCPY is defined.

   This implementation never reads across a page boundary, but may
   read beyond the NUL terminator.  */

/* Load 4 quadwords, merge into one VR for speed and check for NUL
   and branch to label if NUL is found.  */
#define CHECK_64B(offset,addr,label)		\
	lxv	32+v4,(offset+0)(addr);		\
	lxv	32+v5,(offset+16)(addr);	\
	lxv	32+v6,(offset+32)(addr);	\
	lxv	32+v7,(offset+48)(addr);	\
	vminub	v14,v4,v5;			\
	vminub	v15,v6,v7;			\
	vminub	v16,v14,v15;			\
	vcmpequb.	v0,v16,v18;		\
	beq	cr6,$+12;			\
	li	r7,offset;			\
	b	L(label);			\
	stxv	32+v4,(offset+0)(r11);		\
	stxv	32+v5,(offset+16)(r11);		\
	stxv	32+v6,(offset+32)(r11);		\
	stxv	32+v7,(offset+48)(r11)

/* Load quadword at addr+offset to vreg, check for NUL bytes,
   and branch to label if any are found.  */
#define CHECK_16B(vreg,offset,addr,label)	\
	lxv	vreg+32,offset(addr);		\
	vcmpequb.	v15,vreg,v18;		\
	bne	cr6,L(label);

/* Store vreg2 with length if NUL is found.  */
#define STORE_WITH_LEN(vreg1,vreg2,reg)	\
	vctzlsbb	r8,vreg1;		\
	addi	r9,r8,1;			\
	sldi	r9,r9,56;			\
	stxvl	32+vreg2,reg,r9;

.machine	power9
ENTRY_TOCLESS (FUNC_NAME, 4)
	CALL_MCOUNT 2

	vspltisb	v18,0		/* Zeroes in v18.  */
	vspltisb	v19,-1		/* 0xFF bytes in v19.  */

	/* Next 16B-aligned address. Prepare address for L(loop).  */
	addi	r5,r4,16
	clrrdi	r5,r5,4
	subf	r8,r4,r5
	add	r11,r3,r8

	/* Align data and fill bytes not loaded with non matching char.  */
	lvx	v0,0,r4
	lvsr	v1,0,r4
	vperm	v0,v19,v0,v1

	vcmpequb.	v6,v0,v18	/* 0xff if byte is NUL, 0x00 otherwise.  */
	beq	cr6,L(no_null)

	/* There's a NUL byte.  */
	STORE_WITH_LEN(v6,v0,r3)

#ifdef USE_AS_STPCPY
	/* stpcpy returns the dest address plus the size not counting the
	   final '\0'.  */
	add	r3,r3,r8
#endif
	blr

L(no_null):
	sldi	r10,r8,56	/* stxvl wants size in top 8 bits.  */
	stxvl	32+v0,r3,r10	/* Partial store.  */

/* The main loop is optimized for longer strings(> 512 bytes),
   so checking the first bytes in 16B chunks benefits shorter
   strings a lot.  */
	.p2align 4
L(aligned):
	CHECK_16B(v0,0,r5,tail1)
	CHECK_16B(v1,16,r5,tail2)
	CHECK_16B(v2,32,r5,tail3)
	CHECK_16B(v3,48,r5,tail4)
	CHECK_16B(v4,64,r5,tail5)
	CHECK_16B(v5,80,r5,tail6)
	CHECK_16B(v6,96,r5,tail7)
	CHECK_16B(v7,112,r5,tail8)

	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	stxv	32+v3,48(r11)
	stxv	32+v4,64(r11)
	stxv	32+v5,80(r11)
	stxv	32+v6,96(r11)
	stxv	32+v7,112(r11)

	addi	r11,r11,128

	CHECK_16B(v0,128,r5,tail1)
	CHECK_16B(v1,128+16,r5,tail2)
	CHECK_16B(v2,128+32,r5,tail3)
	CHECK_16B(v3,128+48,r5,tail4)
	CHECK_16B(v4,128+64,r5,tail5)
	CHECK_16B(v5,128+80,r5,tail6)
	CHECK_16B(v6,128+96,r5,tail7)
	CHECK_16B(v7,128+112,r5,tail8)

	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	stxv	32+v3,48(r11)
	stxv	32+v4,64(r11)
	stxv	32+v5,80(r11)
	stxv	32+v6,96(r11)
	stxv	32+v7,112(r11)

	addi	r11,r11,128

	CHECK_16B(v0,256,r5,tail1)
	CHECK_16B(v1,256+16,r5,tail2)
	CHECK_16B(v2,256+32,r5,tail3)
	CHECK_16B(v3,256+48,r5,tail4)
	CHECK_16B(v4,256+64,r5,tail5)
	CHECK_16B(v5,256+80,r5,tail6)
	CHECK_16B(v6,256+96,r5,tail7)
	CHECK_16B(v7,256+112,r5,tail8)

	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	stxv	32+v3,48(r11)
	stxv	32+v4,64(r11)
	stxv	32+v5,80(r11)
	stxv	32+v6,96(r11)
	stxv	32+v7,112(r11)

	addi	r11,r11,128

	CHECK_16B(v0,384,r5,tail1)
	CHECK_16B(v1,384+16,r5,tail2)
	CHECK_16B(v2,384+32,r5,tail3)
	CHECK_16B(v3,384+48,r5,tail4)
	CHECK_16B(v4,384+64,r5,tail5)
	CHECK_16B(v5,384+80,r5,tail6)
	CHECK_16B(v6,384+96,r5,tail7)
	CHECK_16B(v7,384+112,r5,tail8)

	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	stxv	32+v3,48(r11)
	stxv	32+v4,64(r11)
	stxv	32+v5,80(r11)
	stxv	32+v6,96(r11)
	stxv	32+v7,112(r11)

	/* Align src pointer down to a 64B boundary.  */
	addi	r5,r4,512
	clrrdi	r5,r5,6
	subf	r7,r4,r5
	add	r11,r3,r7

/* Switch to a more aggressive approach checking 64B each time.  */
	.p2align 5
L(strcpy_loop):
	CHECK_64B(0,r5,tail_64b)
	CHECK_64B(64,r5,tail_64b)
	CHECK_64B(128,r5,tail_64b)
	CHECK_64B(192,r5,tail_64b)

	CHECK_64B(256,r5,tail_64b)
	CHECK_64B(256+64,r5,tail_64b)
	CHECK_64B(256+128,r5,tail_64b)
	CHECK_64B(256+192,r5,tail_64b)
	addi	r5,r5,512
	addi	r11,r11,512

	b	L(strcpy_loop)

	.p2align 5
L(tail_64b):
	/* OK, we found a NUL byte.  Let's look for it in the current 64-byte
	   block and mark it in its corresponding VR.  */
	add	r11,r11,r7
	vcmpequb.	v8,v4,v18
	beq	cr6,L(no_null_16B)
	/* There's a NUL byte.  */
	STORE_WITH_LEN(v8,v4,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

L(no_null_16B):
	stxv	32+v4,0(r11)
	vcmpequb.	v8,v5,v18
	beq	cr6,L(no_null_32B)
	/* There's a NUL byte.  */
	addi	r11,r11,16
	STORE_WITH_LEN(v8,v5,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

L(no_null_32B):
	stxv	32+v5,16(r11)
	vcmpequb.	v8,v6,v18
	beq	cr6,L(no_null_48B)
	/* There's a NUL byte.  */
	addi	r11,r11,32
	STORE_WITH_LEN(v8,v6,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

L(no_null_48B):
	stxv	32+v6,32(r11)
	vcmpequb.	v8,v7,v18;
	/* There's a NUL byte.  */
	addi	r11,r11,48
	STORE_WITH_LEN(v8,v7,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

	.p2align 4
L(tail1):
	/* There's a NUL byte.  */
	STORE_WITH_LEN(v15,v0,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

	.p2align 4
L(tail2):
	stxv	32+v0,0(r11)
	/* There's a NUL byte.  */
	addi	r11,r11,16
	STORE_WITH_LEN(v15,v1,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

	.p2align 4
L(tail3):
	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	addi	r11,r11,32
	STORE_WITH_LEN(v15,v2,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

	.p2align 4
L(tail4):
	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	addi	r11,r11,48
	STORE_WITH_LEN(v15,v3,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

	.p2align 4
L(tail5):
	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	stxv	32+v3,48(r11)
	addi	r11,r11,64
	STORE_WITH_LEN(v15,v4,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

	.p2align 4
L(tail6):
	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	stxv	32+v3,48(r11)
	stxv	32+v4,64(r11)
	addi	r11,r11,80
	STORE_WITH_LEN(v15,v5,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

	.p2align 4
L(tail7):
	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	stxv	32+v3,48(r11)
	stxv	32+v4,64(r11)
	stxv	32+v5,80(r11)
	addi	r11,r11,96
	STORE_WITH_LEN(v15,v6,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

	.p2align 4
L(tail8):
	stxv	32+v0,0(r11)
	stxv	32+v1,16(r11)
	stxv	32+v2,32(r11)
	stxv	32+v3,48(r11)
	stxv	32+v4,64(r11)
	stxv	32+v5,80(r11)
	stxv	32+v6,96(r11)
	addi	r11,r11,112
	STORE_WITH_LEN(v15,v7,r11)
#ifdef USE_AS_STPCPY
	add	r3,r11,r8
#endif
	blr

END (FUNC_NAME)
#ifndef USE_AS_STPCPY
libc_hidden_builtin_def (strcpy)
#endif
