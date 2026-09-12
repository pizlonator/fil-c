/* Pointer obfuscation implementation, assembly version.  SH version.
   Copyright (C) 2005-2026 Free Software Foundation, Inc.
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

#ifndef POINTER_GUARD_ASM_H
#define POINTER_GUARD_ASM_H

#ifdef __ASSEMBLER__
# if IS_IN (rtld) || !defined SHARED
#  define PTR_GUARD_SYM	__pointer_chk_guard_local
# else
#  define PTR_GUARD_SYM	__pointer_chk_guard
# endif
# ifdef PIC
#  define PTR_GUARD_LOAD(tmp)						\
	mov	r0, r3;							\
	mova	.Lptrg_got, r0;						\
	mov.l	.Lptrg_got, tmp;					\
	add	r0, tmp;						\
	mov.l	.Lptrg_sym, r0;						\
	mov.l	@(r0, tmp), tmp;					\
	mov	r3, r0;							\
	mov.l	@tmp, tmp;						\
	bra	.Lptrg_end;						\
	 nop;								\
	.align	2;							\
.Lptrg_got:								\
	.long	_GLOBAL_OFFSET_TABLE_;					\
.Lptrg_sym:								\
	.long	PTR_GUARD_SYM@GOT;					\
.Lptrg_end:
# else
#  define PTR_GUARD_LOAD(tmp)						\
	mov.l	.Lptrg_sym, tmp;					\
	mov.l	@tmp, tmp;						\
	bra	.Lptrg_end;						\
	 nop;								\
	.align	2;							\
.Lptrg_sym:								\
	.long	PTR_GUARD_SYM;						\
.Lptrg_end:
# endif
# define PTR_MANGLE(reg, tmp) \
     PTR_GUARD_LOAD (tmp); PTR_MANGLE2 (reg, tmp)
# define PTR_MANGLE2(reg, tmp) \
     xor tmp,reg;							\
     rotl reg; rotl reg; rotl reg; rotl reg; rotl reg;			\
     rotl reg; rotl reg; rotl reg; rotl reg
# define PTR_DEMANGLE(reg, tmp)        PTR_GUARD_LOAD (tmp); PTR_DEMANGLE2 (reg, tmp)
# define PTR_DEMANGLE2(reg, tmp) \
     rotr reg; rotr reg; rotr reg; rotr reg; rotr reg;			\
     rotr reg; rotr reg; rotr reg; rotr reg;				\
     xor tmp,reg
#endif

#endif /* POINTER_GUARD_ASM_H */
