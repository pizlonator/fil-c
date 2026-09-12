/* Thread-local storage handling in the ELF dynamic linker.
   LoongArch version.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.

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

#define FRAME_SIZE	  (-((-14 * SZREG) & ALMASK))

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
	ADDI	sp, sp, -(4 * SZREG)
	cfi_adjust_cfa_offset (4 * SZREG)
	REG_S	t0, sp, 0
	REG_S	t1, sp, SZREG
	REG_S	t2, sp, 2 * SZREG
	cfi_rel_offset (12, 0)
	cfi_rel_offset (13, SZREG)
	cfi_rel_offset (14, 2 * SZREG)

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
	/* t1 = t1 * sizeof(dtv_pointer) = t1 * (2 * sizeof(void*)) */
	SLLI	t1, t1, (PTRLOG + 1)
	ADD	t1, t1, t0  /* t1 = dtv[td->tlsinfo.ti_module] */
	REG_L	t1, t1, 0   /* t1 = dtv[td->tlsinfo.ti_module].pointer.val */
	LI	t2, TLS_DTV_UNALLOCATED
	/* If dtv[td->tlsinfo.ti_module].pointer.val is TLS_DTV_UNALLOCATED,
	   goto slow path.  */
	beq	t1, t2, .Lslow

	cfi_remember_state
	REG_L	t2, a0, TLSDESC_MODOFF	/* t2 = td->tlsinfo.ti_offset */
	/* dtv[td->tlsinfo.ti_module].pointer.val + td->tlsinfo.ti_offset */
	ADD	a0, t1, t2
.Lret:
	SUB	a0, a0, tp
	REG_L	t0, sp, 0
	REG_L	t1, sp, SZREG
	REG_L	t2, sp, 2 * SZREG
	ADDI	sp, sp, 4 * SZREG
	cfi_adjust_cfa_offset (-(4 * SZREG))
	RET

.Lslow:
	/* This is the slow path.  We need to call __tls_get_addr() which
	   means we need to save and restore all the register that the
	   callee will trash.  */

	/* Save the remaining registers that we must treat as caller save.  */
	cfi_restore_state
	ADDI	sp, sp, -FRAME_SIZE
	cfi_adjust_cfa_offset (FRAME_SIZE)
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
	cfi_rel_offset (1, 0 * SZREG)
	cfi_rel_offset (5, 1 * SZREG)
	cfi_rel_offset (6, 2 * SZREG)
	cfi_rel_offset (7, 3 * SZREG)
	cfi_rel_offset (8, 4 * SZREG)
	cfi_rel_offset (9, 5 * SZREG)
	cfi_rel_offset (10, 6 * SZREG)
	cfi_rel_offset (11, 7 * SZREG)
	cfi_rel_offset (15, 8 * SZREG)
	cfi_rel_offset (16, 9 * SZREG)
	cfi_rel_offset (17, 10 * SZREG)
	cfi_rel_offset (18, 11 * SZREG)
	cfi_rel_offset (19, 12 * SZREG)
	cfi_rel_offset (20, 13 * SZREG)

#ifndef __loongarch_soft_float

	/* Save fcsr0 register.
	   Only one physical fcsr0 register, fcsr1-fcsr3 are aliases
	   of some fields in fcsr0.  */
	movfcsr2gr  t0, fcsr0
	/* Use the spare slot above t2.  */
	st.w	t0, sp, FRAME_SIZE + 3 * SZREG

#ifdef USE_LASX
  #define V_REG_S xvst
  #define V_REG_L xvld
  #define V_SPACE (-((-32 * SZXREG) & ALMASK)) /* Space for LASX registers.  */
  #define V_REG(n) $xr##n
  #define V_REGS 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,  \
		 17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
  #define V_REGSZ SZXREG
#elif defined USE_LSX
  #define V_REG_S vst
  #define V_REG_L vld
  #define V_SPACE (-((-32 * SZVREG) & ALMASK)) /* Space for LSX registers.  */
  #define V_REG(n) $vr##n
  #define V_REGS 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,  \
		 17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
  #define V_REGSZ SZVREG
#else
  #define V_REG_S fst.d
  #define V_REG_L fld.d
  #define V_SPACE (-((-24 * SZFREG) & ALMASK)) /* Space for FLOAT registers.  */
  #define V_REG(n) $f##n
  #define V_REGS 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
  #define V_REGSZ SZFREG
#endif

	ADDI	sp, sp, -V_SPACE
	cfi_adjust_cfa_offset (V_SPACE)
	.irp	i,V_REGS
        V_REG_S	V_REG(\i), sp, \i * V_REGSZ
	.endr

#endif /* #ifndef __loongarch_soft_float */

	bl	HIDDEN_JUMPTARGET(__tls_get_addr)
	ADDI	a0, a0, -TLS_DTV_OFFSET

#ifndef __loongarch_soft_float

	.irp	i,V_REGS
	V_REG_L	V_REG(\i), sp, \i * V_REGSZ
	.endr
	ADDI	sp, sp, V_SPACE
	cfi_adjust_cfa_offset (-V_SPACE)

	/* Restore fcsr0 register.  */
	ld.w	t0, sp, FRAME_SIZE + 3 * SZREG
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
	cfi_adjust_cfa_offset (-FRAME_SIZE)

	b	.Lret
	cfi_endproc
	.size	_dl_tlsdesc_dynamic, .-_dl_tlsdesc_dynamic
	.hidden HIDDEN_JUMPTARGET(__tls_get_addr)
