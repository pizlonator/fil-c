/* Thread-local storage handling in the ELF dynamic linker.
   LoongArch version.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <tls.h>
#include "tlsdesc.h"

	.text

	/* Compute the thread pointer offset for symbols in the static
	   TLS block. The offset is the same for all threads.
	   Prototype:
	   _dl_tlsdesc_return (tlsdesc *);  */
	.hidden _dl_tlsdesc_return
	.global	_dl_tlsdesc_return
	.type	_dl_tlsdesc_return,%function
	cfi_startproc
	.align 2
_dl_tlsdesc_return:
	REG_L  a0, a0, 8
	RET
	cfi_endproc
	.size	_dl_tlsdesc_return, .-_dl_tlsdesc_return

	/* Handler for undefined weak TLS symbols.
	   Prototype:
	   _dl_tlsdesc_undefweak (tlsdesc *);

	   The second word of the descriptor contains the addend.
	   Return the addend minus the thread pointer. This ensures
	   that when the caller adds on the thread pointer it gets back
	   the addend.  */
	.hidden _dl_tlsdesc_undefweak
	.global	_dl_tlsdesc_undefweak
	.type	_dl_tlsdesc_undefweak,%function
	cfi_startproc
	.align  2
_dl_tlsdesc_undefweak:
	REG_L	a0, a0, 8
	sub.d	a0, a0, tp
	RET
	cfi_endproc
	.size	_dl_tlsdesc_undefweak, .-_dl_tlsdesc_undefweak


#ifdef SHARED

#define FRAME_SIZE	  (-((-14 * SZREG) & ALMASK))
#define FRAME_SIZE_LSX	  (-((-32 * SZVREG) & ALMASK))
#define FRAME_SIZE_LASX	  (-((-32 * SZXREG) & ALMASK))
#define FRAME_SIZE_FLOAT  (-((-24 * SZFREG) & ALMASK))

	/* Handler for dynamic TLS symbols.
	   Prototype:
	   _dl_tlsdesc_dynamic (tlsdesc *) ;

	   The second word of the descriptor points to a
	   tlsdesc_dynamic_arg structure.

	   Returns the offset between the thread pointer and the
	   object referenced by the argument.

	   ptrdiff_t
	   _dl_tlsdesc_dynamic (struct tlsdesc *tdp)
	   {
	     struct tlsdesc_dynamic_arg *td = tdp->arg;
	     dtv_t *dtv = *(dtv_t **)((char *)__thread_pointer - SIZE_OF_TCB);
	     if (__glibc_likely (td->gen_count <= dtv[0].counter
		&& (dtv[td->tlsinfo.ti_module].pointer.val
		    != TLS_DTV_UNALLOCATED),
		1))
	       return dtv[td->tlsinfo.ti_module].pointer.val
		+ td->tlsinfo.ti_offset
		- __thread_pointer;

	     return ___tls_get_addr (&td->tlsinfo) - __thread_pointer;
	   }  */
	.hidden _dl_tlsdesc_dynamic
	.global	_dl_tlsdesc_dynamic
	.type	_dl_tlsdesc_dynamic,%function
	cfi_startproc
	.align 2
_dl_tlsdesc_dynamic:
	/* Save just enough registers to support fast path, if we fall
	   into slow path we will save additional registers.  */
	ADDI	sp, sp, -32
	REG_S	t0, sp, 0
	REG_S	t1, sp, 8
	REG_S	t2, sp, 16

/* Runtime Storage Layout of Thread-Local Storage
   TP point to the start of TLS block.

				      dtv
Low address	TCB ----------------> dtv0(counter)
	 TP -->	static_block0  <----- dtv1
		static_block1  <----- dtv2
		static_block2  <----- dtv3
		dynamic_block0 <----- dtv4
Hign address	dynamic_block1 <----- dtv5  */

	REG_L	t0, tp, -SIZE_OF_TCB	  /* t0 = dtv */
	REG_L	a0, a0, TLSDESC_ARG	  /* a0(td) = tdp->arg */
	REG_L	t1, a0, TLSDESC_GEN_COUNT /* t1 = td->gen_count */
	REG_L	t2, t0, DTV_COUNTER	  /* t2 = dtv[0].counter */
	/* If dtv[0].counter < td->gen_count, goto slow path.  */
	bltu	t2, t1, .Lslow

	REG_L	t1, a0, TLSDESC_MODID /* t1 = td->tlsinfo.ti_module */
	/* t1 = t1 * sizeof(dtv_t) = t1 * (2 * sizeof(void*)) */
	slli.d	t1, t1, 4
	add.d	t1, t1, t0  /* t1 = dtv[td->tlsinfo.ti_module] */
	REG_L	t1, t1, 0   /* t1 = dtv[td->tlsinfo.ti_module].pointer.val */
	li.d	t2, TLS_DTV_UNALLOCATED
	/* If dtv[td->tlsinfo.ti_module].pointer.val is TLS_DTV_UNALLOCATED,
	   goto slow path.  */
	beq	t1, t2, .Lslow

	REG_L	t2, a0, TLSDESC_MODOFF	/* t2 = td->tlsinfo.ti_offset */
	/* dtv[td->tlsinfo.ti_module].pointer.val + td->tlsinfo.ti_offset */
	add.d	a0, t1, t2
.Lret:
	sub.d	a0, a0, tp
	REG_L	t0, sp, 0
	REG_L	t1, sp, 8
	REG_L	t2, sp, 16
	ADDI	sp, sp, 32
	RET

.Lslow:
	/* This is the slow path. We need to call __tls_get_addr() which
	   means we need to save and restore all the register that the
	   callee will trash.  */

	/* Save the remaining registers that we must treat as caller save.  */
	ADDI	sp, sp, -FRAME_SIZE
	REG_S	ra, sp, 0 * SZREG
	REG_S	a1, sp, 1 * SZREG
	REG_S	a2, sp, 2 * SZREG
	REG_S	a3, sp, 3 * SZREG
	REG_S	a4, sp, 4 * SZREG
	REG_S	a5, sp, 5 * SZREG
	REG_S	a6, sp, 6 * SZREG
	REG_S	a7, sp, 7 * SZREG
	REG_S	t3, sp, 8 * SZREG
	REG_S	t4, sp, 9 * SZREG
	REG_S	t5, sp, 10 * SZREG
	REG_S	t6, sp, 11 * SZREG
	REG_S	t7, sp, 12 * SZREG
	REG_S	t8, sp, 13 * SZREG

#ifndef __loongarch_soft_float

	/* Save fcsr0 register.
	   Only one physical fcsr0 register, fcsr1-fcsr3 are aliases
	   of some fields in fcsr0.  */
	movfcsr2gr  t0, fcsr0
	st.w	t0, sp, FRAME_SIZE + 24 /* Use the spare slot above t2 */

	/* Whether support LASX.  */
	la.global   t0, _rtld_global_ro
	REG_L	t0, t0, GLRO_DL_HWCAP_OFFSET
	andi	t1, t0, HWCAP_LOONGARCH_LASX
	beqz	t1, .Llsx

	/* Save 256-bit vector registers.
	   FIXME: Without vector ABI, save all vector registers.  */
	ADDI	sp, sp, -FRAME_SIZE_LASX
	xvst	xr0, sp, 0*SZXREG
	xvst	xr1, sp, 1*SZXREG
	xvst	xr2, sp, 2*SZXREG
	xvst	xr3, sp, 3*SZXREG
	xvst	xr4, sp, 4*SZXREG
	xvst	xr5, sp, 5*SZXREG
	xvst	xr6, sp, 6*SZXREG
	xvst	xr7, sp, 7*SZXREG
	xvst	xr8, sp, 8*SZXREG
	xvst	xr9, sp, 9*SZXREG
	xvst	xr10, sp, 10*SZXREG
	xvst	xr11, sp, 11*SZXREG
	xvst	xr12, sp, 12*SZXREG
	xvst	xr13, sp, 13*SZXREG
	xvst	xr14, sp, 14*SZXREG
	xvst	xr15, sp, 15*SZXREG
	xvst	xr16, sp, 16*SZXREG
	xvst	xr17, sp, 17*SZXREG
	xvst	xr18, sp, 18*SZXREG
	xvst	xr19, sp, 19*SZXREG
	xvst	xr20, sp, 20*SZXREG
	xvst	xr21, sp, 21*SZXREG
	xvst	xr22, sp, 22*SZXREG
	xvst	xr23, sp, 23*SZXREG
	xvst	xr24, sp, 24*SZXREG
	xvst	xr25, sp, 25*SZXREG
	xvst	xr26, sp, 26*SZXREG
	xvst	xr27, sp, 27*SZXREG
	xvst	xr28, sp, 28*SZXREG
	xvst	xr29, sp, 29*SZXREG
	xvst	xr30, sp, 30*SZXREG
	xvst	xr31, sp, 31*SZXREG
	b	    .Ltga

.Llsx:
	/* Whether support LSX.  */
	andi	t1, t0, HWCAP_LOONGARCH_LSX
	beqz	t1, .Lfloat

	/* Save 128-bit vector registers.  */
	ADDI	sp, sp, -FRAME_SIZE_LSX
	vst	vr0, sp, 0*SZVREG
	vst	vr1, sp, 1*SZVREG
	vst	vr2, sp, 2*SZVREG
	vst	vr3, sp, 3*SZVREG
	vst	vr4, sp, 4*SZVREG
	vst	vr5, sp, 5*SZVREG
	vst	vr6, sp, 6*SZVREG
	vst	vr7, sp, 7*SZVREG
	vst	vr8, sp, 8*SZVREG
	vst	vr9, sp, 9*SZVREG
	vst	vr10, sp, 10*SZVREG
	vst	vr11, sp, 11*SZVREG
	vst	vr12, sp, 12*SZVREG
	vst	vr13, sp, 13*SZVREG
	vst	vr14, sp, 14*SZVREG
	vst	vr15, sp, 15*SZVREG
	vst	vr16, sp, 16*SZVREG
	vst	vr17, sp, 17*SZVREG
	vst	vr18, sp, 18*SZVREG
	vst	vr19, sp, 19*SZVREG
	vst	vr20, sp, 20*SZVREG
	vst	vr21, sp, 21*SZVREG
	vst	vr22, sp, 22*SZVREG
	vst	vr23, sp, 23*SZVREG
	vst	vr24, sp, 24*SZVREG
	vst	vr25, sp, 25*SZVREG
	vst	vr26, sp, 26*SZVREG
	vst	vr27, sp, 27*SZVREG
	vst	vr28, sp, 28*SZVREG
	vst	vr29, sp, 29*SZVREG
	vst	vr30, sp, 30*SZVREG
	vst	vr31, sp, 31*SZVREG
	b	    .Ltga

.Lfloat:
	/* Save float registers.  */
	ADDI	sp, sp, -FRAME_SIZE_FLOAT
	FREG_S	fa0, sp, 0*SZFREG
	FREG_S	fa1, sp, 1*SZFREG
	FREG_S	fa2, sp, 2*SZFREG
	FREG_S	fa3, sp, 3*SZFREG
	FREG_S	fa4, sp, 4*SZFREG
	FREG_S	fa5, sp, 5*SZFREG
	FREG_S	fa6, sp, 6*SZFREG
	FREG_S	fa7, sp, 7*SZFREG
	FREG_S	ft0, sp, 8*SZFREG
	FREG_S	ft1, sp, 9*SZFREG
	FREG_S	ft2, sp, 10*SZFREG
	FREG_S	ft3, sp, 11*SZFREG
	FREG_S	ft4, sp, 12*SZFREG
	FREG_S	ft5, sp, 13*SZFREG
	FREG_S	ft6, sp, 14*SZFREG
	FREG_S	ft7, sp, 15*SZFREG
	FREG_S	ft8, sp, 16*SZFREG
	FREG_S	ft9, sp, 17*SZFREG
	FREG_S	ft10, sp, 18*SZFREG
	FREG_S	ft11, sp, 19*SZFREG
	FREG_S	ft12, sp, 20*SZFREG
	FREG_S	ft13, sp, 21*SZFREG
	FREG_S	ft14, sp, 22*SZFREG
	FREG_S	ft15, sp, 23*SZFREG

#endif /* #ifndef __loongarch_soft_float */

.Ltga:
	bl	HIDDEN_JUMPTARGET(__tls_get_addr)
	ADDI	a0, a0, -TLS_DTV_OFFSET

#ifndef __loongarch_soft_float

	la.global   t0, _rtld_global_ro
	REG_L	t0, t0, GLRO_DL_HWCAP_OFFSET
	andi	t1, t0, HWCAP_LOONGARCH_LASX
	beqz	t1, .Llsx1

	/* Restore 256-bit vector registers.  */
	xvld	xr0, sp, 0*SZXREG
	xvld	xr1, sp, 1*SZXREG
	xvld	xr2, sp, 2*SZXREG
	xvld	xr3, sp, 3*SZXREG
	xvld	xr4, sp, 4*SZXREG
	xvld	xr5, sp, 5*SZXREG
	xvld	xr6, sp, 6*SZXREG
	xvld	xr7, sp, 7*SZXREG
	xvld	xr8, sp, 8*SZXREG
	xvld	xr9, sp, 9*SZXREG
	xvld	xr10, sp, 10*SZXREG
	xvld	xr11, sp, 11*SZXREG
	xvld	xr12, sp, 12*SZXREG
	xvld	xr13, sp, 13*SZXREG
	xvld	xr14, sp, 14*SZXREG
	xvld	xr15, sp, 15*SZXREG
	xvld	xr16, sp, 16*SZXREG
	xvld	xr17, sp, 17*SZXREG
	xvld	xr18, sp, 18*SZXREG
	xvld	xr19, sp, 19*SZXREG
	xvld	xr20, sp, 20*SZXREG
	xvld	xr21, sp, 21*SZXREG
	xvld	xr22, sp, 22*SZXREG
	xvld	xr23, sp, 23*SZXREG
	xvld	xr24, sp, 24*SZXREG
	xvld	xr25, sp, 25*SZXREG
	xvld	xr26, sp, 26*SZXREG
	xvld	xr27, sp, 27*SZXREG
	xvld	xr28, sp, 28*SZXREG
	xvld	xr29, sp, 29*SZXREG
	xvld	xr30, sp, 30*SZXREG
	xvld	xr31, sp, 31*SZXREG
	ADDI	sp, sp, FRAME_SIZE_LASX
	b .Lfcsr

.Llsx1:
	andi	t1, t0, HWCAP_LOONGARCH_LSX
	beqz	t1, .Lfloat1

	/* Restore 128-bit vector registers.  */
	vld	vr0, sp, 0*SZVREG
	vld	vr1, sp, 1*SZVREG
	vld	vr2, sp, 2*SZVREG
	vld	vr3, sp, 3*SZVREG
	vld	vr4, sp, 4*SZVREG
	vld	vr5, sp, 5*SZVREG
	vld	vr6, sp, 6*SZVREG
	vld	vr7, sp, 7*SZVREG
	vld	vr8, sp, 8*SZVREG
	vld	vr9, sp, 9*SZVREG
	vld	vr10, sp, 10*SZVREG
	vld	vr11, sp, 11*SZVREG
	vld	vr12, sp, 12*SZVREG
	vld	vr13, sp, 13*SZVREG
	vld	vr14, sp, 14*SZVREG
	vld	vr15, sp, 15*SZVREG
	vld	vr16, sp, 16*SZVREG
	vld	vr17, sp, 17*SZVREG
	vld	vr18, sp, 18*SZVREG
	vld	vr19, sp, 19*SZVREG
	vld	vr20, sp, 20*SZVREG
	vld	vr21, sp, 21*SZVREG
	vld	vr22, sp, 22*SZVREG
	vld	vr23, sp, 23*SZVREG
	vld	vr24, sp, 24*SZVREG
	vld	vr25, sp, 25*SZVREG
	vld	vr26, sp, 26*SZVREG
	vld	vr27, sp, 27*SZVREG
	vld	vr28, sp, 28*SZVREG
	vld	vr29, sp, 29*SZVREG
	vld	vr30, sp, 30*SZVREG
	vld	vr31, sp, 31*SZVREG
	ADDI	sp, sp, FRAME_SIZE_LSX
	b	    .Lfcsr

.Lfloat1:
	/* Restore float registers.  */
	FREG_L	fa0, sp, 0*SZFREG
	FREG_L	fa1, sp, 1*SZFREG
	FREG_L	fa2, sp, 2*SZFREG
	FREG_L	fa3, sp, 3*SZFREG
	FREG_L	fa4, sp, 4*SZFREG
	FREG_L	fa5, sp, 5*SZFREG
	FREG_L	fa6, sp, 6*SZFREG
	FREG_L	fa7, sp, 7*SZFREG
	FREG_L	ft0, sp, 8*SZFREG
	FREG_L	ft1, sp, 9*SZFREG
	FREG_L	ft2, sp, 10*SZFREG
	FREG_L	ft3, sp, 11*SZFREG
	FREG_L	ft4, sp, 12*SZFREG
	FREG_L	ft5, sp, 13*SZFREG
	FREG_L	ft6, sp, 14*SZFREG
	FREG_L	ft7, sp, 15*SZFREG
	FREG_L	ft8, sp, 16*SZFREG
	FREG_L	ft9, sp, 17*SZFREG
	FREG_L	ft10, sp, 18*SZFREG
	FREG_L	ft11, sp, 19*SZFREG
	FREG_L	ft12, sp, 20*SZFREG
	FREG_L	ft13, sp, 21*SZFREG
	FREG_L	ft14, sp, 22*SZFREG
	FREG_L	ft15, sp, 23*SZFREG
	ADDI	sp, sp, FRAME_SIZE_FLOAT

.Lfcsr:
	/* Restore fcsr0 register.  */
	ld.w	t0, sp, FRAME_SIZE + 24
	movgr2fcsr  fcsr0, t0

#endif /* #ifndef __loongarch_soft_float */

	REG_L	ra, sp, 0 * SZREG
	REG_L	a1, sp, 1 * SZREG
	REG_L	a2, sp, 2 * SZREG
	REG_L	a3, sp, 3 * SZREG
	REG_L	a4, sp, 4 * SZREG
	REG_L	a5, sp, 5 * SZREG
	REG_L	a6, sp, 6 * SZREG
	REG_L	a7, sp, 7 * SZREG
	REG_L	t3, sp, 8 * SZREG
	REG_L	t4, sp, 9 * SZREG
	REG_L	t5, sp, 10 * SZREG
	REG_L	t6, sp, 11 * SZREG
	REG_L	t7, sp, 12 * SZREG
	REG_L	t8, sp, 13 * SZREG
	ADDI	sp, sp, FRAME_SIZE

	b	.Lret
	cfi_endproc
	.size	_dl_tlsdesc_dynamic, .-_dl_tlsdesc_dynamic
	.hidden HIDDEN_JUMPTARGET(__tls_get_addr)

#endif /* #ifdef SHARED */
