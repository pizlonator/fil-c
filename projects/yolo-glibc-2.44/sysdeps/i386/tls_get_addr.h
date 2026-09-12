/* Thread-local storage handling in the ELF dynamic linker.  i386 version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

	.hidden ___tls_get_addr
	.global	___tls_get_addr
	.type	___tls_get_addr,@function

	/* This function is a wrapper of ___tls_get_addr_internal to
	   preserve caller-saved vector registers.  */

	cfi_startproc
	.align 16
___tls_get_addr:
	/* Like all TLS resolvers, preserve call-clobbered registers.
	   We need two scratch regs anyway.  */
	subl	$32, %esp
	cfi_adjust_cfa_offset (32)
	movl	%ecx, 20(%esp)
	movl	%edx, 24(%esp)
#include "tls-get-addr-wrapper.h"
	movl	20(%esp), %ecx
	movl	24(%esp), %edx
	addl	$32, %esp
	cfi_adjust_cfa_offset (-32)
	ret
	cfi_endproc
	.size	___tls_get_addr, .-___tls_get_addr
