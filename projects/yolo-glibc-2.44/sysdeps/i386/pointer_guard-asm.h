/* Pointer obfuscation implementation, assembly version.  i386 version.
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

/* The PIC variants clobber TMP, a register name suffix as expected by
   LOAD_PIC_REG (e.g. dx for %edx), to load the guard value.  The caller
   must pass a register that is dead at the expansion point: the macros
   do not preserve it, and using a callee-saved register would leak the
   clobber to the caller of the enclosing function.  */
# if IS_IN (rtld) || !defined SHARED
#  ifdef PIC
#   define PTR_MANGLE(reg, tmp)	LOAD_PIC_REG (tmp);			      \
				xorl __pointer_chk_guard_local@GOTOFF(%e##tmp), reg; \
				roll $9, reg
#   define PTR_DEMANGLE(reg, tmp) \
				rorl $9, reg;				      \
				LOAD_PIC_REG (tmp);			      \
				xorl __pointer_chk_guard_local@GOTOFF(%e##tmp), reg
#  else
#   define PTR_MANGLE(reg, tmp)	xorl __pointer_chk_guard_local, reg;	      \
				roll $9, reg
#   define PTR_DEMANGLE(reg, tmp) \
				rorl $9, reg;				      \
				xorl __pointer_chk_guard_local, reg
#  endif
# else
#  define PTR_MANGLE(reg, tmp)	LOAD_PIC_REG (tmp);			      \
				movl __pointer_chk_guard@GOT(%e##tmp), %e##tmp; \
				xorl (%e##tmp), reg;			      \
				roll $9, reg
#  define PTR_DEMANGLE(reg, tmp) \
				rorl $9, reg;				      \
				LOAD_PIC_REG (tmp);			      \
				movl __pointer_chk_guard@GOT(%e##tmp), %e##tmp; \
				xorl (%e##tmp), reg
# endif
#endif

#endif /* POINTER_GUARD_ASM_H */
