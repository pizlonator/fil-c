# Copyright 2021- IBM Inc. All rights reserved
#
# This file is part of Libgcrypt.
#
# Libgcrypt is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Libgcrypt is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, see <http://www.gnu.org/licenses/>.
#
#===================================================================================
# Written by Danny Tsen <dtsen@us.ibm.com>
#
# Poly1305 - this version mainly using vector/VSX/Scalar
#  - 26 bits limbs
#  - Handle multiple 64 byte blcoks but need at least 2 64 bytes block
#
# Improve performance by breaking down polynominal to the sum of products with
#     h4 = m1 * r⁴ + m2 * r³ + m3 * r² + m4 * r
#
#  07/22/21 - this revison based on the above sum of products.  Setup r^4, r^3, r^2, r and s3, s2, s1, s0
#             to 9 vectors for multiplications.
#
# setup r^4, r^3, r^2, r vectors
#    vs    [r^1, r^3, r^2, r^4]
#    vs0 = [r0,.....]
#    vs1 = [r1,.....]
#    vs2 = [r2,.....]
#    vs3 = [r3,.....]
#    vs4 = [r4,.....]
#    vs5 = [r1*5,...]
#    vs6 = [r2*5,...]
#    vs7 = [r2*5,...]
#    vs8 = [r4*5,...]
#
#  Each word in a vector consists a member of a "r/s" in [a * r/s].
#
# r0, r4*5, r3*5, r2*5, r1*5;
# r1, r0,   r4*5, r3*5, r2*5;
# r2, r1,   r0,   r4*5, r3*5;
# r3, r2,   r1,   r0,   r4*5;
# r4, r3,   r2,   r1,   r0  ;
#
#
# gcry_poly1305_p10le_4blocks( uint8_t *k, uint32_t mlen, uint8_t *m)
#  k = 32 bytes key
#  r3 = k (r, s)
#  r4 = mlen
#  r5 = m
#
.text

# Block size 16 bytes
# key = (r, s)
# clamp r &= 0x0FFFFFFC0FFFFFFC 0x0FFFFFFC0FFFFFFF
# p = 2^130 - 5
# a += m
# a = (r + a) % p
# a += s
# 16 bytes (a)
#
# p[0] = a0*r0 + a1*r4*5 + a2*r3*5 + a3*r2*5 + a4*r1*5;
# p[1] = a0*r1 + a1*r0   + a2*r4*5 + a3*r3*5 + a4*r2*5;
# p[2] = a0*r2 + a1*r1   + a2*r0   + a3*r4*5 + a4*r3*5;
# p[3] = a0*r3 + a1*r2   + a2*r1   + a3*r0   + a4*r4*5;
# p[4] = a0*r4 + a1*r3   + a2*r2   + a3*r1   + a4*r0  ;
#
#    [r^2, r^3, r^1, r^4]
#    [m3,  m2,  m4,  m1]
#
# multiply odd and even words
.macro mul_odd
	vmulouw	14, 4, 26
	vmulouw	10, 5, 3
	vmulouw	11, 6, 2
	vmulouw	12, 7, 1
	vmulouw	13, 8, 0
	vmulouw	15, 4, 27
	vaddudm	14, 14, 10
	vaddudm	14, 14, 11
	vmulouw	10, 5, 26
	vmulouw	11, 6, 3
	vaddudm	14, 14, 12
	vaddudm	14, 14, 13	# x0
	vaddudm	15, 15, 10
	vaddudm	15, 15, 11
	vmulouw	12, 7, 2
	vmulouw	13, 8, 1
	vaddudm	15, 15, 12
	vaddudm	15, 15, 13	# x1
	vmulouw	16, 4, 28
	vmulouw	10, 5, 27
	vmulouw	11, 6, 26
	vaddudm	16, 16, 10
	vaddudm	16, 16, 11
	vmulouw	12, 7, 3
	vmulouw	13, 8, 2
	vaddudm	16, 16, 12
	vaddudm	16, 16, 13	# x2
	vmulouw	17, 4, 29
	vmulouw	10, 5, 28
	vmulouw	11, 6, 27
	vaddudm	17, 17, 10
	vaddudm	17, 17, 11
	vmulouw	12, 7, 26
	vmulouw	13, 8, 3
	vaddudm	17, 17, 12
	vaddudm	17, 17, 13	# x3
	vmulouw	18, 4, 30
	vmulouw	10, 5, 29
	vmulouw	11, 6, 28
	vaddudm	18, 18, 10
	vaddudm	18, 18, 11
	vmulouw	12, 7, 27
	vmulouw	13, 8, 26
	vaddudm	18, 18, 12
	vaddudm	18, 18, 13	# x4
.endm

.macro mul_even
	vmuleuw	9, 4, 26
	vmuleuw	10, 5, 3
	vmuleuw	11, 6, 2
	vmuleuw	12, 7, 1
	vmuleuw	13, 8, 0
	vaddudm	14, 14, 9
	vaddudm	14, 14, 10
	vaddudm	14, 14, 11
	vaddudm	14, 14, 12
	vaddudm	14, 14, 13	# x0

	vmuleuw	9, 4, 27
	vmuleuw	10, 5, 26
	vmuleuw	11, 6, 3
	vmuleuw	12, 7, 2
	vmuleuw	13, 8, 1
	vaddudm	15, 15, 9
	vaddudm	15, 15, 10
	vaddudm	15, 15, 11
	vaddudm	15, 15, 12
	vaddudm	15, 15, 13	# x1

	vmuleuw	9, 4, 28
	vmuleuw	10, 5, 27
	vmuleuw	11, 6, 26
	vmuleuw	12, 7, 3
	vmuleuw	13, 8, 2
	vaddudm	16, 16, 9
	vaddudm	16, 16, 10
	vaddudm	16, 16, 11
	vaddudm	16, 16, 12
	vaddudm	16, 16, 13	# x2

	vmuleuw	9, 4, 29
	vmuleuw	10, 5, 28
	vmuleuw	11, 6, 27
	vmuleuw	12, 7, 26
	vmuleuw	13, 8, 3
	vaddudm	17, 17, 9
	vaddudm	17, 17, 10
	vaddudm	17, 17, 11
	vaddudm	17, 17, 12
	vaddudm	17, 17, 13	# x3

	vmuleuw	9, 4, 30
	vmuleuw	10, 5, 29
	vmuleuw	11, 6, 28
	vmuleuw	12, 7, 27
	vmuleuw	13, 8, 26
	vaddudm	18, 18, 9
	vaddudm	18, 18, 10
	vaddudm	18, 18, 11
	vaddudm	18, 18, 12
	vaddudm	18, 18, 13	# x4
.endm

# setup r^4, r^3, r^2, r vectors
#    [r, r^3, r^2, r^4]
#    vs0 = [r0,...]
#    vs1 = [r1,...]
#    vs2 = [r2,...]
#    vs3 = [r3,...]
#    vs4 = [r4,...]
#    vs5 = [r4*5,...]
#    vs6 = [r3*5,...]
#    vs7 = [r2*5,...]
#    vs8 = [r1*5,...]
#
# r0, r4*5, r3*5, r2*5, r1*5;
# r1, r0,   r4*5, r3*5, r2*5;
# r2, r1,   r0,   r4*5, r3*5;
# r3, r2,   r1,   r0,   r4*5;
# r4, r3,   r2,   r1,   r0  ;
#
.macro poly1305_setup_r

	# save r
	xxlor	26, 58, 58
	xxlor	27, 59, 59
	xxlor	28, 60, 60
	xxlor	29, 61, 61
	xxlor	30, 62, 62

	xxlxor	31, 31, 31

#    [r, r^3, r^2, r^4]
	# compute r^2
	vmr	4, 26
	vmr	5, 27
	vmr	6, 28
	vmr	7, 29
	vmr	8, 30
	bl	do_mul		# r^2 r^1
	xxpermdi 58, 58, 36, 0x3		# r0
	xxpermdi 59, 59, 37, 0x3		# r1
	xxpermdi 60, 60, 38, 0x3		# r2
	xxpermdi 61, 61, 39, 0x3		# r3
	xxpermdi 62, 62, 40, 0x3		# r4
	xxpermdi 36, 36, 36, 0x3
	xxpermdi 37, 37, 37, 0x3
	xxpermdi 38, 38, 38, 0x3
	xxpermdi 39, 39, 39, 0x3
	xxpermdi 40, 40, 40, 0x3
	vspltisb 13, 2
	vsld	9, 27, 13
	vsld	10, 28, 13
	vsld	11, 29, 13
	vsld	12, 30, 13
	vaddudm	0, 9, 27
	vaddudm	1, 10, 28
	vaddudm	2, 11, 29
	vaddudm	3, 12, 30

	bl	do_mul		# r^4 r^3
	vmrgow	26, 26, 4
	vmrgow	27, 27, 5
	vmrgow	28, 28, 6
	vmrgow	29, 29, 7
	vmrgow	30, 30, 8
	vspltisb 13, 2
	vsld	9, 27, 13
	vsld	10, 28, 13
	vsld	11, 29, 13
	vsld	12, 30, 13
	vaddudm	0, 9, 27
	vaddudm	1, 10, 28
	vaddudm	2, 11, 29
	vaddudm	3, 12, 30

	# r^2 r^4
	xxlor	0, 58, 58
	xxlor	1, 59, 59
	xxlor	2, 60, 60
	xxlor	3, 61, 61
	xxlor	4, 62, 62
	xxlor	5, 32, 32
	xxlor	6, 33, 33
	xxlor	7, 34, 34
	xxlor	8, 35, 35

	vspltw	9, 26, 3
	vspltw	10, 26, 2
	vmrgow	26, 10, 9
	vspltw	9, 27, 3
	vspltw	10, 27, 2
	vmrgow	27, 10, 9
	vspltw	9, 28, 3
	vspltw	10, 28, 2
	vmrgow	28, 10, 9
	vspltw	9, 29, 3
	vspltw	10, 29, 2
	vmrgow	29, 10, 9
	vspltw	9, 30, 3
	vspltw	10, 30, 2
	vmrgow	30, 10, 9

	vsld	9, 27, 13
	vsld	10, 28, 13
	vsld	11, 29, 13
	vsld	12, 30, 13
	vaddudm	0, 9, 27
	vaddudm	1, 10, 28
	vaddudm	2, 11, 29
	vaddudm	3, 12, 30
.endm

do_mul:
	mul_odd

	# do reduction ( h %= p )
	# carry reduction
	vspltisb 9, 2
	vsrd	10, 14, 31
	vsrd	11, 17, 31
	vand	7, 17, 25
	vand	4, 14, 25
	vaddudm	18, 18, 11
	vsrd	12, 18, 31
	vaddudm	15, 15, 10

	vsrd	11, 15, 31
	vand	8, 18, 25
	vand	5, 15, 25
	vaddudm	4, 4, 12
	vsld	10, 12, 9
	vaddudm	6, 16, 11

	vsrd	13, 6, 31
	vand	6, 6, 25
	vaddudm	4, 4, 10
	vsrd	10, 4, 31
	vaddudm	7, 7, 13

	vsrd	11, 7, 31
	vand	7, 7, 25
	vand	4, 4, 25
	vaddudm	5, 5, 10
	vaddudm	8, 8, 11
	blr

#
# init key
#
do_poly1305_init:
	ld	10, rmask@got(2)
	ld	11, 0(10)
	ld	12, 8(10)

	li	14, 16
	li	15, 32
	ld	10, cnum@got(2)
	lvx	25, 0, 10	# v25 - mask
	lvx	31, 14, 10	# v31 = 1a
	lvx	19, 15, 10	# v19 = 1 << 24
	lxv	24, 48(10)	# vs24
	lxv	25, 64(10)	# vs25

	# initialize
	# load key from r3 to vectors
	ld	9, 16(3)
	ld	10, 24(3)
	ld	11, 0(3)
	ld	12, 8(3)

	# break 26 bits
	extrdi	14, 9, 26, 38
	extrdi	15, 9, 26, 12
	extrdi	16, 9, 12, 0
	mtvsrdd	58, 0, 14
	insrdi	16, 10, 14, 38
	mtvsrdd	59, 0, 15
	extrdi	17, 10, 26, 24
	mtvsrdd	60, 0, 16
	extrdi	18, 10, 24, 0
	mtvsrdd	61, 0, 17
	mtvsrdd	62, 0, 18

	# r1 = r1 * 5, r2 = r2 * 5, r3 = r3 * 5, r4 = r4 * 5
	li	9, 5
	mtvsrdd	36, 0, 9
	vmulouw	0, 27, 4		# v0 = rr0
	vmulouw	1, 28, 4		# v1 = rr1
	vmulouw	2, 29, 4		# v2 = rr2
	vmulouw	3, 30, 4		# v3 = rr3
	blr

#
# gcry_poly1305_p10le_4blocks( uint8_t *k, uint32_t mlen, uint8_t *m)
#  k = 32 bytes key
#  r3 = k (r, s)
#  r4 = mlen
#  r5 = m
#
.global gcry_poly1305_p10le_4blocks
.align 5
gcry_poly1305_p10le_4blocks:
_gcry_poly1305_p10le_4blocks:
	cmpdi	5, 128
	blt	Out_no_poly1305

	stdu 1,-1024(1)
	mflr 0

	std     14,112(1)
	std     15,120(1)
	std     16,128(1)
	std     17,136(1)
	std     18,144(1)
	std     19,152(1)
	std     20,160(1)
	std     21,168(1)
	std     31,248(1)
	li	14, 256
	stvx	20, 14, 1
	addi	14, 14, 16
	stvx	21, 14, 1
	addi	14, 14, 16
	stvx	22, 14, 1
	addi	14, 14, 16
	stvx	23, 14, 1
	addi	14, 14, 16
	stvx	24, 14, 1
	addi	14, 14, 16
	stvx	25, 14, 1
	addi	14, 14, 16
	stvx	26, 14, 1
	addi	14, 14, 16
	stvx	27, 14, 1
	addi	14, 14, 16
	stvx	28, 14, 1
	addi	14, 14, 16
	stvx	29, 14, 1
	addi	14, 14, 16
	stvx	30, 14, 1
	addi	14, 14, 16
	stvx	31, 14, 1

	addi	14, 14, 16
	stxvx	14, 14, 1
	addi	14, 14, 16
	stxvx	15, 14, 1
	addi	14, 14, 16
	stxvx	16, 14, 1
	addi	14, 14, 16
	stxvx	17, 14, 1
	addi	14, 14, 16
	stxvx	18, 14, 1
	addi	14, 14, 16
	stxvx	19, 14, 1
	addi	14, 14, 16
	stxvx	20, 14, 1
	addi	14, 14, 16
	stxvx	21, 14, 1
	addi	14, 14, 16
	stxvx	22, 14, 1
	addi	14, 14, 16
	stxvx	23, 14, 1
	addi	14, 14, 16
	stxvx	24, 14, 1
	addi	14, 14, 16
	stxvx	25, 14, 1
	addi	14, 14, 16
	stxvx	26, 14, 1
	addi	14, 14, 16
	stxvx	27, 14, 1
	addi	14, 14, 16
	stxvx	28, 14, 1
	addi	14, 14, 16
	stxvx	29, 14, 1
	addi	14, 14, 16
	stxvx	30, 14, 1
	addi	14, 14, 16
	stxvx	31, 14, 1
	std	0, 1040(1)

	bl do_poly1305_init

	li	21, 0	# counter to message

	poly1305_setup_r

	# load previous state
	# break/convert r6 to 26 bits
	ld	9, 32(3)
	ld	10, 40(3)
	lwz	19, 48(3)
	sldi	19, 19, 24
	mtvsrdd	41, 0, 19
	extrdi	14, 9, 26, 38
	extrdi	15, 9, 26, 12
	extrdi	16, 9, 12, 0
	mtvsrdd	36, 0, 14
	insrdi	16, 10, 14, 38
	mtvsrdd	37, 0, 15
	extrdi	17, 10, 26, 24
	mtvsrdd	38, 0, 16
	extrdi	18, 10, 24, 0
	mtvsrdd	39, 0, 17
	mtvsrdd	40, 0, 18
	vor	8, 8, 9

	# input m1 m2
	add	20, 4, 21
	xxlor	49, 24, 24
	xxlor	50, 25, 25
	lxvw4x	43, 0, 20
	addi	17, 20, 16
	lxvw4x	44, 0, 17
	vperm	14, 11, 12, 17
	vperm	15, 11, 12, 18
	vand	9, 14, 25	# a0
	vsrd	10, 14, 31	# >> 26
	vsrd	11, 10, 31	# 12 bits left
	vand	10, 10, 25	# a1
	vspltisb 13, 12
	vand	16, 15, 25
	vsld	12, 16, 13
	vor	11, 11, 12
	vand	11, 11, 25	# a2
	vspltisb 13, 14
	vsrd	12, 15, 13	# >> 14
	vsrd	13, 12, 31	# >> 26, a4
	vand	12, 12, 25	# a3

	vaddudm	20, 4, 9
	vaddudm	21, 5, 10
	vaddudm	22, 6, 11
	vaddudm	23, 7, 12
	vaddudm	24, 8, 13

	# m3 m4
	addi	17, 17, 16
	lxvw4x	43, 0, 17
	addi	17, 17, 16
	lxvw4x	44, 0, 17
	vperm	14, 11, 12, 17
	vperm	15, 11, 12, 18
	vand	9, 14, 25	# a0
	vsrd	10, 14, 31	# >> 26
	vsrd	11, 10, 31	# 12 bits left
	vand	10, 10, 25	# a1
	vspltisb 13, 12
	vand	16, 15, 25
	vsld	12, 16, 13
	vspltisb 13, 14
	vor	11, 11, 12
	vand	11, 11, 25	# a2
	vsrd	12, 15, 13	# >> 14
	vsrd	13, 12, 31	# >> 26, a4
	vand	12, 12, 25	# a3

	# Smash 4 message blocks into 5 vectors of [m4,  m2,  m3,  m1]
	vmrgow	4, 9, 20
	vmrgow	5, 10, 21
	vmrgow	6, 11, 22
	vmrgow	7, 12, 23
	vmrgow	8, 13, 24
	vaddudm	8, 8, 19

	addi	5, 5, -64
	addi	21, 21, 64

	li      9, 64
	divdu   31, 5, 9

	mtctr	31

# h4 =   m1 * r⁴ + m2 * r³ + m3 * r² + m4 * r
# Rewrite the polynominal sum of product as follows,
# h1 = (h0 + m1) * r^2,	h2 = (h0 + m2) * r^2
# h3 = (h1 + m3) * r^2,	h4 = (h2 + m4) * r^2  --> (h0 + m1) r*4 + (h3 + m3) r^2, (h0 + m2) r^4 + (h0 + m4) r^2
#  .... Repeat
# h5 = (h3 + m5) * r^2,	h6 = (h4 + m6) * r^2  -->
# h7 = (h5 + m7) * r^2,	h8 = (h6 + m8) * r^1  --> m5 * r^4 + m6 * r^3 + m7 * r^2 + m8 * r
#
loop_4blocks:

	# Multiply odd words and even words
	mul_odd
	mul_even
	# carry reduction
	vspltisb 9, 2
	vsrd	10, 14, 31
	vsrd	11, 17, 31
	vand	7, 17, 25
	vand	4, 14, 25
	vaddudm	18, 18, 11
	vsrd	12, 18, 31
	vaddudm	15, 15, 10

	vsrd	11, 15, 31
	vand	8, 18, 25
	vand	5, 15, 25
	vaddudm	4, 4, 12
	vsld	10, 12, 9
	vaddudm	6, 16, 11

	vsrd	13, 6, 31
	vand	6, 6, 25
	vaddudm	4, 4, 10
	vsrd	10, 4, 31
	vaddudm	7, 7, 13

	vsrd	11, 7, 31
	vand	7, 7, 25
	vand	4, 4, 25
	vaddudm	5, 5, 10
	vaddudm	8, 8, 11

	# input m1  m2  m3  m4
	add	20, 4, 21
	xxlor	49, 24, 24
	xxlor	50, 25, 25
	lxvw4x	43, 0, 20
	addi	17, 20, 16
	lxvw4x	44, 0, 17
	vperm	14, 11, 12, 17
	vperm	15, 11, 12, 18
	addi	17, 17, 16
	lxvw4x	43, 0, 17
	addi	17, 17, 16
	lxvw4x	44, 0, 17
	vperm	17, 11, 12, 17
	vperm	18, 11, 12, 18

	vand	20, 14, 25	# a0
	vand	9, 17, 25	# a0
	vsrd	21, 14, 31	# >> 26
	vsrd	22, 21, 31	# 12 bits left
	vsrd	10, 17, 31	# >> 26
	vsrd	11, 10, 31	# 12 bits left

	vand	21, 21, 25	# a1
	vand	10, 10, 25	# a1

	vspltisb 13, 12
	vand	16, 15, 25
	vsld	23, 16, 13
	vor	22, 22, 23
	vand	22, 22, 25	# a2
	vand	16, 18, 25
	vsld	12, 16, 13
	vor	11, 11, 12
	vand	11, 11, 25	# a2
	vspltisb 13, 14
	vsrd	23, 15, 13	# >> 14
	vsrd	24, 23, 31	# >> 26, a4
	vand	23, 23, 25	# a3
	vsrd	12, 18, 13	# >> 14
	vsrd	13, 12, 31	# >> 26, a4
	vand	12, 12, 25	# a3

	vaddudm	4, 4, 20
	vaddudm	5, 5, 21
	vaddudm	6, 6, 22
	vaddudm	7, 7, 23
	vaddudm	8, 8, 24

	# Smash 4 message blocks into 5 vectors of [m4,  m2,  m3,  m1]
	vmrgow	4, 9, 4
	vmrgow	5, 10, 5
	vmrgow	6, 11, 6
	vmrgow	7, 12, 7
	vmrgow	8, 13, 8
	vaddudm	8, 8, 19

	addi	5, 5, -64
	addi	21, 21, 64

	bdnz	loop_4blocks

	xxlor	58, 0, 0
	xxlor	59, 1, 1
	xxlor	60, 2, 2
	xxlor	61, 3, 3
	xxlor	62, 4, 4
	xxlor	32, 5, 5
	xxlor	33, 6, 6
	xxlor	34, 7, 7
	xxlor	35, 8, 8

	# Multiply odd words and even words
	mul_odd
	mul_even

	# Sum the products.
	xxpermdi 41, 31, 46, 0
	xxpermdi 42, 31, 47, 0
	vaddudm	4, 14, 9
	xxpermdi 36, 31, 36, 3
	vaddudm	5, 15, 10
	xxpermdi 37, 31, 37, 3
	xxpermdi 43, 31, 48, 0
	vaddudm	6, 16, 11
	xxpermdi 38, 31, 38, 3
	xxpermdi 44, 31, 49, 0
	vaddudm	7, 17, 12
	xxpermdi 39, 31, 39, 3
	xxpermdi 45, 31, 50, 0
	vaddudm	8, 18, 13
	xxpermdi 40, 31, 40, 3

	# carry reduction
	vspltisb 9, 2
	vsrd	10, 4, 31
	vsrd	11, 7, 31
	vand	7, 7, 25
	vand	4, 4, 25
	vaddudm	8, 8, 11
	vsrd	12, 8, 31
	vaddudm	5, 5, 10

	vsrd	11, 5, 31
	vand	8, 8, 25
	vand	5, 5, 25
	vaddudm	4, 4, 12
	vsld	10, 12, 9
	vaddudm	6, 6, 11

	vsrd	13, 6, 31
	vand	6, 6, 25
	vaddudm	4, 4, 10
	vsrd	10, 4, 31
	vaddudm	7, 7, 13

	vsrd	11, 7, 31
	vand	7, 7, 25
	vand	4, 4, 25
	vaddudm	5, 5, 10
	vaddudm	8, 8, 11

	b	do_final_update

do_final_update:
	# v4, v5, v6, v7 and v8 are 26 bit vectors
	vsld	5, 5, 31
	vor	20, 4, 5
	vspltisb 11, 12
	vsrd	12, 6, 11
	vsld	6, 6, 31
	vsld	6, 6, 31
	vor	20, 20, 6
	vspltisb 11, 14
	vsld	7, 7, 11
	vor	21, 7, 12
	mfvsrld	16, 40		# save last 2 bytes
	vsld	8, 8, 11
	vsld	8, 8, 31
	vor	21, 21, 8
	mfvsrld	17, 52
	mfvsrld	19, 53
	srdi	16, 16, 24

	std	17, 32(3)
	std	19, 40(3)
	stw	16, 48(3)

Out_loop:
	li	3, 0

	li	14, 256
	lvx	20, 14, 1
	addi	14, 14, 16
	lvx	21, 14, 1
	addi	14, 14, 16
	lvx	22, 14, 1
	addi	14, 14, 16
	lvx	23, 14, 1
	addi	14, 14, 16
	lvx	24, 14, 1
	addi	14, 14, 16
	lvx	25, 14, 1
	addi	14, 14, 16
	lvx	26, 14, 1
	addi	14, 14, 16
	lvx	27, 14, 1
	addi	14, 14, 16
	lvx	28, 14, 1
	addi	14, 14, 16
	lvx	29, 14, 1
	addi	14, 14, 16
	lvx	30, 14, 1
	addi	14, 14, 16
	lvx	31, 14, 1

	addi	14, 14, 16
	lxvx	14, 14, 1
	addi	14, 14, 16
	lxvx	15, 14, 1
	addi	14, 14, 16
	lxvx	16, 14, 1
	addi	14, 14, 16
	lxvx	17, 14, 1
	addi	14, 14, 16
	lxvx	18, 14, 1
	addi	14, 14, 16
	lxvx	19, 14, 1
	addi	14, 14, 16
	lxvx	20, 14, 1
	addi	14, 14, 16
	lxvx	21, 14, 1
	addi	14, 14, 16
	lxvx	22, 14, 1
	addi	14, 14, 16
	lxvx	23, 14, 1
	addi	14, 14, 16
	lxvx	24, 14, 1
	addi	14, 14, 16
	lxvx	25, 14, 1
	addi	14, 14, 16
	lxvx	26, 14, 1
	addi	14, 14, 16
	lxvx	27, 14, 1
	addi	14, 14, 16
	lxvx	28, 14, 1
	addi	14, 14, 16
	lxvx	29, 14, 1
	addi	14, 14, 16
	lxvx	30, 14, 1
	addi	14, 14, 16
	lxvx	31, 14, 1

	ld	0, 1040(1)
	ld      14,112(1)
	ld      15,120(1)
	ld      16,128(1)
	ld      17,136(1)
	ld      18,144(1)
	ld      19,152(1)
	ld      20,160(1)
	ld	21,168(1)
	ld	31,248(1)

	mtlr	0
	addi	1, 1, 1024
	blr

Out_no_poly1305:
	li	3, 0
	blr

.data
.align 5
rmask:
.byte	0xff, 0xff, 0xff, 0x0f, 0xfc, 0xff, 0xff, 0x0f, 0xfc, 0xff, 0xff, 0x0f, 0xfc, 0xff, 0xff, 0x0f
cnum:
.long	0x03ffffff, 0x00000000, 0x03ffffff, 0x00000000
.long	0x1a, 0x00, 0x1a, 0x00
.long	0x01000000, 0x01000000, 0x01000000, 0x01000000
.long	0x00010203, 0x04050607, 0x10111213, 0x14151617
.long	0x08090a0b, 0x0c0d0e0f, 0x18191a1b, 0x1c1d1e1f
.long	0x05, 0x00, 0x00, 0x00
.long	0x02020202, 0x02020202, 0x02020202, 0x02020202
.long	0xffffffff, 0xffffffff, 0x00000000, 0x00000000
