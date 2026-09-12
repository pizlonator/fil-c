/* Thread-local storage handling in the ELF dynamic linker.  x86_64 version.
   Copyright (C) 2004-2026 Free Software Foundation, Inc.
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
#include <cpu-features-offsets.h>
#include <features-offsets.h>
#include <isa-level.h>
#include "tlsdesc.h"

/* Area on stack to save and restore registers used for parameter
   passing when calling _dl_tlsdesc_dynamic.  */
#define REGISTER_SAVE_RCX	0
#define REGISTER_SAVE_RDX	(REGISTER_SAVE_RCX + 8)
#define REGISTER_SAVE_R8	(REGISTER_SAVE_RDX + 8)
#define REGISTER_SAVE_R9	(REGISTER_SAVE_R8 + 8)
#define REGISTER_SAVE_R10	(REGISTER_SAVE_R9 + 8)
#define REGISTER_SAVE_R11	(REGISTER_SAVE_R10 + 8)

	.text

     /* This function is used to compute the TP offset for symbols in
	Static TLS, i.e., whose TP offset is the same for all
	threads.

	The incoming %rax points to the TLS descriptor, such that
	0(%rax) points to _dl_tlsdesc_return itself, and 8(%rax) holds
	the TP offset of the symbol corresponding to the object
	denoted by the argument.  */

	.hidden _dl_tlsdesc_return
	.global	_dl_tlsdesc_return
	.type	_dl_tlsdesc_return,@function
	cfi_startproc
	.align 16
_dl_tlsdesc_return:
	_CET_ENDBR
	movq	8(%rax), %rax
	ret
	cfi_endproc
	.size	_dl_tlsdesc_return, .-_dl_tlsdesc_return

     /* This function is used for undefined weak TLS symbols, for
	which the base address (i.e., disregarding any addend) should
	resolve to NULL.

	%rax points to the TLS descriptor, such that 0(%rax) points to
	_dl_tlsdesc_undefweak itself, and 8(%rax) holds the addend.
	We return the addend minus the TP, such that, when the caller
	adds TP, it gets the addend back.  If that's zero, as usual,
	that's most likely a NULL pointer.  */

	.hidden _dl_tlsdesc_undefweak
	.global	_dl_tlsdesc_undefweak
	.type	_dl_tlsdesc_undefweak,@function
	cfi_startproc
	.align 16
_dl_tlsdesc_undefweak:
	_CET_ENDBR
	movq	8(%rax), %rax
	sub	%fs:0, %RAX_LP
	ret
	cfi_endproc
	.size	_dl_tlsdesc_undefweak, .-_dl_tlsdesc_undefweak

#ifdef SHARED
# if MINIMUM_X86_ISA_LEVEL < AVX_X86_ISA_LEVEL
#  define USE_FXSAVE
#  define STATE_SAVE_ALIGNMENT	16
#  define _dl_tlsdesc_dynamic	_dl_tlsdesc_dynamic_fxsave
#  include "dl-tlsdesc-dynamic.h"
#  undef _dl_tlsdesc_dynamic
#  undef USE_FXSAVE
# endif

# define USE_XSAVE
# define STATE_SAVE_ALIGNMENT	64
# define _dl_tlsdesc_dynamic	_dl_tlsdesc_dynamic_xsave
# include "dl-tlsdesc-dynamic.h"
# undef _dl_tlsdesc_dynamic
# undef USE_XSAVE

# define USE_XSAVEC
# define STATE_SAVE_ALIGNMENT	64
# define _dl_tlsdesc_dynamic	_dl_tlsdesc_dynamic_xsavec
# include "dl-tlsdesc-dynamic.h"
# undef _dl_tlsdesc_dynamic
# undef USE_XSAVEC
#endif /* SHARED */
