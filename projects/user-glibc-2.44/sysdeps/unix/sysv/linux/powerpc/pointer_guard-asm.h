/* Pointer obfuscation implementation, assembly version.  PowerPC version.
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
# include <sysdep.h>

# if IS_IN (rtld) || !defined SHARED
#  define PTR_GUARD_SYM	__pointer_chk_guard_local
# else
#  define PTR_GUARD_SYM	__pointer_chk_guard
# endif

# if defined(__PPC64__) || defined(__powerpc64__)
#  define PTR_GUARD_LOAD(tmpreg)					\
	addis	tmpreg,r2,PTR_GUARD_SYM@got@ha;				\
	ld	tmpreg,PTR_GUARD_SYM@got@l(tmpreg);			\
	ld	tmpreg,0(tmpreg)
# elif defined PIC
#  define PTR_GUARD_LOAD(tmpreg)					\
	mflr	r12;							\
	bcl	20,31,0f;						\
0:	mflr	tmpreg;							\
	addis	tmpreg,tmpreg,_GLOBAL_OFFSET_TABLE_-0b@ha;		\
	addi	tmpreg,tmpreg,_GLOBAL_OFFSET_TABLE_-0b@l;		\
	mtlr	r12;							\
	lwz	tmpreg,PTR_GUARD_SYM@got(tmpreg);			\
	lwz	tmpreg,0(tmpreg)
# else
/* Position-dependent 32-bit code can address the variable directly.  */
#  define PTR_GUARD_LOAD(tmpreg)					\
	lis	tmpreg,PTR_GUARD_SYM@ha;					\
	lwz	tmpreg,PTR_GUARD_SYM@l(tmpreg)
# endif

# if defined(__PPC64__) || defined(__powerpc64__)
#  define PTR_ROT_MANGLE(dst, src)	rotldi	dst,src,17
#  define PTR_ROT_DEMANGLE(dst, src)	rotldi	dst,src,47
# else
#  define PTR_ROT_MANGLE(dst, src)	rotlwi	dst,src,9
#  define PTR_ROT_DEMANGLE(dst, src)	rotlwi	dst,src,23
# endif

# define PTR_MANGLE(reg, tmpreg) \
	PTR_GUARD_LOAD (tmpreg); \
	xor	reg,tmpreg,reg; \
	PTR_ROT_MANGLE (reg, reg)
# define PTR_MANGLE2(reg, tmpreg) \
	xor	reg,tmpreg,reg; \
	PTR_ROT_MANGLE (reg, reg)
# define PTR_MANGLE3(destreg, reg, tmpreg) \
	PTR_GUARD_LOAD (tmpreg); \
	xor	destreg,tmpreg,reg; \
	PTR_ROT_MANGLE (destreg, destreg)
# define PTR_DEMANGLE(reg, tmpreg) \
	PTR_GUARD_LOAD (tmpreg); \
	PTR_ROT_DEMANGLE (reg, reg); \
	xor	reg,tmpreg,reg
# define PTR_DEMANGLE2(reg, tmpreg) \
	PTR_ROT_DEMANGLE (reg, reg); \
	xor	reg,tmpreg,reg
# define PTR_DEMANGLE3(destreg, reg, tmpreg) \
	PTR_GUARD_LOAD (tmpreg); \
	PTR_ROT_DEMANGLE (destreg, reg); \
	xor	destreg,tmpreg,destreg
#endif

#endif /* POINTER_GUARD_ASM_H */
