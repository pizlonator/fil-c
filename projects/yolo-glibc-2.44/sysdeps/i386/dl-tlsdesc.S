/* Thread-local storage handling in the ELF dynamic linker.  i386 version.
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
#include "tlsdesc.h"

	.text

     /* This function is used to compute the TP offset for symbols in
	Static TLS, i.e., whose TP offset is the same for all
	threads.

	The incoming %eax points to the TLS descriptor, such that
	0(%eax) points to _dl_tlsdesc_return itself, and 4(%eax) holds
	the TP offset of the symbol corresponding to the object
	denoted by the argument.  */

	.hidden _dl_tlsdesc_return
	.global	_dl_tlsdesc_return
	.type	_dl_tlsdesc_return,@function
	cfi_startproc
	.align 16
_dl_tlsdesc_return:
	movl	4(%eax), %eax
	ret
	cfi_endproc
	.size	_dl_tlsdesc_return, .-_dl_tlsdesc_return

     /* This function is used for undefined weak TLS symbols, for
	which the base address (i.e., disregarding any addend) should
	resolve to NULL.

	%eax points to the TLS descriptor, such that 0(%eax) points to
	_dl_tlsdesc_undefweak itself, and 4(%eax) holds the addend.
	We return the addend minus the TP, such that, when the caller
	adds TP, it gets the addend back.  If that's zero, as usual,
	that's most likely a NULL pointer.  */

	.hidden _dl_tlsdesc_undefweak
	.global	_dl_tlsdesc_undefweak
	.type	_dl_tlsdesc_undefweak,@function
	cfi_startproc
	.align 16
_dl_tlsdesc_undefweak:
	movl	4(%eax), %eax
	subl	%gs:0, %eax
	ret
	cfi_endproc
	.size	_dl_tlsdesc_undefweak, .-_dl_tlsdesc_undefweak

#ifdef SHARED
# define USE_FNSAVE
# define MINIMUM_ALIGNMENT	4
# define STATE_SAVE_ALIGNMENT	4
# define _dl_tlsdesc_dynamic	_dl_tlsdesc_dynamic_fnsave
# include "dl-tlsdesc-dynamic.h"
# undef _dl_tlsdesc_dynamic
# undef MINIMUM_ALIGNMENT
# undef USE_FNSAVE

# define MINIMUM_ALIGNMENT	16

# define USE_FXSAVE
# define STATE_SAVE_ALIGNMENT	16
# define _dl_tlsdesc_dynamic	_dl_tlsdesc_dynamic_fxsave
# include "dl-tlsdesc-dynamic.h"
# undef _dl_tlsdesc_dynamic
# undef USE_FXSAVE

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
