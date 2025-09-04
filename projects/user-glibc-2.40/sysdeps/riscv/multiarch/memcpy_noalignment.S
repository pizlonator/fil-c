/* memcpy for RISC-V, ignoring buffer alignment
   Copyright (C) 2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <sys/asm.h>

/* memcpy optimization for CPUs with fast unaligned support
   (RISCV_HWPROBE_MISALIGNED_FAST).

   Copies are split into 3 main cases: small copies up to SZREG, copies up to
   BLOCK_SIZE (128 for 64 bits, 64 for 32 bits), and copies larger than BLOCK_SIZE.

   Large copies use a software pipelined loop processing BLOCK_SIZE bytes per
   iteration.  The destination pointer is SZREG-byte aligned to minimize store
   unaligned accesses.

   The tail is handled with branchless copies.  */

#define BLOCK_SIZE (16 * SZREG)

	.attribute unaligned_access, 1
ENTRY (__memcpy_noalignment)
	beq	a2, zero, L(ret)

	/* if LEN < SZREG jump to tail handling.  */
	li	a5, SZREG-1
	mv	a6, a0
	bleu	a2, a5, L(tail)

	/* Copy the first word, align DEST to word, and adjust DEST/SRC/LEN
	   based on the amount adjusted to align DEST.  */
	REG_L	a3, 0(a1)
	andi	a5, a0, SZREG-1
	addi	a2, a2, -SZREG
	li	a4, SZREG
	sub	a4, a4, a5
	REG_S	a3, 0(a0)
	add	a2, a5, a2

	/* If LEN < BLOCK_SIZE jump to word copy.  */
	li	a3, BLOCK_SIZE-1
	add	a5, a0, a4
	add	a1, a1, a4
	bleu	a2, a3, L(word_copy_adjust)
	addi	a7, a2, -BLOCK_SIZE
	andi	a7, a7, -BLOCK_SIZE
	addi	a7, a7, BLOCK_SIZE
	add	a3, a5, a7
	mv	a4, a1
L(block_copy):
	REG_L	a6,          0(a4)
	REG_L	t0,      SZREG(a4)
	REG_L	t1,  (2*SZREG)(a4)
	REG_L	t2,  (3*SZREG)(a4)
	REG_L	t3,  (4*SZREG)(a4)
	REG_L	t4,  (5*SZREG)(a4)
	REG_L	t5,  (6*SZREG)(a4)
	REG_L	t6,  (7*SZREG)(a4)
	REG_S	a6,          0(a5)
	REG_S	t0,      SZREG(a5)
	REG_S	t1,  (2*SZREG)(a5)
	REG_S	t2,  (3*SZREG)(a5)
	REG_S	t3,  (4*SZREG)(a5)
	REG_S	t4,  (5*SZREG)(a5)
	REG_S	t5,  (6*SZREG)(a5)
	REG_S	t6,  (7*SZREG)(a5)
	REG_L	a6,  (8*SZREG)(a4)
	REG_L	t0,  (9*SZREG)(a4)
	REG_L	t1, (10*SZREG)(a4)
	REG_L	t2, (11*SZREG)(a4)
	REG_L	t3, (12*SZREG)(a4)
	REG_L	t4, (13*SZREG)(a4)
	REG_L	t5, (14*SZREG)(a4)
	REG_L	t6, (15*SZREG)(a4)
	addi	a4, a4, BLOCK_SIZE
	REG_S	a6,  (8*SZREG)(a5)
	REG_S	t0,  (9*SZREG)(a5)
	REG_S	t1, (10*SZREG)(a5)
	REG_S	t2, (11*SZREG)(a5)
	REG_S	t3, (12*SZREG)(a5)
	REG_S	t4, (13*SZREG)(a5)
	REG_S	t5, (14*SZREG)(a5)
	REG_S	t6, (15*SZREG)(a5)
	addi	a5, a5, BLOCK_SIZE
	bne	a5, a3, L(block_copy)
	add	a1, a1, a7
	andi	a2, a2, BLOCK_SIZE-1

	/* 0 <= a2/LEN  < BLOCK_SIZE.  */
L(word_copy):
	li	a5, SZREG-1
	/* if LEN < SZREG jump to tail handling.  */
	bleu	a2, a5, L(tail_adjust)
	addi	a7, a2, -SZREG
	andi	a7, a7, -SZREG
	addi	a7, a7, SZREG
	add	a6, a3, a7
	mv	a5, a1
L(word_copy_loop):
	REG_L	a4, 0(a5)
	addi	a3, a3, SZREG
	addi	a5, a5, SZREG
	REG_S	a4, -SZREG(a3)
	bne	a3, a6, L(word_copy_loop)
	add	a1, a1, a7
	andi	a2, a2, SZREG-1

	/* Copy the last word unaligned.  */
	add	a3, a1, a2
	add	a4, a6, a2
	REG_L	t0, -SZREG(a3)
	REG_S	t0, -SZREG(a4)
	ret

L(tail):
	/* Copy 4-7 bytes.  */
	andi	a5, a2, 4
	add	a3, a1, a2
	add	a4, a6, a2
	beq	a5, zero, L(copy_0_3)
	lw	t0, 0(a1)
	lw	t1, -4(a3)
	sw	t0, 0(a6)
	sw	t1, -4(a4)
	ret

	/* Copy 0-3 bytes.  */
L(copy_0_3):
	beq	a2, zero, L(ret)
	srli    a2, a2, 1
	add     t4, a1, a2
	add     t5, a6, a2
	lbu     t0, 0(a1)
	lbu     t1, -1(a3)
	lbu     t2, 0(t4)
	sb      t0, 0(a6)
	sb      t1, -1(a4)
	sb      t2, 0(t5)
L(ret):
	ret
L(tail_adjust):
	mv	a6, a3
	j	L(tail)
L(word_copy_adjust):
	mv	a3, a5
	j	L(word_copy)
END (__memcpy_noalignment)
