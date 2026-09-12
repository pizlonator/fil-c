/* Pointer guard implementation, assembly version.  AArch64 version.
   Copyright (C) 2014-2026 Free Software Foundation, Inc.
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

#ifndef POINTER_GUARD_ASM_H
#define POINTER_GUARD_ASM_H

#ifdef __ASSEMBLER__
# if (IS_IN (rtld) \
      || (!defined SHARED && (IS_IN (libc) \
                              || IS_IN (libpthread))))
#  define PTR_GUARD_LOAD(tmp)						    \
	adrp    tmp, C_SYMBOL_NAME(__pointer_chk_guard_local);		    \
	ldr	tmp, [tmp, :lo12:C_SYMBOL_NAME(__pointer_chk_guard_local)]
# else
#  define PTR_GUARD_LOAD(tmp)						  \
	adrp	tmp, :got:C_SYMBOL_NAME(__pointer_chk_guard);		  \
	ldr	tmp, [tmp, :got_lo12:C_SYMBOL_NAME(__pointer_chk_guard)]; \
	ldr	tmp, [tmp]
# endif
#  define PTR_MANGLE(dst, src, tmp)					    \
	PTR_GUARD_LOAD (tmp);						    \
	eor	dst, src, tmp;						    \
	ror	dst, dst, #(64 - 17)
#  define PTR_DEMANGLE(dst, src, tmp)					    \
	PTR_GUARD_LOAD (tmp);						    \
	ror	dst, src, #17;						    \
	eor	dst, dst, tmp
#endif

#endif /* POINTER_GUARD_ASM_H */
