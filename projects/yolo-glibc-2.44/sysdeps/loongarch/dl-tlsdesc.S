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
	REG_L  a0, a0, SZREG
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
	REG_L	a0, a0, SZREG
	SUB	a0, a0, tp
	RET
	cfi_endproc
	.size	_dl_tlsdesc_undefweak, .-_dl_tlsdesc_undefweak

#ifdef SHARED

#ifndef __loongarch_soft_float

#define USE_LASX
#define _dl_tlsdesc_dynamic _dl_tlsdesc_dynamic_lasx
#define Lret Lret_lasx
#define Lslow Lslow_lasx
#include "dl-tlsdesc-dynamic.h"
#undef FRAME_SIZE
#undef V_REG_S
#undef V_REG_L
#undef V_SPACE
#undef V_REG
#undef V_REGS
#undef V_REGSZ
#undef USE_LASX
#undef _dl_tlsdesc_dynamic
#undef Lret
#undef Lslow

#define USE_LSX
#define _dl_tlsdesc_dynamic _dl_tlsdesc_dynamic_lsx
#define Lret Lret_lsx
#define Lslow Lslow_lsx
#include "dl-tlsdesc-dynamic.h"
#undef FRAME_SIZE
#undef V_REG_S
#undef V_REG_L
#undef V_SPACE
#undef V_REG
#undef V_REGS
#undef V_REGSZ
#undef USE_LSX
#undef _dl_tlsdesc_dynamic
#undef Lret
#undef Lslow

#endif /* #ifndef __loongarch_soft_float */

#include "dl-tlsdesc-dynamic.h"

#endif /* #ifdef SHARED */
