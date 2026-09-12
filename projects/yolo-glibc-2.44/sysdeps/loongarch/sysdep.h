/* Macros for LoongArch.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#ifndef _LOONGARCH_SYSDEP_H
#define _LOONGARCH_SYSDEP_H

#if __loongarch_grlen == 64

#define PTRLOG 3
/* Align reg to 2^n.  Used in C.  */
#define REG_ALIGN_C(reg, n) \
  "bstrins.d\t" __STRING(reg) ", $zero, (" __STRING(n) "-1), 0"

#elif __loongarch_grlen == 32

#define PTRLOG 2
#define REG_ALIGN_C(reg, n) \
  "srli.w\t" __STRING(reg)", " __STRING(reg)", " __STRING(n) "\n\t" \
  "slli.w\t" __STRING(reg)", " __STRING(reg)", " __STRING(n)

#else
#error __loongarch_grlen must equal 32 or 64
#endif

#ifdef	__ASSEMBLER__

/* Stack alignment bytes.  */
#define STACK_ALIGN 16

/* Macros to handle different pointer/register sizes for 32/64-bit code.  */
#if __loongarch_grlen == 64
#define SRAI srai.d

/* Align reg to 2^n.  Used in assembly.  */
#define REG_ALIGN_ASM(reg, n) bstrins.d reg, zero, (n-1), 0

#define LOAD_LOCAL(reg, sym) \
  pcalau12i reg, %pc_hi20(sym); \
  ld.d	    reg, reg, %pc_lo12(sym);

#define LOAD_GLOBAL(reg, sym) \
  la.got    reg, sym; \
  ld.d	    reg, reg, 0;

#define LA_GOT(reg, sym) la.got reg, t0, sym

#define CALL(sym) call36 sym
#define TAIL(sym) tail36 t0, sym

#elif __loongarch_grlen == 32 /* __loongarch_grlen == 64 */

#define SRAI srai.w

/* LA32R not have bstrins.w, use srli.w and slli.w on both LA32S and LA32R.  */
#define REG_ALIGN_ASM(reg, n) \
  srli.w reg, reg, n; \
  slli.w reg, reg, n;

#define LOAD_LOCAL(reg, sym) \
  1: pcaddu12i	reg, %pcadd_hi20(sym); \
     ld.w	reg, reg, %pcadd_lo12(1b);

#define LOAD_GLOBAL(reg, sym) \
  1: pcaddu12i	reg, %got_pcadd_hi20(sym); \
     ld.w	reg, reg, %pcadd_lo12(1b); \
     ld.w	reg, reg, 0;

#define LA_GOT(reg, sym) la.got reg, sym

#define CALL(sym) call30 sym
#define TAIL(sym) tail30 t0, sym

#else /* __loongarch_grlen == 64 */
#error __loongarch_grlen must equal 32 or 64
#endif /* __loongarch_grlen == 64 */

#endif	/* __ASSEMBLER__ */

#endif  /* _LOONGARCH_SYSDEP_H */
