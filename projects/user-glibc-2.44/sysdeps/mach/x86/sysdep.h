/* Copyright (C) 1991-2026 Free Software Foundation, Inc.

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

#ifndef _MACH_X86_SYSDEP_H
#define _MACH_X86_SYSDEP_H 1

/* Defines RTLD_PRIVATE_ERRNO and USE_DL_SYSINFO.  */
#include <dl-sysdep.h>
#include <tls.h>

#define LOSE asm volatile ("hlt")

#define STACK_GROWTH_DOWN

/* Get the machine-independent Mach definitions.  */
#include <sysdeps/mach/sysdep.h>

#undef ENTRY
#undef ALIGN

#ifndef __ASSEMBLER__
void return_to_trampoline(intptr_t *sp, void *pc, intptr_t retval)
    __attribute__((__noreturn__));
#endif

#define RETURN_TO return_to_trampoline

#ifdef __x86_64__
#define RETURN_TO_TRAMPOLINE() \
  asm ("return_to_trampoline:\n" \
       "movq %rdx, %rax\n" \
       "movq %rdi, %rsp\n" \
       "jmp *%rsi\n");
/* This should be rearranged, but at the moment this file provides
   the most useful definitions for assembler syntax details.  */
#include <sysdeps/unix/x86_64/sysdep.h>
#else
#define RETURN_TO_TRAMPOLINE() \
  asm ("return_to_trampoline:\n" \
       "movl 12(%esp), %eax\n" \
       "movl 8(%esp), %edx\n" \
       "movl 4(%esp), %esp\n" \
       "jmp *%edx\n");
#include <sysdeps/unix/i386/sysdep.h>
#endif

#endif /* mach/x86/sysdep.h */
