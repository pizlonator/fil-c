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

	.hidden _dl_tlsdesc_dynamic
	.global	_dl_tlsdesc_dynamic
	.type	_dl_tlsdesc_dynamic,@function

     /* This function is used for symbols that need dynamic TLS.

	%eax points to the TLS descriptor, such that 0(%eax) points to
	_dl_tlsdesc_dynamic itself, and 4(%eax) points to a struct
	tlsdesc_dynamic_arg object.  It must return in %eax the offset
	between the thread pointer and the object denoted by the
	argument, without clobbering any registers.

	The assembly code that follows is a rendition of the following
	C code, hand-optimized a little bit.

ptrdiff_t
__attribute__ ((__regparm__ (1)))
_dl_tlsdesc_dynamic (struct tlsdesc *tdp)
{
  struct tlsdesc_dynamic_arg *td = tdp->arg;
  dtv_t *dtv = *(dtv_t **)((char *)__thread_pointer + DTV_OFFSET);
  if (__builtin_expect (td->gen_count <= dtv[0].counter
			&& (dtv[td->tlsinfo.ti_module].pointer.val
			    != TLS_DTV_UNALLOCATED),
			1))
    return dtv[td->tlsinfo.ti_module].pointer.val + td->tlsinfo.ti_offset
      - __thread_pointer;

  return ___tls_get_addr (&td->tlsinfo) - __thread_pointer;
}
*/
	cfi_startproc
	.align 16
_dl_tlsdesc_dynamic:
	/* Like all TLS resolvers, preserve call-clobbered registers.
	   We need two scratch regs anyway.  */
	subl	$32, %esp
	cfi_adjust_cfa_offset (32)
	movl	%ecx, 20(%esp)
	movl	%edx, 24(%esp)
	movl	TLSDESC_ARG(%eax), %eax
	movl	%gs:DTV_OFFSET, %edx
	movl	TLSDESC_GEN_COUNT(%eax), %ecx
	cmpl	(%edx), %ecx
	ja	2f
	movl	TLSDESC_MODID(%eax), %ecx
	movl	(%edx,%ecx,8), %edx
	cmpl	$-1, %edx
	je	2f
	movl	TLSDESC_MODOFF(%eax), %eax
	addl	%edx, %eax
1:
	movl	20(%esp), %ecx
	subl	%gs:0, %eax
	movl	24(%esp), %edx
	addl	$32, %esp
	cfi_adjust_cfa_offset (-32)
	ret
	.p2align 4,,7
2:
#include "tls-get-addr-wrapper.h"
	jmp	1b
	cfi_endproc
	.size	_dl_tlsdesc_dynamic, .-_dl_tlsdesc_dynamic

#undef STATE_SAVE_ALIGNMENT
