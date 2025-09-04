/* RISC-V PLT trampoline
   Copyright (C) 2017-2024 Free Software Foundation, Inc.

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

#include "dl-link.h"

/* Assembler veneer called from the PLT header code for lazy loading.
   The PLT header passes its own args in t0-t2.  */

#ifdef __riscv_float_abi_soft
# define FRAME_SIZE (-((-10 * SZREG) & ALMASK))
#else
# define FRAME_SIZE (-((-10 * SZREG - 8 * SZFREG) & ALMASK))
#endif

ENTRY (_dl_runtime_resolve)
  # Save arguments to stack.
  addi sp, sp, -FRAME_SIZE
  REG_S ra, 9*SZREG(sp)
  REG_S a0, 1*SZREG(sp)
  REG_S a1, 2*SZREG(sp)
  REG_S a2, 3*SZREG(sp)
  REG_S a3, 4*SZREG(sp)
  REG_S a4, 5*SZREG(sp)
  REG_S a5, 6*SZREG(sp)
  REG_S a6, 7*SZREG(sp)
  REG_S a7, 8*SZREG(sp)

#ifndef __riscv_float_abi_soft
  FREG_S fa0, (10*SZREG + 0*SZFREG)(sp)
  FREG_S fa1, (10*SZREG + 1*SZFREG)(sp)
  FREG_S fa2, (10*SZREG + 2*SZFREG)(sp)
  FREG_S fa3, (10*SZREG + 3*SZFREG)(sp)
  FREG_S fa4, (10*SZREG + 4*SZFREG)(sp)
  FREG_S fa5, (10*SZREG + 5*SZFREG)(sp)
  FREG_S fa6, (10*SZREG + 6*SZFREG)(sp)
  FREG_S fa7, (10*SZREG + 7*SZFREG)(sp)
#endif

  # Update .got.plt and obtain runtime address of callee.
  slli a1, t1, 1
  mv a0, t0       # link map
  add a1, a1, t1  # reloc offset (== thrice the .got.plt offset)
  la a2, _dl_fixup
  jalr a2
  mv t1, a0

  # Restore arguments from stack.
  REG_L ra, 9*SZREG(sp)
  REG_L a0, 1*SZREG(sp)
  REG_L a1, 2*SZREG(sp)
  REG_L a2, 3*SZREG(sp)
  REG_L a3, 4*SZREG(sp)
  REG_L a4, 5*SZREG(sp)
  REG_L a5, 6*SZREG(sp)
  REG_L a6, 7*SZREG(sp)
  REG_L a7, 8*SZREG(sp)

#ifndef __riscv_float_abi_soft
  FREG_L fa0, (10*SZREG + 0*SZFREG)(sp)
  FREG_L fa1, (10*SZREG + 1*SZFREG)(sp)
  FREG_L fa2, (10*SZREG + 2*SZFREG)(sp)
  FREG_L fa3, (10*SZREG + 3*SZFREG)(sp)
  FREG_L fa4, (10*SZREG + 4*SZFREG)(sp)
  FREG_L fa5, (10*SZREG + 5*SZFREG)(sp)
  FREG_L fa6, (10*SZREG + 6*SZFREG)(sp)
  FREG_L fa7, (10*SZREG + 7*SZFREG)(sp)
#endif

  addi sp, sp, FRAME_SIZE

  # Invoke the callee.
  jr t1
END (_dl_runtime_resolve)

#if !defined PROF && defined SHARED
ENTRY (_dl_runtime_profile)
  /* RISC-V we get called with:
  t0          linkr_map pointer
  t1          the scaled offset stored in t0, which can be used
              to calculate the offset of the current symbol in .rela.plt
  t2          %hi(%pcrel(.got.plt)) stored in t2, no use in this function
  t3          dl resolver entry point, no use in this function

  Stack frame layout with hard float:
     RV64      RV32
  [sp, #96] [sp, #48]  La_riscv_regs
  [sp, #48] [sp, #24]  La_riscv_retval
  [sp, #40] [sp, #20]  frame size return from pltenter
  [sp, #32] [sp, #16]  dl_profile_call saved a1
  [sp, #24] [sp, #12]  dl_profile_call saved a0
  [sp, #16] [sp,  #8]  T1
  [sp,  #0] [sp,  #0]  ra, fp   <- fp
   */

# define OFFSET_T1              2*SZREG
# define OFFSET_SAVED_CALL_A0   OFFSET_T1 + SZREG
# define OFFSET_SAVED_CALL_A1   OFFSET_SAVED_CALL_A0 + SZREG
# define OFFSET_FS              OFFSET_SAVED_CALL_A1 + SZREG
# define OFFSET_RV              OFFSET_FS + SZREG
# define OFFSET_RG              OFFSET_RV + DL_SIZEOF_RV

# define SF_SIZE                (-(-(OFFSET_RG + DL_SIZEOF_RG) & ALMASK))

  # Save arguments to stack.
  add sp, sp, -SF_SIZE
  REG_S ra, 0(sp)
  REG_S fp, SZREG(sp)

  mv fp, sp

  REG_S a0, OFFSET_RG + DL_OFFSET_RG_A0 + 0*SZREG(fp)
  REG_S a1, OFFSET_RG + DL_OFFSET_RG_A0 + 1*SZREG(fp)
  REG_S a2, OFFSET_RG + DL_OFFSET_RG_A0 + 2*SZREG(fp)
  REG_S a3, OFFSET_RG + DL_OFFSET_RG_A0 + 3*SZREG(fp)
  REG_S a4, OFFSET_RG + DL_OFFSET_RG_A0 + 4*SZREG(fp)
  REG_S a5, OFFSET_RG + DL_OFFSET_RG_A0 + 5*SZREG(fp)
  REG_S a6, OFFSET_RG + DL_OFFSET_RG_A0 + 6*SZREG(fp)
  REG_S a7, OFFSET_RG + DL_OFFSET_RG_A0 + 7*SZREG(fp)

#ifndef __riscv_float_abi_soft
  FREG_S fa0, OFFSET_RG + DL_OFFSET_RG_FA0 + 0*SZFREG(fp)
  FREG_S fa1, OFFSET_RG + DL_OFFSET_RG_FA0 + 1*SZFREG(fp)
  FREG_S fa2, OFFSET_RG + DL_OFFSET_RG_FA0 + 2*SZFREG(fp)
  FREG_S fa3, OFFSET_RG + DL_OFFSET_RG_FA0 + 3*SZFREG(fp)
  FREG_S fa4, OFFSET_RG + DL_OFFSET_RG_FA0 + 4*SZFREG(fp)
  FREG_S fa5, OFFSET_RG + DL_OFFSET_RG_FA0 + 5*SZFREG(fp)
  FREG_S fa6, OFFSET_RG + DL_OFFSET_RG_FA0 + 6*SZFREG(fp)
  FREG_S fa7, OFFSET_RG + DL_OFFSET_RG_FA0 + 7*SZFREG(fp)
#endif

  # Update .got.plt and obtain runtime address of callee.
  slli a1, t1, 1
  mv a0, t0
  add a1, a1, t1          # link map
  mv a2, ra               # return addr
  addi a3, fp, OFFSET_RG  # La_riscv_regs pointer
  addi a4, fp, OFFSET_FS  # frame size return from pltenter

  REG_S a0, OFFSET_SAVED_CALL_A0(fp)
  REG_S a1, OFFSET_SAVED_CALL_A1(fp)

  la t2, _dl_profile_fixup
  jalr t2

  REG_L t3, OFFSET_FS(fp)
  bgez t3, 1f

  # Save the return.
  mv t4, a0

  # Restore arguments from stack.
  REG_L a0, OFFSET_RG + DL_OFFSET_RG_A0 + 0*SZREG(fp)
  REG_L a1, OFFSET_RG + DL_OFFSET_RG_A0 + 1*SZREG(fp)
  REG_L a2, OFFSET_RG + DL_OFFSET_RG_A0 + 2*SZREG(fp)
  REG_L a3, OFFSET_RG + DL_OFFSET_RG_A0 + 3*SZREG(fp)
  REG_L a4, OFFSET_RG + DL_OFFSET_RG_A0 + 4*SZREG(fp)
  REG_L a5, OFFSET_RG + DL_OFFSET_RG_A0 + 5*SZREG(fp)
  REG_L a6, OFFSET_RG + DL_OFFSET_RG_A0 + 6*SZREG(fp)
  REG_L a7, OFFSET_RG + DL_OFFSET_RG_A0 + 7*SZREG(fp)

#ifndef __riscv_float_abi_soft
  FREG_L fa0, OFFSET_RG + DL_OFFSET_RG_FA0 + 0*SZFREG(fp)
  FREG_L fa1, OFFSET_RG + DL_OFFSET_RG_FA0 + 1*SZFREG(fp)
  FREG_L fa2, OFFSET_RG + DL_OFFSET_RG_FA0 + 2*SZFREG(fp)
  FREG_L fa3, OFFSET_RG + DL_OFFSET_RG_FA0 + 3*SZFREG(fp)
  FREG_L fa4, OFFSET_RG + DL_OFFSET_RG_FA0 + 4*SZFREG(fp)
  FREG_L fa5, OFFSET_RG + DL_OFFSET_RG_FA0 + 5*SZFREG(fp)
  FREG_L fa6, OFFSET_RG + DL_OFFSET_RG_FA0 + 6*SZFREG(fp)
  FREG_L fa7, OFFSET_RG + DL_OFFSET_RG_FA0 + 7*SZFREG(fp)
#endif

  REG_L ra, 0(fp)
  REG_L fp, SZREG(fp)

  addi sp, sp, SF_SIZE
  jr t4

1:
  # The new frame size is in t3.
  sub sp, fp, t3
  andi sp, sp, ALMASK

  REG_S a0, OFFSET_T1(fp)

  mv a0, sp
  addi a1, fp, SF_SIZE
  mv a2, t3
  la t4, memcpy
  jalr t4

  REG_L t4, OFFSET_T1(fp)

  # Call the function.
  REG_L a0, OFFSET_RG + DL_OFFSET_RG_A0 + 0*SZREG(fp)
  REG_L a1, OFFSET_RG + DL_OFFSET_RG_A0 + 1*SZREG(fp)
  REG_L a2, OFFSET_RG + DL_OFFSET_RG_A0 + 2*SZREG(fp)
  REG_L a3, OFFSET_RG + DL_OFFSET_RG_A0 + 3*SZREG(fp)
  REG_L a4, OFFSET_RG + DL_OFFSET_RG_A0 + 4*SZREG(fp)
  REG_L a5, OFFSET_RG + DL_OFFSET_RG_A0 + 5*SZREG(fp)
  REG_L a6, OFFSET_RG + DL_OFFSET_RG_A0 + 6*SZREG(fp)
  REG_L a7, OFFSET_RG + DL_OFFSET_RG_A0 + 7*SZREG(fp)

#ifndef __riscv_float_abi_soft
  FREG_L fa0, OFFSET_RG + DL_OFFSET_RG_FA0 + 0*SZFREG(fp)
  FREG_L fa1, OFFSET_RG + DL_OFFSET_RG_FA0 + 1*SZFREG(fp)
  FREG_L fa2, OFFSET_RG + DL_OFFSET_RG_FA0 + 2*SZFREG(fp)
  FREG_L fa3, OFFSET_RG + DL_OFFSET_RG_FA0 + 3*SZFREG(fp)
  FREG_L fa4, OFFSET_RG + DL_OFFSET_RG_FA0 + 4*SZFREG(fp)
  FREG_L fa5, OFFSET_RG + DL_OFFSET_RG_FA0 + 5*SZFREG(fp)
  FREG_L fa6, OFFSET_RG + DL_OFFSET_RG_FA0 + 6*SZFREG(fp)
  FREG_L fa7, OFFSET_RG + DL_OFFSET_RG_FA0 + 7*SZFREG(fp)
#endif
  jalr t4

  REG_S a0, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_A0(fp)
  REG_S a1, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_A0 + SZREG(fp)

#ifndef __riscv_float_abi_soft
  FREG_S fa0, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_FA0(fp)
  FREG_S fa1, OFFSET_SAVED_CALL_A0 + DL_OFFSET_RV_FA0 + SZFREG(fp)
#endif

  # Setup call to pltexit.
  REG_L a0, OFFSET_SAVED_CALL_A0(fp)
  REG_L a1, OFFSET_SAVED_CALL_A0 + SZREG(fp)
  addi a2, fp, OFFSET_RG
  addi a3, fp, OFFSET_RV
  la t4, _dl_audit_pltexit
  jalr t4

  REG_L a0, OFFSET_RV + DL_OFFSET_RV_A0(fp)
  REG_L a1, OFFSET_RV + DL_OFFSET_RV_A0 + SZREG(fp)

#ifndef __riscv_float_abi_soft
  FREG_L fa0, OFFSET_RV + DL_OFFSET_RV_FA0(fp)
  FREG_L fa1, OFFSET_RV + DL_OFFSET_RV_FA0 + SZFREG(fp)
#endif

  # RA from within La_riscv_reg.
  REG_L ra, OFFSET_RG + DL_OFFSET_RG_RA(fp)
  mv sp, fp
  ADDI sp, sp, SF_SIZE
  REG_S fp, SZREG(fp)

  jr ra

END (_dl_runtime_profile)
#endif /* SHARED */
