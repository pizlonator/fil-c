/* PLT trampolines.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

/* Assembler veneer called from the PLT header code for lazy loading.
   The PLT header passes its own args in t0-t2.  */
#ifdef USE_LASX
# define FRAME_SIZE (-((-9 * SZREG - 8 * SZXREG) & ALMASK))
#elif defined USE_LSX
# define FRAME_SIZE (-((-9 * SZREG - 8 * SZVREG) & ALMASK))
#elif !defined __loongarch_soft_float
# define FRAME_SIZE (-((-9 * SZREG - 8 * SZFREG) & ALMASK))
#else
# define FRAME_SIZE (-((-9 * SZREG) & ALMASK))
#endif

ENTRY (_dl_runtime_resolve)

	/* Save arguments to stack. */
	ADDI	sp, sp, -FRAME_SIZE

	REG_S	ra, sp, 0*SZREG
	REG_S	a0, sp, 1*SZREG
	REG_S	a1, sp, 2*SZREG
	REG_S	a2, sp, 3*SZREG
	REG_S	a3, sp, 4*SZREG
	REG_S	a4, sp, 5*SZREG
	REG_S	a5, sp, 6*SZREG
	REG_S	a6, sp, 7*SZREG
	REG_S	a7, sp, 8*SZREG

#ifdef USE_LASX
	xvst	xr0, sp, 9*SZREG + 0*SZXREG
	xvst	xr1, sp, 9*SZREG + 1*SZXREG
	xvst	xr2, sp, 9*SZREG + 2*SZXREG
	xvst	xr3, sp, 9*SZREG + 3*SZXREG
	xvst	xr4, sp, 9*SZREG + 4*SZXREG
	xvst	xr5, sp, 9*SZREG + 5*SZXREG
	xvst	xr6, sp, 9*SZREG + 6*SZXREG
	xvst	xr7, sp, 9*SZREG + 7*SZXREG
#elif defined USE_LSX
	vst	vr0, sp, 9*SZREG + 0*SZVREG
	vst	vr1, sp, 9*SZREG + 1*SZVREG
	vst	vr2, sp, 9*SZREG + 2*SZVREG
	vst	vr3, sp, 9*SZREG + 3*SZVREG
	vst	vr4, sp, 9*SZREG + 4*SZVREG
	vst	vr5, sp, 9*SZREG + 5*SZVREG
	vst	vr6, sp, 9*SZREG + 6*SZVREG
	vst	vr7, sp, 9*SZREG + 7*SZVREG
#elif !defined __loongarch_soft_float
	FREG_S	fa0, sp, 9*SZREG + 0*SZFREG
	FREG_S	fa1, sp, 9*SZREG + 1*SZFREG
	FREG_S	fa2, sp, 9*SZREG + 2*SZFREG
	FREG_S	fa3, sp, 9*SZREG + 3*SZFREG
	FREG_S	fa4, sp, 9*SZREG + 4*SZFREG
	FREG_S	fa5, sp, 9*SZREG + 5*SZFREG
	FREG_S	fa6, sp, 9*SZREG + 6*SZFREG
	FREG_S	fa7, sp, 9*SZREG + 7*SZFREG
#endif

	/* Update .got.plt and obtain runtime address of callee */
	SLLI	a1, t1, 1
	or	a0, t0, zero
	ADD	a1, a1, t1
	la	a2, _dl_fixup
	jirl	ra, a2, 0
	or	t1, v0, zero

	/* Restore arguments from stack. */
	REG_L	ra, sp, 0*SZREG
	REG_L	a0, sp, 1*SZREG
	REG_L	a1, sp, 2*SZREG
	REG_L	a2, sp, 3*SZREG
	REG_L	a3, sp, 4*SZREG
	REG_L	a4, sp, 5*SZREG
	REG_L	a5, sp, 6*SZREG
	REG_L	a6, sp, 7*SZREG
	REG_L	a7, sp, 8*SZREG

#ifdef USE_LASX
	xvld	xr0, sp, 9*SZREG + 0*SZXREG
	xvld	xr1, sp, 9*SZREG + 1*SZXREG
	xvld	xr2, sp, 9*SZREG + 2*SZXREG
	xvld	xr3, sp, 9*SZREG + 3*SZXREG
	xvld	xr4, sp, 9*SZREG + 4*SZXREG
	xvld	xr5, sp, 9*SZREG + 5*SZXREG
	xvld	xr6, sp, 9*SZREG + 6*SZXREG
	xvld	xr7, sp, 9*SZREG + 7*SZXREG
#elif defined USE_LSX
	vld	vr0, sp, 9*SZREG + 0*SZVREG
	vld	vr1, sp, 9*SZREG + 1*SZVREG
	vld	vr2, sp, 9*SZREG + 2*SZVREG
	vld	vr3, sp, 9*SZREG + 3*SZVREG
	vld	vr4, sp, 9*SZREG + 4*SZVREG
	vld	vr5, sp, 9*SZREG + 5*SZVREG
	vld	vr6, sp, 9*SZREG + 6*SZVREG
	vld	vr7, sp, 9*SZREG + 7*SZVREG
#elif !defined __loongarch_soft_float
	FREG_L	fa0, sp, 9*SZREG + 0*SZFREG
	FREG_L	fa1, sp, 9*SZREG + 1*SZFREG
	FREG_L	fa2, sp, 9*SZREG + 2*SZFREG
	FREG_L	fa3, sp, 9*SZREG + 3*SZFREG
	FREG_L	fa4, sp, 9*SZREG + 4*SZFREG
	FREG_L	fa5, sp, 9*SZREG + 5*SZFREG
	FREG_L	fa6, sp, 9*SZREG + 6*SZFREG
	FREG_L	fa7, sp, 9*SZREG + 7*SZFREG
#endif

	ADDI	sp, sp, FRAME_SIZE

	/* Invoke the callee. */
	jirl	zero, t1, 0
END (_dl_runtime_resolve)

#ifdef SHARED
#include "dl-link.h"

ENTRY (_dl_runtime_profile)
       /* LoongArch we get called with:
	t0	      linkr_map pointer
	t1	      the scaled offset stored in t0, which can be used
		      to calculate the offset of the current symbol in .rela.plt
	t2	      %hi(%pcrel(.got.plt)) stored in t2, no use in this function
	t3	      dl resolver entry point, no use in this function

	Stack frame layout:
	[sp,    #208] La_loongarch_regs
	[sp,    #128] La_loongarch_retval // align: 16
	[sp,    #112] frame size return from pltenter
	[sp,    #80 ] dl_profile_call saved vec1
	[sp,    #48 ] dl_profile_call saved vec0 // align: 16
	[sp,    #32 ] dl_profile_call saved a1
	[sp,    #24 ] dl_profile_call saved a0
	[sp,    #16 ] T1
	[sp,     #0 ] ra, fp   <- fp
       */

# define OFFSET_T1              16
# define OFFSET_SAVED_CALL_A0   OFFSET_T1 + 8
# define OFFSET_FS              OFFSET_SAVED_CALL_A0 + 16 + 8 + 64
# define OFFSET_RV              OFFSET_FS + 8 + 8
# define OFFSET_RG              OFFSET_RV + DL_SIZEOF_RV

# define SF_SIZE                (-(-(OFFSET_RG + DL_SIZEOF_RG) & ALMASK))

	/* Save arguments to stack. */
	ADDI	sp, sp, -SF_SIZE
	REG_S	ra, sp, 0
	REG_S	fp, sp, 8

	or	fp, sp, zero

	REG_S	a0, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 0*SZREG
	REG_S	a1, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 1*SZREG
	REG_S	a2, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 2*SZREG
	REG_S	a3, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 3*SZREG
	REG_S	a4, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 4*SZREG
	REG_S	a5, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 5*SZREG
	REG_S	a6, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 6*SZREG
	REG_S	a7, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 7*SZREG

#ifdef USE_LASX
	xvst	xr0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZXREG
	xvst	xr1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZXREG
	xvst	xr2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZXREG
	xvst	xr3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZXREG
	xvst	xr4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZXREG
	xvst	xr5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZXREG
	xvst	xr6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZXREG
	xvst	xr7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZXREG
#elif defined USE_LSX
	vst	vr0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZVREG
	vst	vr1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZVREG
	vst	vr2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZVREG
	vst	vr3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZVREG
	vst	vr4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZVREG
	vst	vr5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZVREG
	vst	vr6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZVREG
	vst	vr7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZVREG
#elif !defined __loongarch_soft_float
	FREG_S	fa0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZFREG
	FREG_S	fa1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZFREG
	FREG_S	fa2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZFREG
	FREG_S	fa3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZFREG
	FREG_S	fa4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZFREG
	FREG_S	fa5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZFREG
	FREG_S	fa6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZFREG
	FREG_S	fa7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZFREG
#endif

	/* Update .got.plt and obtain runtime address of callee.  */
	SLLI	a1, t1, 1
	or	a0, t0, zero
	ADD	a1, a1, t1
	or	a2, ra, zero		/* return addr */
	ADDI	a3, fp, OFFSET_RG	/* La_loongarch_regs pointer */
	ADDI	a4, fp, OFFSET_FS 	/* frame size return from pltenter */

	REG_S	a0, fp, OFFSET_SAVED_CALL_A0
	REG_S	a1, fp, OFFSET_SAVED_CALL_A0 + SZREG

	la	t2, _dl_profile_fixup
	jirl	ra, t2, 0

	REG_L	t3, fp, OFFSET_FS
	bge	t3, zero, 1f

	/* Save the return.  */
	or	t4, v0, zero

	/* Restore arguments from stack.  */
	REG_L	a0, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 0*SZREG
	REG_L	a1, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 1*SZREG
	REG_L	a2, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 2*SZREG
	REG_L	a3, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 3*SZREG
	REG_L	a4, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 4*SZREG
	REG_L	a5, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 5*SZREG
	REG_L	a6, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 6*SZREG
	REG_L	a7, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 7*SZREG

#ifdef USE_LASX
	xvld	xr0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZXREG
	xvld	xr1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZXREG
	xvld	xr2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZXREG
	xvld	xr3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZXREG
	xvld	xr4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZXREG
	xvld	xr5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZXREG
	xvld	xr6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZXREG
	xvld	xr7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZXREG
#elif defined USE_LSX
	vld	vr0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZVREG
	vld	vr1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZVREG
	vld	vr2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZVREG
	vld	vr3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZVREG
	vld	vr4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZVREG
	vld	vr5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZVREG
	vld	vr6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZVREG
	vld	vr7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZVREG
#elif !defined __loongarch_soft_float
	FREG_L	fa0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZFREG
	FREG_L	fa1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZFREG
	FREG_L	fa2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZFREG
	FREG_L	fa3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZFREG
	FREG_L	fa4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZFREG
	FREG_L	fa5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZFREG
	FREG_L	fa6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZFREG
	FREG_L	fa7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZFREG
#endif

	REG_L   ra, fp, 0
	REG_L   fp, fp, SZREG

	ADDI	sp, sp, SF_SIZE
	jirl	zero, t4, 0

1:
	/* The new frame size is in t3.  */
	SUB	sp, fp, t3
	BSTRINS sp, zero, 3, 0

	REG_S	a0, fp, OFFSET_T1

	or	a0, sp, zero
	ADDI	a1, fp, SF_SIZE
	or	a2, t3,	zero
	la	t5, memcpy
	jirl	ra, t5, 0

	REG_L	t6, fp, OFFSET_T1

	/* Call the function.  */
	REG_L	a0, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 0*SZREG
	REG_L	a1, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 1*SZREG
	REG_L	a2, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 2*SZREG
	REG_L	a3, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 3*SZREG
	REG_L	a4, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 4*SZREG
	REG_L	a5, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 5*SZREG
	REG_L	a6, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 6*SZREG
	REG_L	a7, fp, OFFSET_RG + DL_OFFSET_RG_A0 + 7*SZREG

#ifdef USE_LASX
	xvld	xr0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZXREG
	xvld	xr1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZXREG
	xvld	xr2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZXREG
	xvld	xr3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZXREG
	xvld	xr4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZXREG
	xvld	xr5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZXREG
	xvld	xr6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZXREG
	xvld	xr7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZXREG
#elif defined USE_LSX
	vld	vr0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZVREG
	vld	vr1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZVREG
	vld	vr2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZVREG
	vld	vr3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZVREG
	vld	vr4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZVREG
	vld	vr5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZVREG
	vld	vr6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZVREG
	vld	vr7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZVREG
#elif !defined __loongarch_soft_float
	FREG_L	fa0, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 0*SZFREG
	FREG_L	fa1, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 1*SZFREG
	FREG_L	fa2, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 2*SZFREG
	FREG_L	fa3, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 3*SZFREG
	FREG_L	fa4, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 4*SZFREG
	FREG_L	fa5, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 5*SZFREG
	FREG_L	fa6, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 6*SZFREG
	FREG_L	fa7, fp, OFFSET_RG + DL_OFFSET_RG_VEC0 + 7*SZFREG
#endif

	jirl	ra, t6, 0

	REG_S	a0, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_A0
	REG_S	a1, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_A0 + SZREG

#ifdef USE_LASX
	xvst	xr0, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0
	xvst	xr1, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0 + SZXREG
#elif defined USE_LSX
	vst	vr0, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0
	vst	vr1, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0 + SZVREG
#elif !defined __loongarch_soft_float
	FREG_S	fa0, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0
	FREG_S	fa1, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0 + SZFREG
#endif

	/* Setup call to pltexit.  */
	REG_L	a0, fp, OFFSET_SAVED_CALL_A0
	REG_L	a1, fp, OFFSET_SAVED_CALL_A0 + SZREG
	ADDI	a2, fp, OFFSET_RG
	ADDI	a3, fp, OFFSET_RV
	la	t7, _dl_audit_pltexit
	jirl	ra, t7, 0

	REG_L	a0, fp, OFFSET_RV + DL_OFFSET_RV_A0
	REG_L	a1, fp, OFFSET_RV + DL_OFFSET_RV_A0 + SZREG

#ifdef USE_LASX
	xvld	xr0, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0
	xvld	xr1, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0 + SZXREG
#elif defined USE_LSX
	vld	vr0, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0
	vld	vr1, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0 + SZVREG
#elif !defined __loongarch_soft_float
	FREG_L	fa0, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0
	FREG_L	fa1, fp, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_VEC0 + SZFREG
#endif

	/* RA from within La_loongarch_reg.  */
	REG_L   ra, fp, OFFSET_RG + DL_OFFSET_RG_RA
	or	sp, fp, zero
	ADDI	sp, sp, SF_SIZE
	REG_S   fp, fp, SZREG

	jirl	zero, ra, 0

END (_dl_runtime_profile)
#endif /* SHARED */
