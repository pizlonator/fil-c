/* VAES/AVX2 i386 accelerated AES for Libgcrypt
 * Copyright (C) 2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#if defined(__i386__)
#include <config.h>
#if (defined(HAVE_COMPATIBLE_GCC_I386_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN32_PLATFORM_AS)) && \
    defined(ENABLE_AESNI_SUPPORT) && defined(ENABLE_AVX2_SUPPORT) && \
    defined(HAVE_GCC_INLINE_ASM_VAES_VPCLMUL)

#include "asm-common-i386.h"

.text

DECL_GET_PC_THUNK(eax);

/**********************************************************************
  helper macros
 **********************************************************************/
#define AES_OP4(op, key, b0, b1, b2, b3) \
	op key, b0, b0; \
	op key, b1, b1; \
	op key, b2, b2; \
	op key, b3, b3;

#define VAESENC4(key, b0, b1, b2, b3) \
	AES_OP4(vaesenc, key, b0, b1, b2, b3)

#define VAESDEC4(key, b0, b1, b2, b3) \
	AES_OP4(vaesdec, key, b0, b1, b2, b3)

#define XOR4(key, b0, b1, b2, b3) \
	AES_OP4(vpxor, key, b0, b1, b2, b3)

#define AES_OP2(op, key, b0, b1) \
	op key, b0, b0; \
	op key, b1, b1;

#define VAESENC2(key, b0, b1) \
	AES_OP2(vaesenc, key, b0, b1)

#define VAESDEC2(key, b0, b1) \
	AES_OP2(vaesdec, key, b0, b1)

#define XOR2(key, b0, b1) \
	AES_OP2(vpxor, key, b0, b1)

#define VAESENC6(key, b0, b1, b2, b3, b4, b5) \
	AES_OP4(vaesenc, key, b0, b1, b2, b3); \
	AES_OP2(vaesenc, key, b4, b5)

#define VAESDEC6(key, b0, b1, b2, b3, b4, b5) \
	AES_OP4(vaesdec, key, b0, b1, b2, b3); \
	AES_OP2(vaesdec, key, b4, b5)

#define XOR6(key, b0, b1, b2, b3, b4, b5) \
	AES_OP4(vpxor, key, b0, b1, b2, b3); \
	AES_OP2(vpxor, key, b4, b5)

#define CADDR(name, reg) \
	(name - SYM_NAME(_gcry_vaes_consts))(reg)

/**********************************************************************
  CBC-mode decryption
 **********************************************************************/
ELF(.type SYM_NAME(_gcry_vaes_avx2_cbc_dec_i386),@function)
.globl SYM_NAME(_gcry_vaes_avx2_cbc_dec_i386)
.align 16
SYM_NAME(_gcry_vaes_avx2_cbc_dec_i386):
	/* input:
	 *	(esp + 4): round keys
	 *	(esp + 8): iv
	 *	(esp + 12): dst
	 *	(esp + 16): src
	 *	(esp + 20): nblocks
	 *	(esp + 24): nrounds
	 */
	CFI_STARTPROC();
	pushl %edi;
	CFI_PUSH(%edi);
	pushl %esi;
	CFI_PUSH(%esi);

	movl 8+4(%esp), %edi;
	movl 8+8(%esp), %esi;
	movl 8+12(%esp), %edx;
	movl 8+16(%esp), %ecx;
	movl 8+20(%esp), %eax;

	/* Process 8 blocks per loop. */
.align 8
.Lcbc_dec_blk8:
	cmpl $8, %eax;
	jb .Lcbc_dec_blk4;

	leal -8(%eax), %eax;

	/* Load input and xor first key. Update IV. */
	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	vmovdqu (0 * 16)(%ecx), %ymm0;
	vmovdqu (2 * 16)(%ecx), %ymm1;
	vmovdqu (4 * 16)(%ecx), %ymm2;
	vmovdqu (6 * 16)(%ecx), %ymm3;
	vmovdqu (%esi), %xmm6; /* Load IV. */
	vinserti128 $1, %xmm0, %ymm6, %ymm5;
	vextracti128 $1, %ymm3, (%esi); /* Store IV. */
	vpxor %ymm4, %ymm0, %ymm0;
	vpxor %ymm4, %ymm1, %ymm1;
	vpxor %ymm4, %ymm2, %ymm2;
	vpxor %ymm4, %ymm3, %ymm3;
	vmovdqu (1 * 16)(%ecx), %ymm6;
	vmovdqu (3 * 16)(%ecx), %ymm7;

	/* AES rounds */
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (2 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (3 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (4 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (5 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (6 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (7 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (8 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (9 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (10 * 16)(%edi), %ymm4;
	cmpl $12, 8+24(%esp);
	jb .Lcbc_dec_blk8_last;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (11 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (12 * 16)(%edi), %ymm4;
	jz .Lcbc_dec_blk8_last;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (13 * 16)(%edi), %ymm4;
	VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (14 * 16)(%edi), %ymm4;

	/* Last round and output handling. */
  .Lcbc_dec_blk8_last:
	vpxor %ymm4, %ymm5, %ymm5;
	vpxor %ymm4, %ymm6, %ymm6;
	vpxor %ymm4, %ymm7, %ymm7;
	vpxor (5 * 16)(%ecx), %ymm4, %ymm4;
	leal (8 * 16)(%ecx), %ecx;
	vaesdeclast %ymm5, %ymm0, %ymm0;
	vaesdeclast %ymm6, %ymm1, %ymm1;
	vaesdeclast %ymm7, %ymm2, %ymm2;
	vaesdeclast %ymm4, %ymm3, %ymm3;
	vmovdqu %ymm0, (0 * 16)(%edx);
	vmovdqu %ymm1, (2 * 16)(%edx);
	vmovdqu %ymm2, (4 * 16)(%edx);
	vmovdqu %ymm3, (6 * 16)(%edx);
	leal (8 * 16)(%edx), %edx;

	jmp .Lcbc_dec_blk8;

	/* Handle trailing four blocks. */
.align 8
.Lcbc_dec_blk4:
	cmpl $4, %eax;
	jb .Lcbc_dec_blk1;

	leal -4(%eax), %eax;

	/* Load input and xor first key. Update IV. */
	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	vmovdqu (0 * 16)(%ecx), %ymm0;
	vmovdqu (2 * 16)(%ecx), %ymm1;
	vmovdqu (%esi), %xmm6; /* Load IV. */
	vinserti128 $1, %xmm0, %ymm6, %ymm5;
	vextracti128 $1, %ymm1, (%esi); /* Store IV. */
	vpxor %ymm4, %ymm0, %ymm0;
	vpxor %ymm4, %ymm1, %ymm1;
	vmovdqu (1 * 16)(%ecx), %ymm6;
	leal (4 * 16)(%ecx), %ecx;

	/* AES rounds */
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (2 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (3 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (4 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (5 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (6 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (7 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (8 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (9 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (10 * 16)(%edi), %ymm4;
	cmpl $12, 8+24(%esp);
	jb .Lcbc_dec_blk4_last;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (11 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (12 * 16)(%edi), %ymm4;
	jz .Lcbc_dec_blk4_last;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (13 * 16)(%edi), %ymm4;
	VAESDEC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (14 * 16)(%edi), %ymm4;

	/* Last round and output handling. */
  .Lcbc_dec_blk4_last:
	vpxor %ymm4, %ymm5, %ymm5;
	vpxor %ymm4, %ymm6, %ymm6;
	vaesdeclast %ymm5, %ymm0, %ymm0;
	vaesdeclast %ymm6, %ymm1, %ymm1;
	vmovdqu %ymm0, (0 * 16)(%edx);
	vmovdqu %ymm1, (2 * 16)(%edx);
	leal (4 * 16)(%edx), %edx;

	/* Process trailing one to three blocks, one per loop. */
.align 8
.Lcbc_dec_blk1:
	cmpl $1, %eax;
	jb .Ldone_cbc_dec;

	leal -1(%eax), %eax;

	/* Load input. */
	vmovdqu (%ecx), %xmm2;
	leal 16(%ecx), %ecx;

	/* Xor first key. */
	vpxor (0 * 16)(%edi), %xmm2, %xmm0;

	/* AES rounds. */
	vaesdec (1 * 16)(%edi), %xmm0, %xmm0;
	vaesdec (2 * 16)(%edi), %xmm0, %xmm0;
	vaesdec (3 * 16)(%edi), %xmm0, %xmm0;
	vaesdec (4 * 16)(%edi), %xmm0, %xmm0;
	vaesdec (5 * 16)(%edi), %xmm0, %xmm0;
	vaesdec (6 * 16)(%edi), %xmm0, %xmm0;
	vaesdec (7 * 16)(%edi), %xmm0, %xmm0;
	vaesdec (8 * 16)(%edi), %xmm0, %xmm0;
	vaesdec (9 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (10 * 16)(%edi), %xmm1;
	cmpl $12, 8+24(%esp);
	jb .Lcbc_dec_blk1_last;
	vaesdec %xmm1, %xmm0, %xmm0;
	vaesdec (11 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (12 * 16)(%edi), %xmm1;
	jz .Lcbc_dec_blk1_last;
	vaesdec %xmm1, %xmm0, %xmm0;
	vaesdec (13 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (14 * 16)(%edi), %xmm1;

	/* Last round and output handling. */
  .Lcbc_dec_blk1_last:
	vpxor (%esi), %xmm1, %xmm1;
	vaesdeclast %xmm1, %xmm0, %xmm0;
	vmovdqu %xmm2, (%esi);
	vmovdqu %xmm0, (%edx);
	leal 16(%edx), %edx;

	jmp .Lcbc_dec_blk1;

.align 8
.Ldone_cbc_dec:
	popl %esi;
	CFI_POP(%esi);
	popl %edi;
	CFI_POP(%edi);
	vzeroall;
	ret_spec_stop
	CFI_ENDPROC();
ELF(.size SYM_NAME(_gcry_vaes_avx2_cbc_dec_i386),
	  .-SYM_NAME(_gcry_vaes_avx2_cbc_dec_i386))

/**********************************************************************
  CFB-mode decryption
 **********************************************************************/
ELF(.type SYM_NAME(_gcry_vaes_avx2_cfb_dec_i386),@function)
.globl SYM_NAME(_gcry_vaes_avx2_cfb_dec_i386)
.align 16
SYM_NAME(_gcry_vaes_avx2_cfb_dec_i386):
	/* input:
	 *	(esp + 4): round keys
	 *	(esp + 8): iv
	 *	(esp + 12): dst
	 *	(esp + 16): src
	 *	(esp + 20): nblocks
	 *	(esp + 24): nrounds
	 */
	CFI_STARTPROC();
	pushl %edi;
	CFI_PUSH(%edi);
	pushl %esi;
	CFI_PUSH(%esi);

	movl 8+4(%esp), %edi;
	movl 8+8(%esp), %esi;
	movl 8+12(%esp), %edx;
	movl 8+16(%esp), %ecx;
	movl 8+20(%esp), %eax;

	/* Process 8 blocks per loop. */
.align 8
.Lcfb_dec_blk8:
	cmpl $8, %eax;
	jb .Lcfb_dec_blk4;

	leal -8(%eax), %eax;

	/* Load IV. */
	vmovdqu (%esi), %xmm0;

	/* Load input and xor first key. Update IV. */
	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	vmovdqu (0 * 16)(%ecx), %ymm5;
	vinserti128 $1, %xmm5, %ymm0, %ymm0;
	vmovdqu (1 * 16)(%ecx), %ymm1;
	vmovdqu (3 * 16)(%ecx), %ymm2;
	vmovdqu (5 * 16)(%ecx), %ymm3;
	vmovdqu (7 * 16)(%ecx), %xmm6;
	vpxor %ymm4, %ymm0, %ymm0;
	vpxor %ymm4, %ymm1, %ymm1;
	vpxor %ymm4, %ymm2, %ymm2;
	vpxor %ymm4, %ymm3, %ymm3;
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	vmovdqu %xmm6, (%esi); /* Store IV. */
	vmovdqu (2 * 16)(%ecx), %ymm6;
	vmovdqu (4 * 16)(%ecx), %ymm7;

	/* AES rounds */
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (2 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (3 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (4 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (5 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (6 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (7 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (8 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (9 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (10 * 16)(%edi), %ymm4;
	cmpl $12, 8+24(%esp);
	jb .Lcfb_dec_blk8_last;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (11 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (12 * 16)(%edi), %ymm4;
	jz .Lcfb_dec_blk8_last;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (13 * 16)(%edi), %ymm4;
	VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
	vbroadcasti128 (14 * 16)(%edi), %ymm4;

	/* Last round and output handling. */
  .Lcfb_dec_blk8_last:
	vpxor %ymm4, %ymm5, %ymm5;
	vpxor %ymm4, %ymm6, %ymm6;
	vpxor %ymm4, %ymm7, %ymm7;
	vpxor (6 * 16)(%ecx), %ymm4, %ymm4;
	leal (8 * 16)(%ecx), %ecx;
	vaesenclast %ymm5, %ymm0, %ymm0;
	vaesenclast %ymm6, %ymm1, %ymm1;
	vaesenclast %ymm7, %ymm2, %ymm2;
	vaesenclast %ymm4, %ymm3, %ymm3;
	vmovdqu %ymm0, (0 * 16)(%edx);
	vmovdqu %ymm1, (2 * 16)(%edx);
	vmovdqu %ymm2, (4 * 16)(%edx);
	vmovdqu %ymm3, (6 * 16)(%edx);
	leal (8 * 16)(%edx), %edx;

	jmp .Lcfb_dec_blk8;

	/* Handle trailing four blocks. */
.align 8
.Lcfb_dec_blk4:
	cmpl $4, %eax;
	jb .Lcfb_dec_blk1;

	leal -4(%eax), %eax;

	/* Load IV. */
	vmovdqu (%esi), %xmm0;

	/* Load input and xor first key. Update IV. */
	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	vmovdqu (0 * 16)(%ecx), %ymm5;
	vinserti128 $1, %xmm5, %ymm0, %ymm0;
	vmovdqu (1 * 16)(%ecx), %ymm1;
	vmovdqu (3 * 16)(%ecx), %xmm6;
	vpxor %ymm4, %ymm0, %ymm0;
	vpxor %ymm4, %ymm1, %ymm1;
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	vmovdqu %xmm6, (%esi); /* Store IV. */
	vmovdqu (2 * 16)(%ecx), %ymm6;

	leal (4 * 16)(%ecx), %ecx;

	/* AES rounds */
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (2 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (3 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (4 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (5 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (6 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (7 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (8 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (9 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (10 * 16)(%edi), %ymm4;
	cmpl $12, 8+24(%esp);
	jb .Lcfb_dec_blk4_last;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (11 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (12 * 16)(%edi), %ymm4;
	jz .Lcfb_dec_blk4_last;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (13 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (14 * 16)(%edi), %ymm4;

	/* Last round and output handling. */
  .Lcfb_dec_blk4_last:
	vpxor %ymm4, %ymm5, %ymm5;
	vpxor %ymm4, %ymm6, %ymm6;
	vaesenclast %ymm5, %ymm0, %ymm0;
	vaesenclast %ymm6, %ymm1, %ymm1;
	vmovdqu %ymm0, (0 * 16)(%edx);
	vmovdqu %ymm1, (2 * 16)(%edx);
	leal (4 * 16)(%edx), %edx;

	/* Process trailing one to three blocks, one per loop. */
.align 8
.Lcfb_dec_blk1:
	cmpl $1, %eax;
	jb .Ldone_cfb_dec;

	leal -1(%eax), %eax;

	/* Load IV. */
	vmovdqu (%esi), %xmm0;

	/* Xor first key. */
	vpxor (0 * 16)(%edi), %xmm0, %xmm0;

	/* Load input as next IV. */
	vmovdqu (%ecx), %xmm2;
	leal 16(%ecx), %ecx;

	/* AES rounds. */
	vaesenc (1 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (2 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (3 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (4 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (5 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (6 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (7 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (8 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (9 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (10 * 16)(%edi), %xmm1;
	vmovdqu %xmm2, (%esi); /* Store IV. */
	cmpl $12, 8+24(%esp);
	jb .Lcfb_dec_blk1_last;
	vaesenc %xmm1, %xmm0, %xmm0;
	vaesenc (11 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (12 * 16)(%edi), %xmm1;
	jz .Lcfb_dec_blk1_last;
	vaesenc %xmm1, %xmm0, %xmm0;
	vaesenc (13 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (14 * 16)(%edi), %xmm1;

	/* Last round and output handling. */
  .Lcfb_dec_blk1_last:
	vpxor %xmm2, %xmm1, %xmm1;
	vaesenclast %xmm1, %xmm0, %xmm0;
	vmovdqu %xmm0, (%edx);
	leal 16(%edx), %edx;

	jmp .Lcfb_dec_blk1;

.align 8
.Ldone_cfb_dec:
	popl %esi;
	CFI_POP(%esi);
	popl %edi;
	CFI_POP(%edi);
	vzeroall;
	ret_spec_stop
	CFI_ENDPROC();
ELF(.size SYM_NAME(_gcry_vaes_avx2_cfb_dec_i386),
	  .-SYM_NAME(_gcry_vaes_avx2_cfb_dec_i386))

/**********************************************************************
  CTR-mode encryption
 **********************************************************************/
ELF(.type SYM_NAME(_gcry_vaes_avx2_ctr_enc_i386),@function)
.globl SYM_NAME(_gcry_vaes_avx2_ctr_enc_i386)
.align 16
SYM_NAME(_gcry_vaes_avx2_ctr_enc_i386):
	/* input:
	 *	(esp + 4): round keys
	 *	(esp + 8): iv
	 *	(esp + 12): dst
	 *	(esp + 16): src
	 *	(esp + 20): nblocks
	 *	(esp + 24): nrounds
	 */
	CFI_STARTPROC();

	GET_DATA_POINTER(SYM_NAME(_gcry_vaes_consts), eax);

	pushl %ebp;
	CFI_PUSH(%ebp);
	movl %esp, %ebp;
	CFI_DEF_CFA_REGISTER(%ebp);

	subl $(3 * 32 + 3 * 4), %esp;
	andl $-32, %esp;

	movl %edi, (3 * 32 + 0 * 4)(%esp);
	CFI_REG_ON_STACK(edi, 3 * 32 + 0 * 4);
	movl %esi, (3 * 32 + 1 * 4)(%esp);
	CFI_REG_ON_STACK(esi, 3 * 32 + 1 * 4);
	movl %ebx, (3 * 32 + 2 * 4)(%esp);
	CFI_REG_ON_STACK(ebx, 3 * 32 + 2 * 4);

	movl %eax, %ebx;
	movl 4+4(%ebp), %edi;
	movl 4+8(%ebp), %esi;
	movl 4+12(%ebp), %edx;
	movl 4+16(%ebp), %ecx;

#define prepare_ctr_const(minus_one, minus_two) \
	vpcmpeqd minus_one, minus_one, minus_one; \
	vpsrldq $8, minus_one, minus_one;       /* 0:-1 */ \
	vpaddq minus_one, minus_one, minus_two; /* 0:-2 */

#define inc_le128(x, minus_one, tmp) \
	vpcmpeqq minus_one, x, tmp; \
	vpsubq minus_one, x, x; \
	vpslldq $8, tmp, tmp; \
	vpsubq tmp, x, x;

#define add2_le128(x, minus_one, minus_two, tmp1, tmp2) \
	vpcmpeqq minus_one, x, tmp1; \
	vpcmpeqq minus_two, x, tmp2; \
	vpor tmp1, tmp2, tmp2; \
	vpsubq minus_two, x, x; \
	vpslldq $8, tmp2, tmp2; \
	vpsubq tmp2, x, x;

#define handle_ctr_128bit_add(nblks) \
	movl 12(%esi), %eax; \
	bswapl %eax; \
	addl $nblks, %eax; \
	bswapl %eax; \
	movl %eax, 12(%esi); \
	jnc 1f; \
	\
	movl 8(%esi), %eax; \
	bswapl %eax; \
	adcl $0, %eax; \
	bswapl %eax; \
	movl %eax, 8(%esi); \
	\
	movl 4(%esi), %eax; \
	bswapl %eax; \
	adcl $0, %eax; \
	bswapl %eax; \
	movl %eax, 4(%esi); \
	\
	movl 0(%esi), %eax; \
	bswapl %eax; \
	adcl $0, %eax; \
	bswapl %eax; \
	movl %eax, 0(%esi); \
	.align 8; \
	1:;

	cmpl $12, 4+20(%ebp);
	jae .Lctr_enc_blk12_loop;
	jmp .Lctr_enc_blk4;

	/* Process 12 blocks per loop. */
.align 16
.Lctr_enc_blk12_loop:
	subl $12, 4+20(%ebp);

	vbroadcasti128 (%esi), %ymm6;

	/* detect if carry handling is needed */
	movl 12(%esi), %eax;
	addl $(12 << 24), %eax;
	jc .Lctr_enc_blk12_handle_carry;
	movl %eax, 12(%esi);

  .Lctr_enc_blk12_byte_bige_add:
	/* Increment counters. */
	vpaddb CADDR(.Lbige_addb_0, %ebx), %ymm6, %ymm0;
	vpaddb CADDR(.Lbige_addb_2, %ebx), %ymm6, %ymm1;
	vpaddb CADDR(.Lbige_addb_4, %ebx), %ymm6, %ymm2;
	vpaddb CADDR(.Lbige_addb_6, %ebx), %ymm6, %ymm3;
	vpaddb CADDR(.Lbige_addb_8, %ebx), %ymm6, %ymm5;
	vpaddb CADDR(.Lbige_addb_10, %ebx), %ymm6, %ymm6;

  .Lctr_enc_blk12_rounds:
	/* AES rounds */
	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	XOR6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (2 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (3 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (4 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (5 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (6 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (7 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (8 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (9 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (10 * 16)(%edi), %ymm4;
	cmpl $12, 4+24(%ebp);
	jb .Lctr_enc_blk12_last;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (11 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (12 * 16)(%edi), %ymm4;
	jz .Lctr_enc_blk12_last;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (13 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (14 * 16)(%edi), %ymm4;

	/* Last round and output handling. */
  .Lctr_enc_blk12_last:
	vpxor (0 * 16)(%ecx), %ymm4, %ymm7; /* Xor src to last round key. */
	vaesenclast %ymm7, %ymm0, %ymm0;
	vmovdqu %ymm0, (0 * 16)(%edx);
	vpxor (2 * 16)(%ecx), %ymm4, %ymm7;
	vpxor (4 * 16)(%ecx), %ymm4, %ymm0;
	vaesenclast %ymm7, %ymm1, %ymm1;
	vaesenclast %ymm0, %ymm2, %ymm2;
	vpxor (6 * 16)(%ecx), %ymm4, %ymm7;
	vpxor (8 * 16)(%ecx), %ymm4, %ymm0;
	vpxor (10 * 16)(%ecx), %ymm4, %ymm4;
	leal (12 * 16)(%ecx), %ecx;
	vaesenclast %ymm7, %ymm3, %ymm3;
	vaesenclast %ymm0, %ymm5, %ymm5;
	vaesenclast %ymm4, %ymm6, %ymm6;
	vmovdqu %ymm1, (2 * 16)(%edx);
	vmovdqu %ymm2, (4 * 16)(%edx);
	vmovdqu %ymm3, (6 * 16)(%edx);
	vmovdqu %ymm5, (8 * 16)(%edx);
	vmovdqu %ymm6, (10 * 16)(%edx);
	leal (12 * 16)(%edx), %edx;

	cmpl $12, 4+20(%ebp);
	jae .Lctr_enc_blk12_loop;
	jmp .Lctr_enc_blk4;

  .align 8
  .Lctr_enc_blk12_handle_only_ctr_carry:
	handle_ctr_128bit_add(12);
	jmp .Lctr_enc_blk12_byte_bige_add;

  .align 8
  .Lctr_enc_blk12_handle_carry:
	jz .Lctr_enc_blk12_handle_only_ctr_carry;
	/* Increment counters (handle carry). */
	prepare_ctr_const(%ymm4, %ymm7);
	vmovdqa CADDR(.Lbswap128_mask, %ebx), %ymm2;
	vpshufb %xmm2, %xmm6, %xmm1; /* be => le */
	vmovdqa %xmm1, %xmm0;
	inc_le128(%xmm1, %xmm4, %xmm5);
	vinserti128 $1, %xmm1, %ymm0, %ymm6; /* ctr: +1:+0 */
	handle_ctr_128bit_add(12);
	vpshufb %ymm2, %ymm6, %ymm0;
	vmovdqa %ymm0, (0 * 32)(%esp);
	add2_le128(%ymm6, %ymm4, %ymm7, %ymm5, %ymm1); /* ctr: +3:+2 */
	vpshufb %ymm2, %ymm6, %ymm0;
	vmovdqa %ymm0, (1 * 32)(%esp);
	add2_le128(%ymm6, %ymm4, %ymm7, %ymm5, %ymm1); /* ctr: +5:+4 */
	vpshufb %ymm2, %ymm6, %ymm0;
	vmovdqa %ymm0, (2 * 32)(%esp);
	add2_le128(%ymm6, %ymm4, %ymm7, %ymm5, %ymm1); /* ctr: +7:+6 */
	vpshufb %ymm2, %ymm6, %ymm3;
	add2_le128(%ymm6, %ymm4, %ymm7, %ymm5, %ymm1); /* ctr: +9:+8 */
	vpshufb %ymm2, %ymm6, %ymm5;
	add2_le128(%ymm6, %ymm4, %ymm7, %ymm2, %ymm1); /* ctr: +11:+10 */
	vmovdqa (0 * 32)(%esp), %ymm0;
	vmovdqa (1 * 32)(%esp), %ymm1;
	vmovdqa (2 * 32)(%esp), %ymm2;
	vpshufb CADDR(.Lbswap128_mask, %ebx), %ymm6, %ymm6;

	jmp .Lctr_enc_blk12_rounds;

	/* Handle trailing four blocks. */
.align 8
.Lctr_enc_blk4:
	cmpl $4, 4+20(%ebp);
	jb .Lctr_enc_blk1;

	subl $4, 4+20(%ebp);

	vbroadcasti128 (%esi), %ymm3;

	/* detect if carry handling is needed */
	movl 12(%esi), %eax;
	addl $(4 << 24), %eax;
	jc .Lctr_enc_blk4_handle_carry;
	movl %eax, 12(%esi);

  .Lctr_enc_blk4_byte_bige_add:
	/* Increment counters. */
	vpaddb CADDR(.Lbige_addb_0, %ebx), %ymm3, %ymm0;
	vpaddb CADDR(.Lbige_addb_2, %ebx), %ymm3, %ymm1;

  .Lctr_enc_blk4_rounds:
	/* AES rounds */
	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	XOR2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (2 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (3 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (4 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (5 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (6 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (7 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (8 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (9 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (10 * 16)(%edi), %ymm4;
	cmpl $12, 4+24(%ebp);
	jb .Lctr_enc_blk4_last;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (11 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (12 * 16)(%edi), %ymm4;
	jz .Lctr_enc_blk4_last;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (13 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (14 * 16)(%edi), %ymm4;

	/* Last round and output handling. */
  .Lctr_enc_blk4_last:
	vpxor (0 * 16)(%ecx), %ymm4, %ymm5; /* Xor src to last round key. */
	vpxor (2 * 16)(%ecx), %ymm4, %ymm6;
	leal (4 * 16)(%ecx), %ecx;
	vaesenclast %ymm5, %ymm0, %ymm0;
	vaesenclast %ymm6, %ymm1, %ymm1;
	vmovdqu %ymm0, (0 * 16)(%edx);
	vmovdqu %ymm1, (2 * 16)(%edx);
	leal (4 * 16)(%edx), %edx;

	jmp .Lctr_enc_blk1;

  .align 8
  .Lctr_enc_blk4_handle_only_ctr_carry:
	handle_ctr_128bit_add(4);
	jmp .Lctr_enc_blk4_byte_bige_add;

  .align 8
  .Lctr_enc_blk4_handle_carry:
	jz .Lctr_enc_blk4_handle_only_ctr_carry;
	/* Increment counters (handle carry). */
	prepare_ctr_const(%ymm4, %ymm7);
	vpshufb CADDR(.Lbswap128_mask, %ebx), %xmm3, %xmm1; /* be => le */
	vmovdqa %xmm1, %xmm0;
	inc_le128(%xmm1, %xmm4, %xmm5);
	vinserti128 $1, %xmm1, %ymm0, %ymm3; /* ctr: +1:+0 */
	vpshufb CADDR(.Lbswap128_mask, %ebx), %ymm3, %ymm0;
	handle_ctr_128bit_add(4);
	add2_le128(%ymm3, %ymm4, %ymm7, %ymm5, %ymm6); /* ctr: +3:+2 */
	vpshufb CADDR(.Lbswap128_mask, %ebx), %ymm3, %ymm1;

	jmp .Lctr_enc_blk4_rounds;

	/* Process trailing one to three blocks, one per loop. */
.align 8
.Lctr_enc_blk1:
	cmpl $1, 4+20(%ebp);
	jb .Ldone_ctr_enc;

	subl $1, 4+20(%ebp);

	/* Load and increament counter. */
	vmovdqu (%esi), %xmm0;
	handle_ctr_128bit_add(1);

	/* AES rounds. */
	vpxor (0 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (1 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (2 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (3 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (4 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (5 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (6 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (7 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (8 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (9 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (10 * 16)(%edi), %xmm1;
	cmpl $12, 4+24(%ebp);
	jb .Lctr_enc_blk1_last;
	vaesenc %xmm1, %xmm0, %xmm0;
	vaesenc (11 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (12 * 16)(%edi), %xmm1;
	jz .Lctr_enc_blk1_last;
	vaesenc %xmm1, %xmm0, %xmm0;
	vaesenc (13 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (14 * 16)(%edi), %xmm1;

	/* Last round and output handling. */
  .Lctr_enc_blk1_last:
	vpxor (%ecx), %xmm1, %xmm1; /* Xor src to last round key. */
	leal 16(%ecx), %ecx;
	vaesenclast %xmm1, %xmm0, %xmm0; /* Last round and xor with xmm1. */
	vmovdqu %xmm0, (%edx);
	leal 16(%edx), %edx;

	jmp .Lctr_enc_blk1;

.align 8
.Ldone_ctr_enc:
	vpxor %ymm0, %ymm0, %ymm0;
	movl (3 * 32 + 0 * 4)(%esp), %edi;
	CFI_RESTORE(edi);
	movl (3 * 32 + 1 * 4)(%esp), %esi;
	CFI_RESTORE(esi);
	movl (3 * 32 + 2 * 4)(%esp), %ebx;
	CFI_RESTORE(ebx);
	vmovdqa %ymm0, (0 * 32)(%esp);
	vmovdqa %ymm0, (1 * 32)(%esp);
	vmovdqa %ymm0, (2 * 32)(%esp);
	leave;
	CFI_LEAVE();
	vzeroall;
	ret_spec_stop
	CFI_ENDPROC();
ELF(.size SYM_NAME(_gcry_vaes_avx2_ctr_enc_i386),
	  .-SYM_NAME(_gcry_vaes_avx2_ctr_enc_i386))

/**********************************************************************
  Little-endian 32-bit CTR-mode encryption (GCM-SIV)
 **********************************************************************/
ELF(.type SYM_NAME(_gcry_vaes_avx2_ctr32le_enc_i386),@function)
.globl SYM_NAME(_gcry_vaes_avx2_ctr32le_enc_i386)
.align 16
SYM_NAME(_gcry_vaes_avx2_ctr32le_enc_i386):
	/* input:
	 *	(esp + 4): round keys
	 *	(esp + 8): counter
	 *	(esp + 12): dst
	 *	(esp + 16): src
	 *	(esp + 20): nblocks
	 *	(esp + 24): nrounds
	 */
	CFI_STARTPROC();

	GET_DATA_POINTER(SYM_NAME(_gcry_vaes_consts), eax);

	pushl %ebp;
	CFI_PUSH(%ebp);
	movl %esp, %ebp;
	CFI_DEF_CFA_REGISTER(%ebp);

	subl $(3 * 4), %esp;

	movl %edi, (0 * 4)(%esp);
	CFI_REG_ON_STACK(edi, 0 * 4);
	movl %esi, (1 * 4)(%esp);
	CFI_REG_ON_STACK(esi, 1 * 4);
	movl %ebx, (2 * 4)(%esp);
	CFI_REG_ON_STACK(ebx, 2 * 4);

	movl %eax, %ebx;
	movl 4+4(%ebp), %edi;
	movl 4+8(%ebp), %esi;
	movl 4+12(%ebp), %edx;
	movl 4+16(%ebp), %ecx;
	movl 4+20(%ebp), %eax;

	vbroadcasti128 (%esi), %ymm7; /* Load CTR. */

	/* Process 12 blocks per loop. */
.align 8
.Lctr32le_enc_blk12:
	cmpl $12, %eax;
	jb .Lctr32le_enc_blk4;

	leal -12(%eax), %eax;

	vbroadcasti128 (0 * 16)(%edi), %ymm4;

	/* Increment counters. */
	vpaddd CADDR(.Lle_addd_0, %ebx), %ymm7, %ymm0;
	vpaddd CADDR(.Lle_addd_2, %ebx), %ymm7, %ymm1;
	vpaddd CADDR(.Lle_addd_4, %ebx), %ymm7, %ymm2;
	vpaddd CADDR(.Lle_addd_6, %ebx), %ymm7, %ymm3;
	vpaddd CADDR(.Lle_addd_8, %ebx), %ymm7, %ymm5;
	vpaddd CADDR(.Lle_addd_10, %ebx), %ymm7, %ymm6;

	vpaddd CADDR(.Lle_addd_12_2, %ebx), %ymm7, %ymm7;
	vmovdqu %xmm7, (%esi); /* Store CTR. */

	/* AES rounds */
	XOR6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (2 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (3 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (4 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (5 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (6 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (7 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (8 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (9 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (10 * 16)(%edi), %ymm4;
	cmpl $12, 4+24(%ebp);
	jb .Lctr32le_enc_blk8_last;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (11 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (12 * 16)(%edi), %ymm4;
	jz .Lctr32le_enc_blk8_last;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (13 * 16)(%edi), %ymm4;
	VAESENC6(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3, %ymm5, %ymm6);
	vbroadcasti128 (14 * 16)(%edi), %ymm4;

	/* Last round and output handling. */
  .Lctr32le_enc_blk8_last:
	vpxor (0 * 16)(%ecx), %ymm4, %ymm7; /* Xor src to last round key. */
	vaesenclast %ymm7, %ymm0, %ymm0;
	vpxor (2 * 16)(%ecx), %ymm4, %ymm7;
	vaesenclast %ymm7, %ymm1, %ymm1;
	vpxor (4 * 16)(%ecx), %ymm4, %ymm7;
	vaesenclast %ymm7, %ymm2, %ymm2;
	vpxor (6 * 16)(%ecx), %ymm4, %ymm7;
	vaesenclast %ymm7, %ymm3, %ymm3;
	vpxor (8 * 16)(%ecx), %ymm4, %ymm7;
	vpxor (10 * 16)(%ecx), %ymm4, %ymm4;
	vaesenclast %ymm7, %ymm5, %ymm5;
	vbroadcasti128 (%esi), %ymm7; /* Reload CTR. */
	vaesenclast %ymm4, %ymm6, %ymm6;
	leal (12 * 16)(%ecx), %ecx;
	vmovdqu %ymm0, (0 * 16)(%edx);
	vmovdqu %ymm1, (2 * 16)(%edx);
	vmovdqu %ymm2, (4 * 16)(%edx);
	vmovdqu %ymm3, (6 * 16)(%edx);
	vmovdqu %ymm5, (8 * 16)(%edx);
	vmovdqu %ymm6, (10 * 16)(%edx);
	leal (12 * 16)(%edx), %edx;

	jmp .Lctr32le_enc_blk12;

	/* Handle trailing four blocks. */
.align 8
.Lctr32le_enc_blk4:
	cmpl $4, %eax;
	jb .Lctr32le_enc_blk1;

	leal -4(%eax), %eax;

	vbroadcasti128 (0 * 16)(%edi), %ymm4;

	/* Increment counters. */
	vpaddd CADDR(.Lle_addd_0, %ebx), %ymm7, %ymm0;
	vpaddd CADDR(.Lle_addd_2, %ebx), %ymm7, %ymm1;

	vpaddd CADDR(.Lle_addd_4_2, %ebx), %ymm7, %ymm7;

	/* AES rounds */
	XOR2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (2 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (3 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (4 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (5 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (6 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (7 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (8 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (9 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (10 * 16)(%edi), %ymm4;
	cmpl $12, 4+24(%ebp);
	jb .Lctr32le_enc_blk4_last;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (11 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (12 * 16)(%edi), %ymm4;
	jz .Lctr32le_enc_blk4_last;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (13 * 16)(%edi), %ymm4;
	VAESENC2(%ymm4, %ymm0, %ymm1);
	vbroadcasti128 (14 * 16)(%edi), %ymm4;

	/* Last round and output handling. */
  .Lctr32le_enc_blk4_last:
	vpxor (0 * 16)(%ecx), %ymm4, %ymm5; /* Xor src to last round key. */
	vpxor (2 * 16)(%ecx), %ymm4, %ymm6;
	leal (4 * 16)(%ecx), %ecx;
	vaesenclast %ymm5, %ymm0, %ymm0;
	vaesenclast %ymm6, %ymm1, %ymm1;
	vmovdqu %ymm0, (0 * 16)(%edx);
	vmovdqu %ymm1, (2 * 16)(%edx);
	leal (4 * 16)(%edx), %edx;

	/* Process trailing one to three blocks, one per loop. */
.align 8
.Lctr32le_enc_blk1:
	cmpl $1, %eax;
	jb .Ldone_ctr32le_enc;

	leal -1(%eax), %eax;

	/* Load and increament counter. */
	vmovdqu %xmm7, %xmm0;
	vpaddd CADDR(.Lle_addd_1, %ebx), %xmm7, %xmm7;

	/* AES rounds. */
	vpxor (0 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (1 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (2 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (3 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (4 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (5 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (6 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (7 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (8 * 16)(%edi), %xmm0, %xmm0;
	vaesenc (9 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (10 * 16)(%edi), %xmm1;
	cmpl $12, 4+24(%ebp);
	jb .Lctr32le_enc_blk1_last;
	vaesenc %xmm1, %xmm0, %xmm0;
	vaesenc (11 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (12 * 16)(%edi), %xmm1;
	jz .Lctr32le_enc_blk1_last;
	vaesenc %xmm1, %xmm0, %xmm0;
	vaesenc (13 * 16)(%edi), %xmm0, %xmm0;
	vmovdqa (14 * 16)(%edi), %xmm1;

	/* Last round and output handling. */
  .Lctr32le_enc_blk1_last:
	vpxor (%ecx), %xmm1, %xmm1; /* Xor src to last round key. */
	leal 16(%ecx), %ecx;
	vaesenclast %xmm1, %xmm0, %xmm0; /* Last round and xor with xmm1. */
	vmovdqu %xmm0, (%edx);
	leal 16(%edx), %edx;

	jmp .Lctr32le_enc_blk1;

.align 8
.Ldone_ctr32le_enc:
	vmovdqu %xmm7, (%esi); /* Store CTR. */
	movl (0 * 4)(%esp), %edi;
	CFI_RESTORE(edi);
	movl (1 * 4)(%esp), %esi;
	CFI_RESTORE(esi);
	movl (2 * 4)(%esp), %ebx;
	CFI_RESTORE(ebx);
	leave;
	CFI_LEAVE();
	vzeroall;
	ret_spec_stop
	CFI_ENDPROC();
ELF(.size SYM_NAME(_gcry_vaes_avx2_ctr32le_enc_i386),
	  .-SYM_NAME(_gcry_vaes_avx2_ctr32le_enc_i386))

/**********************************************************************
  OCB-mode encryption/decryption/authentication
 **********************************************************************/
ELF(.type SYM_NAME(_gcry_vaes_avx2_ocb_crypt_i386),@function)
.globl SYM_NAME(_gcry_vaes_avx2_ocb_crypt_i386)
.align 16
SYM_NAME(_gcry_vaes_avx2_ocb_crypt_i386):
	/* input:
	 *	(esp + 4): round keys
	 *	(esp + 8): dst
	 *	(esp + 12): src
	 *	(esp + 16): nblocks
	 *	(esp + 20): nrounds
	 *	(esp + 24): offset
	 *	(esp + 28): checksum
	 *      (esp + 32): blkn
	 *	(esp + 36): L table
	 *	(esp + 44): encrypt/decrypt/auth mode
	 */
	CFI_STARTPROC();

	pushl %ebp;
	CFI_PUSH(%ebp);
	movl %esp, %ebp;
	CFI_DEF_CFA_REGISTER(%ebp);

#define STACK_VEC_POS           0
#define STACK_TMP_Y0            (STACK_VEC_POS + 0 * 32)
#define STACK_TMP_Y1            (STACK_VEC_POS + 1 * 32)
#define STACK_TMP_Y2            (STACK_VEC_POS + 2 * 32)
#define STACK_TMP_Y3            (STACK_VEC_POS + 3 * 32)
#define STACK_TMP_Y4            (STACK_VEC_POS + 4 * 32)
#define STACK_TMP_Y5            (STACK_VEC_POS + 5 * 32)
#define STACK_FXL_KEY           (STACK_VEC_POS + 6 * 32)
#define STACK_OFFSET_AND_F_KEY  (STACK_VEC_POS + 7 * 32)
#define STACK_CHECKSUM          (STACK_VEC_POS + 8 * 32)
#define STACK_GPR_POS           (9 * 32)
#define STACK_END_POS           (STACK_GPR_POS + 3 * 4)

	subl $STACK_END_POS, %esp;
	andl $-32, %esp;

	movl %edi, (STACK_GPR_POS + 0 * 4)(%esp);
	CFI_REG_ON_STACK(edi, STACK_GPR_POS + 0 * 4);
	movl %esi, (STACK_GPR_POS + 1 * 4)(%esp);
	CFI_REG_ON_STACK(esi, STACK_GPR_POS + 1 * 4);
	movl %ebx, (STACK_GPR_POS + 2 * 4)(%esp);
	CFI_REG_ON_STACK(ebx, STACK_GPR_POS + 2 * 4);

	movl 4+4(%ebp), %edi;
	movl 4+8(%ebp), %esi;
	movl 4+12(%ebp), %edx;
	movl 4+32(%ebp), %ebx;

	movl 4+24(%ebp), %eax;
	movl 4+20(%ebp), %ecx;
	leal (, %ecx, 4), %ecx;
	vmovdqu (%eax), %xmm1; /* offset */
	vmovdqa (%edi), %xmm0; /* first key */
	vpxor %xmm0, %xmm1, %xmm1; /* offset ^ first key */
	vpxor (%edi, %ecx, 4), %xmm0, %xmm0; /* first key ^ last key */
	vinserti128 $1, %xmm0, %ymm0, %ymm0;
	vpxor %ymm2, %ymm2, %ymm2;
	vmovdqa %xmm1, (STACK_OFFSET_AND_F_KEY)(%esp);
	vmovdqa %ymm2, (STACK_CHECKSUM)(%esp);
	vmovdqa %ymm0, (STACK_FXL_KEY)(%esp);

	cmpl $12, 4+16(%ebp);
	jae .Locb_crypt_blk12_loop;
	jmp .Locb_crypt_blk4;

	/* Process 12 blocks per loop. */
.align 16
.Locb_crypt_blk12_loop:
	subl $12, 4+16(%ebp);

	movl 4+36(%ebp), %ecx;
	vmovdqa (%ecx), %xmm7; /* Preload L[0] */

	testl $1, %ebx;
	jz .Locb_crypt_blk12_nblk_even;
		/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
		leal 1(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+1)
		shll $4, %eax;
		vmovdqa (STACK_OFFSET_AND_F_KEY)(%esp), %xmm1;
		vpxor (%ecx, %eax), %xmm1, %xmm1;

		vpxor %xmm7, %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm1;
		vmovdqa %ymm1, (STACK_TMP_Y0)(%esp);

		leal 3(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+3)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm0, %xmm1;

		vpxor %xmm7, %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm2;

		leal 5(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+5)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm0, %xmm1;

		vpxor %xmm7, %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm3;

		leal 7(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+7)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm0, %xmm1;

		vpxor %xmm7, %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm4;

		leal 9(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+9)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm0, %xmm1;

		vpxor %xmm7, %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm5;

		leal 11(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+11)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm0, %xmm1;

		leal 12(%ebx), %ebx;
		vpxor %xmm7, %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm6;

		cmpl $1, 4+40(%ebp);
		jb .Locb_dec_blk12;
		ja .Locb_auth_blk12;
		jmp .Locb_enc_blk12;

	.align 8
	.Locb_crypt_blk12_nblk_even:
		/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
		vpxor (STACK_OFFSET_AND_F_KEY)(%esp), %xmm7, %xmm1;

		leal 2(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+2)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm1;
		vmovdqa %ymm1, (STACK_TMP_Y0)(%esp);

		vpxor %xmm7, %xmm0, %xmm1;

		leal 4(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+4)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm2;

		vpxor %xmm7, %xmm0, %xmm1;

		leal 6(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+6)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm3;

		vpxor %xmm7, %xmm0, %xmm1;

		leal 8(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+8)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm4;

		vpxor %xmm7, %xmm0, %xmm1;

		leal 10(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+10)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm5;

		vpxor %xmm7, %xmm0, %xmm1;

		leal 12(%ebx), %ebx;
		tzcntl %ebx, %eax; // ntz(blkn+12)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm1, %xmm0;
		vinserti128 $1, %xmm0, %ymm1, %ymm6;

		cmpl $1, 4+40(%ebp);
		jb .Locb_dec_blk12;
		ja .Locb_auth_blk12;

	.align 8
	.Locb_enc_blk12:
		vmovdqa %ymm2, (STACK_TMP_Y1)(%esp);
		vmovdqa %ymm3, (STACK_TMP_Y2)(%esp);
		vmovdqa %ymm4, (STACK_TMP_Y3)(%esp);
		vmovdqa %ymm5, (STACK_TMP_Y4)(%esp);
		vmovdqa %ymm6, (STACK_TMP_Y5)(%esp);
		vmovdqa %xmm0, (STACK_OFFSET_AND_F_KEY)(%esp);

		vmovdqu 0*16(%edx), %ymm1;
		vmovdqu 2*16(%edx), %ymm2;
		vmovdqu 4*16(%edx), %ymm3;
		vmovdqu 6*16(%edx), %ymm4;
		vmovdqu 8*16(%edx), %ymm5;
		vmovdqu 10*16(%edx), %ymm6;
		leal 12*16(%edx), %edx;

		/* Checksum_i = Checksum_{i-1} xor P_i  */
		vpxor %ymm1, %ymm2, %ymm0;
		vpxor %ymm3, %ymm4, %ymm7;
		vpxor %ymm5, %ymm0, %ymm0;
		vpxor %ymm6, %ymm7, %ymm7;
		vpxor %ymm0, %ymm7, %ymm7;
		vbroadcasti128 (1 * 16)(%edi), %ymm0;
		vpxor (STACK_CHECKSUM)(%esp), %ymm7, %ymm7;

		/* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */
		vpxor (STACK_TMP_Y0)(%esp), %ymm1, %ymm1;
		vpxor (STACK_TMP_Y1)(%esp), %ymm2, %ymm2;
		vpxor (STACK_TMP_Y2)(%esp), %ymm3, %ymm3;
		vpxor (STACK_TMP_Y3)(%esp), %ymm4, %ymm4;
		vpxor (STACK_TMP_Y4)(%esp), %ymm5, %ymm5;
		vpxor (STACK_TMP_Y5)(%esp), %ymm6, %ymm6;

		vmovdqa %ymm7, (STACK_CHECKSUM)(%esp);

		/* AES rounds */
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (2 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (3 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (4 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (5 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (6 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (7 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (8 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (9 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		cmpl $12, 4+20(%ebp);
		jb .Locb_enc_blk12_last;
		vbroadcasti128 (10 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (11 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		jz .Locb_enc_blk12_last;
		vbroadcasti128 (12 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (13 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);

		/* Last round and output handling. */
	  .Locb_enc_blk12_last:
		vmovdqa (STACK_FXL_KEY)(%esp), %ymm0;
		vpxor (STACK_TMP_Y0)(%esp), %ymm0, %ymm7;
		vaesenclast %ymm7, %ymm1, %ymm1;
		vpxor (STACK_TMP_Y1)(%esp), %ymm0, %ymm7;
		vmovdqu %ymm1, 0*16(%esi);
		vpxor (STACK_TMP_Y2)(%esp), %ymm0, %ymm1;
		vaesenclast %ymm7, %ymm2, %ymm2;
		vpxor (STACK_TMP_Y3)(%esp), %ymm0, %ymm7;
		vaesenclast %ymm1, %ymm3, %ymm3;
		vpxor (STACK_TMP_Y4)(%esp), %ymm0, %ymm1;
		vaesenclast %ymm7, %ymm4, %ymm4;
		vpxor (STACK_TMP_Y5)(%esp), %ymm0, %ymm7;
		vaesenclast %ymm1, %ymm5, %ymm5;
		vaesenclast %ymm7, %ymm6, %ymm6;
		vmovdqu %ymm2, 2*16(%esi);
		vmovdqu %ymm3, 4*16(%esi);
		vmovdqu %ymm4, 6*16(%esi);
		vmovdqu %ymm5, 8*16(%esi);
		vmovdqu %ymm6, 10*16(%esi);
		leal 12*16(%esi), %esi;

		cmpl $12, 4+16(%ebp);
		jae .Locb_crypt_blk12_loop;
		jmp .Locb_crypt_blk12_cleanup;

	.align 8
	.Locb_auth_blk12:
		vmovdqa %xmm0, (STACK_OFFSET_AND_F_KEY)(%esp);
		vbroadcasti128 (1 * 16)(%edi), %ymm0;

		/* Sum_i = Sum_{i-1} xor ENCIPHER(K, A_i xor Offset_i)  */
		vmovdqa (STACK_TMP_Y0)(%esp), %ymm1;
		vpxor 0*16(%edx), %ymm1, %ymm1;
		vpxor 2*16(%edx), %ymm2, %ymm2;
		vpxor 4*16(%edx), %ymm3, %ymm3;
		vpxor 6*16(%edx), %ymm4, %ymm4;
		vpxor 8*16(%edx), %ymm5, %ymm5;
		vpxor 10*16(%edx), %ymm6, %ymm6;
		leal 12*16(%edx), %edx;

		/* AES rounds */
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (2 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (3 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (4 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (5 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (6 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (7 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (8 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (9 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (10 * 16)(%edi), %ymm0;
		cmpl $12, 4+20(%ebp);
		jb .Locb_auth_blk12_last;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (11 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (12 * 16)(%edi), %ymm0;
		jz .Locb_auth_blk12_last;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (13 * 16)(%edi), %ymm0;
		VAESENC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (14 * 16)(%edi), %ymm0;

		/* Last round and output handling. */
	  .Locb_auth_blk12_last:
		vaesenclast %ymm0, %ymm1, %ymm1;
		vaesenclast %ymm0, %ymm2, %ymm2;
		vaesenclast %ymm0, %ymm3, %ymm3;
		vaesenclast %ymm0, %ymm4, %ymm4;
		vaesenclast %ymm0, %ymm5, %ymm5;
		vaesenclast %ymm0, %ymm6, %ymm6;

		vpxor %ymm1, %ymm2, %ymm0;
		vpxor %ymm3, %ymm4, %ymm4;
		vpxor %ymm5, %ymm0, %ymm0;
		vpxor %ymm6, %ymm4, %ymm4;
		vpxor %ymm0, %ymm4, %ymm4;
		vpxor (STACK_CHECKSUM)(%esp), %ymm4, %ymm4;
		vmovdqa %ymm4, (STACK_CHECKSUM)(%esp);

		cmpl $12, 4+16(%ebp);
		jae .Locb_crypt_blk12_loop;
		jmp .Locb_crypt_blk12_cleanup;

	.align 8
	.Locb_dec_blk12:
		vmovdqa %xmm0, (STACK_OFFSET_AND_F_KEY)(%esp);

		/* P_i = Offset_i xor DECIPHER(K, C_i xor Offset_i)  */
		vmovdqa (STACK_TMP_Y0)(%esp), %ymm1;
		vmovdqu 0*16(%edx), %ymm0;
		vmovdqu 2*16(%edx), %ymm7;
		vpxor %ymm0, %ymm1, %ymm1;
		vmovdqa %ymm2, (STACK_TMP_Y1)(%esp);
		vpxor %ymm7, %ymm2, %ymm2;
		vmovdqu 4*16(%edx), %ymm0;
		vmovdqu 6*16(%edx), %ymm7;
		vmovdqa %ymm3, (STACK_TMP_Y2)(%esp);
		vmovdqa %ymm4, (STACK_TMP_Y3)(%esp);
		vpxor %ymm0, %ymm3, %ymm3;
		vpxor %ymm7, %ymm4, %ymm4;
		vmovdqu 8*16(%edx), %ymm0;
		vmovdqu 10*16(%edx), %ymm7;
		leal 12*16(%edx), %edx;
		vmovdqa %ymm5, (STACK_TMP_Y4)(%esp);
		vmovdqa %ymm6, (STACK_TMP_Y5)(%esp);
		vpxor %ymm0, %ymm5, %ymm5;
		vbroadcasti128 (1 * 16)(%edi), %ymm0;
		vpxor %ymm7, %ymm6, %ymm6;

		/* AES rounds */
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (2 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (3 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (4 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (5 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (6 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (7 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (8 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (9 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		cmpl $12, 4+20(%ebp);
		jb .Locb_dec_blk12_last;
		vbroadcasti128 (10 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (11 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		jz .Locb_dec_blk12_last;
		vbroadcasti128 (12 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);
		vbroadcasti128 (13 * 16)(%edi), %ymm0;
		VAESDEC6(%ymm0, %ymm1, %ymm2, %ymm3, %ymm4, %ymm5, %ymm6);

		/* Last round and output handling. */
	  .Locb_dec_blk12_last:
		vmovdqa (STACK_FXL_KEY)(%esp), %ymm0;
		vpxor (STACK_TMP_Y0)(%esp), %ymm0, %ymm7;
		vaesdeclast %ymm7, %ymm1, %ymm1;
		vmovdqu %ymm1, 0*16(%esi);
		vpxor (STACK_TMP_Y1)(%esp), %ymm0, %ymm1;
		vpxor (STACK_TMP_Y2)(%esp), %ymm0, %ymm7;
		vaesdeclast %ymm1, %ymm2, %ymm2;
		vpxor (STACK_TMP_Y3)(%esp), %ymm0, %ymm1;
		vaesdeclast %ymm7, %ymm3, %ymm3;
		vpxor (STACK_TMP_Y4)(%esp), %ymm0, %ymm7;
		vaesdeclast %ymm1, %ymm4, %ymm4;
		vpxor (STACK_TMP_Y5)(%esp), %ymm0, %ymm0;
		vaesdeclast %ymm7, %ymm5, %ymm5;
		vaesdeclast %ymm0, %ymm6, %ymm6;

		/* Checksum_i = Checksum_{i-1} xor P_i  */
		vpxor %ymm2, %ymm3, %ymm0;
		vpxor %ymm4, %ymm5, %ymm7;
		vpxor %ymm6, %ymm0, %ymm0;
		vpxor 0*16(%esi), %ymm7, %ymm7;
		vpxor %ymm0, %ymm7, %ymm7;
		vpxor (STACK_CHECKSUM)(%esp), %ymm7, %ymm7;

		vmovdqu %ymm2, 2*16(%esi);
		vmovdqu %ymm3, 4*16(%esi);
		vmovdqu %ymm4, 6*16(%esi);
		vmovdqu %ymm5, 8*16(%esi);
		vmovdqu %ymm6, 10*16(%esi);
		leal 12*16(%esi), %esi;

		vmovdqa %ymm7, (STACK_CHECKSUM)(%esp);

		cmpl $12, 4+16(%ebp);
		jae .Locb_crypt_blk12_loop;

.align 8
.Locb_crypt_blk12_cleanup:
	vpxor %ymm0, %ymm0, %ymm0;
	vmovdqa %ymm0, (STACK_TMP_Y0)(%esp);
	vmovdqa %ymm0, (STACK_TMP_Y1)(%esp);
	vmovdqa %ymm0, (STACK_TMP_Y2)(%esp);
	vmovdqa %ymm0, (STACK_TMP_Y3)(%esp);
	vmovdqa %ymm0, (STACK_TMP_Y4)(%esp);
	vmovdqa %ymm0, (STACK_TMP_Y5)(%esp);

	/* Process trailing four blocks. */
.align 8
.Locb_crypt_blk4:
	cmpl $4, 4+16(%ebp);
	jb .Locb_crypt_blk1;

	subl $4, 4+16(%ebp);

	movl 4+36(%ebp), %ecx;
	vmovdqa (%ecx), %xmm7; /* Preload L[0] */

	testl $1, %ebx;
	jz .Locb_crypt_blk4_nblk_even;
		/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
		leal 1(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+1)
		shll $4, %eax;
		vmovdqa (STACK_OFFSET_AND_F_KEY)(%esp), %xmm1;
		vpxor (%ecx, %eax), %xmm1, %xmm1;

		vpxor %xmm7, %xmm1, %xmm2;
		vinserti128 $1, %xmm2, %ymm1, %ymm6;

		leal 3(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+3)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm2, %xmm3;

		leal 4(%ebx), %ebx;
		vpxor %xmm7, %xmm3, %xmm4;
		vinserti128 $1, %xmm4, %ymm3, %ymm7;
		vmovdqa %xmm4, (STACK_OFFSET_AND_F_KEY)(%esp);

		cmpl $1, 4+40(%ebp);
		jb .Locb_dec_blk4;
		ja .Locb_auth_blk4;
		jmp .Locb_enc_blk4;

	.align 8
	.Locb_crypt_blk4_nblk_even:
		/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
		vmovdqa (STACK_OFFSET_AND_F_KEY)(%esp), %xmm1;
		vpxor %xmm7, %xmm1, %xmm1;

		leal 2(%ebx), %eax;
		tzcntl %eax, %eax; // ntz(blkn+2)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm1, %xmm2;
		vinserti128 $1, %xmm2, %ymm1, %ymm6;

		vpxor %xmm7, %xmm2, %xmm3;

		leal 4(%ebx), %ebx;
		tzcntl %ebx, %eax; // ntz(blkn+4)
		shll $4, %eax;
		vpxor (%ecx, %eax), %xmm3, %xmm4;
		vinserti128 $1, %xmm4, %ymm3, %ymm7;
		vmovdqa %xmm4, (STACK_OFFSET_AND_F_KEY)(%esp);

		cmpl $1, 4+40(%ebp);
		jb .Locb_dec_blk4;
		ja .Locb_auth_blk4;

	.align 8
	.Locb_enc_blk4:
		vmovdqu 0*16(%edx), %ymm1;
		vmovdqu 2*16(%edx), %ymm2;
		leal 4*16(%edx), %edx;

		/* Checksum_i = Checksum_{i-1} xor P_i  */
		vpxor %ymm1, %ymm2, %ymm5;
		vpxor (STACK_CHECKSUM)(%esp), %ymm5, %ymm5;
		vmovdqa %ymm5, (STACK_CHECKSUM)(%esp);

		/* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */
		vpxor %ymm6, %ymm1, %ymm1;
		vpxor %ymm7, %ymm2, %ymm2;

		/* AES rounds */
		vbroadcasti128 (1 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (2 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (3 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (4 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (5 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (6 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (7 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (8 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (9 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		cmpl $12, 4+20(%ebp);
		jb .Locb_enc_blk4_last;
		vbroadcasti128 (10 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (11 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		jz .Locb_enc_blk4_last;
		vbroadcasti128 (12 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (13 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);

		/* Last round and output handling. */
	  .Locb_enc_blk4_last:
		vmovdqa (STACK_FXL_KEY)(%esp), %ymm0;
		vpxor %ymm0, %ymm6, %ymm6; /* Xor offset to last round key. */
		vpxor %ymm0, %ymm7, %ymm7;
		vaesenclast %ymm6, %ymm1, %ymm1;
		vaesenclast %ymm7, %ymm2, %ymm2;
		vmovdqu %ymm1, 0*16(%esi);
		vmovdqu %ymm2, 2*16(%esi);
		leal 4*16(%esi), %esi;

		jmp .Locb_crypt_blk1;

	.align 8
	.Locb_auth_blk4:
		/* Sum_i = Sum_{i-1} xor ENCIPHER(K, A_i xor Offset_i)  */
		vpxor 0*16(%edx), %ymm6, %ymm1;
		vpxor 2*16(%edx), %ymm7, %ymm2;
		leal 4*16(%edx), %edx;

		/* AES rounds */
		vbroadcasti128 (1 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (2 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (3 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (4 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (5 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (6 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (7 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (8 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (9 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (10 * 16)(%edi), %ymm0;
		cmpl $12, 4+20(%ebp);
		jb .Locb_auth_blk4_last;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (11 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (12 * 16)(%edi), %ymm0;
		jz .Locb_auth_blk4_last;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (13 * 16)(%edi), %ymm0;
		VAESENC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (14 * 16)(%edi), %ymm0;

		/* Last round and output handling. */
	  .Locb_auth_blk4_last:
		vaesenclast %ymm0, %ymm1, %ymm1;
		vaesenclast %ymm0, %ymm2, %ymm2;

		/* Checksum_i = Checksum_{i-1} xor P_i  */
		vpxor %ymm1, %ymm2, %ymm5;
		vpxor (STACK_CHECKSUM)(%esp), %ymm5, %ymm5;
		vmovdqa %ymm5, (STACK_CHECKSUM)(%esp);

		jmp .Locb_crypt_blk1;

	.align 8
	.Locb_dec_blk4:
		/* P_i = Offset_i xor DECIPHER(K, C_i xor Offset_i)  */
		vpxor 0*16(%edx), %ymm6, %ymm1;
		vpxor 2*16(%edx), %ymm7, %ymm2;
		leal 4*16(%edx), %edx;

		/* AES rounds */
		vbroadcasti128 (1 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (2 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (3 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (4 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (5 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (6 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (7 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (8 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (9 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		cmpl $12, 4+20(%ebp);
		jb .Locb_dec_blk4_last;
		vbroadcasti128 (10 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (11 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		jz .Locb_dec_blk4_last;
		vbroadcasti128 (12 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);
		vbroadcasti128 (13 * 16)(%edi), %ymm0;
		VAESDEC2(%ymm0, %ymm1, %ymm2);

		/* Last round and output handling. */
	  .Locb_dec_blk4_last:
		vmovdqa (STACK_FXL_KEY)(%esp), %ymm0;
		vpxor %ymm0, %ymm6, %ymm6; /* Xor offset to last round key. */
		vpxor %ymm0, %ymm7, %ymm7;
		vaesdeclast %ymm6, %ymm1, %ymm1;
		vaesdeclast %ymm7, %ymm2, %ymm2;

		/* Checksum_i = Checksum_{i-1} xor P_i  */
		vpxor %ymm1, %ymm2, %ymm5;
		vpxor (STACK_CHECKSUM)(%esp), %ymm5, %ymm5;

		vmovdqu %ymm1, 0*16(%esi);
		vmovdqu %ymm2, 2*16(%esi);
		leal 4*16(%esi), %esi;

		vmovdqa %ymm5, (STACK_CHECKSUM)(%esp);

	/* Process trailing one to three blocks, one per loop. */
.align 8
.Locb_crypt_blk1:
	cmpl $1, 4+16(%ebp);
	jb .Locb_crypt_done;

	subl $1, 4+16(%ebp);

	movl 4+36(%ebp), %ecx;
	leal 1(%ebx), %ebx;
	tzcntl %ebx, %eax; // ntz(blkn+1)
	shll $4, %eax;
	vmovdqa (STACK_OFFSET_AND_F_KEY)(%esp), %xmm7;
	vpxor (%ecx, %eax), %xmm7, %xmm7;

	/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
	vmovdqa %xmm7, (STACK_OFFSET_AND_F_KEY)(%esp);

	cmpl $1, 4+40(%ebp);
	jb .Locb_dec_blk1;
	ja .Locb_auth_blk1;
		vmovdqu (%edx), %xmm0;
		leal 16(%edx), %edx;

		/* Checksum_i = Checksum_{i-1} xor P_i  */
		vpxor (STACK_CHECKSUM)(%esp), %xmm0, %xmm1;
		vmovdqa %xmm1, (STACK_CHECKSUM)(%esp);

		/* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */
		vpxor %xmm7, %xmm0, %xmm0;

		/* AES rounds. */
		vaesenc (1 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (2 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (3 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (4 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (5 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (6 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (7 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (8 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (9 * 16)(%edi), %xmm0, %xmm0;
		cmpl $12, 4+20(%ebp);
		jb .Locb_enc_blk1_last;
		vaesenc (10 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (11 * 16)(%edi), %xmm0, %xmm0;
		jz .Locb_enc_blk1_last;
		vaesenc (12 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (13 * 16)(%edi), %xmm0, %xmm0;

		/* Last round and output handling. */
	  .Locb_enc_blk1_last:
		vpxor (STACK_FXL_KEY)(%esp), %xmm7, %xmm1;
		vaesenclast %xmm1, %xmm0, %xmm0;
		vmovdqu %xmm0, (%esi);
		leal 16(%esi), %esi;

		jmp .Locb_crypt_blk1;

	.align 8
	.Locb_auth_blk1:
		/* Sum_i = Sum_{i-1} xor ENCIPHER(K, A_i xor Offset_i)  */
		vpxor (%edx), %xmm7, %xmm0;
		leal 16(%edx), %edx;

		/* AES rounds. */
		vaesenc (1 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (2 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (3 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (4 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (5 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (6 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (7 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (8 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (9 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (10 * 16)(%edi), %xmm1;
		cmpl $12, 4+20(%ebp);
		jb .Locb_auth_blk1_last;
		vaesenc %xmm1, %xmm0, %xmm0;
		vaesenc (11 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (12 * 16)(%edi), %xmm1;
		jz .Locb_auth_blk1_last;
		vaesenc %xmm1, %xmm0, %xmm0;
		vaesenc (13 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (14 * 16)(%edi), %xmm1;

		/* Last round and output handling. */
	  .Locb_auth_blk1_last:
		vpxor (STACK_CHECKSUM)(%esp), %xmm1, %xmm1;
		vaesenclast %xmm1, %xmm0, %xmm0;
		vmovdqa %xmm0, (STACK_CHECKSUM)(%esp);

		jmp .Locb_crypt_blk1;

	.align 8
	.Locb_dec_blk1:
		/* P_i = Offset_i xor DECIPHER(K, C_i xor Offset_i)  */
		vpxor (%edx), %xmm7, %xmm0;
		leal 16(%edx), %edx;

		/* AES rounds. */
		vaesdec (1 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (2 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (3 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (4 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (5 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (6 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (7 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (8 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (9 * 16)(%edi), %xmm0, %xmm0;
		cmpl $12, 4+20(%ebp);
		jb .Locb_dec_blk1_last;
		vaesdec (10 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (11 * 16)(%edi), %xmm0, %xmm0;
		jz .Locb_dec_blk1_last;
		vaesdec (12 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (13 * 16)(%edi), %xmm0, %xmm0;

		/* Last round and output handling. */
	  .Locb_dec_blk1_last:
		vpxor (STACK_FXL_KEY)(%esp), %xmm7, %xmm1;
		vaesdeclast %xmm1, %xmm0, %xmm0;

		/* Checksum_i = Checksum_{i-1} xor P_i  */
		vpxor (STACK_CHECKSUM)(%esp), %xmm0, %xmm1;

		vmovdqu %xmm0, (%esi);
		leal 16(%esi), %esi;

		vmovdqa %xmm1, (STACK_CHECKSUM)(%esp);

		jmp .Locb_crypt_blk1;

.align 8
.Locb_crypt_done:
	movl 4+24(%ebp), %ecx;
	vmovdqa (STACK_OFFSET_AND_F_KEY)(%esp), %xmm1;
	vpxor (%edi), %xmm1, %xmm1;
	vmovdqu %xmm1, (%ecx);

	movl 4+28(%ebp), %eax;
	vmovdqa (STACK_CHECKSUM)(%esp), %xmm2;
	vpxor (STACK_CHECKSUM + 16)(%esp), %xmm2, %xmm2;
	vpxor (%eax), %xmm2, %xmm2;
	vmovdqu %xmm2, (%eax);

	movl (STACK_GPR_POS + 0 * 4)(%esp), %edi;
	CFI_RESTORE(edi);
	movl (STACK_GPR_POS + 1 * 4)(%esp), %esi;
	CFI_RESTORE(esi);
	movl (STACK_GPR_POS + 2 * 4)(%esp), %ebx;
	CFI_RESTORE(ebx);

	vpxor %ymm0, %ymm0, %ymm0;
	vmovdqa %ymm0, (STACK_OFFSET_AND_F_KEY)(%esp);
	vmovdqa %ymm0, (STACK_CHECKSUM)(%esp);

	xorl %eax, %eax;
	leave;
	CFI_LEAVE();
	vzeroall;
	ret_spec_stop
	CFI_ENDPROC();
ELF(.size SYM_NAME(_gcry_vaes_avx2_ocb_crypt_i386),
	  .-SYM_NAME(_gcry_vaes_avx2_ocb_crypt_i386))

/**********************************************************************
  XTS-mode encryption
 **********************************************************************/
ELF(.type SYM_NAME(_gcry_vaes_avx2_xts_crypt_i386),@function)
.globl SYM_NAME(_gcry_vaes_avx2_xts_crypt_i386)
.align 16
SYM_NAME(_gcry_vaes_avx2_xts_crypt_i386):
	/* input:
	 *	(esp + 4): round keys
	 *	(esp + 8): tweak
	 *	(esp + 12): dst
	 *	(esp + 16): src
	 *	(esp + 20): nblocks
	 *	(esp + 24): nrounds
	 *	(esp + 28): encrypt
	 */
	CFI_STARTPROC();

	GET_DATA_POINTER(SYM_NAME(_gcry_vaes_consts), eax);

	pushl %ebp;
	CFI_PUSH(%ebp);
	movl %esp, %ebp;
	CFI_DEF_CFA_REGISTER(%ebp);

	subl $(4 * 32 + 3 * 4), %esp;
	andl $-32, %esp;

	movl %edi, (4 * 32 + 0 * 4)(%esp);
	CFI_REG_ON_STACK(edi, 4 * 32 + 0 * 4);
	movl %esi, (4 * 32 + 1 * 4)(%esp);
	CFI_REG_ON_STACK(esi, 4 * 32 + 1 * 4);
	movl %ebx, (4 * 32 + 2 * 4)(%esp);
	CFI_REG_ON_STACK(ebx, 4 * 32 + 2 * 4);

	movl %eax, %ebx;
	movl 4+4(%ebp), %edi;
	movl 4+8(%ebp), %esi;
	movl 4+12(%ebp), %edx;
	movl 4+16(%ebp), %ecx;
	movl 4+20(%ebp), %eax;

#define tweak_clmul(shift, out, tweak, hi_tweak, tmp1, tmp2) \
	vpsrld $(32-(shift)), hi_tweak, tmp2; \
	vpsllq $(shift), tweak, out; \
	vpclmulqdq $0, CADDR(.Lxts_gfmul_clmul, %ebx), tmp2, tmp1; \
	vpunpckhqdq tmp2, tmp1, tmp1; \
	vpxor tmp1, out, out;

	/* Prepare tweak. */
	vmovdqu (%esi), %xmm7;
	vpshufb CADDR(.Lxts_high_bit_shuf, %ebx), %xmm7, %xmm6;
	tweak_clmul(1, %xmm5, %xmm7, %xmm6, %xmm0, %xmm1);
	vinserti128 $1, %xmm5, %ymm7, %ymm7; /* tweak:tweak1 */
	vpshufb CADDR(.Lxts_high_bit_shuf, %ebx), %ymm7, %ymm6;

	/* Process eight blocks per loop. */
.align 8
.Lxts_crypt_blk8:
	cmpl $8, %eax;
	jb .Lxts_crypt_blk4;

	leal -8(%eax), %eax;

	vmovdqa %ymm7, (0 * 32)(%esp);
	tweak_clmul(2, %ymm2, %ymm7, %ymm6, %ymm0, %ymm1);
	vmovdqa %ymm2, (1 * 32)(%esp);
	tweak_clmul(4, %ymm2, %ymm7, %ymm6, %ymm0, %ymm1);
	vmovdqa %ymm2, (2 * 32)(%esp);
	tweak_clmul(6, %ymm2, %ymm7, %ymm6, %ymm0, %ymm1);
	vmovdqa %ymm2, (3 * 32)(%esp);
	tweak_clmul(8, %ymm7, %ymm7, %ymm6, %ymm0, %ymm1);
	vpshufb CADDR(.Lxts_high_bit_shuf, %ebx), %ymm7, %ymm6;

	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	vmovdqa (0 * 32)(%esp), %ymm0;
	vmovdqa (1 * 32)(%esp), %ymm1;
	vmovdqa (2 * 32)(%esp), %ymm2;
	vmovdqa (3 * 32)(%esp), %ymm3;
	vpxor (0 * 16)(%ecx), %ymm0, %ymm0;
	vpxor (2 * 16)(%ecx), %ymm1, %ymm1;
	vpxor (4 * 16)(%ecx), %ymm2, %ymm2;
	vpxor (6 * 16)(%ecx), %ymm3, %ymm3;

	leal (8 * 16)(%ecx), %ecx;

	cmpl $1, 4+28(%ebp);
	jne .Lxts_dec_blk8;
		/* AES rounds */
		XOR4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (1 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (2 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (3 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (4 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (5 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (6 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (7 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (8 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (9 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (10 * 16)(%edi), %ymm4;
		cmpl $12, 4+24(%ebp);
		jb .Lxts_enc_blk8_last;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (11 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (12 * 16)(%edi), %ymm4;
		jz .Lxts_enc_blk8_last;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (13 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (14 * 16)(%edi), %ymm4;

		/* Last round and output handling. */
	.Lxts_enc_blk8_last:
		vpxor (0 * 32)(%esp), %ymm4, %ymm5; /* Xor tweak to last round key. */
		vaesenclast %ymm5, %ymm0, %ymm0;
		vpxor (1 * 32)(%esp), %ymm4, %ymm5;
		vaesenclast %ymm5, %ymm1, %ymm1;
		vpxor (2 * 32)(%esp), %ymm4, %ymm5;
		vpxor (3 * 32)(%esp), %ymm4, %ymm4;
		vaesenclast %ymm5, %ymm2, %ymm2;
		vaesenclast %ymm4, %ymm3, %ymm3;
		vmovdqu %ymm0, (0 * 16)(%edx);
		vmovdqu %ymm1, (2 * 16)(%edx);
		vmovdqu %ymm2, (4 * 16)(%edx);
		vmovdqu %ymm3, (6 * 16)(%edx);
		leal (8 * 16)(%edx), %edx;

		jmp .Lxts_crypt_blk8;

	.align 8
	.Lxts_dec_blk8:
		/* AES rounds */
		XOR4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (1 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (2 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (3 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (4 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (5 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (6 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (7 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (8 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (9 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (10 * 16)(%edi), %ymm4;
		cmpl $12, 4+24(%ebp);
		jb .Lxts_dec_blk8_last;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (11 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (12 * 16)(%edi), %ymm4;
		jz .Lxts_dec_blk8_last;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (13 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (14 * 16)(%edi), %ymm4;

		/* Last round and output handling. */
	.Lxts_dec_blk8_last:
		vpxor (0 * 32)(%esp), %ymm4, %ymm5; /* Xor tweak to last round key. */
		vaesdeclast %ymm5, %ymm0, %ymm0;
		vpxor (1 * 32)(%esp), %ymm4, %ymm5;
		vaesdeclast %ymm5, %ymm1, %ymm1;
		vpxor (2 * 32)(%esp), %ymm4, %ymm5;
		vpxor (3 * 32)(%esp), %ymm4, %ymm4;
		vaesdeclast %ymm5, %ymm2, %ymm2;
		vaesdeclast %ymm4, %ymm3, %ymm3;
		vmovdqu %ymm0, (0 * 16)(%edx);
		vmovdqu %ymm1, (2 * 16)(%edx);
		vmovdqu %ymm2, (4 * 16)(%edx);
		vmovdqu %ymm3, (6 * 16)(%edx);
		leal (8 * 16)(%edx), %edx;

		jmp .Lxts_crypt_blk8;

	/* Handle trailing four blocks. */
.align 8
.Lxts_crypt_blk4:
	/* Try exit early as typically input length is large power of 2. */
	cmpl $1, %eax;
	jb .Ldone_xts_crypt;
	cmpl $4, %eax;
	jb .Lxts_crypt_blk1;

	leal -4(%eax), %eax;

	vmovdqa %ymm7, %ymm2;
	tweak_clmul(2, %ymm3, %ymm7, %ymm6, %ymm0, %ymm1);
	tweak_clmul(4, %ymm7, %ymm7, %ymm6, %ymm0, %ymm1);
	vpshufb CADDR(.Lxts_high_bit_shuf, %ebx), %ymm7, %ymm6;

	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	vpxor (0 * 16)(%ecx), %ymm2, %ymm0;
	vpxor (2 * 16)(%ecx), %ymm3, %ymm1;

	leal (4 * 16)(%ecx), %ecx;

	cmpl $1, 4+28(%ebp);
	jne .Lxts_dec_blk4;
		/* AES rounds */
		XOR2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (1 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (2 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (3 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (4 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (5 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (6 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (7 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (8 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (9 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (10 * 16)(%edi), %ymm4;
		cmpl $12, 4+24(%ebp);
		jb .Lxts_enc_blk4_last;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (11 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (12 * 16)(%edi), %ymm4;
		jz .Lxts_enc_blk4_last;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (13 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (14 * 16)(%edi), %ymm4;

		/* Last round and output handling. */
	.Lxts_enc_blk4_last:
		vpxor %ymm4, %ymm2, %ymm2; /* Xor tweak to last round key. */
		vpxor %ymm4, %ymm3, %ymm3;
		vaesenclast %ymm2, %ymm0, %ymm0;
		vaesenclast %ymm3, %ymm1, %ymm1;
		vmovdqu %ymm0, (0 * 16)(%edx);
		vmovdqu %ymm1, (2 * 16)(%edx);
		leal (4 * 16)(%edx), %edx;

		jmp .Lxts_crypt_blk1;

	.align 8
	.Lxts_dec_blk4:
		/* AES rounds */
		XOR2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (1 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (2 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (3 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (4 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (5 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (6 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (7 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (8 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (9 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (10 * 16)(%edi), %ymm4;
		cmpl $12, 4+24(%ebp);
		jb .Lxts_dec_blk4_last;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (11 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (12 * 16)(%edi), %ymm4;
		jz .Lxts_dec_blk4_last;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (13 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (14 * 16)(%edi), %ymm4;

		/* Last round and output handling. */
	.Lxts_dec_blk4_last:
		vpxor %ymm4, %ymm2, %ymm2; /* Xor tweak to last round key. */
		vpxor %ymm4, %ymm3, %ymm3;
		vaesdeclast %ymm2, %ymm0, %ymm0;
		vaesdeclast %ymm3, %ymm1, %ymm1;
		vmovdqu %ymm0, (0 * 16)(%edx);
		vmovdqu %ymm1, (2 * 16)(%edx);
		leal (4 * 16)(%edx), %edx;

	/* Process trailing one to three blocks, one per loop. */
.align 8
.Lxts_crypt_blk1:
	cmpl $1, %eax;
	jb .Ldone_xts_crypt;

	leal -1(%eax), %eax;

	vpxor (%ecx), %xmm7, %xmm0;
	vmovdqa %xmm7, %xmm5;
	tweak_clmul(1, %xmm7, %xmm7, %xmm6, %xmm2, %xmm3);
	vpshufb CADDR(.Lxts_high_bit_shuf, %ebx), %xmm7, %xmm6;

	leal 16(%ecx), %ecx;

	cmpl $1, 4+28(%ebp);
	jne .Lxts_dec_blk1;
		/* AES rounds. */
		vpxor (0 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (1 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (2 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (3 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (4 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (5 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (6 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (7 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (8 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (9 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (10 * 16)(%edi), %xmm1;
		cmpl $12, 4+24(%ebp);
		jb .Lxts_enc_blk1_last;
		vaesenc %xmm1, %xmm0, %xmm0;
		vaesenc (11 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (12 * 16)(%edi), %xmm1;
		jz .Lxts_enc_blk1_last;
		vaesenc %xmm1, %xmm0, %xmm0;
		vaesenc (13 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (14 * 16)(%edi), %xmm1;

		/* Last round and output handling. */
	.Lxts_enc_blk1_last:
		vpxor %xmm1, %xmm5, %xmm5; /* Xor tweak to last round key. */
		vaesenclast %xmm5, %xmm0, %xmm0;
		vmovdqu %xmm0, (%edx);
		leal 16(%edx), %edx;

		jmp .Lxts_crypt_blk1;

	.align 8
	.Lxts_dec_blk1:
		/* AES rounds. */
		vpxor (0 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (1 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (2 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (3 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (4 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (5 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (6 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (7 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (8 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (9 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (10 * 16)(%edi), %xmm1;
		cmpl $12, 4+24(%ebp);
		jb .Lxts_dec_blk1_last;
		vaesdec %xmm1, %xmm0, %xmm0;
		vaesdec (11 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (12 * 16)(%edi), %xmm1;
		jz .Lxts_dec_blk1_last;
		vaesdec %xmm1, %xmm0, %xmm0;
		vaesdec (13 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (14 * 16)(%edi), %xmm1;

		/* Last round and output handling. */
	.Lxts_dec_blk1_last:
		vpxor %xmm1, %xmm5, %xmm5; /* Xor tweak to last round key. */
		vaesdeclast %xmm5, %xmm0, %xmm0;
		vmovdqu %xmm0, (%edx);
		leal 16(%edx), %edx;

		jmp .Lxts_crypt_blk1;

.align 8
.Ldone_xts_crypt:
	/* Store IV. */
	vmovdqu %xmm7, (%esi);

	vpxor %ymm0, %ymm0, %ymm0;
	movl (4 * 32 + 0 * 4)(%esp), %edi;
	CFI_RESTORE(edi);
	movl (4 * 32 + 1 * 4)(%esp), %esi;
	CFI_RESTORE(esi);
	movl (4 * 32 + 2 * 4)(%esp), %ebx;
	CFI_RESTORE(ebx);
	vmovdqa %ymm0, (0 * 32)(%esp);
	vmovdqa %ymm0, (1 * 32)(%esp);
	vmovdqa %ymm0, (2 * 32)(%esp);
	vmovdqa %ymm0, (3 * 32)(%esp);
	leave;
	CFI_LEAVE();
	vzeroall;
	xorl %eax, %eax;
	ret_spec_stop
	CFI_ENDPROC();
ELF(.size SYM_NAME(_gcry_vaes_avx2_xts_crypt_i386),
	  .-SYM_NAME(_gcry_vaes_avx2_xts_crypt_i386))

/**********************************************************************
  ECB-mode encryption
 **********************************************************************/
ELF(.type SYM_NAME(_gcry_vaes_avx2_ecb_crypt_i386),@function)
.globl SYM_NAME(_gcry_vaes_avx2_ecb_crypt_i386)
.align 16
SYM_NAME(_gcry_vaes_avx2_ecb_crypt_i386):
	/* input:
	 *	(esp + 4): round keys
	 *	(esp + 8): encrypt
	 *	(esp + 12): dst
	 *	(esp + 16): src
	 *	(esp + 20): nblocks
	 *	(esp + 24): nrounds
	 */
	CFI_STARTPROC();
	pushl %edi;
	CFI_PUSH(%edi);
	pushl %esi;
	CFI_PUSH(%esi);

	movl 8+4(%esp), %edi;
	movl 8+8(%esp), %esi;
	movl 8+12(%esp), %edx;
	movl 8+16(%esp), %ecx;
	movl 8+20(%esp), %eax;

	/* Process 8 blocks per loop. */
.align 8
.Lecb_blk8:
	cmpl $8, %eax;
	jb .Lecb_blk4;

	leal -8(%eax), %eax;

	/* Load input and xor first key. */
	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	vmovdqu (0 * 16)(%ecx), %ymm0;
	vmovdqu (2 * 16)(%ecx), %ymm1;
	vmovdqu (4 * 16)(%ecx), %ymm2;
	vmovdqu (6 * 16)(%ecx), %ymm3;
	vpxor %ymm4, %ymm0, %ymm0;
	vpxor %ymm4, %ymm1, %ymm1;
	vpxor %ymm4, %ymm2, %ymm2;
	vpxor %ymm4, %ymm3, %ymm3;
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	leal (8 * 16)(%ecx), %ecx;

	testl %esi, %esi;
	jz .Lecb_dec_blk8;
		/* AES rounds */
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (2 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (3 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (4 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (5 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (6 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (7 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (8 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (9 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (10 * 16)(%edi), %ymm4;
		cmpl $12, 8+24(%esp);
		jb .Lecb_enc_blk8_last;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (11 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (12 * 16)(%edi), %ymm4;
		jz .Lecb_enc_blk8_last;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (13 * 16)(%edi), %ymm4;
		VAESENC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (14 * 16)(%edi), %ymm4;
	  .Lecb_enc_blk8_last:
		vaesenclast %ymm4, %ymm0, %ymm0;
		vaesenclast %ymm4, %ymm1, %ymm1;
		vaesenclast %ymm4, %ymm2, %ymm2;
		vaesenclast %ymm4, %ymm3, %ymm3;
		vmovdqu %ymm0, (0 * 16)(%edx);
		vmovdqu %ymm1, (2 * 16)(%edx);
		vmovdqu %ymm2, (4 * 16)(%edx);
		vmovdqu %ymm3, (6 * 16)(%edx);
		leal (8 * 16)(%edx), %edx;
		jmp .Lecb_blk8;

	  .align 8
	  .Lecb_dec_blk8:
		/* AES rounds */
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (2 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (3 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (4 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (5 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (6 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (7 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (8 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (9 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (10 * 16)(%edi), %ymm4;
		cmpl $12, 8+24(%esp);
		jb .Lecb_dec_blk8_last;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (11 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (12 * 16)(%edi), %ymm4;
		jz .Lecb_dec_blk8_last;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (13 * 16)(%edi), %ymm4;
		VAESDEC4(%ymm4, %ymm0, %ymm1, %ymm2, %ymm3);
		vbroadcasti128 (14 * 16)(%edi), %ymm4;
	  .Lecb_dec_blk8_last:
		vaesdeclast %ymm4, %ymm0, %ymm0;
		vaesdeclast %ymm4, %ymm1, %ymm1;
		vaesdeclast %ymm4, %ymm2, %ymm2;
		vaesdeclast %ymm4, %ymm3, %ymm3;
		vmovdqu %ymm0, (0 * 16)(%edx);
		vmovdqu %ymm1, (2 * 16)(%edx);
		vmovdqu %ymm2, (4 * 16)(%edx);
		vmovdqu %ymm3, (6 * 16)(%edx);
		leal (8 * 16)(%edx), %edx;
		jmp .Lecb_blk8;

	/* Handle trailing four blocks. */
.align 8
.Lecb_blk4:
	cmpl $4, %eax;
	jb .Lecb_blk1;

	leal -4(%eax), %eax;

	/* Load input and xor first key. */
	vbroadcasti128 (0 * 16)(%edi), %ymm4;
	vmovdqu (0 * 16)(%ecx), %ymm0;
	vmovdqu (2 * 16)(%ecx), %ymm1;
	vpxor %ymm4, %ymm0, %ymm0;
	vpxor %ymm4, %ymm1, %ymm1;
	vbroadcasti128 (1 * 16)(%edi), %ymm4;
	leal (4 * 16)(%ecx), %ecx;

	testl %esi, %esi;
	jz .Lecb_dec_blk4;
		/* AES rounds */
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (2 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (3 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (4 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (5 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (6 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (7 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (8 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (9 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (10 * 16)(%edi), %ymm4;
		cmpl $12, 8+24(%esp);
		jb .Lecb_enc_blk4_last;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (11 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (12 * 16)(%edi), %ymm4;
		jz .Lecb_enc_blk4_last;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (13 * 16)(%edi), %ymm4;
		VAESENC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (14 * 16)(%edi), %ymm4;
	  .Lecb_enc_blk4_last:
		vaesenclast %ymm4, %ymm0, %ymm0;
		vaesenclast %ymm4, %ymm1, %ymm1;
		vmovdqu %ymm0, (0 * 16)(%edx);
		vmovdqu %ymm1, (2 * 16)(%edx);
		leal (4 * 16)(%edx), %edx;
		jmp .Lecb_blk1;

	  .align 8
	  .Lecb_dec_blk4:
		/* AES rounds */
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (2 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (3 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (4 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (5 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (6 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (7 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (8 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (9 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (10 * 16)(%edi), %ymm4;
		cmpl $12, 8+24(%esp);
		jb .Lecb_dec_blk4_last;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (11 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (12 * 16)(%edi), %ymm4;
		jz .Lecb_dec_blk4_last;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (13 * 16)(%edi), %ymm4;
		VAESDEC2(%ymm4, %ymm0, %ymm1);
		vbroadcasti128 (14 * 16)(%edi), %ymm4;
	  .Lecb_dec_blk4_last:
		vaesdeclast %ymm4, %ymm0, %ymm0;
		vaesdeclast %ymm4, %ymm1, %ymm1;
		vmovdqu %ymm0, (0 * 16)(%edx);
		vmovdqu %ymm1, (2 * 16)(%edx);
		leal (4 * 16)(%edx), %edx;

	/* Process trailing one to three blocks, one per loop. */
.align 8
.Lecb_blk1:
	cmpl $1, %eax;
	jb .Ldone_ecb;

	leal -1(%eax), %eax;

	/* Load input. */
	vmovdqu (%ecx), %xmm2;
	leal 16(%ecx), %ecx;

	/* Xor first key. */
	vpxor (0 * 16)(%edi), %xmm2, %xmm0;

	testl %esi, %esi;
	jz .Lecb_dec_blk1;
		/* AES rounds. */
		vaesenc (1 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (2 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (3 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (4 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (5 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (6 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (7 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (8 * 16)(%edi), %xmm0, %xmm0;
		vaesenc (9 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (10 * 16)(%edi), %xmm1;
		cmpl $12, 8+24(%esp);
		jb .Lecb_enc_blk1_last;
		vaesenc %xmm1, %xmm0, %xmm0;
		vaesenc (11 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (12 * 16)(%edi), %xmm1;
		jz .Lecb_enc_blk1_last;
		vaesenc %xmm1, %xmm0, %xmm0;
		vaesenc (13 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (14 * 16)(%edi), %xmm1;
	  .Lecb_enc_blk1_last:
		vaesenclast %xmm1, %xmm0, %xmm0;
		jmp .Lecb_blk1_end;

	  .align 8
	  .Lecb_dec_blk1:
		/* AES rounds. */
		vaesdec (1 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (2 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (3 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (4 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (5 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (6 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (7 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (8 * 16)(%edi), %xmm0, %xmm0;
		vaesdec (9 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (10 * 16)(%edi), %xmm1;
		cmpl $12, 8+24(%esp);
		jb .Lecb_dec_blk1_last;
		vaesdec %xmm1, %xmm0, %xmm0;
		vaesdec (11 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (12 * 16)(%edi), %xmm1;
		jz .Lecb_dec_blk1_last;
		vaesdec %xmm1, %xmm0, %xmm0;
		vaesdec (13 * 16)(%edi), %xmm0, %xmm0;
		vmovdqa (14 * 16)(%edi), %xmm1;
	  .Lecb_dec_blk1_last:
		vaesdeclast %xmm1, %xmm0, %xmm0;
		jmp .Lecb_blk1_end;

  .align 8
  .Lecb_blk1_end:
	vmovdqu %xmm0, (%edx);
	leal 16(%edx), %edx;

	jmp .Lecb_blk1;

.align 8
.Ldone_ecb:
	popl %esi;
	CFI_POP(%esi);
	popl %edi;
	CFI_POP(%edi);
	vzeroall;
	ret_spec_stop
	CFI_ENDPROC();
ELF(.size SYM_NAME(_gcry_vaes_avx2_ecb_crypt_i386),
	  .-SYM_NAME(_gcry_vaes_avx2_ecb_crypt_i386))

/**********************************************************************
  constants
 **********************************************************************/
SECTION_RODATA

ELF(.type SYM_NAME(_gcry_vaes_consts),@object)
.align 32
SYM_NAME(_gcry_vaes_consts):
.Lbige_addb_0:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lbige_addb_1:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
.Lbige_addb_2:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2
.Lbige_addb_3:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3
.Lbige_addb_4:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4
.Lbige_addb_5:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5
.Lbige_addb_6:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6
.Lbige_addb_7:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7
.Lbige_addb_8:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8
.Lbige_addb_9:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9
.Lbige_addb_10:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10
.Lbige_addb_11:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11

.Lle_addd_0:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_1:
	.byte 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_2:
	.byte 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_3:
	.byte 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_4:
	.byte 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_5:
	.byte 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_6:
	.byte 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_7:
	.byte 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_8:
	.byte 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_9:
	.byte 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_10:
	.byte 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_11:
	.byte 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

.Lle_addd_4_2:
	.byte 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	.byte 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.Lle_addd_12_2:
	.byte 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	.byte 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

.Lxts_gfmul_clmul:
	.long 0x00, 0x87, 0x00, 0x00
	.long 0x00, 0x87, 0x00, 0x00
.Lxts_high_bit_shuf:
	.byte -1, -1, -1, -1, 12, 13, 14, 15
	.byte 4, 5, 6, 7, -1, -1, -1, -1
	.byte -1, -1, -1, -1, 12, 13, 14, 15
	.byte 4, 5, 6, 7, -1, -1, -1, -1
.Lbswap128_mask:
	.byte 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
	.byte 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

ELF(.size SYM_NAME(_gcry_vaes_consts),.-SYM_NAME(_gcry_vaes_consts))

#endif /* HAVE_GCC_INLINE_ASM_VAES */
#endif /* __i386__ */
